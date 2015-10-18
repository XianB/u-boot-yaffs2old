/*
 * Copyright (C) 2007 Freescale Semiconductor, Inc
 *
 * Andy Fleming <afleming@freescale.com>
 *
 * Based vaguely on the pxa mmc code:
 * (C) Copyright 2003
 * Kyle Harris, Nexus Technologies, Inc. kharris@nexus-tech.net
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <mmc.h>
#include <part.h>
#include <malloc.h>
#include <asm/io.h>

#ifdef CONFIG_MMC

struct fsl_esdhc {
	uint	dsaddr;
	uint	blkattr;
	uint	cmdarg;
	uint	xfertyp;
	uint	cmdrsp0;
	uint	cmdrsp1;
	uint	cmdrsp2;
	uint	cmdrsp3;
	uint	datport;
	uint	prsstat;
	uint	proctl;
	uint	sysctl;
	uint	irqstat;
	uint	irqstaten;
	uint	irqsigen;
	uint	autoc12err;
	uint	hostcapblt;
	uint	wml;
	char	reserved1[8];
	uint	fevt;
	char	reserved2[168];
	uint	hostver;
	char	reserved3[780];
	uint	scr;
};

enum {
	MMC_CMD_RSP_NONE,
	MMC_CMD_R1 = 1,
	MMC_CMD_R1b,
	MMC_CMD_R2,
	MMC_CMD_R3,
	MMC_CMD_R4,
	MMC_CMD_R5,
	MMC_CMD_R5b,
	MMC_CMD_R6,
	MMC_CMD_R7
};

uint	xfertyps[] = {
	XFERTYP_RSPTYP_NONE,
	XFERTYP_RSPTYP_48 | XFERTYP_CICEN | XFERTYP_CCCEN,
	XFERTYP_RSPTYP_48_BUSY | XFERTYP_CICEN | XFERTYP_CCCEN,
	XFERTYP_RSPTYP_136 | XFERTYP_CCCEN,
	XFERTYP_RSPTYP_48,
	XFERTYP_RSPTYP_48,
	XFERTYP_RSPTYP_48 | XFERTYP_CICEN | XFERTYP_CCCEN,
	XFERTYP_RSPTYP_48_BUSY | XFERTYP_CICEN | XFERTYP_CCCEN,
	XFERTYP_RSPTYP_48 | XFERTYP_CICEN | XFERTYP_CCCEN,
	XFERTYP_RSPTYP_48 | XFERTYP_CICEN | XFERTYP_CCCEN
};

struct mmc {
	volatile void *regs;
	uint version;
	int high_capacity;
	uint mode;
	uint ocr;
	uint scr[2];
	uint csd[512];
	ushort rca;
	uint tran_speed;
	uint read_bl_len;
	uint write_bl_len;
	block_dev_desc_t block_dev;
};

static struct mmc *mmc_dev;

block_dev_desc_t *mmc_get_dev(int dev)
{
	return (dev == 0) ? &mmc_dev->block_dev : NULL;
}

struct mmc_cmd {
	ushort	cmdidx;
	int	resp_type;
	uint	cmdarg;
	char	response[18];
	uint	flags;
};

struct mmc_data {
	char	*buffer;
	uint	flags;
	int	blocks;
	int	blocksize;
};

/* Return the XFERTYP flags for a given command and data packet */
uint esdhc_xfertyp(struct mmc_cmd *cmd, struct mmc_data *data)
{
	uint xfertyp = 0;

	if (data) {
		xfertyp |= XFERTYP_DPSEL | XFERTYP_DMAEN;

		if (data->blocks > 1) {
			xfertyp |= XFERTYP_MSBSEL;
			xfertyp |= XFERTYP_BCEN;
		}

		if (data->flags & MMC_DATA_READ)
			xfertyp |= XFERTYP_DTDSEL;
	}

	xfertyp |= xfertyps[cmd->resp_type];

	return xfertyp;
}


/*
 * Sends a command out on the bus.  Takes the mmc pointer,
 * a command pointer, and an optional data pointer.
 */
