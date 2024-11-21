/*
 * (C) Copyright 2017 Rockchip Electronics Co., Ltd.
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <amp.h>
#include <bidram.h>
#include <boot_rkimg.h>
#include <cli.h>
#include <clk.h>
#include <console.h>
#include <debug_uart.h>
#include <dm.h>
#include <dvfs.h>
#include <io-domain.h>
#include <key.h>
#include <memblk.h>
#include <misc.h>
#include <of_live.h>
#include <ram.h>
#include <rockchip_debugger.h>
#include <syscon.h>
#include <sysmem.h>
#include <video_rockchip.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dm/uclass-internal.h>
#include <dm/root.h>
#include <power/charge_display.h>
#include <power/regulator.h>
#include <asm/arch/boot_mode.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hotkey.h>
#include <asm/arch/param.h>
#include <asm/arch/periph.h>
#include <asm/arch/resource_img.h>
#include <asm/arch/rk_atags.h>
#include <asm/arch/vendor.h>
#ifdef CONFIG_DM_I2C_COMPAT
#include <i2c.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

__weak int rk_board_late_init(void)
{
	return 0;
}

__weak int rk_board_fdt_fixup(void *blob)
{
	return 0;
}

__weak int soc_clk_dump(void)
{
	return 0;
}

__weak int set_armclk_rate(void)
{
	return 0;
}

__weak int rk_board_init(void)
{
	return 0;
}

/*
 * define serialno max length, the max length is 512 Bytes
 * The remaining bytes are used to ensure that the first 512 bytes
 * are valid when executing 'env_set("serial#", value)'.
 */
#define VENDOR_SN_MAX	513
#define CPUID_LEN	0x10
#define CPUID_OFF	0x07

static int rockchip_set_ethaddr(void)
{
#ifdef CONFIG_ROCKCHIP_VENDOR_PARTITION
	char buf[ARP_HLEN_ASCII + 1];
	u8 ethaddr[ARP_HLEN];
	int ret;

	ret = vendor_storage_read(VENDOR_LAN_MAC_ID, ethaddr, sizeof(ethaddr));
	if (ret > 0 && is_valid_ethaddr(ethaddr)) {
		sprintf(buf, "%pM", ethaddr);
		env_set("ethaddr", buf);
	}
#endif
	return 0;
}

static int rockchip_set_serialno(void)
{
	u8 low[CPUID_LEN / 2], high[CPUID_LEN / 2];
	u8 cpuid[CPUID_LEN] = {0};
	char serialno_str[VENDOR_SN_MAX];
	int ret = 0, i;
	u64 serialno;

	/* Read serial number from vendor storage part */
	memset(serialno_str, 0, VENDOR_SN_MAX);

#ifdef CONFIG_ROCKCHIP_VENDOR_PARTITION
	ret = vendor_storage_read(VENDOR_SN_ID, serialno_str, (VENDOR_SN_MAX-1));
	if (ret > 0) {
		env_set("serial#", serialno_str);
	} else {
#endif
#ifdef CONFIG_ROCKCHIP_EFUSE
		struct udevice *dev;

		/* retrieve the device */
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_GET_DRIVER(rockchip_efuse),
						  &dev);
		if (ret) {
			printf("%s: could not find efuse device\n", __func__);
			return ret;
		}

		/* read the cpu_id range from the efuses */
		ret = misc_read(dev, CPUID_OFF, &cpuid, sizeof(cpuid));
		if (ret) {
			printf("%s: read cpuid from efuses failed, ret=%d\n",
			       __func__, ret);
			return ret;
		}
#else
		/* generate random cpuid */
		for (i = 0; i < CPUID_LEN; i++)
			cpuid[i] = (u8)(rand());
#endif
		/* Generate the serial number based on CPU ID */
		for (i = 0; i < 8; i++) {
			low[i] = cpuid[1 + (i << 1)];
			high[i] = cpuid[i << 1];
		}

		serialno = crc32_no_comp(0, low, 8);
		serialno |= (u64)crc32_no_comp(serialno, high, 8) << 32;
		snprintf(serialno_str, sizeof(serialno_str), "%llx", serialno);

		env_set("serial#", serialno_str);
#ifdef CONFIG_ROCKCHIP_VENDOR_PARTITION
	}
