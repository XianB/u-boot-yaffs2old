/*
 * Copyright (C) Freescale Semiconductor, Inc. 2006.
 *
 * Initialized by Nick.Spence@freescale.com
 *                Wilson.Lo@freescale.com
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

#include <common.h>

#if defined(CONFIG_CMD_NAND)
#if defined(CFG_NAND_LEGACY)
 #error "U-Boot legacy NAND commands not supported."
#else

#include <malloc.h>
#include <asm/errno.h>
#include <nand.h>

//#undef CFG_FCM_DEBUG
#define CFG_FCM_DEBUG
#define CFG_FCM_DEBUG_LVL 1
#ifdef CFG_FCM_DEBUG
#define FCM_DEBUG(n, args...)				\
	do {						\
		if (n <= (CFG_FCM_DEBUG_LVL + 0))	\
			printf(args);			\
	} while (0)
#else /* CONFIG_FCM_DEBUG */
#define FCM_DEBUG(n, args...) do { } while (0)
#endif

#define MIN(x, y)		((x < y) ? x : y)

#define ERR_BYTE 0xFF	/* Value returned for read bytes when read failed */

#define FCM_TIMEOUT_USECS 100000 /* Maximum number of uSecs to wait for FCM */

/* Private structure holding NAND Flash device specific information */
struct fcm_nand {
	int		bank;       /* Chip select bank number             */
	unsigned int	base;       /* Chip select base address            */
	int		pgs;        /* NAND page size                      */
	int		oobbuf;     /* Pointer to OOB block                */
	unsigned int	page;       /* Last page written to / read from    */
	unsigned int	fmr;        /* FCM Flash Mode Register value       */
	unsigned int	mdr;        /* UPM/FCM Data Register value         */
	unsigned int	use_mdr;    /* Non zero if the MDR is to be set    */
	u_char	       *addr;       /* Address of assigned FCM buffer      */
	unsigned int	read_bytes; /* Number of bytes read during command */
	unsigned int	index;      /* Pointer to next byte to 'read'      */
	unsigned int	req_bytes;  /* Number of bytes read if command ok  */
	unsigned int	req_index;  /* New read index if command ok        */
	unsigned int	status;     /* status read from LTESR after last op*/
};


/* These map to the positions used by the FCM hardware ECC generator */

/* Small Page FLASH with FMR[ECCM] = 0 */
static struct nand_oobinfo nooobinfo = { /* TODO */
	.useecc = 0, /* MTD_NANDECC_PLACEONLY, */
};

/* Small Page FLASH with FMR[ECCM] = 0 */
static struct nand_oobinfo fcm_oob_sp_eccm0 = { /* TODO */
	.useecc = MTD_NANDECC_AUTOPL_USR, /* MTD_NANDECC_PLACEONLY, */
	.eccbytes = 3,
	.eccpos = {6, 7, 8},
	.oobfree = { {0, 5}, {9, 7} }
};

/* Small Page FLASH with FMR[ECCM] = 1 */
static struct nand_oobinfo fcm_oob_sp_eccm1 = { /* TODO */
	.useecc = MTD_NANDECC_AUTOPL_USR, /* MTD_NANDECC_PLACEONLY, */
	.eccbytes = 3,
	.eccpos = {8, 9, 10},
	.oobfree = { {0, 5}, {6, 2}, {11, 5} }
};

/* Large Page FLASH with FMR[ECCM] = 0 */
static struct nand_oobinfo fcm_oob_lp_eccm0 = {
	.useecc = MTD_NANDECC_AUTOPL_USR, /* MTD_NANDECC_PLACEONLY, */
	.eccbytes = 12,
	.eccpos = {6, 7, 8, 22, 23, 24, 38, 39, 40, 54, 55, 56},
	.oobfree = { {1, 5}, {9, 13}, {25, 13}, {41, 13}, {57, 7} }
};

/* Large Page FLASH with FMR[ECCM] = 1 */
static struct nand_oobinfo fcm_oob_lp_eccm1 = {
	.useecc = MTD_NANDECC_AUTOPL_USR, /* MTD_NANDECC_PLACEONLY, */
	.eccbytes = 12,
	.eccpos = {8, 9, 10, 24, 25, 26, 40, 41, 42, 56, 57, 58},
	.oobfree = { {1, 7}, {11, 13}, {27, 13}, {43, 13}, {59, 5} }
};

