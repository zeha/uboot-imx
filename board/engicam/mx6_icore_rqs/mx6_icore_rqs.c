/*
 * Copyright (C) 2012 Engicam srl
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/mx6.h>
#include <asm/arch/mx6_pins.h>
#include <asm/arch/mx6dl_pins.h>
#include <asm/arch/iomux-v3.h>
#include <asm/errno.h>
#include <miiphy.h>
#if CONFIG_I2C_MXC
#include <i2c.h>
#endif

#ifdef CONFIG_CMD_MMC
#include <mmc.h>
#include <fsl_esdhc.h>
#endif

#ifdef CONFIG_ARCH_MMU
#include <asm/mmu.h>
#include <asm/arch/mmu.h>
#endif

#ifdef CONFIG_CMD_CLOCK
#include <asm/clock.h>
#endif

#ifdef CONFIG_IMX_ECSPI
#include <imx_spi.h>
#endif

#ifdef CONFIG_IMX_UDC
#include <usb/imx_udc.h>
#endif

#ifdef CONFIG_ANDROID_RECOVERY
#include <recovery.h>
#include <part.h>
#include <ext2fs.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <ubi_uboot.h>
#include <jffs2/load_kernel.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

static u32 system_rev;
static enum boot_device boot_dev;

static void set_gpio_output_val(unsigned base, unsigned mask, unsigned val)
{
	unsigned reg = readl(base + GPIO_DR);
	if (val & 1)
		reg |= mask;	/* set high */
	else
		reg &= ~mask;	/* clear low */
	writel(reg, base + GPIO_DR);

	reg = readl(base + GPIO_GDIR);
	reg |= mask;		/* configure GPIO line as output */
	writel(reg, base + GPIO_GDIR);
}

static inline void setup_boot_device(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);
	uint bt_mem_ctl = (soc_sbmr & 0x000000FF) >> 4;
	uint bt_mem_type = (soc_sbmr & 0x00000008) >> 3;

	switch (bt_mem_ctl) {
	case 0x0:
		if (bt_mem_type)
			boot_dev = ONE_NAND_BOOT;
		else
			boot_dev = WEIM_NOR_BOOT;
		break;
	case 0x2:
		boot_dev = SATA_BOOT;
		break;
	case 0x3:
		if (bt_mem_type)
			boot_dev = SPI_NOR_BOOT;
		else
			boot_dev = I2C_BOOT;
		break;
	case 0x4:
	case 0x5:
		boot_dev = SD_BOOT;
		break;
	case 0x6:
	case 0x7:
		boot_dev = MMC_BOOT;
		break;
	case 0x8 ... 0xf:
		boot_dev = NAND_BOOT;
		break;
	default:
		boot_dev = UNKNOWN_BOOT;
		break;
	}
}

enum boot_device get_boot_device(void)
{
	return boot_dev;
}

u32 get_board_rev(void)
{
	return fsl_system_rev;
}

#ifdef CONFIG_ARCH_MMU
void board_mmu_init(void)
{
	unsigned long ttb_base = PHYS_SDRAM_1 + 0x4000;
	unsigned long i;

	/*
	 * Set the TTB register
	 */
	asm volatile ("mcr  p15,0,%0,c2,c0,0" : : "r" (ttb_base) /*: */);

	/*
	 * Set the Domain Access Control Register
	 */
	i = ARM_ACCESS_DACR_DEFAULT;
	asm volatile ("mcr  p15,0,%0,c3,c0,0" : : "r" (i) /*: */);

	/*
	 * First clear all TT entries - ie Set them to Faulting
	 */
	memset((void *)ttb_base, 0, ARM_FIRST_LEVEL_PAGE_TABLE_SIZE);
	/* Actual   Virtual  Size   Attributes          Function */
	/* Base     Base     MB     cached? buffered?  access permissions */
	/* xxx00000 xxx00000 */
	X_ARM_MMU_SECTION(0x000, 0x000, 0x001, ARM_UNCACHEABLE, ARM_UNBUFFERABLE, ARM_ACCESS_PERM_RW_RW);	/* ROM, 1M */
	X_ARM_MMU_SECTION(0x001, 0x001, 0x008, ARM_UNCACHEABLE, ARM_UNBUFFERABLE, ARM_ACCESS_PERM_RW_RW);	/* 8M */
	X_ARM_MMU_SECTION(0x009, 0x009, 0x001, ARM_UNCACHEABLE, ARM_UNBUFFERABLE, ARM_ACCESS_PERM_RW_RW);	/* IRAM */
	X_ARM_MMU_SECTION(0x00A, 0x00A, 0x0F6, ARM_UNCACHEABLE, ARM_UNBUFFERABLE, ARM_ACCESS_PERM_RW_RW);	/* 246M */
	/* 2 GB memory starting at 0x10000000, only map 1.875 GB */
	X_ARM_MMU_SECTION(0x100, 0x100, 0x780,
			  ARM_CACHEABLE, ARM_BUFFERABLE, ARM_ACCESS_PERM_RW_RW);
	/* uncached alias of the same 1.875 GB memory */
	X_ARM_MMU_SECTION(0x100, 0x880, 0x780,
			  ARM_UNCACHEABLE, ARM_UNBUFFERABLE,
			  ARM_ACCESS_PERM_RW_RW);

	/* Enable MMU */
	MMU_ON();
}
#endif