#endif

	return ret;
}

#if defined(CONFIG_USB_FUNCTION_FASTBOOT)
int fb_set_reboot_flag(void)
{
	printf("Setting reboot to fastboot flag ...\n");
	writel(BOOT_FASTBOOT, CONFIG_ROCKCHIP_BOOT_MODE_REG);

	return 0;
}
#endif

#ifdef CONFIG_ROCKCHIP_USB_BOOT
static int boot_from_udisk(void)
{
	struct blk_desc *desc;
	char *devtype;
	char *devnum;

	devtype = env_get("devtype");
	devnum = env_get("devnum");

	/* Booting priority: mmc1 > udisk */
	if (!strcmp(devtype, "mmc") && !strcmp(devnum, "1"))
		return 0;

	if (!run_command("usb start", -1)) {
		desc = blk_get_devnum_by_type(IF_TYPE_USB, 0);
		if (!desc) {
			printf("No usb device found\n");
			return -ENODEV;
		}

		if (!run_command("rkimgtest usb 0", -1)) {
			rockchip_set_bootdev(desc);
			env_set("devtype", "usb");
			env_set("devnum", "0");
			env_set("reboot_mode", "recovery-usb");
			printf("Boot from usb 0\n");
		} else {
			printf("No usb dev 0 found\n");
			return -ENODEV;
		}
	}

	return 0;
}
#endif

int board_late_init(void)
{
	rockchip_set_ethaddr();
	rockchip_set_serialno();
#if (CONFIG_ROCKCHIP_BOOT_MODE_REG > 0)
	setup_boot_mode();
#endif
#ifdef CONFIG_ROCKCHIP_USB_BOOT
	boot_from_udisk();
#endif
#ifdef CONFIG_DM_CHARGE_DISPLAY
	charge_display();
#endif
#ifdef CONFIG_DRM_ROCKCHIP
	rockchip_show_logo();
#endif
	soc_clk_dump();

	return rk_board_late_init();
}

#ifdef CONFIG_USING_KERNEL_DTB
/* Here, only fixup cru phandle, pmucru is not included */
static int phandles_fixup(void *fdt)
{
	const char *props[] = { "clocks", "assigned-clocks" };
	struct udevice *dev;
	struct uclass *uc;
	const char *comp;
	u32 id, nclocks;
	u32 *clocks;
	int phandle, ncells;
	int off, offset;
	int ret, length;
	int i, j;
	int first_phandle = -1;

	phandle = -ENODATA;
	ncells = -ENODATA;

	/* fdt points to kernel dtb, getting cru phandle and "#clock-cells" */
	for (offset = fdt_next_node(fdt, 0, NULL);
	     offset >= 0;
	     offset = fdt_next_node(fdt, offset, NULL)) {
		comp = fdt_getprop(fdt, offset, "compatible", NULL);
		if (!comp)
			continue;

		/* Actually, this is not a good method to get cru node */
		off = strlen(comp) - strlen("-cru");
		if (off > 0 && !strncmp(comp + off, "-cru", 4)) {
			phandle = fdt_get_phandle(fdt, offset);
			ncells = fdtdec_get_int(fdt, offset,
						"#clock-cells", -ENODATA);
			break;
		}
	}

	if (phandle == -ENODATA || ncells == -ENODATA)
		return 0;

	debug("%s: target cru: clock-cells:%d, phandle:0x%x\n",
	      __func__, ncells, fdt32_to_cpu(phandle));

	/* Try to fixup all cru phandle from U-Boot dtb nodes */
	for (id = 0; id < UCLASS_COUNT; id++) {
		ret = uclass_get(id, &uc);
		if (ret)
			continue;

		if (list_empty(&uc->dev_head))
			continue;

		list_for_each_entry(dev, &uc->dev_head, uclass_node) {
			/* Only U-Boot node go further */
			if (!dev_read_bool(dev, "u-boot,dm-pre-reloc") ||
			    !dev_read_bool(dev, "u-boot,dm-spl"))
				continue;

			for (i = 0; i < ARRAY_SIZE(props); i++) {
				if (!dev_read_prop(dev, props[i], &length))
					continue;

				clocks = malloc(length);
				if (!clocks)
					return -ENOMEM;

				/* Read "props[]" which contains cru phandle */
				nclocks = length / sizeof(u32);
				if (dev_read_u32_array(dev, props[i],
						       clocks, nclocks)) {
					free(clocks);
					continue;
				}

				/* Fixup with kernel cru phandle */
				for (j = 0; j < nclocks; j += (ncells + 1)) {
					/*
					 * Check: update pmucru phandle with cru
					 * phandle by mistake.
					 */
					if (first_phandle == -1)
						first_phandle = clocks[j];

					if (clocks[j] != first_phandle) {
						debug("WARN: %s: first cru phandle=%d, this=%d\n",
						      dev_read_name(dev),
						      first_phandle, clocks[j]);
						continue;
					}

					clocks[j] = phandle;
				}

				/*
				 * Override live dt nodes but not fdt nodes,
				 * because all U-Boot nodes has been imported
				 * to live dt nodes, should use "dev_xxx()".
				 */
				dev_write_u32_array(dev, props[i],
						    clocks, nclocks);
				free(clocks);
			}
		}
	}

	return 0;
}