/*
 * execute FCM command and wait for it to complete
 */
static int fcm_run_command(struct mtd_info *mtd)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;
	long long end_tick;

	/* Setup the FMR[OP] to execute without write protection */
	lbc->fmr = fcm->fmr | 3;
	if (fcm->use_mdr)
		lbc->mdr = fcm->mdr;

	FCM_DEBUG(5, "fcm_run_command: fmr= %08X fir= %08X fcr= %08X\n",
		lbc->fmr, lbc->fir, lbc->fcr);
	FCM_DEBUG(5, "fcm_run_command: fbar=%08X fpar=%08X fbcr=%08X bank=%d\n",
		lbc->fbar, lbc->fpar, lbc->fbcr, fcm->bank);

	/* clear event registers */
	lbc->lteatr = 0;
	lbc->ltesr |= (LTESR_FCT | LTESR_PAR | LTESR_CC);

	/* execute special operation */
	lbc->lsor = fcm->bank;

	/* wait for FCM complete flag or timeout */
	fcm->status = 0;
	end_tick = usec2ticks(FCM_TIMEOUT_USECS) + get_ticks();

	while (end_tick > get_ticks()) {
		if (lbc->ltesr & LTESR_CC) {
			fcm->status = lbc->ltesr &
					(LTESR_FCT | LTESR_PAR | LTESR_CC);
			break;
		}
	}

	/* store mdr value in case it was needed */
	if (fcm->use_mdr)
		fcm->mdr = lbc->mdr;

	fcm->use_mdr = 0;

	FCM_DEBUG(5, "fcm_run_command: stat=%08X mdr= %08X fmr= %08X\n",
		fcm->status, fcm->mdr, lbc->fmr);

	/* if the operation completed ok then set the read buffer pointers */
	if (fcm->status == LTESR_CC) {
		fcm->read_bytes = fcm->req_bytes;
		fcm->index      = fcm->req_index;
		return 0;
	}

	return -1;
}

/*
 * Set up the FCM hardware block and page address fields, and the fcm
 * structure addr field to point to the correct FCM buffer in memory
 */
static void set_addr(struct mtd_info *mtd, int column, int page_addr, int oob)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;
	int buf_num;

	fcm->page = page_addr;

	lbc->fbar = page_addr >> (this->phys_erase_shift - this->page_shift);
	if (fcm->pgs) {
		lbc->fpar = ((page_addr << FPAR_LP_PI_SHIFT) & FPAR_LP_PI) |
			    (oob ? FPAR_LP_MS : 0) |
			     column;
		buf_num = (page_addr & 1) << 2;
	} else {
		lbc->fpar = ((page_addr << FPAR_SP_PI_SHIFT) & FPAR_SP_PI) |
			    (oob ? FPAR_SP_MS : 0) |
			     column;
		buf_num = page_addr & 7;
	}
	fcm->addr = (unsigned char *)(fcm->base + (buf_num * 1024));

	/* for OOB data point to the second half of the buffer */
	if (oob)
		fcm->addr += (fcm->pgs ? 2048 : 512);
}

/* not required for FCM */
static void fcm_hwcontrol(struct mtd_info *mtdinfo, int cmd)
{
	return;
}


/*
 * FCM does not support 16 bit data busses
 */
static u16 fcm_read_word(struct mtd_info *mtd)
{
	printf("fcm_read_word: UNIMPLEMENTED.\n");
	return 0;
}
static void fcm_write_word(struct mtd_info *mtd, u16 word)
{
	printf("fcm_write_word: UNIMPLEMENTED.\n");
}

/*
 * Write buf to the FCM Controller Data Buffer
 */
static void fcm_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;

	FCM_DEBUG(3, "fcm_write_buf: writing %d bytes starting with 0x%x"
		     " at %d.\n", len, *((unsigned long *) buf), fcm->index);

	/* If armed catch the address of the OOB buffer so that it can be */
	/* updated with the real signature after the program comletes */
	if (!fcm->oobbuf)
		fcm->oobbuf = (int) buf;

	/* copy the data into the FCM hardware buffer and update the index */
	memcpy(&(fcm->addr[fcm->index]), buf, len);
	fcm->index += len;
	return;
}


/*
 * FCM does not support individual writes. Instead these are either commands
 * or data being written, both of which are handled through the cmdfunc
 * handler.
 */