int dram_init(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
	return 0;
}


static void setup_uart(void)
{
#if defined CONFIG_MX6Q
	/* UART1 TXD */
//MP:TBD	mxc_iomux_v3_setup_pad(MX6Q_PAD_SD3_DAT6__UART1_TXD);

	/* UART1 RXD */
//MP:TBD	mxc_iomux_v3_setup_pad(MX6Q_PAD_SD3_DAT7__UART1_RXD);

	/* UART2 TXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D26__UART2_TXD);

	/* UART2 RXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D27__UART2_RXD);

	/* UART4 TXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL0__UART4_TXD);

	/* UART4 RXD */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW0__UART4_RXD);
#elif defined CONFIG_MX6DL
	/* UART1 TXD */
//MP:TBD	mxc_iomux_v3_setup_pad(MX6DL_PAD_SD3_DAT6__UART1_TXD);

	/* UART1 RXD */
//MP:TBD	mxc_iomux_v3_setup_pad(MX6DL_PAD_SD3_DAT7__UART1_RXD);

	/* UART2 TXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D26__UART2_TXD);

	/* UART2 RXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D27__UART2_RXD);

	/* UART4 TXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL0__UART4_TXD);

	/* UART4 RXD */
	mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW0__UART4_RXD);
#endif
}


#ifdef CONFIG_I2C_MXC
static void setup_i2c(unsigned int module_base)
{
	unsigned int reg;

	switch (module_base) {
	case I2C1_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D28__I2C1_SDA);
		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D21__I2C1_SCL);
#elif defined CONFIG_MX6DL
		/* i2c1 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D28__I2C1_SDA);
		/* i2c1 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D21__I2C1_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC0;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C2_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_ROW3__I2C2_SDA);

		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_KEY_COL3__I2C2_SCL);
#elif defined CONFIG_MX6DL
		/* i2c2 SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_ROW3__I2C2_SDA);

		/* i2c2 SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_KEY_COL3__I2C2_SCL);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0x300;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	case I2C3_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* GPIO_5 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_5__I2C3_SCL);

		/* GPIO_16 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D18__I2C3_SDA);
#elif defined CONFIG_MX6DL
		/* GPIO_5 for I2C3_SCL */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_5__I2C3_SCL);

		/* GPIO_16 for I2C3_SDA */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D18__I2C3_SDA);
#endif

		/* Enable i2c clock */
		reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR2);
		reg |= 0xC00;
		writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR2);

		break;
	default:
		printf("Invalid I2C base: 0x%x\n", module_base);
		break;
	}
}

#endif


#ifdef CONFIG_IMX_ECSPI
s32 spi_get_cfg(struct imx_spi_dev_t *dev)
{
	switch (dev->slave.cs) {
	case 0:
		/* PMIC */
		dev->base = ECSPI1_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_HIGH;
		dev->ss = 0;
		dev->fifo_sz = 64 * 4;
		dev->us_delay = 0;
		break;
	case 1:
		/* SPI-NOR */
		dev->base = ECSPI1_BASE_ADDR;
		dev->freq = 25000000;
		dev->ss_pol = IMX_SPI_ACTIVE_LOW;
		dev->ss = 1;
		dev->fifo_sz = 64 * 4;
		dev->us_delay = 0;
		break;
	default:
		printf("Invalid Bus ID!\n");
	}

	return 0;
}



