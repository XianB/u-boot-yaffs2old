/*
 * Copyright (C) 2007 Freescale Semiconductor, Inc.
 *
 * Author: Tony Li <tony.li@freescale.com>
 * Based on PCI initialization.
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
#include <pci.h>

#include <asm/io.h>
#include <mpc83xx.h>

#ifdef CONFIG_PCIE
#define PCIE_MAX_BUSES 2

DECLARE_GLOBAL_DATA_PTR;

static struct pci_controller pcie_hose[PCIE_MAX_BUSES];
static int pcie_num_buses;

#define cfg_read(val, addr, type, op)	*val = op((type *)(addr))
#define cfg_write(val, addr, type, op)	op((type *)(addr), (val))

#define PCIE_READ_OP(size, type, op)					\
static int								\
pcie_read_config_##size(struct pci_controller *hose,			\
			pci_dev_t dev, int offset, type *val)		\
{									\
	u32 b, d, f;							\
	if (hose->indirect_type == INDIRECT_TYPE_NO_PCIE_LINK) {	\
		*val = (type)0xffffffff;				\
		return -1;						\
	}								\
	b = PCI_BUS(dev); d = PCI_DEV(dev) & 0x1f; f = PCI_FUNC(dev) & 0x7;\
	if (d > 0) {							\
		*val = (type)0xffffffff;				\
		return 0;						\
	}								\
	b = b - hose->first_busno;					\
	dev = (b << 24) | (d << 19) | (f << 16) | (offset & 0xfff);	\
	cfg_read(val, (u32)hose->cfg_addr + (u32)dev, type, op);	\
	return 0;							\
}
#define PCIE_WRITE_OP(size, type, op)					\
static int								\
pcie_write_config_##size(struct pci_controller *hose,			\
			pci_dev_t dev, int offset, type val)		\
{									\
	u32 b, d, f;							\
	if (hose->indirect_type == INDIRECT_TYPE_NO_PCIE_LINK)		\
		return -1;						\
	b = PCI_BUS(dev); d = PCI_DEV(dev) & 0x1f; f = PCI_FUNC(dev) & 0x7;\
	if (d > 0)							\
		return 0;						\
	b = b - hose->first_busno;					\
	dev = (b << 24) | (d << 19) | (f << 16) | (offset & 0xfff);	\
	cfg_write(val, (u32)hose->cfg_addr + (u32)dev, type, op);	\
	return 0;							\
}

PCIE_READ_OP(byte, u8, in_8)
PCIE_READ_OP(word, u16, in_le16)
PCIE_READ_OP(dword, u32, in_le32)
PCIE_WRITE_OP(byte, u8, out_8)
PCIE_WRITE_OP(word, u16, out_le16)
PCIE_WRITE_OP(dword, u32, out_le32)

void pcie_setup_ops(struct pci_controller *hose, u32 cfg_addr)
{
	pci_set_ops(hose,
			pcie_read_config_byte,
			pcie_read_config_word,
			pcie_read_config_dword,
			pcie_write_config_byte,
			pcie_write_config_word,
			pcie_write_config_dword);

	hose->cfg_addr = (unsigned long *)cfg_addr;
}

extern struct pci_controller pci_hose[4];
extern int max_bus;
static int pcie_self_num = 0;

static void pcie_init_bus(int bus, struct pci_region *reg)
{
	volatile immap_t *immr = (volatile immap_t *)CFG_IMMR;
	volatile pex83xx_t *pex = &immr->pciexp[bus];
	volatile struct pex_outbound_window *out_win;
	volatile struct pex_inbound_window *in_win;
	struct pci_controller *hose;
	volatile void *hose_cfg_base;
	unsigned int ram_sz, barl, tar;
	u16 reg16;
	int i, j;

	if (bus == 0)
		pcie_self_num = max_bus;
	hose = &pci_hose[bus + pcie_self_num];

	/* Enable pex csb bridge inbound & outbound transactions */
	out_le32(&pex->bridge.pex_csb_ctrl,
		in_le32(&pex->bridge.pex_csb_ctrl) | PEX_CSB_CTRL_OBPIOE |
			PEX_CSB_CTRL_IBPIOE);

	/* Enable bridge outbound */
	out_le32(&pex->bridge.pex_csb_obctrl, PEX_CSB_OBCTRL_PIOE |
		PEX_CSB_OBCTRL_MEMWE | PEX_CSB_OBCTRL_IOWE |
		PEX_CSB_OBCTRL_CFGWE);

	out_win = &pex->bridge.pex_outbound_win[0];
	if (bus) {
		out_le32(&out_win->ar, PEX_OWAR_EN | PEX_OWAR_TYPE_CFG |
			CFG_PCIE2_CFG_SIZE);
		out_le32(&out_win->bar, CFG_PCIE2_CFG_BASE);
	} else {
		out_le32(&out_win->ar, PEX_OWAR_EN | PEX_OWAR_TYPE_CFG |
			CFG_PCIE1_CFG_SIZE);
		out_le32(&out_win->bar, CFG_PCIE1_CFG_BASE);
	}
	out_le32(&out_win->tarl, 0);
	out_le32(&out_win->tarh, 0);

	for (i = 0; i < 2; i++, reg++) {
		u32 ar;
		if (reg->size == 0)
			break;

		hose->regions[i] = *reg;
		hose->region_count++;

		out_win = &pex->bridge.pex_outbound_win[i + 1];
		out_le32(&out_win->bar, reg->phys_start);
		out_le32(&out_win->tarl, reg->bus_start);
		out_le32(&out_win->tarh, 0);
		ar = PEX_OWAR_EN | (reg->size & PEX_OWAR_SIZE);
		if (reg->flags & PCI_REGION_IO)
			ar |= PEX_OWAR_TYPE_IO;
		else
			ar |= PEX_OWAR_TYPE_MEM;
		out_le32(&out_win->ar, ar);
	}

	out_le32(&pex->bridge.pex_csb_ibctrl, PEX_CSB_IBCTRL_PIOE);

	ram_sz = gd->ram_size;
	barl = 0;
	tar = 0;
	j = 0;
	while (ram_sz > 0) {
		in_win = &pex->bridge.pex_inbound_win[j];
		out_le32(&in_win->barl, barl);
		out_le32(&in_win->barh, 0x0);
		out_le32(&in_win->tar, tar);
		if (ram_sz >= 0x10000000) {
			out_le32(&in_win->ar, PEX_IWAR_EN | PEX_IWAR_NSOV |
				PEX_IWAR_TYPE_PF | 0x0FFFF000);
			barl += 0x10000000;
			tar += 0x10000000;
			ram_sz -= 0x10000000;
		}
		else {
			/* The UM  is not clear here.
			 * So, round up to even Mb boundary */
			ram_sz = ram_sz >> 20 +
					((ram_sz & 0xFFFFF) ? 1 : 0);
			if (!(ram_sz % 2))
				ram_sz -= 1;
			out_le32(&in_win->ar, PEX_IWAR_EN | PEX_IWAR_NSOV |
				PEX_IWAR_TYPE_PF | (ram_sz << 20) | 0xFF000);
			ram_sz = 0;
		}
		j++;
	}
	i = hose->region_count++;
	hose->regions[i].bus_start = 0;
	hose->regions[i].phys_start = 0;
	hose->regions[i].size = gd->ram_size;
	hose->regions[i].flags = PCI_REGION_MEM | PCI_REGION_MEMORY;

	in_win = &pex->bridge.pex_inbound_win[j];
	out_le32(&in_win->barl, CFG_IMMR);
	out_le32(&in_win->barh, 0);
	out_le32(&in_win->tar, CFG_IMMR);
	out_le32(&in_win->ar, PEX_IWAR_EN |
		PEX_IWAR_TYPE_NO_PF | PEX_IWAR_SIZE_1M);

	i = hose->region_count++;
	hose->regions[i].bus_start = CFG_IMMR;
	hose->regions[i].phys_start = CFG_IMMR;
	hose->regions[i].size = 0x100000;
	hose->regions[i].flags = PCI_REGION_MEM | PCI_REGION_MEMORY;

	hose->first_busno = max_bus;
	hose->last_busno = 0xff;

	/* Enable the host virtual INTX interrupts */
	out_le32(&pex->bridge.pex_int_axi_misc_enb,
		in_le32(&pex->bridge.pex_int_axi_misc_enb) | 0x1E0);

	pcie_setup_ops(hose, bus ? CFG_PCIE2_CFG_BASE : CFG_PCIE1_CFG_BASE);

	pci_register_hose(hose);

	/* Hose configure header is memory-mapped */
	hose_cfg_base = (void *)pex;

	get_clocks();
	/* Configure the PCIE controller core clock ratio */
	out_le32(hose_cfg_base + PEX_GCLK_RATIO,
		(((bus ? gd->pciexp2_clk : gd->pciexp1_clk)
				/ 1000000) * 16) / 333);
	udelay(1000000);

	/* Do Type 1 bridge configuration */
	out_8(hose_cfg_base + PCI_PRIMARY_BUS, 0);
	out_8(hose_cfg_base + PCI_SECONDARY_BUS, 1);
	out_8(hose_cfg_base + PCI_SUBORDINATE_BUS, 255);

	/* Write to Command register */
	reg16 = in_le16(hose_cfg_base + PCI_COMMAND);
	reg16 |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO |
			PCI_COMMAND_SERR | PCI_COMMAND_PARITY;
	out_le16(hose_cfg_base + PCI_COMMAND, reg16);

	/* Clear non-reserved bits in status register. */
	out_le16(hose_cfg_base + PCI_STATUS, 0xffff);
	out_8(hose_cfg_base + PCI_LATENCY_TIMER, 0x80);
	out_8(hose_cfg_base + PCI_CACHE_LINE_SIZE, 0x08);

	printf("PCIE%d: ", bus);

	reg16 = in_le16(hose_cfg_base + PEX_LTSSM_STAT);
	if (reg16 < 0x16) {
		printf("No link\n");
		hose->indirect_type = INDIRECT_TYPE_NO_PCIE_LINK;
	} else {
		printf("Link\n");
	}

#ifdef CONFIG_PCI_SCAN_SHOW
	printf("PCI:   Bus Dev VenId DevId Class Int\n");
#endif

	/* Hose scan. */
	hose->last_busno = pci_hose_scan(hose);
	max_bus = hose->last_busno + 1;
}

/*
 * The caller must have already set SCCR, SERDES and the PCIE_LAW BARs
 * must have been set to cover all of the requested regions.
 */
void mpc83xx_pcie_init(int num_buses, struct pci_region **reg, int warmboot)
{
	int i;

	if (num_buses > PCIE_MAX_BUSES) {
		printf("%d PCI buses requsted, %d supported\n",
			num_buses, PCIE_MAX_BUSES);

		num_buses = PCIE_MAX_BUSES;
	}

	pcie_num_buses = num_buses;

	/*
	 * Release PCI RST Output signal.
	 * Power on to RST high must be at least 100 ms as per PCI spec.
	 * On warm boots only 1 ms is required.
	 */
	udelay(warmboot ? 1000 : 100000);

	for (i = 0; i < num_buses; i++)
		pcie_init_bus(i, reg[i]);
}

#endif /* CONFIG_83XX_GENERIC_PCIE */