static void fcm_write_byte(struct mtd_info *mtd, u_char byte)
{
	printf("fcm_write_byte: UNIMPLEMENTED.\n");
}

/*
 * read a byte from either the FCM hardware buffer if it has any data left
 * otherwise issue a command to read a single byte.
 */
static u_char fcm_read_byte(struct mtd_info *mtd)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;
	unsigned char byte;

	/* If there are still bytes in the FCM then use the next byte */
	if (fcm->index < fcm->read_bytes) {
		byte = fcm->addr[(fcm->index)++];
		FCM_DEBUG(4, "fcm_read_byte: byte %u (%02X): %d of %d.\n",
			  byte, byte, fcm->index-1, fcm->read_bytes);
	} else {
		/* otherwise issue a command to read 1 byte */
		lbc->fir = (FIR_OP_RSW << FIR_OP0_SHIFT);
		fcm->use_mdr = 1;
		fcm->read_bytes = 0;
		fcm->index = 0;
		fcm->req_bytes = 0;
		fcm->req_index = 0;
		byte = fcm_run_command(mtd) ? ERR_BYTE : fcm->mdr & 0xff;
		FCM_DEBUG(4, "fcm_read_byte: byte %u (%02X) from bus.\n",
			  byte, byte);
	}

	return byte;
}


/*
 * Read from the FCM Controller Data Buffer
 */
static void fcm_read_buf(struct mtd_info *mtd, u_char * buf, int len)
{
	volatile immap_t *im  = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;
	int i;
	int rest;

	FCM_DEBUG(3, "fcm_read_buf: reading %d bytes.\n", len);

	/* If last read failed then return error bytes */
	if (fcm->status != LTESR_CC) {
		/* just keep copying bytes so that the oob works */
		memcpy(buf, &(fcm->addr[(fcm->index)]), len);
		fcm->index += len;
	} else {
		/* see how much is still in the FCM buffer */
		i = min(len, (fcm->read_bytes - fcm->index));
		rest = i - len;
		len = i;

		memcpy(buf, &(fcm->addr[(fcm->index)]), len);
		fcm->index += len;

		/* If more data is needed then issue another block read */
		if (rest) {
			FCM_DEBUG(3, "fcm_read_buf: getting %d more bytes.\n",
				  rest);
			buf += len;
			lbc->fir = (FIR_OP_RBW << FIR_OP0_SHIFT);
			set_addr(mtd, 0, 0, 0);
			lbc->fbcr = rest;
			fcm->req_bytes = lbc->fbcr;
			fcm->req_index = 0;
			fcm->use_mdr = 0;
			if (!fcm_run_command(mtd))
				fcm_read_buf(mtd, buf, rest);
			else
				memcpy(buf, fcm->addr, rest);
		}
	}
	return;
}


/*
 * Verify buffer against the FCM Controller Data Buffer
 */
static int fcm_verify_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;
	int i;
	int rest;

	FCM_DEBUG(3, "fcm_verify_buf: checking %d bytes starting with "
		     "0x%02x.\n", len, *((unsigned long *) buf));
	/* If last read failed then return error bytes */
	if (fcm->status != LTESR_CC)
		return EFAULT;

	/* see how much is still in the FCM buffer */
	i = min(len, (fcm->read_bytes - fcm->index));
	rest = i - len;
	len = i;

	if (memcmp(buf,	&(fcm->addr[(fcm->index)]), len))
		return EFAULT;

	fcm->index += len;
	if (rest) {
		FCM_DEBUG(3, "fcm_verify_buf: getting %d more bytes.\n", rest);
		buf += len;
		lbc->fir = (FIR_OP_RBW << FIR_OP0_SHIFT);
		set_addr(mtd, 0, 0, 0);
		lbc->fbcr = rest;
		fcm->req_bytes = lbc->fbcr;
		fcm->req_index = 0;
		fcm->use_mdr = 0;
		if (fcm_run_command(mtd))
			return EFAULT;
		return fcm_verify_buf(mtd, buf, rest);

	}
	return 0;
}

/* this function is called after Program and Erase Operations to
 * check for success or failure */