void spi_io_init(struct imx_spi_dev_t *dev)
{
	switch (dev->base) {
	case ECSPI1_BASE_ADDR:
#if defined CONFIG_MX6Q
		/* SCLK */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D16__ECSPI1_SCLK);

		/* MISO */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D17__ECSPI1_MISO);

		/* MOSI */
		mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D18__ECSPI1_MOSI);

		if (dev->ss == 1)
			mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D19__ECSPI1_SS1);
#elif defined CONFIG_MX6DL
		/* SCLK */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D16__ECSPI1_SCLK);

		/* MISO */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D17__ECSPI1_MISO);

		/* MOSI */
		mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D18__ECSPI1_MOSI);

		if (dev->ss == 1)
			mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D19__ECSPI1_SS1);
#endif
		break;
	case ECSPI2_BASE_ADDR:
	case ECSPI3_BASE_ADDR:
		/* ecspi2-3 fall through */
		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_NAND_GPMI
#if defined CONFIG_MX6Q
iomux_v3_cfg_t nfc_pads[] = {
	MX6Q_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6Q_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6Q_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6Q_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6Q_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6Q_PAD_NANDF_CS1__RAWNAND_CE1N,
	MX6Q_PAD_NANDF_CS2__RAWNAND_CE2N,
	MX6Q_PAD_NANDF_CS3__RAWNAND_CE3N,
	MX6Q_PAD_SD4_CMD__RAWNAND_RDN,
	MX6Q_PAD_SD4_CLK__RAWNAND_WRN,
	MX6Q_PAD_NANDF_D0__RAWNAND_D0,
	MX6Q_PAD_NANDF_D1__RAWNAND_D1,
	MX6Q_PAD_NANDF_D2__RAWNAND_D2,
	MX6Q_PAD_NANDF_D3__RAWNAND_D3,
	MX6Q_PAD_NANDF_D4__RAWNAND_D4,
	MX6Q_PAD_NANDF_D5__RAWNAND_D5,
	MX6Q_PAD_NANDF_D6__RAWNAND_D6,
	MX6Q_PAD_NANDF_D7__RAWNAND_D7,
	MX6Q_PAD_SD4_DAT0__RAWNAND_DQS,
};
#elif defined CONFIG_MX6DL
iomux_v3_cfg_t nfc_pads[] = {
	MX6DL_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6DL_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6DL_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6DL_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6DL_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6DL_PAD_NANDF_CS1__RAWNAND_CE1N,
	MX6DL_PAD_NANDF_CS2__RAWNAND_CE2N,
	MX6DL_PAD_NANDF_CS3__RAWNAND_CE3N,
	MX6DL_PAD_SD4_CMD__RAWNAND_RDN,
	MX6DL_PAD_SD4_CLK__RAWNAND_WRN,
	MX6DL_PAD_NANDF_D0__RAWNAND_D0,
	MX6DL_PAD_NANDF_D1__RAWNAND_D1,
	MX6DL_PAD_NANDF_D2__RAWNAND_D2,
	MX6DL_PAD_NANDF_D3__RAWNAND_D3,
	MX6DL_PAD_NANDF_D4__RAWNAND_D4,
	MX6DL_PAD_NANDF_D5__RAWNAND_D5,
	MX6DL_PAD_NANDF_D6__RAWNAND_D6,
	MX6DL_PAD_NANDF_D7__RAWNAND_D7,
	MX6DL_PAD_SD4_DAT0__RAWNAND_DQS,
};
#endif

int setup_gpmi_nand(void)
{
	unsigned int reg;

	/* config gpmi nand iomux */
	mxc_iomux_v3_setup_multiple_pads(nfc_pads,
			ARRAY_SIZE(nfc_pads));

	/* config gpmi and bch clock to 20Mhz, from pll2 400M pfd*/
	reg = readl(CCM_BASE_ADDR + CLKCTL_CS2CDR);
	reg &= 0xF800FFFF;
	reg |= 0x02630000;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CS2CDR);

	/* enable gpmi and bch clock gating */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR4);
	reg |= 0xFF003000;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR4);

	/* enable apbh clock gating */
	reg = readl(CCM_BASE_ADDR + CLKCTL_CCGR0);
	reg |= 0x0030;
	writel(reg, CCM_BASE_ADDR + CLKCTL_CCGR0);

}
#endif

