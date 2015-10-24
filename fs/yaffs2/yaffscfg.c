/*
 * YAFFS: Yet another FFS. A NAND-flash specific file system.
 * yaffscfg.c  The configuration for the "direct" use of yaffs.
 *
 * This file is intended to be modified to your requirements.
 * There is no need to redistribute this file.
 */

#include <config.h>
#include "yaffscfg.h"
#include "yaffsfs.h"
#include "nand.h"
#include "yaffs_mtdif.h"
//#include <errno.h>

unsigned yaffs_traceMask = 0xFFFFFFFF;


static int yaffs_errno;

static int isMounted = 0;
#define MOUNT_POINT "/flash"
extern nand_info_t nand_info[];

void yaffsfs_SetError(int err)
{
	//Do whatever to set error
	yaffs_errno = err;
}

void yaffsfs_Lock(void)
{
}

void yaffsfs_Unlock(void)
{
}

__u32 yaffsfs_CurrentTime(void)
{
	return 0;
}

void yaffsfs_LocalInitialisation(void)
{
	// Define locking semaphore.
}

// Configuration for:
// /ram  2MB ramdisk
// /boot 2MB boot disk (flash)
// /flash 14MB flash disk (flash)
// NB Though /boot and /flash occupy the same physical device they
// are still disticnt "yaffs_Devices. You may think of these as "partitions"
// using non-overlapping areas in the same device.
// 

//#include "yaffs_ramdisk.h"
//#include "yaffs_flashif.h"

static yaffs_Device ramDev;
static yaffs_Device bootDev;
static yaffs_Device flashDev;

static yaffsfs_DeviceConfiguration yaffsfs_config[] = {

//	{ "/ram", &ramDev},
//	{ "/boot", &bootDev},
	{ "/flash", &flashDev},
	{(void *)0,(void *)0}
};


int yaffs_StartUp(void)
{
	// Stuff to configure YAFFS
	// Stuff to initialise anything special (eg lock semaphore).
	yaffsfs_LocalInitialisation();
	
	// Set up devices

#if 0
	// /ram
	ramDev.nBytesPerChunk = YAFFS_BYTES_PER_CHUNK;
	ramDev.nChunksPerBlock = YAFFS_CHUNKS_PER_BLOCK;
	ramDev.nReservedBlocks = 2; // Set this smaller for RAM
	ramDev.startBlock = 0; // Can now use block zero
	ramDev.endBlock = 127; // Last block in 2MB.	
	ramDev.useNANDECC = 1;
	ramDev.nShortOpCaches = 0;	// Disable caching on this device.
	ramDev.genericDevice = (void *) 0;	// Used to identify the device in fstat.
	ramDev.writeChunkToNAND = yramdisk_WriteChunkToNAND;
	ramDev.readChunkFromNAND = yramdisk_ReadChunkFromNAND;
	ramDev.eraseBlockInNAND = yramdisk_EraseBlockInNAND;
	ramDev.initialiseNAND = yramdisk_InitialiseNAND;

	// /boot
	bootDev.nBytesPerChunk = YAFFS_BYTES_PER_CHUNK;
	bootDev.nChunksPerBlock = YAFFS_CHUNKS_PER_BLOCK;
	bootDev.nReservedBlocks = 5;
	bootDev.startBlock = 0; // Can now use block zero
	bootDev.endBlock = 127; // Last block in 2MB.	
	bootDev.useNANDECC = 0; // use YAFFS's ECC
	bootDev.nShortOpCaches = 10; // Use caches
	bootDev.genericDevice = (void *) 1;	// Used to identify the device in fstat.
	bootDev.writeChunkToNAND = yflash_WriteChunkToNAND;
	bootDev.readChunkFromNAND = yflash_ReadChunkFromNAND;
	bootDev.eraseBlockInNAND = yflash_EraseBlockInNAND;
	bootDev.initialiseNAND = yflash_InitialiseNAND;
#endif

	struct mtd_info *mtd = &nand_info[0];
		// /flash
	flashDev.nBytesPerChunk = YAFFS_BYTES_PER_CHUNK;
	flashDev.nChunksPerBlock = YAFFS_CHUNKS_PER_BLOCK;
	flashDev.nReservedBlocks = 5;
	flashDev.startBlock = 10; // First block after 2MB
	flashDev.endBlock = 40; // Last block in 16MB
	flashDev.useNANDECC = 0; // use YAFFS's ECC
	flashDev.nShortOpCaches = 10; // Use caches
	flashDev.genericDevice = mtd;	// Used to identify the device in fstat.
	flashDev.writeChunkToNAND = nandmtd_WriteChunkToNAND;
	flashDev.readChunkFromNAND = nandmtd_ReadChunkFromNAND;
	flashDev.eraseBlockInNAND = nandmtd_EraseBlockInNAND;
	flashDev.initialiseNAND = nandmtd_InitialiseNAND;

	yaffs_initialise(yaffsfs_config);
	
	return 0;
}