static int
mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	uint	xfertyp;
	uint	irqstat;
	volatile struct fsl_esdhc *regs = mmc->regs;

	regs->irqstat = -1;

	sync();

	/* Wait for the bus to be idle */
	while (!(regs->prsstat & PRSSTAT_CLSL) || (regs->prsstat & PRSSTAT_DLA))
		sync();

	/* Set up for a data transfer if we have one */
	if (data) {
		uint wml_value;

		wml_value = data->blocksize/4;

		if (data->flags & MMC_DATA_READ) {
			if (wml_value > 0x10)
				wml_value = 0x10;

			wml_value = 0x100000 | wml_value;
		} else {
			if (wml_value > 0x80)
				wml_value = 0x80;

			wml_value = wml_value << 16 | 0x10;
		}

		regs->dsaddr = (uint)data->buffer;

		regs->wml = wml_value;

		regs->blkattr = data->blocks << 16 | data->blocksize;
	}

	/* Figure out the transfer arguments */
	xfertyp = esdhc_xfertyp(cmd, data);

	/* Send the command */
	regs->cmdarg = cmd->cmdarg;
	sync();
	regs->xfertyp = XFERTYP_CMD(cmd->cmdidx) | xfertyp;

	/* Wait for the command to complete */
	while (!(regs->irqstat & IRQSTAT_CC))
		sync();

	irqstat = regs->irqstat;

	sync();

	regs->irqstat = irqstat;

	sync();

	if (irqstat & CMD_ERR)
		return COMM_ERR;

	if (irqstat & IRQSTAT_CTOE)
		return TIMEOUT;

	/* Copy the response to the response buffer */
	if (cmd->resp_type == MMC_CMD_R2) {
		((uint *)(cmd->response))[0] =
			(regs->cmdrsp3 << 8) | (regs->cmdrsp2 >> 24);
		((uint *)(cmd->response))[1] =
			(regs->cmdrsp2 << 8) | (regs->cmdrsp1 >> 24);
		((uint *)(cmd->response))[2] =
			(regs->cmdrsp1 << 8) | (regs->cmdrsp0 >> 24);
		((uint *)(cmd->response))[3] = (regs->cmdrsp0 << 8);
	} else
		((uint *)(cmd->response))[0] = regs->cmdrsp0;

	/* Wait until all of the blocks are transferred */
	if (data) {
		do {
			if (regs->irqstat & DATA_ERR) {
				printf("the data err 0x%8x\n", regs->irqstat);
				return COMM_ERR;
			}

			if (regs->irqstat & IRQSTAT_DTOE) {
				printf("the data timeout 0x%8x\n",
					regs->irqstat);
				return TIMEOUT;
			}
			sync();
		} while (!(regs->irqstat & IRQSTAT_TC));
	}

	regs->irqstat = -1;
	sync();

	return 0;
}


int
mmc_block_write(ulong dst, uchar *src, int len)
{
#warning write not yet implemented
	return 0;
}

int mmc_set_blocklen(struct mmc *mmc, int len)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = SET_BLOCKLEN;
	cmd.resp_type = MMC_CMD_R1;
	cmd.cmdarg = len;
	cmd.flags = 0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

#define mmc_supports_partial_read(x) (mmc->csd[1] & 0x80000000)

int
mmc_read(ulong src, uchar *dst, int size)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int err;
	char *buffer;
	int bufsize;
	int stoperr = 0;
	struct mmc *mmc = mmc_dev;
	int blklen = mmc->read_bl_len;
	int blkcnt = size / blklen + ((size % blklen) ? 1 : 0);

	/* Make a buffer big enough to hold all the blocks we might read */
	bufsize = (((src + size) - (src & ~(blklen - 1)))/blklen + 1) * blklen;
	buffer = malloc(bufsize);

	if (!buffer) {
		printf("Could not allocate buffer for MMC read!\n");
		return -1;
	}