static int fcm_wait(struct mtd_info *mtd, struct nand_chip *this, int state)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	struct fcm_nand *fcm = this->priv;

	if (fcm->status != LTESR_CC) {
		return(0x1); /* Status Read error */
	}

	/* Use READ_STATUS command, but wait for the device to be ready */
	fcm->use_mdr = 0;
	fcm->req_index = 0;
	fcm->read_bytes = 0;
	fcm->index = 0;
	fcm->oobbuf = -1;
	lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
		   (FIR_OP_RBW << FIR_OP1_SHIFT);
	lbc->fcr = (NAND_CMD_STATUS << FCR_CMD0_SHIFT);
	set_addr(mtd, 0, 0, 0);
	lbc->fbcr = 1;
	fcm->req_bytes = lbc->fbcr;
#ifndef CONFIG_MPC8315ERDB
	fcm_run_command(mtd);
#else
/* ERASE is 400ms for Spansion S30ML-P ORNAND series */
/* timeout exceed the max LCLK in CWTO */
/* NAND will not accept next cmd when busy (LFRB high) */
/* so it's okay to issue multi command to it as a way to poll the LFRB */
	while (1) {
		fcm_run_command(mtd);
		if ((fcm->status&LTESR_FCT) != LTESR_FCT)
			break;
	}
#endif
	if (fcm->status != LTESR_CC)
		return(0x1); /* Status Read error */

	return this->read_byte(mtd);
}


/* cmdfunc send commands to the FCM */
static void fcm_cmdfunc(struct mtd_info *mtd, unsigned command,
			int column, int page_addr)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;

	fcm->use_mdr = 0;
	fcm->req_index = 0;

	/* clear the read buffer */
	fcm->read_bytes = 0;
	if (command != NAND_CMD_PAGEPROG) {
		fcm->index = 0;
		fcm->oobbuf = -1;
	}

	switch (command) {
	/* READ0 and READ1 read the entire buffer to use hardware ECC */
	case NAND_CMD_READ1:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_READ1, page_addr: "
			     "0x%x, column: 0x%x.\n", page_addr, column);
		fcm->req_index = column + 256;
		goto read0;
	case NAND_CMD_READ0:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_READ0, page_addr: "
			     "0x%x, column: 0x%x.\n", page_addr, column);
		fcm->req_index = column;
