/*
 * YAFFS: Yet another FFS. A NAND-flash specific file system.
 * yboot: A yaffs bootloader.
 *
 * Copyright (C) 2002 Aleph One Ltd.
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 *
 * Note: This code (yboot.c) and YAFFS headers are LGPL, the rest of YAFFS C code is covered by GPL.
 *       The rationale behind this is to allow easy incorporation of yaffs booting with 
 *       prorietary code.
 *
 */
#include <string.h>
#include <stdio.h>
#include "yaffs_guts.h"

const char *yboot_c_version="$Id: yboot.c,v 1.2 2005-07-19 19:51:57 charles Exp $";


#define MAX_FILE_SIZE	4000000
#define MAX_CHUNKS	(MAX_FILE_SIZE/YAFFS_BYTES_PER_CHUNK + 1)

static int chunkLocations[MAX_CHUNKS];


// External functions for ECC on data
void nand_calculate_ecc (const unsigned char*dat, unsigned char*ecc_code);
int nand_correct_data (unsigned char*dat, unsigned char*read_ecc, unsigned char*calc_ecc);

static const char yaffs_countBits[256] =
{
0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
};


static void spareToTags(yaffs_Spare *spare, yaffs_Tags *tag)
{
	unsigned char *bytes = (char *)tag;
	bytes[0] = spare->tagByte0;
	bytes[1] = spare->tagByte1;
	bytes[2] = spare->tagByte2;
	bytes[3] = spare->tagByte3;
	bytes[4] = spare->tagByte4;
	bytes[5] = spare->tagByte5;
	bytes[6] = spare->tagByte6;
	bytes[7] = spare->tagByte7;
}



// yboot_ScanForFile finds
static int yaffsboot_ScanForFile(yaffs_Device *dev, const char *fileName)
{
	int pg;
	int blk;
	yaffs_ObjectHeader data;
	yaffs_Spare spare;
	yaffs_Tags tags;


	if (!fileName)
	{
		//printf("NULL filename\n");
		return -1;
	}



	//printf("Searching block range %d to %d for %s\n", dev->startBlock, dev->endBlock, fileName);

	for (blk = dev->startBlock; blk < dev->endBlock; blk++)
	{
	
		for (pg = 0; pg < YAFFS_CHUNKS_PER_BLOCK; pg++)
		{
			dev->readChunkFromNAND(dev, (blk*YAFFS_CHUNKS_PER_BLOCK) + pg, (__u8 *)&data, (yaffs_Spare *)&spare);
			if (yaffs_countBits[spare.blockStatus] >=7 && // block OK
				yaffs_countBits[spare.pageStatus] >= 7)   // page ok
			{
				spareToTags(&spare, &tags);
				
				if ( tags.chunkId == 0 &&  // it's a header
				     data.parentObjectId == YAFFS_OBJECTID_ROOT && // it's in the root
				     strcmp(data.name, fileName) == 0 // name matches
				)
				{
					//printf("%s found at chunk %x objectId is %x\n", fileName, blk* YAFFS_PAGES_PER_BLOCK + pg, tag.objectId);
					
					return tags.objectId;
				}
			}
		}
	}
	// Sad day... not found.
	// printf("%s not found\n",filename);
	return -1;
}



static unsigned char bufferData[YAFFS_BYTES_PER_CHUNK];
static int bufferPos = 0;
static int bufferChunk = 0;
static int bufferInitialised = 0;
static int bufferLength = 0;

static yaffs_Device *bufferDevice;

static int chunkEnd = -1;
static int topChunk = -1;
static int fileSize = -1;