void make_a_file(char *yaffsName,char bval,int sizeOfFile)
{
	int outh;
	int i;
	unsigned char buffer[100];

	outh = yaffs_open(yaffsName, O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
	if (outh < 0)
	{
		printf("Error opening file: %d\n", outh);
		return;
	}

	memset(buffer,bval,100);

	do{
		i = sizeOfFile;
		if(i > 100) i = 100;
		sizeOfFile -= i;

		yaffs_write(outh,buffer,i);

	} while (sizeOfFile > 0);


	yaffs_close(outh);
}

void read_a_file(char *fn)
{
	int h;
	int i = 0;
	unsigned char b;

	h = yaffs_open(fn, O_RDWR,0);
	if(h<0)
	{
		printf("File not found\n");
		return;
	}

	while(yaffs_read(h,&b,1)> 0)
	{
		printf("%02x ",b);
		i++;
		if(i > 32)
		{
		   printf("\n");
		   i = 0;;
		 }
	}
	printf("\n");
	yaffs_close(h);
}

void cmd_yaffs_mount(char *mp)
{
	yaffs_StartUp();
	int retval = yaffs_mount(mp);
	if( retval != -1)
		isMounted = 1;
	else
		printf("Error mounting %s, return value: %d\n", mp, yaffsfs_GetError());
}

static void checkMount(void)
{
	if( !isMounted )
	{
		cmd_yaffs_mount(MOUNT_POINT);
	}
}

void cmd_yaffs_umount(char *mp)
{
	checkMount();
	if( yaffs_unmount(mp) == -1)
		printf("Error umounting %s, return value: %d\n", mp, yaffsfs_GetError());
}

void cmd_yaffs_write_file(char *yaffsName,char bval,int sizeOfFile)
{
	checkMount();
	make_a_file(yaffsName,bval,sizeOfFile);
}


void cmd_yaffs_read_file(char *fn)
{
	checkMount();
	read_a_file(fn);
}


void cmd_yaffs_mread_file(char *fn, char *addr)
{
	int h;
	struct yaffs_stat s;

	checkMount();

	yaffs_stat(fn,&s);

	printf ("Copy %s to 0x%p... ", fn, addr);
	h = yaffs_open(fn, O_RDWR,0);
	if(h<0)
	{
		printf("File not found\n");
		return;
	}

	yaffs_read(h,addr,(int)s.yst_size);
	printf("\t[DONE]\n");

	yaffs_close(h);
}


void cmd_yaffs_mwrite_file(char *fn, char *addr, int size)
{
	int outh;

	checkMount();
	outh = yaffs_open(fn, O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
	if (outh < 0)
	{
		printf("Error opening file: %d\n", outh);
	}

	yaffs_write(outh,addr,size);

	yaffs_close(outh);
}


void cmd_yaffs_ls(const char *mountpt, int longlist)
{
	int i;
	yaffs_DIR *d;
	yaffs_dirent *de;
	struct yaffs_stat stat;
	char tempstr[255];

	checkMount();
	d = yaffs_opendir(mountpt);

	if(!d)
	{
		printf("opendir failed\n");
	}
	else
	{
		for(i = 0; (de = yaffs_readdir(d)) != NULL; i++)
		{
			if (longlist)
			{
				sprintf(tempstr, "%s/%s", mountpt, de->d_name);
				yaffs_stat(tempstr, &stat);
				printf("%-25s\t%7ld\n",de->d_name, stat.yst_size);
			}
			else
			{
				printf("%s\n",de->d_name);
			}
		}
	}
}


void cmd_yaffs_mkdir(const char *dir)
{
	checkMount();

	int retval = yaffs_mkdir(dir, 0);

	if ( retval < 0)
		printf("yaffs_mkdir returning error: %d\n", retval);
}

void cmd_yaffs_rmdir(const char *dir)
{
	checkMount();

	int retval = yaffs_rmdir(dir);

	if ( retval < 0)
		printf("yaffs_rmdir returning error: %d\n", retval);
}

void cmd_yaffs_rm(const char *path)
{
	checkMount();

	int retval = yaffs_unlink(path);

	if ( retval < 0)
		printf("yaffs_unlink returning error: %d\n", retval);
}

void cmd_yaffs_mv(const char *oldPath, const char *newPath)
{
	checkMount();

	int retval = yaffs_rename(newPath, oldPath);

	if ( retval < 0)
		printf("yaffs_unlink returning error: %d\n", retval);
}