read0:
		if (fcm->pgs) {
			lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
				   (FIR_OP_CA  << FIR_OP1_SHIFT) |
				   (FIR_OP_PA  << FIR_OP2_SHIFT) |
				   (FIR_OP_CW1 << FIR_OP3_SHIFT) |
				   (FIR_OP_RBW << FIR_OP4_SHIFT);
		} else {
			lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
				   (FIR_OP_CA  << FIR_OP1_SHIFT) |
				   (FIR_OP_PA  << FIR_OP2_SHIFT) |
				   (FIR_OP_RBW << FIR_OP3_SHIFT);
		}
		lbc->fcr = (NAND_CMD_READ0     << FCR_CMD0_SHIFT) |
			   (NAND_CMD_READSTART << FCR_CMD1_SHIFT);
		lbc->fbcr = 0; /* read entire page to enable ECC */
		set_addr(mtd, 0, page_addr, 0);
		fcm->req_bytes = mtd->oobblock + mtd->oobsize;
		goto write_cmd2;
	/* READOOB read only the OOB becasue no ECC is performed */
	case NAND_CMD_READOOB:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_READOOB, page_addr: "
			     "0x%x, column: 0x%x.\n", page_addr, column);
		if (fcm->pgs) {
			lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
				   (FIR_OP_CA  << FIR_OP1_SHIFT) |
				   (FIR_OP_PA  << FIR_OP2_SHIFT) |
				   (FIR_OP_CW1 << FIR_OP3_SHIFT) |
				   (FIR_OP_RBW << FIR_OP4_SHIFT);
			lbc->fcr = (NAND_CMD_READ0     << FCR_CMD0_SHIFT) |
				   (NAND_CMD_READSTART << FCR_CMD1_SHIFT);
		} else {
			lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
				   (FIR_OP_CA  << FIR_OP1_SHIFT) |
				   (FIR_OP_PA  << FIR_OP2_SHIFT) |
				   (FIR_OP_RBW << FIR_OP3_SHIFT);
			lbc->fcr = (NAND_CMD_READOOB << FCR_CMD0_SHIFT);
		}
		lbc->fbcr = mtd->oobsize - column;
		set_addr(mtd, column, page_addr, 1);
		goto write_cmd1;
	/* READID must read all 5 possible bytes while CEB is active */
	case NAND_CMD_READID:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_READID.\n");
		lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
			   (FIR_OP_UA  << FIR_OP1_SHIFT) |
			   (FIR_OP_RBW << FIR_OP2_SHIFT);
		lbc->fcr = (NAND_CMD_READID << FCR_CMD0_SHIFT);
		lbc->fbcr = 5; /* 5 bytes for manuf, device and exts */
		fcm->use_mdr = 1;
		fcm->mdr = 0;
		goto write_cmd0;
	/* ERASE1 stores the block and page address */
	case NAND_CMD_ERASE1:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_ERASE1, page_addr: "
			     "0x%x.\n", page_addr);
		set_addr(mtd, 0, page_addr, 0);
		goto end;
	/* ERASE2 uses the block and page address from ERASE1 */
	case NAND_CMD_ERASE2:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_ERASE2.\n");
		lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
			   (FIR_OP_PA  << FIR_OP1_SHIFT) |
			   (FIR_OP_CM1 << FIR_OP2_SHIFT);
		lbc->fcr = (NAND_CMD_ERASE1 << FCR_CMD0_SHIFT) |
			   (NAND_CMD_ERASE2 << FCR_CMD1_SHIFT);
		lbc->fbcr = 0;
		goto write_cmd1;
	/* SEQIN sets up the addr buffer and all registers except the length */
	case NAND_CMD_SEQIN:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_SEQIN/PAGE_PROG, "
			     "page_addr: 0x%x, column: 0x%x.\n",
			  page_addr, column);
		if (column == 0) {
			lbc->fbcr = 0; /* write entire page to enable ECC */
		} else {
			lbc->fbcr = 1; /* mark as partial page so no HW ECC */
		}
		if (fcm->pgs) {
			/* always use READ0 for large page devices */
			lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
				   (FIR_OP_CA  << FIR_OP1_SHIFT) |
				   (FIR_OP_PA  << FIR_OP2_SHIFT) |
				   (FIR_OP_WB  << FIR_OP3_SHIFT) |
				   (FIR_OP_CW1 << FIR_OP4_SHIFT);
			lbc->fcr = (NAND_CMD_SEQIN << FCR_CMD0_SHIFT) |
				   (NAND_CMD_PAGEPROG << FCR_CMD1_SHIFT);
			set_addr(mtd, column, page_addr, 0);
		} else {
			lbc->fir = (FIR_OP_CW0 << FIR_OP0_SHIFT) |
				   (FIR_OP_CM2 << FIR_OP1_SHIFT) |
				   (FIR_OP_CA  << FIR_OP2_SHIFT) |
				   (FIR_OP_PA  << FIR_OP3_SHIFT) |
				   (FIR_OP_WB  << FIR_OP4_SHIFT) |
				   (FIR_OP_CW1 << FIR_OP5_SHIFT);
			if (column >= mtd->oobblock) {
				/* OOB area --> READOOB */
				column -= mtd->oobblock;
				lbc->fcr = (NAND_CMD_READOOB << FCR_CMD0_SHIFT)
					 | (NAND_CMD_PAGEPROG << FCR_CMD1_SHIFT)
					 | (NAND_CMD_SEQIN << FCR_CMD2_SHIFT);
				set_addr(mtd, column, page_addr, 1);
			} else if (column < 256) {
				/* First 256 bytes --> READ0 */
				lbc->fcr = (NAND_CMD_READ0 << FCR_CMD0_SHIFT)
					 | (NAND_CMD_PAGEPROG << FCR_CMD1_SHIFT)
					 | (NAND_CMD_SEQIN << FCR_CMD2_SHIFT);
				set_addr(mtd, column, page_addr, 0);
			} else {
				/* Second 256 bytes --> READ1 */
				column -= 256;
				lbc->fcr = (NAND_CMD_READ1 << FCR_CMD0_SHIFT)
					 | (NAND_CMD_PAGEPROG << FCR_CMD1_SHIFT)
					 | (NAND_CMD_SEQIN << FCR_CMD2_SHIFT);
				set_addr(mtd, column, page_addr, 0);
			}
		}
		goto end;
	/* PAGEPROG reuses all of the setup from SEQIN and adds the length */
	case NAND_CMD_PAGEPROG:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_PAGEPROG"
			     " writing %d bytes.\n", fcm->index);
		/* if the write did not start at 0 or is not a full page */
		/* then set the exact length, otherwise use a full page  */
		/* write so the HW generates the ECC. */
		if (lbc->fbcr ||
		   (fcm->index != (mtd->oobblock + mtd->oobsize)))
			lbc->fbcr = fcm->index;
		fcm->req_bytes = 0;
		goto write_cmd2;
	/* CMD_STATUS must read the status byte while CEB is active */
	/* Note - it does not wait for the ready line */
	case NAND_CMD_STATUS:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_STATUS.\n");
		lbc->fir = (FIR_OP_CM0 << FIR_OP0_SHIFT) |
			   (FIR_OP_RBW << FIR_OP1_SHIFT);
		lbc->fcr = (NAND_CMD_STATUS << FCR_CMD0_SHIFT);
		lbc->fbcr = 1;
		goto write_cmd0;
	/* RESET without waiting for the ready line */
	case NAND_CMD_RESET:
		FCM_DEBUG(2, "fcm_cmdfunc: NAND_CMD_RESET.\n");
		lbc->fir = (FIR_OP_CM0 << FIR_OP0_SHIFT);
		lbc->fcr = (NAND_CMD_RESET << FCR_CMD0_SHIFT);
		lbc->fbcr = 0;
		goto write_cmd0;
	default:
		printf("fcm_cmdfunc: error, unsupported command.\n");
		goto end;
	}

	/* Short cuts fall through to save code */
 write_cmd0:
	set_addr(mtd, 0, 0, 0);
 write_cmd1:
	fcm->req_bytes = lbc->fbcr;
 write_cmd2:
	fcm_run_command(mtd);

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	/* if we wrote a page then read back the oob to get the ECC */
	if ((command == NAND_CMD_PAGEPROG) &&
	    (this->eccmode > NAND_ECC_SOFT) &&
	    (lbc->fbcr == 0) &&
	    (fcm->oobbuf != 0) &&
	    (fcm->oobbuf != -1)) {
		int i;
		uint *oob_config;
		unsigned char *oob_buf;

		i = fcm->page;
		oob_buf = (unsigned char *) fcm->oobbuf;
		oob_config = this->autooob->eccpos;

		/* wait for the write to complete and check it passed */
		if (!(this->waitfunc(mtd, this, FL_WRITING) & 0x01)) {
			/* read back the OOB */
			fcm_cmdfunc(mtd, NAND_CMD_READOOB, 0, i);
			/* if it succeeded then copy the ECC bytes */
			if (fcm->status == LTESR_CC) {
				for (i = 0; i < this->eccbytes; i++)
					oob_buf[oob_config[i]] =
						fcm->addr[oob_config[i]];
			}
		}
	}
