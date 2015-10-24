/*
* Test code for the "direct" interface. 
*/


#include <stdio.h>
#include <string.h>

#include "yaffsfs.h"

char xx[600];

void copy_in_a_file(char *yaffsName,char *inName)
{
	int inh,outh;
	unsigned char buffer[100];
	int ni,no;
	inh = open(inName,O_RDONLY);
	outh = yaffs_open(yaffsName, O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
	
	while((ni = read(inh,buffer,100)) > 0)
	{
		no = yaffs_write(outh,buffer,ni);
		if(ni != no)
		{
			printf("problem writing yaffs file\n");
		}
		
	}
	
	yaffs_close(outh);
	close(inh);
}

void make_a_file(char *yaffsName,char bval,int sizeOfFile)
{
	int outh;
	int i;
	unsigned char buffer[100];

	outh = yaffs_open(yaffsName, O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
	
	memset(buffer,bval,100);
	
	do{
		i = sizeOfFile;
		if(i > 100) i = 100;
		sizeOfFile -= i;
		
		yaffs_write(outh,buffer,i);
		
	} while (sizeOfFile > 0);
	
		
	yaffs_close(outh);

}




void fill_disk(char *path,int nfiles)
{
	int h;
	int n;
	int result;
	int f;
	
	char str[50];
	
	for(n = 0; n < nfiles; n++)
	{
		sprintf(str,"%s/%d",path,n);
		
		h = yaffs_open(str, O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
		
		printf("writing file %s handle %d ",str, h);
		
		while ((result = yaffs_write(h,xx,600)) == 600)
		{
			f = yaffs_freespace("/boot");
		}
		result = yaffs_close(h);
		printf(" close %d\n",result);
	}
}

void fill_disk_and_delete(char *path, int nfiles, int ncycles)
{
	int i,j;
	char str[50];
	int result;
	
	for(i = 0; i < ncycles; i++)
	{
		printf("@@@@@@@@@@@@@@ cycle %d\n",i);
		fill_disk(path,nfiles);
		
		for(j = 0; j < nfiles; j++)
		{
			sprintf(str,"%s/%d",path,j);
			result = yaffs_unlink(str);
			printf("unlinking file %s, result %d\n",str,result);
		}
	}
}


void fill_files(char *path,int flags, int maxIterations,int siz)
{
	int i;
	int j;
	char str[50];
	int h;
	
	i = 0;
	
	do{
		sprintf(str,"%s/%d",path,i);
		h = yaffs_open(str, O_CREAT | O_TRUNC | O_RDWR,S_IREAD | S_IWRITE);
		yaffs_close(h);

		if(h >= 0)
		{
			for(j = 0; j < siz; j++)
			{
				yaffs_write(h,str,1);
			}
		}
		
		if( flags & 1)
		{
			yaffs_unlink(str);
		}
		i++;
	} while(h >= 0 && i < maxIterations);
	
	if(flags & 2)
	{
		i = 0;
		do{
			sprintf(str,"%s/%d",path,i);
			printf("unlink %s\n",str);
			i++;
		} while(yaffs_unlink(str) >= 0);
	}
}

void leave_unlinked_file(char *path,int maxIterations,int siz)
{
	int i;
	char str[50];
	int h;
	
	i = 0;
	
	do{
		sprintf(str,"%s/%d",path,i);
		printf("create %s\n",str);
		h = yaffs_open(str, O_CREAT | O_TRUNC | O_RDWR,S_IREAD | S_IWRITE);
		if(h >= 0)
		{
			yaffs_unlink(str);
		}
		i++;
	} while(h < 0 && i < maxIterations);
	
	if(h >= 0)
	{
		for(i = 0; i < siz; i++)
		{
			yaffs_write(h,str,1);
		}
	}
	
	printf("Leaving file %s open\n",str);

}

void dumpDirFollow(const char *dname)
{
	yaffs_DIR *d;
	yaffs_dirent *de;
	struct yaffs_stat s;
	char str[100];
			
	d = yaffs_opendir(dname);
	
	if(!d)
	{
		printf("opendir failed\n");
	}
	else
	{
		while((de = yaffs_readdir(d)) != NULL)
		{
			sprintf(str,"%s/%s",dname,de->d_name);
			
			yaffs_stat(str,&s);
			
			printf("%s length %d mode %X ",de->d_name,s.yst_size,s.yst_mode);
			switch(s.yst_mode & S_IFMT)
			{
				case S_IFREG: printf("data file"); break;
				case S_IFDIR: printf("directory"); break;
				case S_IFLNK: printf("symlink -->");
							  if(yaffs_readlink(str,str,100) < 0)
							  	printf("no alias");
							  else
								printf("\"%s\"",str);	 
							  break;
				default: printf("unknown"); break;
			}
			
			printf("\n");		
		}
		
		yaffs_closedir(d);
	}
	printf("\n");
	
	printf("Free space in %s is %d\n\n",dname,yaffs_freespace(dname));

}
void dumpDir(const char *dname)
{
	yaffs_DIR *d;
	yaffs_dirent *de;
	struct yaffs_stat s;
	char str[100];
			
	d = yaffs_opendir(dname);
	
	if(!d)
	{
		printf("opendir failed\n");
	}
	else
	{
		while((de = yaffs_readdir(d)) != NULL)
		{
			sprintf(str,"%s/%s",dname,de->d_name);
			
			yaffs_lstat(str,&s);
			
			printf("%s length %d mode %X ",de->d_name,s.yst_size,s.yst_mode);
			switch(s.yst_mode & S_IFMT)
			{
				case S_IFREG: printf("data file"); break;
				case S_IFDIR: printf("directory"); break;
				case S_IFLNK: printf("symlink -->");
							  if(yaffs_readlink(str,str,100) < 0)
							  	printf("no alias");
							  else
								printf("\"%s\"",str);	 
							  break;
				default: printf("unknown"); break;
			}
			
			printf("\n");		
		}
		
		yaffs_closedir(d);
	}
	printf("\n");
	
	printf("Free space in %s is %d\n\n",dname,yaffs_freespace(dname));

}


static void PermissionsCheck(const char *path, mode_t tmode, int tflags,int expectedResult)
{
	int fd;
	
	if(yaffs_chmod(path,tmode)< 0) printf("chmod failed\n");
	
	fd = yaffs_open(path,tflags,0);
	
	if((fd >= 0) != (expectedResult > 0))
	{
		printf("Permissions check %x %x %d failed\n",tmode,tflags,expectedResult);
	}
	else
	{
		printf("Permissions check %x %x %d OK\n",tmode,tflags,expectedResult);
	}
	
	
	yaffs_close(fd);
	
	
}

int long_test(int argc, char *argv[])
{

	int f;
	int r;
	char buffer[20];
	
	char str[100];
	
	int h;
	mode_t temp_mode;
	struct yaffs_stat ystat;
	
	yaffs_StartUp();
	
	yaffs_mount("/boot");
	yaffs_mount("/data");
	yaffs_mount("/flash");
	yaffs_mount("/ram");
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	printf("\nDirectory look-up of /data\n");
	dumpDir("/data");
	printf("\nDirectory look-up of /flash\n");
	dumpDir("/flash");

	//leave_unlinked_file("/flash",20000,0);
	//leave_unlinked_file("/data",20000,0);
	
	leave_unlinked_file("/ram",20,0);
	

	f = yaffs_open("/boot/b1", O_RDONLY,0);
	
	printf("open /boot/b1 readonly, f=%d\n",f);
	
	f = yaffs_open("/boot/b1", O_CREAT,S_IREAD | S_IWRITE);
	
	printf("open /boot/b1 O_CREAT, f=%d\n",f);
	
	
	r = yaffs_write(f,"hello",1);
	printf("write %d attempted to write to a read-only file\n",r);
	
	r = yaffs_close(f);
	
	printf("close %d\n",r);

	f = yaffs_open("/boot/b1", O_RDWR,0);
	
	printf("open /boot/b1 O_RDWR,f=%d\n",f);
	
	
	r = yaffs_write(f,"hello",2);
	printf("write %d attempted to write to a writeable file\n",r);
	r = yaffs_write(f,"world",3);
	printf("write %d attempted to write to a writeable file\n",r);
	
	r= yaffs_lseek(f,0,SEEK_END);
	printf("seek end %d\n",r);
	memset(buffer,0,20);
	r = yaffs_read(f,buffer,10);
	printf("read %d \"%s\"\n",r,buffer);
	r= yaffs_lseek(f,0,SEEK_SET);
	printf("seek set %d\n",r);
	memset(buffer,0,20);
	r = yaffs_read(f,buffer,10);
	printf("read %d \"%s\"\n",r,buffer);
	memset(buffer,0,20);
	r = yaffs_read(f,buffer,10);
	printf("read %d \"%s\"\n",r,buffer);

	// Check values reading at end.
	// A read past end of file should return 0 for 0 bytes read.
		
	r= yaffs_lseek(f,0,SEEK_END);
	r = yaffs_read(f,buffer,10);
	printf("read at end returned  %d\n",r);	
	r= yaffs_lseek(f,500,SEEK_END);
	r = yaffs_read(f,buffer,10);
	printf("read past end returned  %d\n",r);	
	
	r = yaffs_close(f);
	
	printf("close %d\n",r);
	
	copy_in_a_file("/boot/yyfile","xxx");
	
	// Create a file with a long name
	
	copy_in_a_file("/boot/file with a long name","xxx");
	
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");

	// Check stat
	r = yaffs_stat("/boot/file with a long name",&ystat);
	
	// Check rename
	
	r = yaffs_rename("/boot/file with a long name","/boot/r1");
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	
	// Check unlink
	r = yaffs_unlink("/boot/r1");
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");

	// Check mkdir
	
	r = yaffs_mkdir("/boot/directory1",0);
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	printf("\nDirectory look-up of /boot/directory1\n");
	dumpDir("/boot/directory1");

	// add a file to the directory			
	copy_in_a_file("/boot/directory1/file with a long name","xxx");
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	printf("\nDirectory look-up of /boot/directory1\n");
	dumpDir("/boot/directory1");
	
	//  Attempt to delete directory (should fail)
	
	r = yaffs_rmdir("/boot/directory1");
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	printf("\nDirectory look-up of /boot/directory1\n");
	dumpDir("/boot/directory1");
	
	// Delete file first, then rmdir should work
	r = yaffs_unlink("/boot/directory1/file with a long name");
	r = yaffs_rmdir("/boot/directory1");
	
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	printf("\nDirectory look-up of /boot/directory1\n");
	dumpDir("/boot/directory1");

#if 0
	fill_disk_and_delete("/boot",20,20);
			
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
#endif

	yaffs_symlink("yyfile","/boot/slink");
	
	yaffs_readlink("/boot/slink",str,100);
	printf("symlink alias is %s\n",str);
	
	
	
	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	printf("\nDirectory look-up of /boot (using stat instead of lstat)\n");
	dumpDirFollow("/boot");
	printf("\nDirectory look-up of /boot/directory1\n");
	dumpDir("/boot/directory1");

	h = yaffs_open("/boot/slink",O_RDWR,0);
	
	printf("file length is %d\n",yaffs_lseek(h,0,SEEK_END));
	
	yaffs_close(h);
	
	yaffs_unlink("/boot/slink");

	
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	
	// Check chmod
	
	yaffs_stat("/boot/yyfile",&ystat);
	temp_mode = ystat.yst_mode;
	
	yaffs_chmod("/boot/yyfile",0x55555);
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	
	yaffs_chmod("/boot/yyfile",temp_mode);
	printf("\nDirectory look-up of /boot\n");
	dumpDir("/boot");
	
	// Permission checks...
	PermissionsCheck("/boot/yyfile",0, O_WRONLY,0);
	PermissionsCheck("/boot/yyfile",0, O_RDONLY,0);
	PermissionsCheck("/boot/yyfile",0, O_RDWR,0);

	PermissionsCheck("/boot/yyfile",S_IREAD, O_WRONLY,0);
	PermissionsCheck("/boot/yyfile",S_IREAD, O_RDONLY,1);
	PermissionsCheck("/boot/yyfile",S_IREAD, O_RDWR,0);

	PermissionsCheck("/boot/yyfile",S_IWRITE, O_WRONLY,1);
	PermissionsCheck("/boot/yyfile",S_IWRITE, O_RDONLY,0);
	PermissionsCheck("/boot/yyfile",S_IWRITE, O_RDWR,0);
	
	PermissionsCheck("/boot/yyfile",S_IREAD | S_IWRITE, O_WRONLY,1);
	PermissionsCheck("/boot/yyfile",S_IREAD | S_IWRITE, O_RDONLY,1);
	PermissionsCheck("/boot/yyfile",S_IREAD | S_IWRITE, O_RDWR,1);

	yaffs_chmod("/boot/yyfile",temp_mode);
	
	//create a zero-length file and unlink it (test for scan bug)
	
	h = yaffs_open("/boot/zlf",O_CREAT | O_TRUNC | O_RDWR,0);
	yaffs_close(h);
	
	yaffs_unlink("/boot/zlf");
	
	
	yaffs_DumpDevStruct("/boot");
	
	fill_disk_and_delete("/boot",20,20);
	
	yaffs_DumpDevStruct("/boot");
	
	fill_files("/boot",1,10000,0);
	fill_files("/boot",1,10000,5000);
	fill_files("/boot",2,10000,0);
	fill_files("/boot",2,10000,5000);
	
	leave_unlinked_file("/data",20000,0);
	leave_unlinked_file("/data",20000,5000);
	leave_unlinked_file("/data",20000,5000);
	leave_unlinked_file("/data",20000,5000);
	leave_unlinked_file("/data",20000,5000);
	leave_unlinked_file("/data",20000,5000);
	
	yaffs_DumpDevStruct("/boot");
	yaffs_DumpDevStruct("/data");
	
		
		
	return 0;

}



int directory_rename_test(void)
{
	int r;
	yaffs_StartUp();
	
	yaffs_mount("/ram");
	yaffs_mkdir("/ram/a",0);
	yaffs_mkdir("/ram/a/b",0);
	yaffs_mkdir("/ram/c",0);
	
	printf("\nDirectory look-up of /ram\n");
	dumpDir("/ram");
	dumpDir("/ram/a");
	dumpDir("/ram/a/b");

	printf("Do rename (should fail)\n");
		
	r = yaffs_rename("/ram/a","/ram/a/b/d");
	printf("\nDirectory look-up of /ram\n");
	dumpDir("/ram");
	dumpDir("/ram/a");
	dumpDir("/ram/a/b");

	printf("Do rename (should not fail)\n");
		
	r = yaffs_rename("/ram/c","/ram/a/b/d");
	printf("\nDirectory look-up of /ram\n");
	dumpDir("/ram");
	dumpDir("/ram/a");
	dumpDir("/ram/a/b");
	
	
	return 1;
	
}

int cache_read_test(void)
{
	int a,b,c;
	int i;
	int sizeOfFiles = 500000;
	char buffer[100];
	
	yaffs_StartUp();
	
	yaffs_mount("/boot");
	
	make_a_file("/boot/a",'a',sizeOfFiles);
	make_a_file("/boot/b",'b',sizeOfFiles);

	a = yaffs_open("/boot/a",O_RDONLY,0);
	b = yaffs_open("/boot/b",O_RDONLY,0);
	c = yaffs_open("/boot/c", O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);

	do{
		i = sizeOfFiles;
		if (i > 100) i = 100;
		sizeOfFiles  -= i;
		yaffs_read(a,buffer,i);
		yaffs_read(b,buffer,i);
		yaffs_write(c,buffer,i);
	} while(sizeOfFiles > 0);
	
	
	
	return 1;
	
}

int cache_bypass_bug_test(void)
{
	// This test reporoduces a bug whereby YAFFS caching is buypassed
	// resulting in erroneous reads after writes.
	int a;
	int i;
	char buffer1[1000];
	char buffer2[1000];
	
	memset(buffer1,0,sizeof(buffer1));
	memset(buffer2,0,sizeof(buffer2));
		
	yaffs_StartUp();
	
	yaffs_mount("/boot");
	
	// Create a file of 2000 bytes.
	make_a_file("/boot/a",'X',2000);

	a = yaffs_open("/boot/a",O_RDWR, S_IREAD | S_IWRITE);
	
	// Write a short sequence to the file.
	// This will go into the cache.
	yaffs_lseek(a,0,SEEK_SET);
	yaffs_write(a,"abcdefghijklmnopqrstuvwxyz",20); 

	// Read a short sequence from the file.
	// This will come from the cache.
	yaffs_lseek(a,0,SEEK_SET);
	yaffs_read(a,buffer1,30); 

	// Read a page size sequence from the file.
	yaffs_lseek(a,0,SEEK_SET);
	yaffs_read(a,buffer2,512); 
	
	printf("buffer 1 %s\n",buffer1);
	printf("buffer 2 %s\n",buffer2);
	
	if(strncmp(buffer1,buffer2,20))
	{
		printf("Cache bypass bug detected!!!!!\n");
	}
	
	
	return 1;
}


int free_space_check(void)
{
	int f;
	
		yaffs_StartUp();
		yaffs_mount("/boot");
	    fill_disk("/boot/",2);
	    f = yaffs_freespace("/boot");
	    
	    printf("%d free when disk full\n",f);	    
	    return 1;
}


int BeatsTest(void)
{
	int h;
	char b[2000];
	int freeSpace;
	int fsize;
	yaffs_StartUp();
	yaffs_mount("/ram");
	
	h = yaffs_open("/ram/f1", O_CREAT | O_TRUNC | O_RDWR, S_IREAD | S_IWRITE);
	
	freeSpace = yaffs_freespace("/ram");
	printf("start free space %d\n",freeSpace);
	
	while(yaffs_write(h,b,600) > 0) {
		fsize = yaffs_lseek(h,0,SEEK_CUR);
		freeSpace = yaffs_freespace("/ram");
		printf(" %d  = %d + %d\n",fsize + freeSpace,fsize,freeSpace);
	}
	yaffs_close(h);
	
	freeSpace = yaffs_freespace("/ram");
	
	return 1;
	
}

int main(int argc, char *argv[])
{
	//return long_test(argc,argv);
	
	//return cache_read_test();
	
	//return cache_bypass_bug_test();
	
	// return free_space_check();
	
	return BeatsTest();
	
}