int init_kernel_dtb(void)
{
	ulong fdt_addr;
	int ret;

	fdt_addr = env_get_ulong("fdt_addr_r", 16, 0);
	if (!fdt_addr) {
		printf("No Found FDT Load Address.\n");
		return -1;
	}

	ret = rockchip_read_dtb_file((void *)fdt_addr);
	if (ret < 0) {
		if (!fdt_check_header(gd->fdt_blob_kern)) {
			fdt_addr = (ulong)memalign(ARCH_DMA_MINALIGN,
					fdt_totalsize(gd->fdt_blob_kern));
			if (!fdt_addr)
				return -ENOMEM;

			memcpy((void *)fdt_addr, gd->fdt_blob_kern,
			       fdt_totalsize(gd->fdt_blob_kern));
			printf("DTB: embedded kern.dtb\n");
		} else {
			printf("Failed to get kernel dtb, ret=%d\n", ret);
			return ret;
		}
	}

	gd->fdt_blob = (void *)fdt_addr;

	/*
	 * There is a phandle miss match between U-Boot and kernel dtb node,
	 * the typical is cru phandle, we fixup it in U-Boot live dt nodes.
	 */
	phandles_fixup((void *)gd->fdt_blob);

	of_live_build((void *)gd->fdt_blob, (struct device_node **)&gd->of_root);
	dm_scan_fdt((void *)gd->fdt_blob, false);

	/* Reserve 'reserved-memory' */
	ret = boot_fdt_add_sysmem_rsv_regions((void *)gd->fdt_blob);
	if (ret)
		return ret;

	return 0;
}
#endif