#endif

 end:
	return;
}

/*
 * fcm_enable_hwecc - start ECC generation
 */
static void fcm_enable_hwecc(struct mtd_info *mtd, int mode)
{
	return;
}

/*
 * fcm_calculate_ecc - Calculate the ECC bytes
 * This is done by hardware during the write process, so we use this
 * to arm the oob buf capture on the next write_buf() call. The ECC bytes
 * only need to be captured if CONFIG_MTD_NAND_VERIFY_WRITE is defined which
 * reads back the pages and checks they match the data and oob buffers.
 */
static int fcm_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
			     u_char *ecc_code)
{
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;

#ifdef CONFIG_MTD_NAND_VERIFY_WRITE
	/* arm capture of oob buf ptr on next write_buf */
	fcm->oobbuf = 0;
#endif
	return 0;
}

/*
 * fcm_correct_data - Detect and correct bit error(s)
 * The detection and correction is done automatically by the hardware,
 * if the complete page was read. If the status code is okay then there
 * was no error, otherwise we return an error code indicating an uncorrectable
 * error.
 */
static int fcm_correct_data(struct mtd_info *mtd, u_char *dat,
			    u_char *read_ecc, u_char *calc_ecc)
{
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;

	/* No errors */
	if (fcm->status == LTESR_CC)
		return 0;

	return -1; /* uncorrectable error */
}

/*
 * Dummy scan_bbt to complete setup of the FMR based on NAND size
 */
