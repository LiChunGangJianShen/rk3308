/*
 * Copyright (c) 2018 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */
#include <common.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/grf_rk3308.h>
#include <asm/arch/hardware.h>
#include <asm/arch/rk_atags.h>
#include <asm/gpio.h>
#include <debug_uart.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>
static struct mm_region rk3308_mem_map[] = {
	{
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = 0xff000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		.virt = 0xff000000UL,
		.phys = 0xff000000UL,
		.size = 0x01000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = rk3308_mem_map;
#endif

#define GRF_BASE	0xff000000
#define SGRF_BASE	0xff2b0000

enum {

	GPIO1C7_SHIFT		= 8,
	GPIO1C7_MASK		= GENMASK(11, 8),
	GPIO1C7_GPIO		= 0,
	GPIO1C7_UART1_RTSN,
	GPIO1C7_UART2_TX_M0,
	GPIO1C7_SPI2_MOSI,
	GPIO1C7_JTAG_TMS,

	GPIO1C6_SHIFT		= 4,
	GPIO1C6_MASK		= GENMASK(7, 4),
	GPIO1C6_GPIO		= 0,
	GPIO1C6_UART1_CTSN,
	GPIO1C6_UART2_RX_M0,
	GPIO1C6_SPI2_MISO,
	GPIO1C6_JTAG_TCLK,

	GPIO4D3_SHIFT           = 6,
	GPIO4D3_MASK            = GENMASK(7, 6),
	GPIO4D3_GPIO            = 0,
	GPIO4D3_SDMMC_D3,
	GPIO4D3_UART2_TX_M1,

	GPIO4D2_SHIFT           = 4,
	GPIO4D2_MASK            = GENMASK(5, 4),
	GPIO4D2_GPIO            = 0,
	GPIO4D2_SDMMC_D2,
	GPIO4D2_UART2_RX_M1,

	UART2_IO_SEL_SHIFT	= 2,
	UART2_IO_SEL_MASK	= GENMASK(3, 2),
	UART2_IO_SEL_M0		= 0,
	UART2_IO_SEL_M1,
	UART2_IO_SEL_USB,

	GPIO3B3_SEL_SRC_CTRL_SHIFT	= 7,
	GPIO3B3_SEL_SRC_CTRL_MASK	= BIT(7),
	GPIO3B3_SEL_SRC_CTRL_IOMUX	= 0,
	GPIO3B3_SEL_SRC_CTRL_SEL_PLUS,

	GPIO3B3_SEL_PLUS_SHIFT		= 4,
	GPIO3B3_SEL_PLUS_MASK		= GENMASK(6, 4),
	GPIO3B3_SEL_PLUS_GPIO3_B3	= 0,
	GPIO3B3_SEL_PLUS_FLASH_ALE,
	GPIO3B3_SEL_PLUS_EMMC_PWREN,
	GPIO3B3_SEL_PLUS_SPI1_CLK,
	GPIO3B3_SEL_PLUS_LCDC_D23_M1,

	GPIO3B2_SEL_SRC_CTRL_SHIFT	= 3,
	GPIO3B2_SEL_SRC_CTRL_MASK	= BIT(3),
	GPIO3B2_SEL_SRC_CTRL_IOMUX	= 0,
	GPIO3B2_SEL_SRC_CTRL_SEL_PLUS,

	GPIO3B2_SEL_PLUS_SHIFT		= 0,
	GPIO3B2_SEL_PLUS_MASK		= GENMASK(2, 0),
	GPIO3B2_SEL_PLUS_GPIO3_B2	= 0,
	GPIO3B2_SEL_PLUS_FLASH_RDN,
	GPIO3B2_SEL_PLUS_EMMC_RSTN,
	GPIO3B2_SEL_PLUS_SPI1_MISO,
	GPIO3B2_SEL_PLUS_LCDC_D22_M1,
};

enum {
	IOVSEL3_CTRL_SHIFT	= 8,
	IOVSEL3_CTRL_MASK	= BIT(8),
	VCCIO3_SEL_BY_GPIO	= 0,
	VCCIO3_SEL_BY_IOVSEL3,