#ifdef CONFIG_NET_MULTI
int board_eth_init(bd_t *bis)
{
	int rc = -ENODEV;

	return rc;
}
#endif

#ifdef CONFIG_CMD_MMC

struct fsl_esdhc_cfg usdhc_cfg[3] = {
       {USDHC3_BASE_ADDR, 1},
       {USDHC4_BASE_ADDR, 1},
       {USDHC1_BASE_ADDR, 1},
};

#ifdef CONFIG_DYNAMIC_MMC_DEVNO
int get_mmc_env_devno(void)
{
	uint soc_sbmr = readl(SRC_BASE_ADDR + 0x4);

	/* BOOT_CFG2[3] and BOOT_CFG2[4] */
	return (soc_sbmr & 0x00001800) >> 11;
}
#endif

iomux_v3_cfg_t mx6_usdhc1_pads[] = {
#if defined CONFIG_MX6Q
	MX6Q_PAD_SD1_CLK__USDHC1_CLK,
	MX6Q_PAD_SD1_CMD__USDHC1_CMD,
	MX6Q_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6Q_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6Q_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6Q_PAD_SD1_DAT3__USDHC1_DAT3,
       MX6Q_PAD_GPIO_1__GPIO_1_1, 	/* CD */
#elif defined CONFIG_MX6DL
	MX6DL_PAD_SD1_CLK__USDHC1_CLK,
	MX6DL_PAD_SD1_CMD__USDHC1_CMD,
	MX6DL_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6DL_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6DL_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6DL_PAD_SD1_DAT3__USDHC1_DAT3,
       MX6DL_PAD_GPIO_1__GPIO_1_1, 	/* CD */
#endif
};


iomux_v3_cfg_t mx6_usdhc2_pads[] = {
#if defined CONFIG_MX6Q
	MX6Q_PAD_SD2_CLK__USDHC2_CLK,
	MX6Q_PAD_SD2_CMD__USDHC2_CMD,
	MX6Q_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6Q_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6Q_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6Q_PAD_SD2_DAT3__USDHC2_DAT3,
#elif defined CONFIG_MX6DL
	MX6DL_PAD_SD2_CLK__USDHC2_CLK,
	MX6DL_PAD_SD2_CMD__USDHC2_CMD,
	MX6DL_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6DL_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6DL_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6DL_PAD_SD2_DAT3__USDHC2_DAT3,
#endif
};

iomux_v3_cfg_t mx6_usdhc3_pads[] = {
#if defined CONFIG_MX6Q
	MX6Q_PAD_SD3_CLK__USDHC3_CLK,
	MX6Q_PAD_SD3_CMD__USDHC3_CMD,
	MX6Q_PAD_SD3_DAT0__USDHC3_DAT0,
	MX6Q_PAD_SD3_DAT1__USDHC3_DAT1,
	MX6Q_PAD_SD3_DAT2__USDHC3_DAT2,
	MX6Q_PAD_SD3_DAT3__USDHC3_DAT3,
	MX6Q_PAD_SD3_DAT4__USDHC3_DAT4,
	MX6Q_PAD_SD3_DAT5__USDHC3_DAT5,
	MX6Q_PAD_SD3_DAT6__USDHC3_DAT6,
	MX6Q_PAD_SD3_DAT7__USDHC3_DAT7,
#elif defined CONFIG_MX6DL
	MX6DL_PAD_SD3_CLK__USDHC3_CLK,
	MX6DL_PAD_SD3_CMD__USDHC3_CMD,
	MX6DL_PAD_SD3_DAT0__USDHC3_DAT0,
	MX6DL_PAD_SD3_DAT1__USDHC3_DAT1,
	MX6DL_PAD_SD3_DAT2__USDHC3_DAT2,
	MX6DL_PAD_SD3_DAT3__USDHC3_DAT3,
	MX6DL_PAD_SD3_DAT4__USDHC3_DAT4,
	MX6DL_PAD_SD3_DAT5__USDHC3_DAT5,
	MX6DL_PAD_SD3_DAT6__USDHC3_DAT6,
	MX6DL_PAD_SD3_DAT7__USDHC3_DAT7,
#endif
};