static int fcm_scan_bbt(struct mtd_info *mtd)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	register struct nand_chip *this = mtd->priv;
	struct fcm_nand *fcm = this->priv;
	unsigned int i;
	unsigned int al;

	if (!fcm) {
		printf("fcm_scan_bbt(): " \
		       "Failed to allocate chip specific data structure\n");
		return -1;
	}

	/* calculate FMR Address Length field */
	al = 0;
	for (i = this->pagemask >> 16; i ; i >>= 8)
		al++;

	/* add to ECCM mode set in fcm_init */
	fcm->fmr |= 12 << FMR_CWTO_SHIFT |  /* Timeout > 12 mSecs */
		    al << FMR_AL_SHIFT;

	FCM_DEBUG(1, "fcm_init: nand->options  =   %08X\n", this->options);
	FCM_DEBUG(1, "fcm_init: nand->numchips = %10d\n", this->numchips);
	FCM_DEBUG(1, "fcm_init: nand->chipsize = %10d\n", this->chipsize);
	FCM_DEBUG(1, "fcm_init: nand->pagemask = %10X\n", this->pagemask);
	FCM_DEBUG(1, "fcm_init: nand->eccmode  = %10d\n", this->eccmode);
	FCM_DEBUG(1, "fcm_init: nand->eccsize  = %10d\n", this->eccsize);
	FCM_DEBUG(1, "fcm_init: nand->eccbytes = %10d\n", this->eccbytes);
	FCM_DEBUG(1, "fcm_init: nand->eccsteps = %10d\n", this->eccsteps);
	FCM_DEBUG(1, "fcm_init: nand->chip_delay = %8d\n", this->chip_delay);
	FCM_DEBUG(1, "fcm_init: nand->badblockpos = %7d\n", this->badblockpos);
	FCM_DEBUG(1, "fcm_init: nand->chip_shift = %8d\n", this->chip_shift);
	FCM_DEBUG(1, "fcm_init: nand->page_shift = %8d\n", this->page_shift);
	FCM_DEBUG(1, "fcm_init: nand->phys_erase_shift = %2d\n",
						      this->phys_erase_shift);
	FCM_DEBUG(1, "fcm_init: mtd->flags     =   %08X\n", mtd->flags);
	FCM_DEBUG(1, "fcm_init: mtd->size      = %10d\n", mtd->size);
	FCM_DEBUG(1, "fcm_init: mtd->erasesize = %10d\n", mtd->erasesize);
	FCM_DEBUG(1, "fcm_init: mtd->oobblock  = %10d\n", mtd->oobblock);
	FCM_DEBUG(1, "fcm_init: mtd->oobsize   = %10d\n", mtd->oobsize);
	FCM_DEBUG(1, "fcm_init: mtd->oobavail  = %10d\n", mtd->oobavail);
	FCM_DEBUG(1, "fcm_init: mtd->ecctype   = %10d\n", mtd->ecctype);
	FCM_DEBUG(1, "fcm_init: mtd->eccsize   = %10d\n", mtd->eccsize);

	/* adjust Option Register and ECC to match Flash page size */
	if (mtd->oobblock == 512)
		lbc->bank[fcm->bank].or &= ~(OR_FCM_PGS);
	else if (mtd->oobblock == 2048) {
		lbc->bank[fcm->bank].or |= OR_FCM_PGS;
		/* adjust ecc setup if needed */
		if ((lbc->bank[fcm->bank].br & BR_DECC) == BR_DECC_CHK_GEN) {
			mtd->eccsize = 2048;
			mtd->oobavail -= 9;
			this->eccmode = NAND_ECC_HW12_2048;
			this->eccsize = 2048;
			this->eccbytes += 9;
			this->eccsteps = 1;
			this->autooob = (fcm->fmr & FMR_ECCM) ?
					&fcm_oob_lp_eccm1 : &fcm_oob_lp_eccm0;
			memcpy(&mtd->oobinfo, this->autooob,
					sizeof(mtd->oobinfo));
		}
	} else {
		printf("fcm_init: page size %d is not supported\n",
			mtd->oobblock);
		return -1;
	}
	fcm->pgs = (lbc->bank[fcm->bank].or>>OR_FCM_PGS_SHIFT) & 1;

	if (al > 2) {
		printf("fcm_init: %d address bytes is not supported\n", al+2);
		return -1;
	}

	/* restore default scan_bbt function and call it */
	this->scan_bbt = nand_default_bbt;
	return nand_default_bbt(mtd);
}

