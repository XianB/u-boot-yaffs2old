/*
 * Copyright (C) 2007 Freescale Semiconductor, Inc
 *
 * Andy Fleming <afleming@freescale.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __FSL_ESDHC_H__
#define __FSL_ESDHC_H__

#define SD_VERSION_SD		0x20000
#define SD_VERSION_2		(SD_VERSION_SD | 0x20)
#define SD_VERSION_1_0		(SD_VERSION_SD | 0x10)
#define SD_VERSION_1_1		(SD_VERSION_SD | 0x11)
#define MMC_VERSION_MMC		0x10000
#define MMC_VERSION_UNKNOWN	(MMC_VERSION_MMC)
#define MMC_VERSION_1_2		(MMC_VERSION_MMC | 0x12)
#define MMC_VERSION_1_4		(MMC_VERSION_MMC | 0x14)
#define MMC_VERSION_2_2		(MMC_VERSION_MMC | 0x22)
#define MMC_VERSION_3		(MMC_VERSION_MMC | 0x30)
#define MMC_VERSION_4		(MMC_VERSION_MMC | 0x40)

#define MMC_MODE_4BIT		0x00000001
#define MMC_MODE_HS		0x00000002

#define SD_DATA_4BIT		0x00040000

#define IS_SD(x) (mmc->version & SD_VERSION_SD)

#define MMC_DATA_READ		1
#define MMC_DATA_WRITE		2

#define NO_CARD_ERR		-16 /* No SD/MMC card inserted */
#define UNUSABLE_ERR		-17 /* Unusable Card */
#define COMM_ERR		-18
#define TIMEOUT			-19

#define PROCTL_DTW_4		0x00000022
/* The base clock is assumed to be 132 MHz */
#define SYSCTL			0x0002e02c
#define SYSCTL_400KHz		0x000e10a0	/* base/(32*11) = ~375KHz */
#define SYSCTL_13MHz		0x000e0140	/* base/(2*5) = ~11MHz */
#define SYSCTL_20MHz		0x000e0140	/* base/(2*4) = 16.5MHz */
#define SYSCTL_26MHz		0x000e0120	/* base/(2*3) = 22MHz */
#define SYSCTL_52MHz		0x000e0110	/* base/(2*2) = 33MHz */
#define SYSCTL_50MHz		0x000e0110
#define SYSCTL_INITA		0x08000000

#define IRQSTAT			0x0002e030
#define IRQSTAT_DMAE		(0x10000000)
#define IRQSTAT_AC12E		(0x01000000)
#define IRQSTAT_DEBE		(0x00400000)
#define IRQSTAT_DCE		(0x00200000)
#define IRQSTAT_DTOE		(0x00100000)
#define IRQSTAT_CIE		(0x00080000)
#define IRQSTAT_CEBE		(0x00040000)
#define IRQSTAT_CCE		(0x00020000)
#define IRQSTAT_CTOE		(0x00010000)
#define IRQSTAT_CINT		(0x00000100)
#define IRQSTAT_CRM		(0x00000080)
#define IRQSTAT_CINS		(0x00000040)
#define IRQSTAT_BRR		(0x00000020)
#define IRQSTAT_BWR		(0x00000010)
#define IRQSTAT_DINT		(0x00000008)
#define IRQSTAT_BGE		(0x00000004)
#define IRQSTAT_TC		(0x00000002)
#define IRQSTAT_CC		(0x00000001)

#define CMD_ERR		(IRQSTAT_CIE | IRQSTAT_CEBE | IRQSTAT_CCE)
#define DATA_ERR	(IRQSTAT_DEBE | IRQSTAT_DCE | IRQSTAT_DTOE)

#define IRQSTATEN		0x0002e034
#define IRQSTATEN_DMAE		(0x10000000)
#define IRQSTATEN_AC12E		(0x01000000)
#define IRQSTATEN_DEBE		(0x00400000)
#define IRQSTATEN_DCE		(0x00200000)
#define IRQSTATEN_DTOE		(0x00100000)
#define IRQSTATEN_CIE		(0x00080000)
#define IRQSTATEN_CEBE		(0x00040000)
#define IRQSTATEN_CCE		(0x00020000)
#define IRQSTATEN_CTOE		(0x00010000)
#define IRQSTATEN_CINT		(0x00000100)
#define IRQSTATEN_CRM		(0x00000080)
#define IRQSTATEN_CINS		(0x00000040)
#define IRQSTATEN_BRR		(0x00000020)
#define IRQSTATEN_BWR		(0x00000010)
#define IRQSTATEN_DINT		(0x00000008)
#define IRQSTATEN_BGE		(0x00000004)
#define IRQSTATEN_TC		(0x00000002)
#define IRQSTATEN_CC		(0x00000001)

