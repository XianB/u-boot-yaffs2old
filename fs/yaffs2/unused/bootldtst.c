

#include <stdio.h>

#include "yaffs_guts.h"
#include "yaffs_flashif.h"

#include "yboot.h"


unsigned yaffs_traceMask = 0xFFFFFFFF;

static void InitDevice(yaffs_Device *dev)
{
	// Initialise the NAND device. This should agree with what is set in yaffscfg for /boot
	
	// /boot
	// Only some of these parameters are actually used.
	dev->nBytesPerChunk = YAFFS_BYTES_PER_CHUNK;
	dev->nChunksPerBlock = YAFFS_CHUNKS_PER_BLOCK;
	dev->startBlock = 1; // Can't use block 0
	dev->endBlock = 127; // Last block in 2MB.	
	dev->useNANDECC = 0; // use YAFFS's ECC
	dev->nShortOpCaches = 10; // Use caches
	dev->genericDevice = (void *) 1;	// Used to identify the device in fstat.
	dev->writeChunkToNAND = yflash_WriteChunkToNAND;
	dev->readChunkFromNAND = yflash_ReadChunkFromNAND;
	dev->eraseBlockInNAND = yflash_EraseBlockInNAND;
	dev->initialiseNAND = yflash_InitialiseNAND;
}


int main()
{
	int oId;
	
	char ch;
	int nBytes = 0;
	int fsize;
	
	yaffs_Device dev;
	
	printf("Test boot code\n");
	
	InitDevice(&dev);
	
	oId = yaffsboot_InitFile(&dev,"yyfile",&fsize);
	
	printf("ObjectId = %d, size is %d\n",oId,fsize);
	
	if(oId < 0)
	{
		printf("File not found\n");
	}
	else
	{
		printf("dumping file as text\n\n");
		
		nBytes = 0;
		
		while(yaffsboot_ReadByte(&ch) >= 0)
		{
			printf("%c",ch);
			nBytes++;
		}
		
		printf("\n\n%d bytes read\n",nBytes);
	}
	
	
	
	
}