void board_env_fixup(void)
{
	struct memblock mem;
	ulong u_addr_r;
	phys_size_t end;
	char *addr_r;

#ifdef ENV_MEM_LAYOUT_SETTINGS1
	const char *env_addr0[] = {
		"scriptaddr", "pxefile_addr_r",
		"fdt_addr_r", "kernel_addr_r", "ramdisk_addr_r",
	};
	const char *env_addr1[] = {
		"scriptaddr1", "pxefile_addr1_r",
		"fdt_addr1_r", "kernel_addr1_r", "ramdisk_addr1_r",
	};
	int i;

	/* 128M is a typical ram size for most platform, so as default here */
	if (gd->ram_size <= SZ_128M) {
		/* Replace orignal xxx_addr_r */
		for (i = 0; i < ARRAY_SIZE(env_addr1); i++) {
			addr_r = env_get(env_addr1[i]);
			if (addr_r)
				env_set(env_addr0[i], addr_r);
		}
	}
#endif
	/* If bl32 is disabled, maybe kernel can be load to lower address. */
	if (!(gd->flags & GD_FLG_BL32_ENABLED)) {
		addr_r = env_get("kernel_addr_no_bl32_r");
		if (addr_r)
			env_set("kernel_addr_r", addr_r);
	/* If bl32 is enlarged, we move ramdisk addr right behind it */
	} else {
		mem = param_parse_optee_mem();
		end = mem.base + mem.size;
		u_addr_r = env_get_ulong("ramdisk_addr_r", 16, 0);
		if (u_addr_r >= mem.base && u_addr_r < end)
			env_set_hex("ramdisk_addr_r", end);
	}
}

static void early_download_init(void)
{
#if defined(CONFIG_PWRKEY_DNL_TRIGGER_NUM) && \
		(CONFIG_PWRKEY_DNL_TRIGGER_NUM > 0)
	if (pwrkey_download_init())
		printf("Pwrkey download init failed\n");
#endif

	if (!tstc())
		return;

	gd->console_evt = getc();
	if (gd->console_evt <= 0x1a) /* 'z' */
		printf("Hotkey: ctrl+%c\n", (gd->console_evt + 'a' - 1));

#if (CONFIG_ROCKCHIP_BOOT_MODE_REG > 0)
	if (is_hotkey(HK_BROM_DNL)) {
		printf("Enter bootrom download...");
		flushc();
		writel(BOOT_BROM_DOWNLOAD, CONFIG_ROCKCHIP_BOOT_MODE_REG);
		do_reset(NULL, 0, 0, NULL);
		printf("failed!\n");
	}
#endif
}

#ifdef CONFIG_DM_I2C_COMPAT
enum{
	led_1 = 0,
	led_2,
	led_3,
	led_4,
	led_max,
};
static int slave_addr[led_max] = {0x3c, 0x3d, 0x3e, 0x3f};
static unsigned char register_duty[36] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 
    0x21, 0x22, 0x23, 0x24
};

unsigned char register_control[36] = {
    0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D,
    0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
    0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D,
    0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 
    0x46, 0x47, 0x48, 0x49
};
static void set_software_shutdown_dome(int chip, int mode)
{
	unsigned char buff[2] = {0};
	int ret = -1;
	
	buff[0] = mode;
	ret = i2c_write(chip, 0x00, 1, buff, 1);
	if(ret){
		printf("[%s-%d]set_software_shutdown_dome failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]set_software_shutdown_dome success\n", __func__, __LINE__);
	}
}

static void globe_ctrl(int chip, int enable)
{
	unsigned char buff[2] = {0};
	int ret = -1;

    if(enable)
        buff[0] = 0x00;
    else
        buff[0] = 0x01;

    ret = i2c_write(chip, 0x4A, 1, buff, 1);
	if(ret){
		printf("[%s-%d]set_software_shutdown_dome failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]set_software_shutdown_dome success\n", __func__, __LINE__);
	}
}
#define OUTPUT_FREQ_3K    0
#define OUTPUT_FREQ_22K   1
static void set_output_freq(int chip, int freq)
{
	unsigned char buff[2] = {0};
	int ret = -1;

    if(freq == OUTPUT_FREQ_3K)
        buff[0] = 0x00;
    else if(freq == OUTPUT_FREQ_22K)
        buff[0] = 0x01;
    else{
        printf("[%s-%d]invlaid output frequency\n", __func__, __LINE__);
        buff[0] = 0x01;
    }
	
	ret = i2c_write(chip, 0x4B, 1, buff, 1);
	if(ret){
		printf("[%s-%d]set_output_freq failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]set_output_freq success\n", __func__, __LINE__);
	}
}