#define PRSSTAT			0x0002e024
#define PRSSTAT_CLSL		(0x00800000)
#define PRSSTAT_WPSPL		(0x00080000)
#define PRSSTAT_CDPL		(0x00040000)
#define PRSSTAT_CINS		(0x00010000)
#define PRSSTAT_BREN		(0x00000800)
#define PRSSTAT_DLA		(0x00000004)

#define CMDARG			0x0002e008

#define XFERTYP			0x0002e00c
#define XFERTYP_CMD(x)		((x & 0x3f) << 24)
#define XFERTYP_CMDTYP_NORMAL	0x0
#define XFERTYP_CMDTYP_SUSPEND	0x00400000
#define XFERTYP_CMDTYP_RESUME	0x00800000
#define XFERTYP_CMDTYP_ABORT	0x00c00000
#define XFERTYP_DPSEL		0x00200000
#define XFERTYP_CICEN		0x00100000
#define XFERTYP_CCCEN		0x00080000
#define XFERTYP_RSPTYP_NONE	0
#define XFERTYP_RSPTYP_136	0x00010000
#define XFERTYP_RSPTYP_48	0x00020000
#define XFERTYP_RSPTYP_48_BUSY	0x00030000
#define XFERTYP_MSBSEL		0x00000020
#define XFERTYP_DTDSEL		0x00000010
#define XFERTYP_AC12EN		0x00000004
#define XFERTYP_BCEN		0x00000002
#define XFERTYP_DMAEN		0x00000001

#define CINS_TIMEOUT		1000

#define SD_CMD_R1(x)		(XFERTYP_CMD(x) | XFERTYP_RSPTYP_48 \
				| XFERTYP_CICEN | XFERTYP_CCCEN)

#define SD_CMD_R2(x)		(XFERTYP_CMD(x) | XFERTYP_RSPTYP_136 \
				| XFERTYP_CCCEN)

#define SD_CMD_R3(x)		(XFERTYP_CMD(x) | XFERTYP_RSPTYP_48)
#define SD_CMD_R4(x)		(XFERTYP_CMD(x) | XFERTYP_RSPTYP_48)

#define SD_CMD_R5(x)		SD_CMD_R1(x)
#define SD_CMD_R6(x)		SD_CMD_R1(x)
#define SD_CMD_R7(x)		SD_CMD_R1(x)

#define SD_CMD_R1b(x)		(XFERTYP_CMD(x) | XFERTYP_RSPTYP_48_BUSY \
				| XFERTYP_CICEN | XFERTYP_CCCEN)

#define SD_CMD_R5b(x)		SD_CMD_R1b(x)

#define GO_IDLE_STATE		0x0
#define SEND_IF_COND		(8)

#define APP_CMD			(55)

#define MMC_SEND_OP_COND	(1)

#define SD_SEND_OP_COND		(41)

#define ALL_SEND_CID		(2)

#define SEND_RELATIVE_ADDR	(3)

#define DEFAULT_RCA		0x00020000

#define SET_BUS_WIDTH		(6)

#define SELECT_CARD		(7)

#define SEND_SCR		(51)

#define SEND_EXT_CSD		(8)

#define SEND_CSD		(9)

#define SEND_STATUS		(13)

#define SWITCH_FUNC		(6)

#define STOP_TRANSMISSION	(12)

#define SET_BLOCKLEN		(16)

#define READ_SINGLE_BLOCK	(17)
#define READ_MULTIPLE_BLOCKS	(18)

#define READ_SINGLE_BLOCK_DMA	(17)
#define READ_MULTI_BLOCK_DMA	(18)

#define SD_HIGHSPEED_BUSY	0x00020000
#define SD_HIGHSPEED_SUPPORTED	0x00020000

#define MMC_HS_TIMING		0x00000100

#define DSADDR		0x2e004

#define CMDRSP0		0x2e010
#define CMDRSP1		0x2e014
#define CMDRSP2		0x2e018
#define CMDRSP3		0x2e01c

#define DATPORT		0x2e020

#define WML		0x2e044
#define WML_WRITE	0x00010000

#define BLKATTR		0x2e004
#define BLKATTR_CNT(x)	((x & 0xffff) << 16)
#define BLKATTR_SIZE(x)	(x & 0x1fff)
#define MAX_BLK_CNT	0xffff

#define ONE_SECOND	83333333

#define OCR_BUSY	0x80
#define OCR_HCS		0x40000000

#endif /* __FSL_ESDHC_H__ */
