/*
 * Copyright (C) 2007 Freescale Semiconductor, Inc.
 * Tony Li <tony.li@freescale.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include <asm/mmu.h>
#include <asm/io.h>
#include <common.h>
#include <mpc83xx.h>
#include <pci.h>
#include <asm/fsl_serdes.h>

#if defined(CONFIG_PCIE)
static struct pci_region pci_regions_0[] = {
{
bus_start: CFG_PCIE1_MEM_BASE,
phys_start: CFG_PCIE1_MEM_PHYS,
size: CFG_PCIE1_MEM_SIZE,
flags: PCI_REGION_MEM
},
{
bus_start: CFG_PCIE1_IO_BASE,
phys_start: CFG_PCIE1_IO_PHYS,
size: CFG_PCIE1_IO_SIZE,
flags: PCI_REGION_IO
}
};

static struct pci_region pci_regions_1[] = {
{
bus_start: CFG_PCIE2_MEM_BASE,
phys_start: CFG_PCIE2_MEM_PHYS,
size: CFG_PCIE2_MEM_SIZE,
flags: PCI_REGION_MEM
},
{
bus_start: CFG_PCIE2_IO_BASE,
phys_start: CFG_PCIE2_IO_PHYS,
size: CFG_PCIE2_IO_SIZE,
flags: PCI_REGION_IO
}
};

void pcie_init_board(void)
{
	volatile immap_t *immr = (volatile immap_t *)CFG_IMMR;
	volatile clk83xx_t *clk = (volatile clk83xx_t *)&immr->clk;
	volatile sysconf83xx_t *sysconf = &immr->sysconf;
	volatile law83xx_t *pcie_law = sysconf->pcielaw;
	struct pci_region *reg[] = { pci_regions_0, pci_regions_1 };

	disable_addr_trans();

	/* Configure the clock for PCIE controller */
	clk->sccr &= ~0x003C0000;
	clk->sccr |= 0x00140000;

	/* Deassert the resets in the control register */
	sysconf->pecr1 = 0xE0008000;
#if !defined(CONFIG_PCIE_X2)
	sysconf->pecr2 = 0xE0008000;
#endif
	udelay(2000);

	/* Configure PCI Express Local Access Windows */
	pcie_law[0].bar = CFG_PCIE1_BASE & LAWBAR_BAR;
	pcie_law[0].ar = LBLAWAR_EN | LBLAWAR_512MB;

	pcie_law[1].bar = CFG_PCIE2_BASE & LAWBAR_BAR;
	pcie_law[1].ar = LBLAWAR_EN | LBLAWAR_512MB;

#if defined(CONFIG_PCIE_X2)
	mpc83xx_pcie_init(1, reg, 0);
#else
	mpc83xx_pcie_init(2, reg, 0);
#endif
}
#endif /* CONFIG_PCIE */