static void set_pwm_duty(int chip, int duty, int led_idx)
{
    unsigned char buff[2] = {0};
	int ret = -1;

    buff[0] = duty;
    ret = i2c_write(chip, register_duty[led_idx], 1, buff, 1);
	if(ret){
		printf("[%s-%d]set_pwm_duty failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]set_pwm_duty success\n", __func__, __LINE__);
	}
}

static void set_cs(int chip, int current, int state, int led_idx)
{
	unsigned char buff[2] = {0};
	int ret = -1;
	
	buff[0] = (( current << 2 ) | state);
	ret = i2c_write(chip, register_control[led_idx], 1, buff, 1);
	if(ret){
		printf("[%s-%d]set_cs failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]set_cs success\n", __func__, __LINE__);
	}
}

static void update_duty_current_state(int chip)
{
    unsigned char buff[2] = {0};
	int ret = -1;

    buff[0] = 0x00;
    ret = i2c_write(chip, 0x25, 1, buff, 1);
	if(ret){
		printf("[%s-%d]update_duty_current_state failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]update_duty_current_state success\n", __func__, __LINE__);
	}
}

static void set_all_red_led_state(int led_state) {
    int duty[36];
    int current[36];
    int state[36];
    int i, j;

    for (j = 0; j < led_max; j++) {
        for (i = 0; i < 36; i += 3) {
            duty[i] = 0xff;
            set_pwm_duty(slave_addr[j], duty[i], i);

            current[i] = 0x00;
            if (led_state)
                state[i] = 0x01;
            else
                state[i] = 0x00;
            set_cs(slave_addr[j], current[i], state[i], i);
        }
        if (led_state)
            set_software_shutdown_dome(slave_addr[j], 1);
        else
            set_software_shutdown_dome(slave_addr[j], 0);
        update_duty_current_state(slave_addr[j]);
    }
}
static void set_all_green_led_state(int led_state) {
    int duty[36];
    int current[36];
    int state[36];
    int i, j;

    for (j = 0; j < led_max; j++) {
        for (i = 1; i < 36; i += 3) {
            duty[i] = 0x40;
            set_pwm_duty(slave_addr[j], duty[i], i);

            current[i] = 0x00;
            if (led_state)
                state[i] = 0x01;
            else
                state[i] = 0x00;
            set_cs(slave_addr[j], current[i], state[i], i);
        }
        if (led_state)
            set_software_shutdown_dome(slave_addr[j], 1);
        else
            set_software_shutdown_dome(slave_addr[j], 0);
        update_duty_current_state(slave_addr[j]);
    }
}

// static void set_all_blue_led_state(int led_state)
// {
	// int duty[36];
	// int current[36];
	// int state[36];
	// int i,j;

	// for(j = 0; j < led_max; j++){
		// for(i = 2; i < 36; i += 3){
			// duty[i] = 0xc8;		
			// set_pwm_duty(slave_addr[j], duty[i], i);
			
			// current[i] = 0x00;
			// if(led_state)
				// state[i] = 0x01;
			// else
				// state[i] = 0x00;
			// set_cs(slave_addr[j], current[i], state[i], i);
		// }
		// if(led_state)
			// set_software_shutdown_dome(slave_addr[j], 1);
		// else
			// set_software_shutdown_dome(slave_addr[j],  0);
		// update_duty_current_state(slave_addr[j]);
	// }
// }

// static void set_all_white_led_state(int led_state)
// {
	// int duty[36];
	// int current[36];
	// int state[36];
	// int i,j;

	// for(j = 0; j < led_max; j++){
		// for(i = 0; i < 36; i += 1){
			// duty[i] = 0xc8;		
			// set_pwm_duty(slave_addr[j], duty[i], i);
			
			// current[i] = 0x00;
			// if(led_state)
				// state[i] = 0x01;
			// else
				// state[i] = 0x00;
			// set_cs(slave_addr[j], current[i], state[i], i);
		// }
		// if(led_state)
			// set_software_shutdown_dome(slave_addr[j], 1);
		// else
			// set_software_shutdown_dome(slave_addr[j],  0);
		// update_duty_current_state(slave_addr[j]);
	// }