int yaffsboot_InitFile(yaffs_Device *dev, const char *fileName,int *loadedFileSize)
{

	yaffs_Spare spare;
	yaffs_Tags  tags;
	int fileObjId;
	int i;
	
	int blk;
	int pg;
	int missing;
	int chunksMissing = 0;

	bufferDevice = dev;	
	
	fileObjId = yaffsboot_ScanForFile(dev,fileName);

	if(fileObjId < 0)
	{
		return -1;
	}
	


	//printf("Gathering chunks...\n");
	
	for (i = 0; i < MAX_CHUNKS; i++)
	{
		chunkLocations[i] = -1;
	}


	for (blk = dev->startBlock; blk <= dev->endBlock; blk++)
	{
		for (pg = 0; pg < YAFFS_CHUNKS_PER_BLOCK; pg++)
		{
			dev->readChunkFromNAND(dev, blk * YAFFS_CHUNKS_PER_BLOCK + pg, NULL, &spare);
			if (yaffs_countBits[spare.blockStatus] >= 7 &&
			    yaffs_countBits[spare.pageStatus] >= 7)
			{
				spareToTags(&spare, &tags);

				if (tags.objectId == fileObjId && tags.chunkId > 0)
				{

					if(tags.chunkId >= MAX_CHUNKS)
					{
						printf("Chunk %d out of bounds (max is %d)\n",tags.chunkId, MAX_CHUNKS - 1);
						return -1;
					}

					chunkLocations[tags.chunkId] = (blk*32) + pg;
					
					chunkEnd = (tags.chunkId -1) * YAFFS_BYTES_PER_CHUNK + tags.byteCount;
					
					if(chunkEnd > fileSize) fileSize = chunkEnd;
    					
					if(tags.chunkId > topChunk) topChunk = tags.chunkId;
				}
			}
		}
	}


	for (missing= 0, i= 1; i<= topChunk; i++)
	{
		if (chunkLocations[i] < 0)
		{
			//printf("chunk %x missing\n",i);
			chunksMissing++;
		}
	}

	*loadedFileSize = fileSize;
	
	return fileObjId;
}



int yaffsboot_Reinitialise(void)
{
	bufferInitialised = 0;
	return 0;
}


int yaffsboot_ReadByte(unsigned char *bPtr)
{
	if(!bufferInitialised)
	{
		//printf("Read buffer initialisation\n");
		bufferInitialised = 1;
		bufferChunk = 0;
		bufferLength = 0;
		bufferPos = -1;
	}

	if(bufferPos < 0)
	{
		bufferChunk++;
		if(bufferChunk> topChunk)
		{
			printf("Chunk %d past end of file\r\n",bufferChunk);

			return -1;
		}

		if (chunkLocations[bufferChunk] < 0)
		{
				printf("No chunk %d, zero page\n",bufferChunk);
				memset(bufferData,0,YAFFS_BYTES_PER_CHUNK);
				bufferLength = YAFFS_BYTES_PER_CHUNK;
		}
		else
		{
			yaffs_Spare localSpare;
			yaffs_Tags tags;
			__u8 calcEcc[3];

			bufferDevice->readChunkFromNAND(bufferDevice, chunkLocations[bufferChunk], bufferData, &localSpare);

			spareToTags(&localSpare, &tags);

			if(0 && bufferChunk <topChunk)
			{
				bufferLength = YAFFS_BYTES_PER_CHUNK;
			}
			else
			{
				bufferLength = tags.byteCount;
			}

			//printf("Read chunk %d, size %d bytes\n",bufferChunk,bufferLength);

			nand_calculate_ecc(bufferData,calcEcc);
			nand_correct_data (bufferData,localSpare.ecc1, calcEcc);
			nand_calculate_ecc(&bufferData[256],calcEcc);
			nand_correct_data (&bufferData[256],localSpare.ecc2, calcEcc);
		}

		bufferPos = 0;

		if(bufferLength <= bufferPos)
		{
			return -1;
		}
	}

	*bPtr = bufferData[bufferPos];
	bufferPos++;
	if(bufferPos >= bufferLength)
	{
		//printf("End of page %d at byte %d\r\n",bufferChunk,bufferLength);
		bufferPos = -1;
	}
	return 0;
}