#warning deal with larger reads (more than 65536 blks) and partial block reads
	/* We only support full block reads from the card */
	err = mmc_set_blocklen(mmc, mmc->read_bl_len);

	if (err)
		return err;

	if (blkcnt > 1)
		cmd.cmdidx = READ_MULTIPLE_BLOCKS;
	else
		cmd.cmdidx = READ_SINGLE_BLOCK;

	if (mmc->high_capacity)
		cmd.cmdarg = src / mmc->read_bl_len;
	else
		cmd.cmdarg = src & ~(mmc->read_bl_len - 1);

	cmd.resp_type = MMC_CMD_R1;
	cmd.flags = 0;

	data.buffer = buffer;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);

	if (blkcnt > 1) {
		cmd.cmdidx = STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_CMD_R1b;
		cmd.flags = 0;
		stoperr = mmc_send_cmd(mmc, &cmd, NULL);
	}


	if (err) {
		printf("the read data have err %d\n", err);
		return err;
	}

	memcpy(dst, buffer + (src & (blklen - 1)), size);

	free(buffer);

	return stoperr;
}

int
mmc_write(uchar *src, ulong dst, int size)
{
	return 0;
}

ulong
mmc_bread(int dev_num, ulong blknr, ulong blkcnt, void *dst)
{
	int err;
	ulong src = blknr * mmc_dev->read_bl_len;

	err = mmc_read(src, dst, blkcnt * mmc_dev->read_bl_len);

	if (err) {
		printf("block read failed: %d\n", err);
		return 0;
	}

	return blkcnt;
}

int
sd_send_op_cond(struct mmc *mmc)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	do {
		cmd.cmdidx = APP_CMD;
		cmd.resp_type = MMC_CMD_R1;
		cmd.cmdarg = 0;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		cmd.cmdidx = SD_SEND_OP_COND;
		cmd.resp_type = MMC_CMD_R3;
		cmd.cmdarg = 0xff8000;

		if (mmc->version == SD_VERSION_2)
			cmd.cmdarg |= 0x40000000;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		udelay(1000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0)
		return UNUSABLE_ERR;

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	mmc->ocr = ((uint *)(cmd.response))[0];

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;

	return 0;
}

int mmc_send_op_cond(struct mmc *mmc)
{
	int timeout = 1000;
	struct mmc_cmd cmd;
	int err;

	do {
		cmd.cmdidx = MMC_SEND_OP_COND;
		cmd.resp_type = MMC_CMD_R3;
		cmd.cmdarg = 0x8ff00;
		cmd.flags = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		udelay(1000);
	} while (!(cmd.response[0] & OCR_BUSY) && timeout--);

	if (timeout <= 0)
		return UNUSABLE_ERR;

	mmc->version = MMC_VERSION_UNKNOWN;
	mmc->ocr = ((uint *)(cmd.response))[0];
	mmc->rca = 0;

	return 0;
}


int mmc_change_freq(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	volatile struct fsl_esdhc *regs = mmc->regs;
	uint csd[128];
	int err;

	/* Only version 4 supports high-speed */
	if (mmc->version != MMC_VERSION_4) {
		if (mmc->tran_speed >= 20000000)
			regs->sysctl = SYSCTL_20MHz;

		return 0;
	}

	/* Get the Card Status Register */
	cmd.cmdidx = SEND_EXT_CSD;
	cmd.resp_type = MMC_CMD_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	data.buffer = (char *)&csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);

#warning Not done !
	return err;
}