// }

// static void reset_all_register_default_value(int chip)
// {
    // unsigned char buff[2] = {0};
	// int ret = -1;

    // buff[0] = 0x00;
    // ret = i2c_write(chip, 0x4F, 1, buff, 1);
	// if(ret){
		// printf("[%s-%d]reset_all_register_default_value failed\n", __func__, __LINE__);
	// }
	// else{
		// printf("[%s-%d]reset_all_register_default_value success\n", __func__, __LINE__);
	// }
// }

// static void board_i2c_led_exit(void)
// {
	// int  i;
	// for(i = 0; i < led_max; i++){
		// set_software_shutdown_dome(slave_addr[i], 0);
		// reset_all_register_default_value(slave_addr[i]);
	// }
// }

static void board_i2c_led_init(void)
{
	int ret = -1;
	int i;
	// unsigned char buff[2] = {0};
	
	printf("----->board_i2c_led_init\n");
	ret = i2c_set_bus_num(3);
	if(ret){
		printf("[%s-%d]i2c_set_bus_num failed\n", __func__, __LINE__);
	}
	else{
		printf("[%s-%d]i2c_set_bus_num success\n", __func__, __LINE__);
	}
	
	// buff[0] = 0x01;
	// ret = i2c_write(slave_addr[0], 0x00, 1, buff, 1);
	// if(ret){
		// printf("[%s-%d]i2c_write failed\n", __func__, __LINE__);
	// }
	// else{
		// printf("[%s-%d]i2c_write success\n", __func__, __LINE__);
	// }
	
	for(i = 0; i < 1; i++){
		set_software_shutdown_dome(slave_addr[i], 0);
		globe_ctrl(slave_addr[i], 1);
		set_output_freq(slave_addr[i], OUTPUT_FREQ_22K);
	}
	
	set_all_red_led_state(1);
	// udelay(1000000);
	// board_i2c_led_exit();
	set_all_green_led_state(1);
	// udelay(1000000);
	// board_i2c_led_exit();
	// set_all_blue_led_state(1);
	// udelay(1000000);
	// board_i2c_led_exit();
	// set_all_white_led_state(1);
}
#endif

int board_init(void)
{
	board_debug_uart_init();

#ifdef CONFIG_USING_KERNEL_DTB
	init_kernel_dtb();
#endif
	early_download_init();

	/*
	 * pmucru isn't referenced on some platforms, so pmucru driver can't
	 * probe that the "assigned-clocks" is unused.
	 */
	clks_probe();
#ifdef CONFIG_DM_REGULATOR
	if (regulators_enable_boot_on(is_hotkey(HK_REGULATOR)))
		debug("%s: Can't enable boot on regulator\n", __func__);
#endif

#ifdef CONFIG_ROCKCHIP_IO_DOMAIN
	io_domain_init();
#endif

	set_armclk_rate();

#ifdef CONFIG_DM_DVFS
	dvfs_init(true);
#endif

#ifdef CONFIG_DM_I2C_COMPAT
	board_i2c_led_init();
#endif

	return rk_board_init();
}

int interrupt_debugger_init(void)
{
#ifdef CONFIG_ROCKCHIP_DEBUGGER
	return rockchip_debugger_init();
#else
	return 0;
#endif
}

int board_fdt_fixup(void *blob)
{
	/* Common fixup for DRM */
#ifdef CONFIG_DRM_ROCKCHIP
	rockchip_display_fixup(blob);
#endif

	return rk_board_fdt_fixup(blob);
}

