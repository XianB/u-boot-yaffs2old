/*
 * YAFFS: Yet another FFS. A NAND-flash specific file system. 
 * yboot.h: Interface to yaffs boot file reading code
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
 * Note: Only YAFFS headers are LGPL, YAFFS C code is covered by GPL.
 *
 * $Id: yboot.h,v 1.1 2003-01-21 03:32:17 charles Exp $
 */
 
 #ifndef __YBOOT_H__
#define __YBOOT_H__

int yaffsboot_InitFile(yaffs_Device *dev, const char *fileName, int *loadedFileSize);

int yaffsboot_Reinitialise(void);

int yaffsboot_ReadByte(unsigned char *bPtr);
#endif