int sd_change_freq(struct mmc *mmc)
{
	int err;
	struct mmc_cmd cmd;
	uint scr[2];
	uint switch_status[16];
	volatile struct fsl_esdhc *regs = mmc->regs;
	struct mmc_data data;
	int timeout;

	/* Nothing to change if it doesn't support 13MHz */
	if (mmc->tran_speed < 13000000)
		return 0;

#warning need to actually calculate clock speed
	/* Up it to 13 MHz right now */
	regs->sysctl = SYSCTL_13MHz;

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = APP_CMD;
	cmd.resp_type = MMC_CMD_R1;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	cmd.cmdidx = SEND_SCR;
	cmd.resp_type = MMC_CMD_R1;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	timeout = 3;

retry_scr:
	data.buffer = (char *)&scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(mmc, &cmd, &data);

	if (err) {
		if (timeout--)
			goto retry_scr;

		return err;
	}

	mmc->scr[0] = scr[0];
	mmc->scr[1] = scr[1];

	switch ((mmc->scr[0] >> 24) & 0xf) {
	case 0:
		mmc->version = SD_VERSION_1_0;
		break;
	case 1:
		mmc->version = SD_VERSION_1_1;
		break;
	case 2:
		mmc->version = SD_VERSION_2;
		break;
	default:
		mmc->version = SD_VERSION_1_0;
		break;
	}

	/* Version 1.0 doesn't support switching */
	if (mmc->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		/* Switch the frequency */
		cmd.cmdidx = SWITCH_FUNC;
		cmd.resp_type = MMC_CMD_R1;
		cmd.cmdarg = 0xfffff1;
		cmd.flags = 0;

		data.buffer = (char *)&switch_status;
		data.blocksize = 64;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;

		err = mmc_send_cmd(mmc, &cmd, &data);

		if (err)
			return err;

		/* The high-speed function is busy.  Try again */
		if (switch_status[7] & SD_HIGHSPEED_BUSY)
			continue;

		/* If high-speed isn't supported, we return */
		if (!(switch_status[3] & SD_HIGHSPEED_SUPPORTED))
			return 0;

		cmd.cmdidx = SWITCH_FUNC;
		cmd.resp_type = MMC_CMD_R1;
		cmd.cmdarg = 0x80fffff1;
		cmd.flags = 0;

		data.buffer = (char *)&switch_status;
		data.blocksize = 64;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;

		err = mmc_send_cmd(mmc, &cmd, &data);

		if (err)
			return err;

		if ((switch_status[4] & 0x0f000000) != 0x0f000000) {
			regs->sysctl = SYSCTL_50MHz;
			mmc->mode |= MMC_MODE_HS;
			break;
		}
	}

	return 0;
}

/* frequency bases */
/* divided by 10 to be nice to platforms without floating point */
int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
int multipliers[] = {
	0,	/* reserved */
	10,
	12,
	13,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	55,
	60,
	70,
	80,
};


int mmc_startup(struct mmc *mmc)
{
	int err;
	uint mult, freq;
	uint proctl_value;
	struct mmc_cmd cmd;
	volatile struct fsl_esdhc *regs = mmc->regs;

	/* Put the Card in Identify Mode */
	cmd.cmdidx = ALL_SEND_CID;
	cmd.resp_type = MMC_CMD_R2;
	cmd.cmdarg = 0;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	/* fill in device description */
	mmc->block_dev.if_type = IF_TYPE_MMC;
	mmc->block_dev.part_type = PART_TYPE_DOS;
	mmc->block_dev.dev = 0;
	mmc->block_dev.lun = 0;
	mmc->block_dev.type = 0;
	mmc->block_dev.blksz = mmc->read_bl_len;
	mmc->block_dev.lba = 0x10000;

	sprintf(mmc->block_dev.vendor, "Man %02x%04x Snr %08x", cmd.response[0],
			(cmd.response[1] << 8) | cmd.response[2],
			(((uint *)(cmd.response))[2] << 8) | cmd.response[12]);
	strncpy((char *)mmc->block_dev.product, &(cmd.response[3]), 5);
	sprintf(mmc->block_dev.revision, "%x %x", cmd.response[8] >> 4,
			cmd.response[8] & 0x0f);
	mmc->block_dev.removable = 0;
	mmc->block_dev.block_read = mmc_bread;

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */
	cmd.cmdidx = SEND_RELATIVE_ADDR;
	cmd.cmdarg = mmc->rca << 16;
	cmd.resp_type = MMC_CMD_R6;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	if (IS_SD(mmc))
		mmc->rca = (((uint *)(cmd.response))[0] >> 16) & 0xffff;

	/* Get the Card-Specific Data */
	cmd.cmdidx = SEND_CSD;
	cmd.resp_type = MMC_CMD_R2;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	mmc->csd[0] = ((uint *)(cmd.response))[0];
	mmc->csd[1] = ((uint *)(cmd.response))[1];
	mmc->csd[2] = ((uint *)(cmd.response))[2];
	mmc->csd[3] = ((uint *)(cmd.response))[3];

	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = cmd.response[0] >> 2;

		switch (version) {
		case 0:
			mmc->version = MMC_VERSION_1_2;
			break;
		case 1:
			mmc->version = MMC_VERSION_1_4;
			break;
		case 2:
			mmc->version = MMC_VERSION_2_2;
			break;
		case 3:
			mmc->version = MMC_VERSION_3;
			break;
		case 4:
			mmc->version = MMC_VERSION_4;
			break;
		default:
			mmc->version = MMC_VERSION_1_2;
			break;
		}
	}

	/* divide frequency by 10, since the mults are 10x bigger */
	freq = fbase[(cmd.response[3] & 0x7)];
	mult = multipliers[((cmd.response[3] >> 3) & 0xf)];

	mmc->tran_speed = freq * mult;

	mmc->read_bl_len = 1 << ((((uint *)(cmd.response))[1] >> 16) & 0xf);

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((((uint *)(cmd.response))[3] >> 22) &
					 0xf);

	/* Select the card, and put it into Transfer Mode */
	cmd.cmdidx = SELECT_CARD;
	cmd.resp_type = MMC_CMD_R1b;
	cmd.cmdarg = mmc->rca << 16;
	cmd.flags = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	if (IS_SD(mmc))
		err = sd_change_freq(mmc);
	else
		err = mmc_change_freq(mmc);

	if (err)
		return err;

	if (IS_SD(mmc) && (mmc->scr[0] & SD_DATA_4BIT)) {
		cmd.cmdidx = APP_CMD;
		cmd.resp_type = MMC_CMD_R1;
		cmd.cmdarg = mmc->rca << 16;
		cmd.flags = 0;
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			printf("app_cmd %d\n", err);

		cmd.cmdidx = SET_BUS_WIDTH;
		cmd.resp_type = MMC_CMD_R1;
		cmd.cmdarg = 0x00000002;
		cmd.flags = 0;
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			printf("set_bus_width %d\n", err);

		proctl_value = regs->proctl;
		regs->proctl = proctl_value | PROCTL_DTW_4;
		mmc->mode |= MMC_MODE_4BIT;
	}

	init_part(&mmc->block_dev);

	if (mmc->mode & MMC_MODE_4BIT)
		printf("4bit-buswidth");
	else
		printf("1bit-buswidth");

	if (mmc->mode & MMC_MODE_HS)
		printf(" & High-speed mode\n");
	else
		printf(" mode\n");

	return 0;
}