#ifdef CONFIG_ARM64_BOOT_AARCH32
/*
 * Fixup MMU region attr for OP-TEE on ARMv8 CPU:
 *
 * What ever U-Boot is 64-bit or 32-bit mode, the OP-TEE is always 64-bit mode.
 *
 * Command for OP-TEE:
 *	64-bit mode: dcache is always enabled;
 *	32-bit mode: dcache is always disabled(Due to some unknown issue);
 *
 * Command for U-Boot:
 *	64-bit mode: MMU table is static defined in rkxxx.c file, all memory
 *		     regions are mapped. That's good to match OP-TEE MMU policy.
 *
 *	32-bit mode: MMU table is setup according to gd->bd->bi_dram[..] where
 *		     the OP-TEE region has been reserved, so it can not be
 *		     mapped(i.e. dcache is disabled). That's also good to match
 *		     OP-TEE MMU policy.
 *
 * For the data coherence when communication between U-Boot and OP-TEE, U-Boot
 * should follow OP-TEE MMU policy.
 *
 * Here is the special:
 *	When CONFIG_ARM64_BOOT_AARCH32 is enabled, U-Boot is 32-bit mode while
 *	OP-TEE is still 64-bit mode. U-Boot would not map MMU table for OP-TEE
 *	region(but OP-TEE requires it cacheable) so we fixup here.
 */
int board_initr_caches_fixup(void)
{
	struct memblock mem;

	mem = param_parse_optee_mem();
	if (mem.size)
		mmu_set_region_dcache_behaviour(mem.base, mem.size,
						DCACHE_WRITEBACK);
	return 0;
}
#endif

void arch_preboot_os(uint32_t bootm_state)
{
	if (bootm_state & BOOTM_STATE_OS_PREP)
		hotkey_run(HK_CLI_OS_PRE);
}

void board_quiesce_devices(void)
{
	hotkey_run(HK_CMDLINE);
	hotkey_run(HK_CLI_OS_GO);

#ifdef CONFIG_ROCKCHIP_PRELOADER_ATAGS
	/* Destroy atags makes next warm boot safer */
	atags_destroy();
#endif

#if defined(CONFIG_CONSOLE_RECORD)
	/* Print record console data */
	console_record_print_purge();
#endif
}

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}

#ifdef CONFIG_LMB
/*
 * Using last bi_dram[...] to initialize "bootm_low" and "bootm_mapsize".
 * This makes lmb_alloc_base() always alloc from tail of sdram.
 * If we don't assign it, bi_dram[0] is used by default and it may cause
 * lmb_alloc_base() fail when bi_dram[0] range is small.
 */
void board_lmb_reserve(struct lmb *lmb)
{
	char bootm_mapsize[32];
	char bootm_low[32];
	u64 start, size;
	int i;

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (!gd->bd->bi_dram[i].size)
			break;
	}

	start = gd->bd->bi_dram[i - 1].start;
	size = gd->bd->bi_dram[i - 1].size;

	/*
	 * 32-bit kernel: ramdisk/fdt shouldn't be loaded to highmem area(768MB+),
	 * otherwise "Unable to handle kernel paging request at virtual address ...".
	 *
	 * So that we hope limit highest address at 768M, but there comes the the
	 * problem: ramdisk is a compressed image and it expands after descompress,
	 * so it accesses 768MB+ and brings the above "Unable to handle kernel ...".
	 *
	 * We make a appointment that the highest memory address is 512MB, it
	 * makes lmb alloc safer.
	 */
#ifndef CONFIG_ARM64
	if (start >= ((u64)CONFIG_SYS_SDRAM_BASE + SZ_512M)) {
		start = gd->bd->bi_dram[i - 2].start;
		size = gd->bd->bi_dram[i - 2].size;
	}

	if ((start + size) > ((u64)CONFIG_SYS_SDRAM_BASE + SZ_512M))
		size = (u64)CONFIG_SYS_SDRAM_BASE + SZ_512M - start;
#endif
	sprintf(bootm_low, "0x%llx", start);
	sprintf(bootm_mapsize, "0x%llx", size);
	env_set("bootm_low", bootm_low);
	env_set("bootm_mapsize", bootm_mapsize);
}
#endif