iomux_v3_cfg_t mx6_usdhc4_pads[] = {
#if defined CONFIG_MX6Q
	MX6Q_PAD_SD4_CLK__USDHC4_CLK,
	MX6Q_PAD_SD4_CMD__USDHC4_CMD,
	MX6Q_PAD_SD4_DAT0__USDHC4_DAT0,
	MX6Q_PAD_SD4_DAT1__USDHC4_DAT1,
	MX6Q_PAD_SD4_DAT2__USDHC4_DAT2,
	MX6Q_PAD_SD4_DAT3__USDHC4_DAT3,
	MX6Q_PAD_SD4_DAT4__USDHC4_DAT4,
	MX6Q_PAD_SD4_DAT5__USDHC4_DAT5,
	MX6Q_PAD_SD4_DAT6__USDHC4_DAT6,
	MX6Q_PAD_SD4_DAT7__USDHC4_DAT7,
	MX6Q_PAD_NANDF_ALE__USDHC4_RST,
#elif defined CONFIG_MX6DL
	MX6DL_PAD_SD4_CLK__USDHC4_CLK,
	MX6DL_PAD_SD4_CMD__USDHC4_CMD,
	MX6DL_PAD_SD4_DAT0__USDHC4_DAT0,
	MX6DL_PAD_SD4_DAT1__USDHC4_DAT1,
	MX6DL_PAD_SD4_DAT2__USDHC4_DAT2,
	MX6DL_PAD_SD4_DAT3__USDHC4_DAT3,
	MX6DL_PAD_SD4_DAT4__USDHC4_DAT4,
	MX6DL_PAD_SD4_DAT5__USDHC4_DAT5,
	MX6DL_PAD_SD4_DAT6__USDHC4_DAT6,
	MX6DL_PAD_SD4_DAT7__USDHC4_DAT7,
	MX6DL_PAD_NANDF_ALE__USDHC4_RST,
#endif
};

int usdhc_gpio_init(bd_t *bis)
{
	s32 status = 0;
	u32 index = 0;

	for (index = 0; index < CONFIG_SYS_FSL_USDHC_NUM; index++) {
		switch (index) {
		case 0:
			mxc_iomux_v3_setup_multiple_pads(mx6_usdhc3_pads,
							 sizeof
							 (mx6_usdhc3_pads) /
							 sizeof(mx6_usdhc3_pads
								[0]));
			break;
		case 1:
			/* TBD */
			mxc_iomux_v3_setup_multiple_pads(mx6_usdhc4_pads,
							 sizeof
							 (mx6_usdhc4_pads) /
							 sizeof(mx6_usdhc4_pads
								[0]));
			break;
		case 2:
			/* TBD */
			mxc_iomux_v3_setup_multiple_pads(mx6_usdhc1_pads,
							 sizeof
							 (mx6_usdhc1_pads) /
							 sizeof(mx6_usdhc1_pads
								[0]));
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
			       "(%d) then supported by the board (%d)\n",
			       index + 1, CONFIG_SYS_FSL_USDHC_NUM);
			return status;
		}
		printf("inizializing USDHC %d CONFIG_SYS_FSL_USDHC_NUM %d \n",index,CONFIG_SYS_FSL_USDHC_NUM);
		status |= fsl_esdhc_initialize(bis, &usdhc_cfg[index]);
	}

	return status;
}

int board_mmc_init(bd_t *bis)
{
	if (!usdhc_gpio_init(bis))
		return 0;
	else
		return -1;
}

/* For DDR mode operation, provide target delay parameter for each SD port.
 * Use cfg->esdhc_base to distinguish the SD port #. The delay for each port
 * is dependent on signal layout for that particular port.  If the following
 * CONFIG is not defined, then the default target delay value will be used.
 */
#ifdef CONFIG_GET_DDR_TARGET_DELAY
u32 get_ddr_delay(struct fsl_esdhc_cfg *cfg)
{
	/* No delay required  */
	return 0;
}
#endif

#endif

int board_init(void)
{
#ifdef CONFIG_MFG
/* MFG firmware need reset usb to avoid host crash firstly */
#define USBCMD 0x140
	int val = readl(OTG_BASE_ADDR + USBCMD);
	val &= ~0x1;		/*RS bit */
	writel(val, OTG_BASE_ADDR + USBCMD);
#endif
	mxc_iomux_v3_init((void *)IOMUXC_BASE_ADDR);
	setup_boot_device();
	fsl_set_system_rev();

	/* board id for linux */
	gd->bd->bi_arch_number = MACH_TYPE_MX6Q_SABRELITE;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	setup_uart();

#ifdef CONFIG_I2C_MXC
	setup_i2c(CONFIG_SYS_I2C_PORT);
#endif

#ifdef CONFIG_NAND_GPMI
	setup_gpmi_nand();
#endif

	return 0;
}

