/*************************************************************************
    > File Name: yaffs_function_test.c
    > Author: Tang Xianbin
    > Mail: xianbintang@outlook.com 
    > Created Time: Tue 03 Nov 2015 02:13:13 PM CST
    > Description: 
 ************************************************************************/

#include <common.h>
#include <command.h>
#include <rtc.h>
#include <i2c.h>

#include "yaffsfs.h"

DECLARE_GLOBAL_DATA_PTR;
extern const char *weekdays[];

#define RELOC(a)	((typeof(a))((unsigned long)(a) + gd->reloc_off))

static void print_date();
static void read_speed_test(int bytes, int buflen);
static void write_speed_test(int bytes, int buflen);


void yaffs_read_string(const char *path)
{
	int fd = -1;
	int buf[1000];
	int count = 0;

	fd = yaffs_open(path, O_RDWR, S_IREAD | S_IWRITE);
	if (fd < 0) {
		printf("error while open file: %s\n", path);
	}
	
	count = yaffs_read(fd, buf, 100) ;
	buf[count] = 0;
	printf("%s\n", buf);
	yaffs_close(fd);

	return;
}
void yaffs_write_string(const char *path, const char * str)
{
	int fd = -1;

	fd = yaffs_open(path, O_CREAT | O_RDWR | O_APPEND, S_IREAD | S_IWRITE);
	if (fd < 0) {
		printf("error while open file: %s\n", path);
	}
	
	yaffs_write(fd, str, strlen(str)) ;
	yaffs_close(fd);

	return;
}


void run_test(int testcase, int bytes, int buflen)
{
	printf("run testcase: %d\n", testcase);
	switch (testcase) {
		case 1: 
			write_speed_test(bytes, buflen);
			break;
		case 2:
			write_speed_test(bytes, buflen);
			break;

		default:
			printf("no such testcase!\n");
	}
}


static void print_date()
{
	struct rtc_time tm;
	int old_bus;

	/* switch to correct I2C bus */
	old_bus = I2C_GET_BUS();
	I2C_SET_BUS(CFG_RTC_BUS_NUM);
	rtc_get (&tm);
	printf ("Date: %4d-%02d-%02d (%sday)    Time: %2d:%02d:%02d\n",
	tm.tm_year, tm.tm_mon, tm.tm_mday,
	(tm.tm_wday<0 || tm.tm_wday>6) ?
			"unknown " : RELOC(weekdays[tm.tm_wday]),
	tm.tm_hour, tm.tm_min, tm.tm_sec);

	I2C_SET_BUS(old_bus);
	
}

static void write_speed_test(int bytes, int buflen)
{
	printf("test writing speed---- now write %d bytes in file\n", bytes);
	printf("total bytes: %d buflen: %d\n", bytes, buflen);
	print_date();

	int fd;
	int buf[1000];
	int count = 0;
	int totbytes = 0;

	fd = yaffs_open("/flash/ftf1", O_CREAT | O_TRUNC | O_RDWR, S_IREAD | S_IWRITE);

	if (fd < 0) {
		printf("error while open file: %s\n", "flash/ftf1");
	}

	while ((count = yaffs_write(fd, buf, buflen)) > 0) {
		totbytes += count;
		if (totbytes > bytes)
			break;
	}

	yaffs_close(fd);
	print_date();
	printf("end of writing speed test\n");
}


static void read_speed_test(int bytes, int buflen)
{
	printf("test reading speed---- now read %d bytes in file\n", bytes);
	printf("total bytes: %d buflen: %d\n", bytes, buflen);
	print_date();

	int fd;
	int buf[1000];
	int count = 0;
	int	totbytes = 0;

	fd = yaffs_open("/flash/ftf1", O_RDWR, S_IREAD | S_IWRITE);

	if (fd < 0) {
		printf("error while open file: %s\n", "flash/ftf1");
	}

	while ((count = yaffs_read(fd, buf, buflen)) > 0) {
		totbytes += count;
		if (totbytes > bytes)
			break;
	}

	yaffs_close(fd);
	print_date();
	printf("end of writing speed test\n");
}