#ifdef CONFIG_BIDRAM
int board_bidram_reserve(struct bidram *bidram)
{
	struct memblock mem;
	int ret;

	/* ATF */
	mem = param_parse_atf_mem();
	ret = bidram_reserve(MEMBLK_ID_ATF, mem.base, mem.size);
	if (ret)
		return ret;

	/* PSTORE/ATAGS/SHM */
	mem = param_parse_common_resv_mem();
	ret = bidram_reserve(MEMBLK_ID_SHM, mem.base, mem.size);
	if (ret)
		return ret;

	/* OP-TEE */
	mem = param_parse_optee_mem();
	ret = bidram_reserve(MEMBLK_ID_OPTEE, mem.base, mem.size);
	if (ret)
		return ret;

	return 0;
}

parse_fn_t board_bidram_parse_fn(void)
{
	return param_parse_ddr_mem;
}
#endif

#ifdef CONFIG_ROCKCHIP_AMP
void cpu_secondary_init_r(void)
{
	amp_cpus_on();
}
#endif

#if defined(CONFIG_ROCKCHIP_PRELOADER_SERIAL) && \
    defined(CONFIG_ROCKCHIP_PRELOADER_ATAGS)
int board_init_f_init_serial(void)
{
	struct tag *t = atags_get_tag(ATAG_SERIAL);

	if (t) {
		gd->serial.using_pre_serial = t->u.serial.enable;
		gd->serial.addr = t->u.serial.addr;
		gd->serial.baudrate = t->u.serial.baudrate;
		gd->serial.id = t->u.serial.id;

		debug("%s: enable=%d, addr=0x%lx, baudrate=%d, id=%d\n",
		      __func__, gd->serial.using_pre_serial,
		      gd->serial.addr, gd->serial.baudrate,
		      gd->serial.id);
	}

	return 0;
}
#endif

#if defined(CONFIG_USB_GADGET) && defined(CONFIG_USB_GADGET_DWC2_OTG)
#include <fdt_support.h>
#include <usb.h>
#include <usb/dwc2_udc.h>

static struct dwc2_plat_otg_data otg_data = {
	.rx_fifo_sz	= 512,
	.np_tx_fifo_sz	= 16,
	.tx_fifo_sz	= 128,
};

int board_usb_init(int index, enum usb_init_type init)
{
	const void *blob = gd->fdt_blob;
	const fdt32_t *reg;
	fdt_addr_t addr;
	int node;

	/* find the usb_otg node */
	node = fdt_node_offset_by_compatible(blob, -1, "snps,dwc2");

retry:
	if (node > 0) {
		reg = fdt_getprop(blob, node, "reg", NULL);
		if (!reg)
			return -EINVAL;

		addr = fdt_translate_address(blob, node, reg);
		if (addr == OF_BAD_ADDR) {
			pr_err("Not found usb_otg address\n");
			return -EINVAL;
		}

#if defined(CONFIG_ROCKCHIP_RK3288)
		if (addr != 0xff580000) {
			node = fdt_node_offset_by_compatible(blob, node,
							     "snps,dwc2");
			goto retry;
		}
#endif
	} else {
		/*
		 * With kernel dtb support, rk3288 dwc2 otg node
		 * use the rockchip legacy dwc2 driver "dwc_otg_310"
		 * with the compatible "rockchip,rk3288_usb20_otg",
		 * and rk3368 also use the "dwc_otg_310" driver with
		 * the compatible "rockchip,rk3368-usb".
		 */
#if defined(CONFIG_ROCKCHIP_RK3288)
		node = fdt_node_offset_by_compatible(blob, -1,
				"rockchip,rk3288_usb20_otg");
#elif defined(CONFIG_ROCKCHIP_RK3368)
		node = fdt_node_offset_by_compatible(blob, -1,
				"rockchip,rk3368-usb");
#endif
		if (node > 0) {
			goto retry;
		} else {
			pr_err("Not found usb_otg device\n");
			return -ENODEV;
		}
	}

	otg_data.regs_otg = (uintptr_t)addr;

	return dwc2_udc_probe(&otg_data);
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	return 0;
}
#endif