#ifdef CONFIG_ANDROID_RECOVERY

int check_recovery_cmd_file(void)
{
	int button_pressed = 0;
	int recovery_mode = 0;
	u32 reg;

	recovery_mode = check_and_clean_recovery_flag();

	/* Check Recovery Combo Button press or not. */
#if defined CONFIG_MX6Q
	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_19__GPIO_4_5);
#elif defined CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_19__GPIO_4_5);
#endif
	reg = readl(GPIO4_BASE_ADDR + GPIO_GDIR);
	reg &= ~(1<<5);
	writel(reg, GPIO4_BASE_ADDR + GPIO_GDIR);
	reg = readl(GPIO4_BASE_ADDR + GPIO_PSR);
	if (!(reg & (1 << 5))) { /* VOL_DN key is low assert */
		button_pressed = 1;
		printf("Recovery key pressed\n");
	}

	return recovery_mode || button_pressed;
}
#endif

int board_late_init(void)
{
	return 0;
}


iomux_v3_cfg_t enet_pads[] = {
#if defined CONFIG_MX6Q
	MX6Q_PAD_ENET_MDIO__ENET_MDIO,
	MX6Q_PAD_ENET_MDC__ENET_MDC,
	MX6Q_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6Q_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6Q_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6Q_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6Q_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6Q_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	/* pin 35 - 1 (PHY_AD2) on reset */
	MX6Q_PAD_RGMII_RXC__GPIO_6_30,
	/* pin 32 - 1 - (MODE0) all */
	MX6Q_PAD_RGMII_RD0__GPIO_6_25,
	/* pin 31 - 1 - (MODE1) all */
	MX6Q_PAD_RGMII_RD1__GPIO_6_27,
	/* pin 28 - 1 - (MODE2) all */
	MX6Q_PAD_RGMII_RD2__GPIO_6_28,
	/* pin 27 - 1 - (MODE3) all */
	MX6Q_PAD_RGMII_RD3__GPIO_6_29,
	/* pin 33 - 1 - (CLK125_EN) 125Mhz clockout enabled */
	MX6Q_PAD_RGMII_RX_CTL__GPIO_6_24,
	MX6Q_PAD_GPIO_0__CCM_CLKO,
	MX6Q_PAD_GPIO_3__CCM_CLKO2,
	MX6Q_PAD_ENET_REF_CLK__ENET_TX_CLK,
#elif defined CONFIG_MX6DL
	MX6DL_PAD_ENET_MDIO__ENET_MDIO,
	MX6DL_PAD_ENET_MDC__ENET_MDC,
	MX6DL_PAD_RGMII_TXC__ENET_RGMII_TXC,
	MX6DL_PAD_RGMII_TD0__ENET_RGMII_TD0,
	MX6DL_PAD_RGMII_TD1__ENET_RGMII_TD1,
	MX6DL_PAD_RGMII_TD2__ENET_RGMII_TD2,
	MX6DL_PAD_RGMII_TD3__ENET_RGMII_TD3,
	MX6DL_PAD_RGMII_TX_CTL__ENET_RGMII_TX_CTL,
	/* pin 35 - 1 (PHY_AD2) on reset */
	MX6DL_PAD_RGMII_RXC__GPIO_6_30,
	/* pin 32 - 1 - (MODE0) all */
	MX6DL_PAD_RGMII_RD0__GPIO_6_25,
	/* pin 31 - 1 - (MODE1) all */
	MX6DL_PAD_RGMII_RD1__GPIO_6_27,
	/* pin 28 - 1 - (MODE2) all */
	MX6DL_PAD_RGMII_RD2__GPIO_6_28,
	/* pin 27 - 1 - (MODE3) all */
	MX6DL_PAD_RGMII_RD3__GPIO_6_29,
	/* pin 33 - 1 - (CLK125_EN) 125Mhz clockout enabled */
	MX6DL_PAD_RGMII_RX_CTL__GPIO_6_24,
	MX6DL_PAD_GPIO_0__CCM_CLKO,
	MX6DL_PAD_GPIO_3__CCM_CLKO2,
	MX6DL_PAD_ENET_REF_CLK__ENET_TX_CLK,
#endif
};