int mmc_go_idle(struct mmc *mmc)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_CMD_RSP_NONE;
	cmd.flags = 0;

	return mmc_send_cmd(mmc, &cmd, NULL);
}

int mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = SEND_IF_COND;
	cmd.cmdarg = 0x1aa;
	cmd.resp_type = MMC_CMD_R7;
	cmd.flags = 0;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	if (((uint *)(cmd.response))[0] != 0x1aa)
		return UNUSABLE_ERR;
	else
		mmc->version = SD_VERSION_2;

	return 0;
}

int
mmc_init(int verbose)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	struct fsl_esdhc *regs = (struct fsl_esdhc *)&im->sdhc;
	int timeout = 1000;
	struct mmc *mmc;
	int err;

	mmc = malloc(sizeof(struct mmc));

	mmc->regs = regs;
	mmc->mode = 0;

	mmc_dev = mmc;

	/* Set the clock speed */
	regs->sysctl = SYSCTL_400KHz;

	/* Disable the BRR and BWR bits in IRQSTAT */
	regs->irqstaten &= ~(IRQSTATEN_BRR | IRQSTATEN_BWR);

	while (!(regs->prsstat & PRSSTAT_CINS) && timeout--)
		udelay(1000);

	if (timeout <= 0) {
		printf("No SD/MMC card found\n");
		return NO_CARD_ERR;
	}

	/* Reset the Card */
	err = mmc_go_idle(mmc);

	if (err)
		return err;

	/* Test for SD version 2 */
	err = mmc_send_if_cond(mmc);

	/* If we got an error other than timeout, we bail */
	if (err && err != TIMEOUT)
		return err;

	/* Now try to get the SD card's operating condition */
	err = sd_send_op_cond(mmc);

	/* If the command timed out, we check for an MMC card */
	if (err == TIMEOUT) {
		err = mmc_send_op_cond(mmc);

		if (err)
			return UNUSABLE_ERR;
	}

	return mmc_startup(mmc);
}

int
mmc2info(ulong addr)
{
	/* Hmm... */
	return 0;
}

#endif	/* CONFIG_MMC */