	IOVSEL3_SHIFT		= 3,
	IOVSEL3_MASK		= BIT(3),
	VCCIO3_3V3		= 0,
	VCCIO3_1V8,
};

/*
 * The voltage of VCCIO3(which is the voltage domain of emmc/flash/sfc
 * interface) can indicated by GPIO0_A4 or io_vsel3. The SOC defaults
 * use GPIO0_A4 to indicate power supply voltage for VCCIO3 by hardware,
 * then we can switch to io_vsel3 after system power on, and release GPIO0_A4
 * for other usage.
 */

#define GPIO0_A4	4

int rk_board_init(void)
{
	static struct rk3308_grf * const grf = (void *)GRF_BASE;
	u32 val;
	int ret;

	ret = gpio_request(GPIO0_A4, "gpio0_a4");
	if (ret < 0) {
		printf("request for gpio0_a4 failed:%d\n", ret);
		return 0;
	}

	gpio_direction_input(GPIO0_A4);

	if (gpio_get_value(GPIO0_A4))
		val = VCCIO3_SEL_BY_IOVSEL3 << IOVSEL3_CTRL_SHIFT |
		      VCCIO3_1V8 << IOVSEL3_SHIFT;
	else
		val = VCCIO3_SEL_BY_IOVSEL3 << IOVSEL3_CTRL_SHIFT |
		      VCCIO3_3V3 << IOVSEL3_SHIFT;
	rk_clrsetreg(&grf->soc_con0, IOVSEL3_CTRL_MASK | IOVSEL3_MASK, val);

	gpio_free(GPIO0_A4);
	return 0;
}

#ifdef CONFIG_SPL_BUILD
int rk_board_init_f(void)
{
	static struct rk3308_grf * const grf = (void *)GRF_BASE;
	unsigned long mask;
	unsigned long value;

	mask = GPIO3B2_SEL_PLUS_MASK | GPIO3B2_SEL_SRC_CTRL_MASK |
		GPIO3B3_SEL_PLUS_MASK | GPIO3B3_SEL_SRC_CTRL_MASK;
	value = (GPIO3B2_SEL_PLUS_FLASH_RDN << GPIO3B2_SEL_PLUS_SHIFT) |
		(GPIO3B2_SEL_SRC_CTRL_SEL_PLUS << GPIO3B2_SEL_SRC_CTRL_SHIFT) |
		(GPIO3B3_SEL_PLUS_FLASH_ALE << GPIO3B3_SEL_PLUS_SHIFT) |
		(GPIO3B3_SEL_SRC_CTRL_SEL_PLUS << GPIO3B3_SEL_SRC_CTRL_SHIFT);

	if (get_bootdev_by_brom_bootsource() == BOOT_TYPE_NAND) {
		if (soc_is_rk3308b() || soc_is_rk3308bs())
			rk_clrsetreg(&grf->soc_con15, mask, value);
	}

	return 0;
}
#endif

void board_debug_uart_init(void)
{
	static struct rk3308_grf * const grf = (void *)GRF_BASE;

	if (gd && gd->serial.using_pre_serial)
		return;

	/* Enable early UART2 channel m1 on the rk3308 */
	rk_clrsetreg(&grf->soc_con5, UART2_IO_SEL_MASK,
		     UART2_IO_SEL_M1 << UART2_IO_SEL_SHIFT);
	rk_clrsetreg(&grf->gpio4d_iomux,
		     GPIO4D3_MASK | GPIO4D2_MASK,
		     GPIO4D2_UART2_RX_M1 << GPIO4D2_SHIFT |
		     GPIO4D3_UART2_TX_M1 << GPIO4D3_SHIFT);
}

#if defined(CONFIG_SPL_BUILD)
int arch_cpu_init(void)
{
	static struct rk3308_sgrf * const sgrf = (void *)SGRF_BASE;

	/* Set CRYPTO SDMMC EMMC NAND SFC USB master bus to be secure access */
	rk_clrreg(&sgrf->con_secure0, 0x2b83);

	return 0;
}
#endif

static int fdt_fixup_cpu_idle(const void *blob)
{
	int cpu_node, sub_node, len;
	u32 *pp;

	cpu_node = fdt_path_offset(blob, "/cpus");
	if (cpu_node < 0) {
		printf("Failed to get cpus node\n");
		return -EINVAL;
	}

	fdt_for_each_subnode(sub_node, blob, cpu_node) {
		pp = (u32 *)fdt_getprop(blob, sub_node, "cpu-idle-states",
					&len);
		if (!pp)
			continue;
		if (fdt_delprop((void *)blob, sub_node, "cpu-idle-states") < 0)
			printf("Failed to del cpu-idle-states prop\n");
	}

	return 0;
}

static int fdt_fixup_cpu_opp_table(const void *blob)
{
	int opp_node, cpu_node, sub_node;
	int len;
	u32 phandle;
	u32 *pp;

	/* Replace opp table */
	opp_node = fdt_path_offset(blob, "/rk3308bs-cpu0-opp-table");
	if (opp_node < 0) {
		printf("Failed to get rk3308bs-cpu0-opp-table node\n");
		return -EINVAL;
	}

	phandle = fdt_get_phandle(blob, opp_node);
	if (!phandle) {
		printf("Failed to get cpu opp table phandle\n");
		return -EINVAL;
	}

	cpu_node = fdt_path_offset(blob, "/cpus");
	if (cpu_node < 0) {
		printf("Failed to get cpus node\n");
		return -EINVAL;
	}

	fdt_for_each_subnode(sub_node, blob, cpu_node) {
		pp = (u32 *)fdt_getprop(blob, sub_node, "operating-points-v2",
					&len);
		if (!pp)
			continue;
		pp[0] = cpu_to_fdt32(phandle);
	}

	return 0;
}

static int fdt_fixup_dmc_opp_table(const void *blob)
{
	int opp_node, dmc_node;
	int len;
	u32 phandle;
	u32 *pp;

	opp_node = fdt_path_offset(blob, "/rk3308bs-dmc-opp-table");
	if (opp_node < 0) {
		printf("Failed to get rk3308bs-dmc-opp-table node\n");
		return -EINVAL;
	}

	phandle = fdt_get_phandle(blob, opp_node);
	if (!phandle) {
		printf("Failed to get dmc opp table phandle\n");
		return -EINVAL;
	}

	dmc_node = fdt_path_offset(blob, "/dmc");
	if (dmc_node < 0) {
		printf("Failed to get dmc node\n");
		return -EINVAL;
	}

	pp = (u32 *)fdt_getprop(blob, dmc_node, "operating-points-v2", &len);
	if (!pp)
		return 0;
	pp[0] = cpu_to_fdt32(phandle);

	return 0;
}

static void fixup_pcfg_drive_strength(const void *blob, int noffset)
{
	u32 *ds, *dss;
	u32 val;

	dss = (u32 *)fdt_getprop(blob, noffset, "drive-strength-s", NULL);
	if (!dss)
		return;

	val = dss[0];
	ds = (u32 *)fdt_getprop(blob, noffset, "drive-strength", NULL);
	if (ds) {
		ds[0] = val;
	} else {
		if (fdt_setprop((void *)blob, noffset,
				"drive-strength", &val, 4) < 0)
			printf("Failed to add drive-strength prop\n");
	}
}

static int fdt_fixup_pcfg(const void *blob)
{
	int depth1_node;
	int depth2_node;
	int root_node;

	root_node = fdt_path_offset(blob, "/");
	if (root_node < 0)
		return root_node;

	fdt_for_each_subnode(depth1_node, blob, root_node) {
		debug("depth1: %s\n", fdt_get_name(blob, depth2_node, NULL));
		fixup_pcfg_drive_strength(blob, depth1_node);
		fdt_for_each_subnode(depth2_node, blob, depth1_node) {
			debug("    depth2: %s\n",
			      fdt_get_name(blob, depth2_node, NULL));
			fixup_pcfg_drive_strength(blob, depth2_node);
		}
	}

	return 0;
}

static int fdt_fixup_thermal_zones(const void *blob)
{
	int thermal_node;
	int len;
	u32 *pp;
	u32 val;

	thermal_node = fdt_path_offset(blob, "/thermal-zones/soc-thermal");
	if (thermal_node < 0) {
		printf("Failed to get soc thermal node\n");
		return -EINVAL;
	}

	pp = (u32 *)fdt_getprop(blob, thermal_node,
				"rk3308bs-sustainable-power", &len);
	if (pp) {
		val = fdt32_to_cpu(*pp);
		pp = (u32 *)fdt_getprop(blob, thermal_node,
					"sustainable-power", &len);
		if (pp)
			pp[0] = cpu_to_fdt32(val);
	}

	return 0;
}

int rk_board_fdt_fixup(const void *blob)
{
	if (soc_is_rk3308bs()) {
		fdt_increase_size((void *)blob, SZ_8K);
		fdt_fixup_cpu_idle(blob);
		fdt_fixup_cpu_opp_table(blob);
		fdt_fixup_dmc_opp_table(blob);
		fdt_fixup_pcfg(blob);
		fdt_fixup_thermal_zones(blob);
	}

	return 0;
}

int rk_board_early_fdt_fixup(const void *blob)
{
	rk_board_fdt_fixup(blob);

	return 0;
}