iomux_v3_cfg_t enet_pads_final[] = {
#if defined CONFIG_MX6Q
	MX6Q_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6Q_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6Q_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6Q_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6Q_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6Q_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
#elif defined CONFIG_MX6DL
	MX6DL_PAD_RGMII_RXC__ENET_RGMII_RXC,
	MX6DL_PAD_RGMII_RD0__ENET_RGMII_RD0,
	MX6DL_PAD_RGMII_RD1__ENET_RGMII_RD1,
	MX6DL_PAD_RGMII_RD2__ENET_RGMII_RD2,
	MX6DL_PAD_RGMII_RD3__ENET_RGMII_RD3,
	MX6DL_PAD_RGMII_RX_CTL__ENET_RGMII_RX_CTL,
#endif
};


#if 1 //def DEBUG
static int phy_read(char *devname, unsigned char addr, unsigned char reg,
		    unsigned short *pdata)
{
	int ret = miiphy_read(devname, addr, reg, pdata);
	if (ret)
		printf("Error reading from %s PHY addr=%02x reg=%02x\n",
		       devname, addr, reg);
	return ret;
}
#endif

static int phy_write(char *devname, unsigned char addr, unsigned char reg,
		     unsigned short value)
{
	int ret = miiphy_write(devname, addr, reg, value);
	if (ret)
		printf("Error writing to %s PHY addr=%02x reg=%02x\n", devname,
		       addr, reg);

	return ret;
}

int mx6_rgmii_rework(char *devname, int phy_addr)
{
 unsigned short app;
#if 0
	/* To advertise only 10 Mbs */
	phy_write(devname, phy_addr, 0x4, 0x61);
	phy_write(devname, phy_addr, 0x9, 0x0c00);
#endif

	/* enable master mode, force phy to 100Mbps */
	phy_write(devname, phy_addr, 0x9, 0x1c00);
#if 0
	/* min rx data delay */
	phy_write(devname, phy_addr, 0x0b, 0x8105);
	phy_write(devname, phy_addr, 0x0c, 0x0000);

	/* max rx/tx clock delay, min rx/tx control delay */
	phy_write(devname, phy_addr, 0x0b, 0x8104);
	phy_write(devname, phy_addr, 0x0c, 0xf0f0);
	phy_write(devname, phy_addr, 0x0b, 0x104);
#endif
#if defined CONFIG_MX6Q    
       //write register 6 addr 2 TXD[0:3] skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0006);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x3333);

       //write register 5 addr 2 RXD[0:3] skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0005);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x7777);

       //write register 4 addr 2 RX_DV TX_EN skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0004);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x0037);

       //write register 8 addr 2 RX_DV TX_EN skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0008);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x01E7);
#elif defined CONFIG_MX6DL
      //write register 6 addr 2 TXD[0:3] skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0006);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x1111);

       //write register 5 addr 2 RXD[0:3] skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0005);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x2222);

       //write register 4 addr 2 RX_DV TX_EN skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0004);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x0037);

       //write register 8 addr 2 RX_DV TX_EN skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0008);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_write(devname, phy_addr, 0x0e, 0x01E7);

#endif
#if 0 
        //read register 6 addr 2 TXD[0:3] skew
	phy_write(devname, phy_addr, 0x0d, 0x0002);
	phy_write(devname, phy_addr, 0x0e, 0x0008);
	phy_write(devname, phy_addr, 0x0d, 0x4002);
	phy_read(devname, phy_addr, 0x0e, &app);
        
        printf("MP: read register 2:6 %02x\n",app);
#endif



	return 0;
}