/*
 * Board-specific NAND initialization. The following members of the
 * argument are board-specific (per include/linux/mtd/nand_new.h):
 * - IO_ADDR_R?: address to read the 8 I/O lines of the flash device
 * - IO_ADDR_W?: address to write the 8 I/O lines of the flash device
 * - hwcontrol: hardwarespecific function for accesing control-lines
 * - dev_ready: hardwarespecific function for accesing device ready/busy line
 * - enable_hwecc: function to enable (reset) hardware ecc generator. Must
 *   only be provided if a hardware ECC is available
 * - eccmode: mode of ecc, see defines
 * - chip_delay: chip dependent delay for transfering data from array to
 *   read regs (tR)
 * - options: various chip options. They can partly be set to inform
 *   nand_scan about special functionality. See the defines for further
 *   explanation
 * Members with a "?" were not set in the merged testing-NAND branch,
 * so they are not set here either.
 */
int board_nand_init(struct nand_chip *nand)
{
	volatile immap_t *im = (immap_t *) CFG_IMMR;
	volatile lbus83xx_t *lbc = &im->lbus;
	struct fcm_nand *fcm;
	unsigned int bank;

	/* Enable FCM detection of timeouts, ECC errors and completion */
	lbc->ltedr &= ~(LTESR_FCT | LTESR_PAR | LTESR_CC);

	fcm = malloc(sizeof(struct fcm_nand));
	if (!fcm) {
		printf("board_nand_init(): " \
		       "Cannot allocate read buffer data structure\n");
		return -1;
	}

	/* Find which chip select bank is being used for this device */
	for (bank = 0; bank < 8; bank++) {
		if ((lbc->bank[bank].br & BR_V) &&
		   ((lbc->bank[bank].br & BR_MSEL) == BR_MS_FCM) &&
		   ((lbc->bank[bank].br & BR_BA) ==
		    (lbc->bank[bank].or & OR_FCM_AM &
		     (unsigned int)(nand->IO_ADDR_R)))) {
			fcm->bank = bank;
/* TODO */		fcm->fmr = FMR_ECCM; /* rest filled in later */
			fcm->fmr = 0; /* rest filled in later */
			fcm->read_bytes = 0;
			fcm->index = 0;
			fcm->pgs = (lbc->bank[bank].or>>OR_FCM_PGS_SHIFT) & 1;
			fcm->base = lbc->bank[bank].br & BR_BA;
			fcm->addr = (unsigned char *) (fcm->base);
			nand->priv = fcm;
			fcm->oobbuf = -1;
			break;
		}
	}

	if (!nand->priv) {
		printf("board_nand_init(): " \
		       "Could not find matching Chip Select\n");
		return -1;
	}

	/* set up nand options */
	nand->options = 0;
	/* set up function call table */
	nand->hwcontrol = fcm_hwcontrol;
	nand->waitfunc = fcm_wait;
	nand->read_byte = fcm_read_byte;
	nand->write_byte = fcm_write_byte;
	nand->read_word = fcm_read_word;
	nand->write_word = fcm_write_word;
	nand->read_buf = fcm_read_buf;
	nand->verify_buf = fcm_verify_buf;
	nand->write_buf = fcm_write_buf;
	nand->cmdfunc = fcm_cmdfunc;
	nand->scan_bbt = fcm_scan_bbt;

	/* If CS Base Register selects full hardware ECC then use it */
	if (((lbc->bank[bank].br & BR_DECC) >> BR_DECC_SHIFT) == 2) {
		/* put in small page settings and adjust later if needed */
		nand->eccmode = NAND_ECC_NONE;
		printf("eccmode == NAND_ECC_NONE\n");
		nand->autooob = (fcm->fmr & FMR_ECCM) ?
				&fcm_oob_sp_eccm1 : &fcm_oob_sp_eccm0;
		if (nand->autooob)
			printf("nand.c --- line 863 nand->autooob: %d\n");
		nand->autooob = &nooobinfo;
		nand->calculate_ecc = fcm_calculate_ecc;
		nand->correct_data = fcm_correct_data;
		nand->enable_hwecc = fcm_enable_hwecc;
	} else {
		/* otherwise fall back to default software ECC */
		nand->eccmode = NAND_ECC_SOFT;
	}
	return 0;
}

#endif
#endif
