#include "yaffsfs.h"


void fill_disk(char *path,int nfiles)
{
	int h;
	int n;
	int result;
	int f;
	
	char xx[600];
	char str[50];
	
	for(n = 0; n < nfiles; n++)
	{
		sprintf(str,"%s/%d",path,n);
		
		h = yaffs_open(str, O_CREAT | O_RDWR | O_TRUNC, S_IREAD | S_IWRITE);
		
		printf("writing file %s handle %d ",str, h);
		
		while ((result = yaffs_write(h,xx,600)) == 600)
		{
			f = yaffs_freespace("/flash");
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



int free_space_check(void)
{
	int f;
	
		yaffs_StartUp();
		yaffs_mount("/flash");
	    fill_disk("/flash/",2);
	    f = yaffs_freespace("/flash");
	    
	    printf("%d free when disk full\n",f);	    
	    return 1;
}


int BeatsTest(void)
{
	int h;
	char b[2000];
	int freeSpace;
	int fsize;

	yaffs_mount("/flash");
	
	h = yaffs_open("/flash/f1", O_CREAT | O_TRUNC | O_RDWR, S_IREAD | S_IWRITE);
	
	freeSpace = yaffs_freespace("/flash");
	printf("start free space %d\n",freeSpace);
	
	while(yaffs_write(h,b,600) > 0) {
		fsize = yaffs_lseek(h,0,SEEK_CUR);
		freeSpace = yaffs_freespace("/flash");
		printf(" %d  = %d + %d\n",fsize + freeSpace,fsize,freeSpace);
	}
	yaffs_close(h);
	
	freeSpace = yaffs_freespace("/flash");
	
	return 1;
	
}

int run_yaffs(int argc, char *argv[])
{
	
	return BeatsTest();
	
}