void enet_board_init(void)
{
#if defined CONFIG_MX6Q
	iomux_v3_cfg_t enet_reset =
	    (MX6Q_PAD_ENET_RX_ER__GPIO_1_24 &
	     ~MUX_PAD_CTRL_MASK) | MUX_PAD_CTRL(0x48);
#elif defined CONFIG_MX6DL
	iomux_v3_cfg_t enet_reset =
	    (MX6DL_PAD_ENET_RX_ER__GPIO_1_24 &
	     ~MUX_PAD_CTRL_MASK) | MUX_PAD_CTRL(0x48);
#endif

	/* phy reset: gpio1-24 */
	set_gpio_output_val(GPIO1_BASE_ADDR, (1 << 24), 0);
	set_gpio_output_val(GPIO6_BASE_ADDR, (1 << 30),
			    (CONFIG_FEC0_PHY_ADDR >> 2));
	set_gpio_output_val(GPIO6_BASE_ADDR, (1 << 25), 1);
	set_gpio_output_val(GPIO6_BASE_ADDR, (1 << 27), 1);
	set_gpio_output_val(GPIO6_BASE_ADDR, (1 << 28), 1);
	set_gpio_output_val(GPIO6_BASE_ADDR, (1 << 29), 1);
	mxc_iomux_v3_setup_multiple_pads(enet_pads, ARRAY_SIZE(enet_pads));
	mxc_iomux_v3_setup_pad(enet_reset);
	set_gpio_output_val(GPIO6_BASE_ADDR, (1 << 24), 1);
	printf("ENET board init!!\n");
	udelay(500);
	set_gpio_output_val(GPIO1_BASE_ADDR, (1 << 24), 1);
	mxc_iomux_v3_setup_multiple_pads(enet_pads_final,
					 ARRAY_SIZE(enet_pads_final));
}



int checkboard(void)
{
#if defined CONFIG_ICORE_QUAD
	printf("Board: MX6Q-RQS-i.Core:[ ");
#elif defined CONFIG_ICORE_DUAL
	printf("Board: MX6D-RQS-i.Core:[ ");
#elif defined CONFIG_ICORE_DUALLIGHT
	printf("Board: MX6DL-RQS-i.Core:[ ");
#elif defined CONFIG_ICORE_SOLO
	printf("Board: MX6S-RQS-i.Core:[ ");
#endif
	switch (__REG(SRC_BASE_ADDR + 0x8)) {
	case 0x0001:
		printf("POR");
		break;
	case 0x0009:
		printf("RST");
		break;
	case 0x0010:
	case 0x0011:
		printf("WDOG");
		break;
	default:
		printf("unknown");
	}
	printf("]\n");

	printf("Boot Device: ");
	switch (get_boot_device()) {
	case WEIM_NOR_BOOT:
		printf("NOR\n");
		break;
	case ONE_NAND_BOOT:
		printf("ONE NAND\n");
		break;
	case PATA_BOOT:
		printf("PATA\n");
		break;
	case SATA_BOOT:
		printf("SATA\n");
		break;
	case I2C_BOOT:
		printf("I2C\n");
		break;
	case SPI_NOR_BOOT:
		printf("SPI NOR\n");
		break;
	case SD_BOOT:
		printf("SD\n");
		break;
	case MMC_BOOT:
		printf("MMC\n");
		break;
	case NAND_BOOT:
		printf("NAND\n");
		break;
	case UNKNOWN_BOOT:
	default:
		printf("UNKNOWN\n");
		break;
	}
	return 0;
}

#ifdef CONFIG_IMX_UDC

void udc_pins_setting(void)
{
#define GPIO_3_22_BIT_MASK (1<<22)
	u32 reg;
#if defined CONFIG_MX6Q
	mxc_iomux_v3_setup_pad(MX6Q_PAD_GPIO_1__USBOTG_ID);
	/* USB_OTG_PWR */
	mxc_iomux_v3_setup_pad(MX6Q_PAD_EIM_D22__GPIO_3_22);
#elif defined CONFIG_MX6DL
	mxc_iomux_v3_setup_pad(MX6DL_PAD_GPIO_1__USBOTG_ID);

	mxc_iomux_v3_setup_pad(MX6DL_PAD_EIM_D22__GPIO_3_22);
#endif
	reg = readl(GPIO3_BASE_ADDR + GPIO_GDIR);
	/* set gpio_3_22 as output */
	reg |= GPIO_3_22_BIT_MASK;
	writel(reg, GPIO3_BASE_ADDR + GPIO_GDIR);

	/* set USB_OTG_PWR to 0 */
	reg = readl(GPIO3_BASE_ADDR + GPIO_DR);
	reg &= ~GPIO_3_22_BIT_MASK;
	writel(reg, GPIO3_BASE_ADDR + GPIO_DR);

	mxc_iomux_set_gpr_register(1, 13, 1, 1);
}
#endif
