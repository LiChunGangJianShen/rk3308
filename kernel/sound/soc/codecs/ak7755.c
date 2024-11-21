/*
 * ak7755.c  --  audio driver for AK7755
 *
 * Copyright (C) 2014 Asahi Kasei Microdevices Corporation
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      14/04/22	    1.0
 *                      15/06/15	    1.1
 *                      15/10/23	    2.0
 *                      16/02/22	    2.1
 *                      16/03/25	    2.11
 *                      16/05/20        2.2
 *                      16/09/27        2.4
 *                      17/06/19        2.5  Kernel 4.4.XX
 *                      17/12/25        2.6  Kernel 4.9.XX
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#include <linux/regmap.h>  // '15/10/23
#include <linux/of_gpio.h> // '15/10/23

#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <linux/kobject.h>




#include "ak7755.h"
#include "ak7755_dsp_code.h"
#include <linux/types.h>

#define AK7755_PD_SUSPEND     // '15/06/15

#define AK7755_DEBUG			//used at debug mode

#ifdef AK7755_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif


#define SND_SOC_SPI 1
#define SND_SOC_I2C 2

char* vhd_get_hw_version(void);

/* AK7755 Codec Private Data */
struct ak7755_priv {
	int control_type;   // '15/10/23
	struct snd_soc_codec *codec;
	struct spi_device *spi;
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct clk *mclk;
	struct kobject *ak7755_control_kobj ;
	int fs;
	int rclk;			//Master Clock
	int lign;			//LIGN3-0(LineIn Volume)
	int selmix;			//SELMIX2-0
	int status;
	int MIRNo;
	int bickfs;
	int pdn_gpio;
	int i2c_sel_gpio;
	int amp_mute_gpio;  //功放mute控制
	int amp_control_gpio; //功放电源控制
	int spk_mute_delay;

	u16  DSPPramMode;
	u16  DSPCramMode;
	u16  DSPOfregMode;
	u16  DSPAcramMode;
	int  EQLevel[5];
	int  HPFfc[2];
	int  LimRel;
	int  LimVol;
};

struct _ak7755_pd_handler {
	int ref_count;
	struct mutex lock;
	struct ak7755_priv *data;
} ak7755_pd_handler = {
	.ref_count = -1,
	.data = NULL,
};

static struct ak7755_priv *ak7755_data;
static int is_first_unmute;
static int is_ak7755_mute;
static int ak7755_dsp_gain_enable=1;

/*DSP 音量控制对应table*/
#define AK7755_DSP_GAIN_NUM 22
// const uint8_t ak7755_vol_tables[AK7755_DSP_GAIN_NUM][3]=
unsigned char ak7755_vol_tables[AK7755_DSP_GAIN_NUM][3]=
{
{0x00,0x00,0x00},//MUTE
{0x01,0x9A,0x90},//-26db
{0x02,0x8A,0xB0},//-22db
{0x04,0x07,0x50},//-18db
{0x06,0x62,0x80},//-14db
{0x0A,0x1E,0x90},//-10db
{0x0C,0xBD,0x50},//-8db
{0x10,0x09,0xC0},//-6db
{0x14,0x30,0xD0},//-4db
{0x19,0x6B,0x20},//-2db
{0x20,0x00,0x00},//0db

{0x00,0x00,0x00},//MUTE
{0x01,0x9A,0x90},//-26db
{0x02,0x8A,0xB0},//-22db
{0x03,0x33,0x30},//-20db
{0x04,0x07,0x50},//-18db
{0x05,0x12,0x60},//-16db
{0x06,0x62,0x80},//-14db
{0x08,0x09,0xC0},//-12db
{0x0A,0x1E,0x90},//-10db
{0x0C,0xBD,0x50},//-8db
{0x10,0x09,0xC0}//-6db
};
/* ak7755 register cache & default register settings */
static const struct reg_default ak7755_reg[] = { // '15/10/23
	{0x0,0},{0x1,0},{0x2,0},{0x3,0},{0x4,0},{0x5,0},{0x6,0},{0x7,0},
	{0x8,0},{0x9,0},{0xA,0},{0xB,0},{0xC,0},{0xD,0},{0xE,0},{0xF,0},
	{0x10,0},{0x11,0},{0x12,0},{0x13,0},{0x14,0},{0x15,0},{0x16,0},{0x17,0},
	{0x18,0},{0x19,0},{0x1A,0},{0x1B,0},{0x1C,0},{0x1D,0},{0x1E,0},{0x1F,0},
	{0x20,0},{0x21,0},{0x22,0},{0x23,0},{0x24,0},{0x25,0},{0x26,0},{0x27,0},
	{0x28,0},{0x29,0},{0x2A,0},{0x2B,0},{0x2C,0},{0x2D,0},{0x2E,0},{0x2F,0},
	{0x30,0},{0x31,0},{0x32,0},{0x33,0},{0x34,0},{0x35,0},{0x36,0},{0x37,0},
	{0x38,0},{0x39,0},{0x3A,0},{0x3B,0},{0x3C,0},{0x3D,0},{0x3E,0},{0x3F,0},
	{0x40,0},{0x41,0},{0x42,0},{0x43,0},{0x44,0},{0x45,0},{0x46,0},{0x47,0},
	{0x48,0},{0x49,0},{0x4A,0},{0x4B,0},{0x4C,0},{0x4D,0},{0x4E,0},{0x4F,0},
	{0x50,0},{0x51,0},{0x52,0},{0x53,0},{0x54,0},{0x55,0},{0x56,0},{0x57,0},
	{0x58,0},{0x59,0},{0x5A,0},{0x5B,0},{0x5C,0},{0x5D,0},{0x5E,0},{0x5F,0},
	{0x60,0},{0x61,0},{0x62,0},{0x63,0},{0x64,0},{0x65,0},{0x66,0},{0x67,0},
	{0x68,0},{0x69,0},{0x6A,0},{0x6B,0},{0x6C,0},{0x6D,0},{0x6E,0},{0x6F,0},
	{0x70,0},{0x71,0},{0x72,0},{0x73,0},{0x74,0},{0x75,0},{0x76,0},{0x77,0},
	{0x78,0},{0x79,0},{0x7A,0},{0x7B,0},{0x7C,0},{0x7D,0},{0x7E,0},{0x7F,0},
	{0x80,0},{0x81,0},{0x82,0},{0x83,0},{0x84,0},{0x85,0},{0x86,0},{0x87,0},
	{0x88,0},{0x89,0},{0x8A,0},{0x8B,0},{0x8C,0},{0x8D,0},{0x8E,0},{0x8F,0},
	{0x90,0},{0x91,0},{0x92,0},{0x93,0},{0x94,0},{0x95,0},{0x96,0},{0x97,0},
	{0x98,0},{0x99,0},{0x9A,0},{0x9B,0},{0x9C,0},{0x9D,0},{0x9E,0},{0x9F,0},
	{0xA0,0},{0xA1,0},{0xA2,0},{0xA3,0},{0xA4,0},{0xA5,0},{0xA6,0},{0xA7,0},
	{0xA8,0},{0xA9,0},{0xAA,0},{0xAB,0},{0xAC,0},{0xAD,0},{0xAE,0},{0xAF,0},
	{0xB0,0},{0xB1,0},{0xB2,0},{0xB3,0},{0xB4,0},{0xB5,0},{0xB6,0},{0xB7,0},
	{0xB8,0},{0xB9,0},{0xBA,0},{0xBB,0},{0xBC,0},{0xBD,0},{0xBE,0},{0xBF,0},
	{ 0xC0, 0x00 },  /*	0xC0	AK7755_C0_CLOCK_SETTING1			*/
	{ 0xC1, 0x00 },  /*	0xC1	AK7755_C1_CLOCK_SETTING2			*/
	{ 0xC2, 0x00 },  /*	0xC2	AK7755_C2_SERIAL_DATA_FORMAT		*/
	{ 0xC3, 0x00 },  /*	0xC3	AK7755_C3_DELAY_RAM_DSP_IO			*/
	{ 0xC4, 0x00 },  /*	0xC4	AK7755_C4_DATARAM_CRAM_SETTING		*/
	{ 0xC5, 0x00 },  /*	0xC5	AK7755_C5_ACCELARETOR_SETTING		*/
	{ 0xC6, 0x00 },  /*	0xC6	AK7755_C6_DAC_DEM_SETTING			*/
	{ 0xC7, 0x00 },  /*	0xC7	AK7755_C7_DSP_OUT_SETTING			*/
	{ 0xC8, 0x00 },  /*	0xC8	AK7755_C8_DAC_IN_SETTING			*/
	{ 0xC9, 0x00 },  /*	0xC9	AK7755_C9_ANALOG_IO_SETTING			*/
	{ 0xCA, 0x00 },  /*	0xCA	AK7755_CA_CLK_SDOUT_SETTING			*/
	{ 0xCB, 0x00 },  /*	0xCB	AK7755_CB_TEST_SETTING				*/
	{ 0xCC, 0x00 },  /*	0xCC	AK7755_CC_VOLUME_TRANSITION			*/
	{ 0xCD, 0x80 },  /*	0xCD	AK7755_CD_STO_DLS_SETTING			*/
	{ 0xCE, 0x00 },  /*	0xCE	AK7755_CE_POWER_MANAGEMENT			*/
	{ 0xCF, 0x00 },  /*	0xCF	AK7755_CF_RESET_POWER_SETTING		*/
	{ 0xD0, 0x00 },  /*	0xD0	AK7755_D0_FUNCTION_SETTING			*/
	{ 0xD1, 0x00 },  /*	0xD1	AK7755_D1_DSPMCLK_SETTING			*/
	{ 0xD2, 0x00 },  /*	0xD2	AK7755_D2_MIC_GAIN_SETTING			*/
	{ 0xD3, 0x00 },  /*	0xD3	AK7755_D3_LIN_LO3_VOLUME_SETTING	*/
	{ 0xD4, 0x00 },  /*	0xD4	AK7755_D4_LO1_LO2_VOLUME_SETTING	*/
	{ 0xD5, 0x30 },  /*	0xD5	AK7755_D5_ADC_DVOLUME_SETTING1		*/
	{ 0xD6, 0x30 },  /*	0xD6	AK7755_D6_ADC_DVOLUME_SETTING2		*/
	{ 0xD7, 0x30 },  /*	0xD7	AK7755_D7_ADC2_DVOLUME_SETTING1		*/
	{ 0xD8, 0x18 },  /*	0xD8	AK7755_D8_DAC_DVOLUME_SETTING1		*/
	{ 0xD9, 0x18 },  /*	0xD9	AK7755_D9_DAC_DVOLUME_SETTING2		*/
	{ 0xDA, 0x00 },  /*	0xDA	AK7755_DA_MUTE_ADRC_ZEROCROSS_SET	*/
	{ 0xDB, 0x00 },  /*	0xDB	AK7755_DB_ADRC_MIC_GAIN_READ		*/
	{ 0xDC, 0x00 },  /*	0xDC	AK7755_DC_TEST_SETTING				*/
	{ 0xDD, 0x30 },  /*	0xDD	AK7755_DD_ADC2_DVOLUME_SETTING2		*/
	{ 0xDE, 0x00 },  /*	0xDE	AK7755_DE_DMIC_IF_SETTING			*/
};

/* MIC Input Volume control:
 * from 0 to 36 dB (quantity of each step is various) */
static DECLARE_TLV_DB_MINMAX(mgnl_tlv, 0, 3600);
static DECLARE_TLV_DB_MINMAX(mgnr_tlv, 0, 3600);

/* Line-out Volume control:
 * from -30 to 0 dB in 2 dB steps (mute instead of -30 dB) */
static DECLARE_TLV_DB_SCALE(lovol1_tlv, -3000, 200, 0);
static DECLARE_TLV_DB_SCALE(lovol2_tlv, -3000, 200, 0);
static DECLARE_TLV_DB_SCALE(lovol3_tlv, -3000, 200, 0);

static const char *line_in_texts[]  =
{
	"-21dB","-18dB","-15dB","-12dB","-9dB","-6dB","-3dB",
	"0dB","+3dB","+6dB","+9dB","+12dB","+15dB","+18dB","+21dB",
};

/*
   static const struct soc_enum ak7755_linein_enum[] = {
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(line_in_texts), line_in_texts),
   };
 */
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_linein_enum, line_in_texts);  // '16/03/25


/* ADC, ADC2 Digital Volume control:
 * from -103.5 to 24 dB in 0.5 dB steps (mute instead of -103.5 dB) */
static DECLARE_TLV_DB_SCALE(voladl_tlv, -10350, 50, 0);
static DECLARE_TLV_DB_SCALE(voladr_tlv, -10350, 50, 0);
static DECLARE_TLV_DB_SCALE(volad2l_tlv, -10350, 50, 0);
static DECLARE_TLV_DB_SCALE(volad2r_tlv, -10350, 50, 0);

/* DAC Digital Volume control:
 * from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB) */
static DECLARE_TLV_DB_SCALE(voldal_tlv, -11550, 50, 0);
static DECLARE_TLV_DB_SCALE(voldar_tlv, -11550, 50, 0);


static const char *ak7755_bank_select_texts[] =
{"0:8192", "1024:7168","2048:6144","3072:5120","4096:4096",
	"5120:3072","6144:2048","7168:1024","8192:0"};
static const char *ak7755_drms_select_texts[] =
{"512:1536", "1024:1024", "1536:512"};
static const char *ak7755_dram_select_texts[] =
{"Ring:Ring", "Ring:Linear", "Linear:Ring", "Linear:Linear"};
static const char *ak7755_pomode_select_texts[] = {"DBUS Immediate", "OFREG"};
static const char *ak7755_wavp_select_texts[] =
{"33 word", "65 word", "129 word", "257 word"};
static const char *ak7755_filmode1_select_texts[] = {"Adaptive Filter", "FIR Filter"};
static const char *ak7755_filmode2_select_texts[] = {"Adaptive Filter", "FIR Filter"};
static const char *ak7755_submode1_select_texts[] = {"Fullband", "Subband"};
static const char *ak7755_submode2_select_texts[] = {"Fullband", "Subband"};
static const char *ak7755_memdiv_select_texts[] =
{"2048:-", "1792:256", "1536:512", "1024:1024"};
static const char *ak7755_dem_select_texts[] = {"Off", "48kHz", "44.1kHz", "32kHz"};
static const char *ak7755_clkoe_select_texts[] = {"CLKO=L", "CLKO Out Enable"};
static const char *ak7755_clks_select_texts[] =
{"12.288MHz", "6.144MHz", "3.072MHz", "8.192MHz",
	"4.096MHz", "2.048MHz", "256fs", "XTI or BICK"};

static const struct soc_enum ak7755_set_enum[] = {
	SOC_ENUM_SINGLE(AK7755_C3_DELAY_RAM_DSP_IO, 0,
			ARRAY_SIZE(ak7755_bank_select_texts), ak7755_bank_select_texts),
	SOC_ENUM_SINGLE(AK7755_C4_DATARAM_CRAM_SETTING, 6,
			ARRAY_SIZE(ak7755_drms_select_texts), ak7755_drms_select_texts),
	SOC_ENUM_SINGLE(AK7755_C4_DATARAM_CRAM_SETTING, 4,
			ARRAY_SIZE(ak7755_dram_select_texts), ak7755_dram_select_texts),
	SOC_ENUM_SINGLE(AK7755_C4_DATARAM_CRAM_SETTING, 3,
			ARRAY_SIZE(ak7755_pomode_select_texts), ak7755_pomode_select_texts),
	SOC_ENUM_SINGLE(AK7755_C4_DATARAM_CRAM_SETTING, 0,
			ARRAY_SIZE(ak7755_wavp_select_texts), ak7755_wavp_select_texts),
	SOC_ENUM_SINGLE(AK7755_C5_ACCELARETOR_SETTING, 5,
			ARRAY_SIZE(ak7755_filmode1_select_texts), ak7755_filmode1_select_texts),
	SOC_ENUM_SINGLE(AK7755_C5_ACCELARETOR_SETTING, 4,
			ARRAY_SIZE(ak7755_filmode2_select_texts), ak7755_filmode2_select_texts),
	SOC_ENUM_SINGLE(AK7755_C5_ACCELARETOR_SETTING, 3,
			ARRAY_SIZE(ak7755_submode1_select_texts), ak7755_submode1_select_texts),
	SOC_ENUM_SINGLE(AK7755_C5_ACCELARETOR_SETTING, 2,
			ARRAY_SIZE(ak7755_submode2_select_texts), ak7755_submode2_select_texts),
	SOC_ENUM_SINGLE(AK7755_C5_ACCELARETOR_SETTING, 0,
			ARRAY_SIZE(ak7755_memdiv_select_texts), ak7755_memdiv_select_texts),
	SOC_ENUM_SINGLE(AK7755_C6_DAC_DEM_SETTING, 6,
			ARRAY_SIZE(ak7755_dem_select_texts), ak7755_dem_select_texts),
	SOC_ENUM_SINGLE(AK7755_CA_CLK_SDOUT_SETTING, 7,
			ARRAY_SIZE(ak7755_clkoe_select_texts), ak7755_clkoe_select_texts),
	SOC_ENUM_SINGLE(AK7755_C1_CLOCK_SETTING2, 1,
			ARRAY_SIZE(ak7755_clks_select_texts), ak7755_clks_select_texts),
};

// '15/06/15
static const char *ak7755_bick_select_texts[] =
{"64fs", "48fs", "32fs", "TDM" };

/*
   static const struct soc_enum ak7755_bick_enum[] = {
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_bick_select_texts), ak7755_bick_select_texts)
   };
 */
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_bick_enum, ak7755_bick_select_texts);  // '16/03/25


static int get_bickfs(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//   struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7755->bickfs;

	return 0;
}

static int set_bickfs(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);

	ak7755->bickfs = ucontrol->value.enumerated.item[0];
	return 0;
}
// '15/06/15

static const char *selmix_set_texts[]  =
{
	"SDOUTAD", "ADL_AD2L/ADR", "ADL/ADR_AD2R", "SDOUTAD2",
	"DSPL/AD2R", "AD2L/DSPR", "DSPL/ADR", "ADL/DSPR",
};

/*
   static const struct soc_enum ak7755_selmix_enum[] = {
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(selmix_set_texts), selmix_set_texts),
   };
 */
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_selmix_enum, selmix_set_texts);  // '16/03/25

// Added for AK7755
/*
   static const struct soc_enum ak7755_firmware_enum[] =
   {
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_firmware_pram), ak7755_firmware_pram),
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_firmware_cram), ak7755_firmware_cram),
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_firmware_ofreg), ak7755_firmware_ofreg),
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_firmware_acram), ak7755_firmware_acram),
   };
 */
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_firmware_enum0, ak7755_firmware_pram);  // '16/03/25
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_firmware_enum1, ak7755_firmware_cram);  // '16/03/25
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_firmware_enum2, ak7755_firmware_ofreg);  // '16/03/25
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_firmware_enum3, ak7755_firmware_acram);  // '16/03/25


static int ak7755_firmware_write_ram(u16 mode, u16 cmd);
static int ak7755_set_status(enum ak7755_status status);
static int ak7755_write_cram(int addr, int len, unsigned char *cram_data);

static int get_linein(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7755->lign;

	return 0;
}

static int set_linein(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//   struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);

	ak7755->lign = ucontrol->value.enumerated.item[0];

	if ( ak7755->lign <= 7) {
		snd_soc_update_bits(codec, AK7755_D3_LIN_LO3_VOLUME_SETTING, 0xF0, ((7 - ak7755->lign) << 4) );
	}
	else {
		snd_soc_update_bits(codec, AK7755_D3_LIN_LO3_VOLUME_SETTING, 0xF0, ((ak7755->lign + 1) << 4) );
	}

	return 0;
}

static int get_selmix(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = ak7755->selmix;

	return 0;
}

static int set_selmix(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);

	ak7755->selmix = ucontrol->value.enumerated.item[0];

	if ( ak7755->selmix < 4) {	//SELMIX2=0, SELMIX1-0=selmix
		snd_soc_update_bits(codec, AK7755_C9_ANALOG_IO_SETTING, 0x01, 0x00);
		snd_soc_update_bits(codec, AK7755_C8_DAC_IN_SETTING, 0x03, ak7755->selmix);
	}
	else {						//SELMIX2=1, SELMIX1-0=(selmix & 0x03)
		snd_soc_update_bits(codec, AK7755_C9_ANALOG_IO_SETTING, 0x01, 0x01);
		snd_soc_update_bits(codec, AK7755_C8_DAC_IN_SETTING, 0x03, (ak7755->selmix & 0x03));
	}

	return 0;
}

static int get_DSP_write_pram(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->DSPPramMode;

	return 0;

}

static int set_DSP_write_pram(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	/*
	   if ( currMode == ak7755_data->DSPPramMode ) {
	   akdbgprt("\t%s Same PRAM mode =%d\n",__FUNCTION__, currMode);
	   return(0);
	   }
	 */
	akdbgprt("%s PRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7755_firmware_write_ram(RAMTYPE_PRAM, currMode);
	if ( ret != 0 ) return(-1);

	ak7755_data->DSPPramMode = currMode;

	return(0);
}

static int get_DSP_write_cram(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->DSPCramMode;

	return 0;

}

static int set_DSP_write_cram(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	/*
	   if ( currMode == ak7755_data->DSPCramMode ) {
	   akdbgprt("\t%s Same CRAM mode =%d\n",__FUNCTION__, currMode);
	   return(0);
	   }
	 */
	akdbgprt("%s CRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7755_firmware_write_ram(RAMTYPE_CRAM, currMode);
	if ( ret != 0 ) return(-1);

	ak7755_data->DSPCramMode =currMode;

	return(0);
}

static int get_DSP_write_ofreg(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->DSPOfregMode;

	return 0;

}

static int set_DSP_write_ofreg(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	/*
	   if ( currMode == ak7755_data->DSPOfregMode ) {
	   akdbgprt("\t%s Same CRAM mode =%d\n",__FUNCTION__, currMode);
	   return(0);
	   }
	   akdbgprt("\t%s OFREG mode =%d\n",__FUNCTION__, currMode);
	 */
	ret = ak7755_firmware_write_ram(RAMTYPE_OFREG, currMode);
	if ( ret != 0 ) return(-1);

	ak7755_data->DSPOfregMode =currMode;

	return(0);
}

static int get_DSP_write_acram(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->DSPAcramMode;

	return 0;

}

static int set_DSP_write_acram(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	/*
	   if ( currMode == ak7755_data->DSPAcramMode ) {
	   akdbgprt("\t%s Same CRAM mode =%d\n",__FUNCTION__, currMode);
	   return(0);
	   }
	 */
	akdbgprt("%s ACRAM mode =%d\n",__FUNCTION__, currMode);

	ret = ak7755_firmware_write_ram(RAMTYPE_ACRAM, currMode);
	if ( ret != 0 ) return(-1);

	ak7755_data->DSPAcramMode =currMode;

	return(0);
}

#ifdef  AK7755_CRAM_BASIC_COTROL
/*
   static const struct soc_enum ak7755_cram_write_enum[] =
   {
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_cram_EQ_Level), ak7755_cram_EQ_Level),
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_cram_HPF_fc), ak7755_cram_HPF_fc),
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_cram_Lim_Rel), ak7755_cram_Lim_Rel),
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ak7755_cram_Lim_Vol), ak7755_cram_Lim_Vol),
   };
 */
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_cram_write_enum0, ak7755_cram_EQ_Level);  // '16/03/25
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_cram_write_enum1, ak7755_cram_HPF_fc);  // '16/03/25
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_cram_write_enum2, ak7755_cram_Lim_Rel);  // '16/03/25
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_cram_write_enum3, ak7755_cram_Lim_Vol);  // '16/03/25


static int ak7755_cram_write_eq_level(
		int addr,
		int level)
{
	u8   bCram[3];
	int  n;
	int  ret;

	n = 0;
	bCram[n++] = (0xFF & (dwEQLevel[level] >> 16));
	bCram[n++] = (0xFF & (dwEQLevel[level] >> 8));
	bCram[n++] = (0xFF & (dwEQLevel[level]));

	ret = ak7755_write_cram(addr, 3, bCram);
	ret = ak7755_write_cram(addr+6, 3, bCram);

	return(ret);

}

static int get_cram_write_eq1(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->EQLevel[0];

	return 0;

}

static int set_cram_write_eq1(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_cram_write_eq_level(AK7755_CRAMADDR_EQ1_LEVEL, (int)currMode);
	if ( ret < 0 ) return(-1);

	ak7755_data->EQLevel[0] =currMode;

	return(0);
}

static int get_cram_write_eq2(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->EQLevel[1];

	return 0;

}

static int set_cram_write_eq2(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_cram_write_eq_level(AK7755_CRAMADDR_EQ2_LEVEL, (int)currMode);
	if ( ret < 0 ) return(-1);

	ak7755_data->EQLevel[1] =currMode;

	return(0);
}

static int get_cram_write_eq3(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->EQLevel[2];

	return 0;

}

static int set_cram_write_eq3(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_cram_write_eq_level(AK7755_CRAMADDR_EQ3_LEVEL, (int)currMode);
	if ( ret < 0 ) return(-1);

	ak7755_data->EQLevel[2] =currMode;

	return(0);
}


static int get_cram_write_eq4(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->EQLevel[3];

	return 0;

}

static int set_cram_write_eq4(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_cram_write_eq_level(AK7755_CRAMADDR_EQ4_LEVEL, (int)currMode);
	if ( ret < 0 ) return(-1);

	ak7755_data->EQLevel[3] =currMode;

	return(0);
}


static int get_cram_write_eq5(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->EQLevel[4];

	return 0;

}

static int set_cram_write_eq5(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_cram_write_eq_level(AK7755_CRAMADDR_EQ5_LEVEL, (int)currMode);
	if ( ret < 0 ) return(-1);

	ak7755_data->EQLevel[4] =currMode;

	return(0);
}

static int get_cram_write_hpf1fc(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->HPFfc[0];

	return 0;

}

static int set_cram_write_hpf1fc(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_write_cram(AK7755_CRAMADDR_HPF1, 15, HPFCoefTab[currMode]);
	if ( ret < 0 ) return(-1);

	ak7755_data->HPFfc[0] =currMode;

	return(0);
}

static int get_cram_write_hpf2fc(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->HPFfc[1];

	return 0;

}

static int set_cram_write_hpf2fc(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_write_cram(AK7755_CRAMADDR_HPF2, 15, HPFCoefTab[currMode]);
	if ( ret < 0 ) return(-1);

	ak7755_data->HPFfc[1] =currMode;

	return(0);
}

static int get_cram_write_limrel(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->LimRel;

	return 0;

}

static int set_cram_write_limrel(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;

	ret = ak7755_write_cram(AK7755_CRAMADDR_LIM_REL, 6, LIMRelTab[currMode]);
	if ( ret < 0 ) return(-1);

	ak7755_data->LimRel =currMode;

	return(0);
}


static int get_cram_write_limvol(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = ak7755_data->LimVol;

	return 0;

}

static int set_cram_write_limvol(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    ret;
	int    limlevel, nVolLim;
	u8     bMixVolLIM[3];
	u8     bMixVolHPF[3];

	bMixVolHPF[1] = 0;
	bMixVolHPF[2] = 0;
	bMixVolLIM[1] = 0;
	bMixVolLIM[2] = 0;
	if ( currMode == 0 ) {
		limlevel = 0;
		bMixVolLIM[0] = 0;
		bMixVolHPF[0] = 0x08;
	}
	else {
		limlevel = currMode - 1;
		bMixVolHPF[0] = 0;
		nVolLim = (0x08 << currMode);
		bMixVolLIM[0] = (0xFF & nVolLim);
	}

	ret = ak7755_write_cram(AK7755_CRAMADDR_LIM_LEVEL, 3, LIMLevelTab[limlevel]);
	if ( ret < 0 ) return(-1);

	ret = ak7755_write_cram(AK7755_CRAMADDR_MIX_LIM_L, 3, bMixVolLIM);
	if ( ret < 0 ) return(-1);
	ret = ak7755_write_cram(AK7755_CRAMADDR_MIX_HPF_L, 3, bMixVolHPF);
	if ( ret < 0 ) return(-1);
	ret = ak7755_write_cram(AK7755_CRAMADDR_MIX_LIM_R, 3, bMixVolLIM);
	if ( ret < 0 ) return(-1);
	ret = ak7755_write_cram(AK7755_CRAMADDR_MIX_HPF_R, 3, bMixVolHPF);
	if ( ret < 0 ) return(-1);

	ak7755_data->LimVol =currMode;

	return(0);
}
#endif

static int ak7755_reads(u8 *, size_t, u8 *, size_t);
static unsigned int ak7755_i2c_read(u8 *, int, u8 *, int);
#if 0
static int ak7755_reg_write(struct snd_soc_codec *codec, unsigned int reg,unsigned int value)	//16/05/20
{
	int ret = -1;
	unsigned char tx[2];
	size_t	len = 2;
	akdbgprt("[AK7755] %s address = %02X Data = %02X\n",__FUNCTION__, (int)reg, (int)value);

	tx[0] = reg;
	tx[1] = value;

	if (ak7755_data->control_type == SND_SOC_SPI) {
		ret = spi_write(ak7755_data->spi, tx, len );
	}else{
		ret = i2c_master_send(ak7755_data->i2c,tx, len );
	}

	if( ret != len ){
		return -EIO;
	}

	return 0;

}
#endif
unsigned int ak7755_reg_read(struct snd_soc_codec *codec, unsigned int reg)
{
	unsigned char tx[1], rx[1];
	int	wlen, rlen;
	int ret;
	unsigned int rdata;

	wlen = 1;
	rlen = 1;
	tx[0] = (unsigned char)(0x7F & reg);

	if (ak7755_data->control_type == SND_SOC_SPI) {
		ret = spi_write_then_read(ak7755_data->spi, tx, wlen, rx, rlen);
	}
	else {
		ret = ak7755_i2c_read(tx, wlen, rx, rlen);
	}

	if (ret < 0) {
		akdbgprt("[AK7755] %s error ret = %d\n",__FUNCTION__, ret);
		rdata = -EIO;
	}
	else {
		rdata = (unsigned int)rx[0];
	}
	akdbgprt("\t[AK7755] %s addr, read data =(%x, %x)\n",__FUNCTION__, reg, (int)rdata);

	return rdata;
}

#ifdef AK7755_DEBUG  // '16/09/27 move

static int test_read_ram(int mode)
{
	u8	tx[3], rx[512];
	int i, n, plen, clen;

	akdbgprt("[AK7755] %s(%d)\n",__FUNCTION__,__LINE__);

	snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x1, 0x1);  // DLRDY bit = 1
	mdelay(1);
	if ( mode == 1 ) {
		plen = 10;
		for ( n = 0 ; n < (5 * plen) ; n++ ) rx[n] = 0;
		tx[0] = 0x38;
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7755_reads(tx, 3, rx, 5 * plen);
		printk("*****%s PRAM LEN = %d *******\n", __func__, plen);
		n = 0;
		for ( i = 0 ; i < plen ; i ++ ) {
			printk("PAddr=%x %x %x %x %x %x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2], (int)rx[n+3], (int)rx[n+4]);
			n += 5;
		}
	}
	else if ( mode < 5 ) {
		clen = 80;
		for ( n = 0 ; n < (3 * clen) ; n++ ) rx[n] = 0;
		if ( mode == 2 ) tx[0] = 0x34;
		else if ( mode == 3 ) {
			tx[0] = 0x32;
			clen = 16;
		}
		else  tx[0] = 0x3B;
		tx[1] = 0x0;
		tx[2] = 0x0;
		ak7755_reads(tx, 3, rx, 3 * clen);
		printk("*****%s RAM CMD=%d,  LEN = %d*******\n", __func__, (int)tx[0], clen);
		n = 0;
		for ( i = 0 ; i < clen ; i ++ ) {
			printk("CAddr=%x %x %x %x\n", i,(int)rx[n], (int)rx[n+1], (int)rx[n+2]);
			n += 3;
		}
	}
	mdelay(1);
	snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x1, 0x0);  // DLRDY bit = 0

	return(0);

}

static const char *test_reg_select[]   =
{
	"read AK7755 Reg 00:24",
	"read PRAM",
	"read CRAM",
	"read OFREG",
	"read ACRAM",
};

/*
   static const struct soc_enum ak7755_enum[] =
   {
   SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
   };
 */
static SOC_ENUM_SINGLE_EXT_DECL(ak7755_test_enum, test_reg_select);  // '16/03/25


static int nTestRegNo = 0;

static int get_test_reg(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = nTestRegNo;

	return 0;
}

static int set_test_reg(
		struct snd_kcontrol       *kcontrol,
		struct snd_ctl_elem_value  *ucontrol)
{
	//    struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);	//16/05/20
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    i, value;
	int	   regs, rege;

	nTestRegNo = currMode;

	if ( currMode == 0 ) {
		regs = 0xC0;
		rege = 0xDE;

		for ( i = regs ; i <= rege ; i++ ){
			value = ak7755_reg_read(codec, i);
			printk("***AK7755 Addr,Reg=(%x, %x)\n", i, value);
		}
	}
	else if ( currMode < 5 ) {
		test_read_ram(currMode);
	}

	return 0;

}
#endif

static const struct snd_kcontrol_new ak7755_snd_controls[] = {
	SOC_SINGLE_TLV("MIC Input Volume L",
			AK7755_D2_MIC_GAIN_SETTING, 0, 0x0F, 0, mgnl_tlv),
	SOC_SINGLE_TLV("MIC Input Volume R",
			AK7755_D2_MIC_GAIN_SETTING, 4, 0x0F, 0, mgnr_tlv),
	SOC_SINGLE_TLV("Line Out Volume 1",
			AK7755_D4_LO1_LO2_VOLUME_SETTING, 0, 0x0F, 0, lovol1_tlv),
	SOC_SINGLE_TLV("Line Out Volume 2",
			AK7755_D4_LO1_LO2_VOLUME_SETTING, 4, 0x0F, 0, lovol2_tlv),
	SOC_SINGLE_TLV("Line Out Volume 3",
			AK7755_D3_LIN_LO3_VOLUME_SETTING, 0, 0x0F, 0, lovol3_tlv),
	SOC_ENUM_EXT("Line Input Volume", ak7755_linein_enum, get_linein, set_linein),  // 16/03/25 ak7755_linein_enum[0] => ak7755_linein_enum
	SOC_SINGLE_TLV("ADC Digital Volume L",
			AK7755_D5_ADC_DVOLUME_SETTING1, 0, 0xFF, 1, voladl_tlv),
	SOC_SINGLE_TLV("ADC Digital Volume R",
			AK7755_D6_ADC_DVOLUME_SETTING2, 0, 0xFF, 1, voladr_tlv),
	SOC_SINGLE_TLV("ADC2 Digital Volume L",
			AK7755_D7_ADC2_DVOLUME_SETTING1, 0, 0xFF, 1, volad2l_tlv),
	SOC_SINGLE_TLV("ADC2 Digital Volume R",
			AK7755_DD_ADC2_DVOLUME_SETTING2, 0, 0xFF, 1, volad2r_tlv),
	SOC_SINGLE_TLV("DAC Digital Volume L",
			AK7755_D8_DAC_DVOLUME_SETTING1, 0, 0xFF, 1, voldal_tlv),
	SOC_SINGLE_TLV("DAC Digital Volume R",
			AK7755_D9_DAC_DVOLUME_SETTING2, 0, 0xFF, 1, voldar_tlv),

	SOC_SINGLE("ADC Mute", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 7, 1, 0),
	SOC_SINGLE("ADC2 Mute", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 6, 1, 0),
	SOC_SINGLE("DAC Mute", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 5, 1, 0),
	SOC_SINGLE("Analog DRC Lch", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 2, 1, 0),
	SOC_SINGLE("Analog DRC Rch", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 3, 1, 0),
	SOC_SINGLE("MICGAIN Lch Zero-cross", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0, 1, 0),
	SOC_SINGLE("MICGAIN Rch Zero-cross", AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 1, 1, 0),

	SOC_ENUM("DAC De-emphasis", ak7755_set_enum[10]),

	SOC_SINGLE("JX0 Enable", AK7755_C2_SERIAL_DATA_FORMAT, 0, 1, 0),
	SOC_SINGLE("JX1 Enable", AK7755_C2_SERIAL_DATA_FORMAT, 1, 1, 0),
	SOC_SINGLE("JX0 Format", AK7755_C2_SERIAL_DATA_FORMAT, 4, 1, 0),
	SOC_SINGLE("JX2 Enable", AK7755_C1_CLOCK_SETTING2, 0, 1, 0),
	SOC_SINGLE("JX3 Enable", AK7755_C5_ACCELARETOR_SETTING, 6, 1, 0),

	SOC_ENUM("DLRAM Mode(Bank1:Bank0)", ak7755_set_enum[0]),
	SOC_ENUM("DRAM Size(Bank1:Bank0)", ak7755_set_enum[1]),
	SOC_ENUM("DRAM Addressing Mode(Bank1:Bank0)", ak7755_set_enum[2]),
	SOC_ENUM("POMODE DLRAM Pointer 0", ak7755_set_enum[3]),
	SOC_ENUM("CRAM Memory Assignment", ak7755_set_enum[4]),
	SOC_ENUM("FIRMODE1 Accelerator Ch1", ak7755_set_enum[5]),
	SOC_ENUM("FIRMODE2 Accelerator Ch2", ak7755_set_enum[6]),
	SOC_ENUM("SUBMODE1 Accelerator Ch1", ak7755_set_enum[7]),
	SOC_ENUM("SUBMODE2 Accelerator Ch2", ak7755_set_enum[8]),
	SOC_ENUM("Accelerator Memory(ch1:ch2)", ak7755_set_enum[9]),
	SOC_ENUM("CLKO pin", ak7755_set_enum[11]),
	SOC_ENUM("CLKO Output Clock", ak7755_set_enum[12]),

	SOC_ENUM_EXT("BICK fs", ak7755_bick_enum, get_bickfs, set_bickfs),     // '16/03/25  ak7755_bick_enum[0] => ak7755_bick_enum

	// Added for AK7755
	SOC_ENUM_EXT("DSP Firmware PRAM", ak7755_firmware_enum0, get_DSP_write_pram, set_DSP_write_pram),    // '16/03/25
	SOC_ENUM_EXT("DSP Firmware CRAM", ak7755_firmware_enum1, get_DSP_write_cram, set_DSP_write_cram),    // '16/03/25
	SOC_ENUM_EXT("DSP Firmware OFREG", ak7755_firmware_enum2, get_DSP_write_ofreg, set_DSP_write_ofreg), // '16/03/25
	SOC_ENUM_EXT("DSP Firmware ACRAM", ak7755_firmware_enum3, get_DSP_write_acram, set_DSP_write_acram), // '16/03/25

#ifdef AK7755_CRAM_BASIC_COTROL
	SOC_ENUM_EXT("CRAM EQ1 Level", ak7755_cram_write_enum0, get_cram_write_eq1, set_cram_write_eq1),    // '16/03/25
	SOC_ENUM_EXT("CRAM EQ2 Level", ak7755_cram_write_enum0, get_cram_write_eq2, set_cram_write_eq2),    // '16/03/25
	SOC_ENUM_EXT("CRAM EQ3 Level", ak7755_cram_write_enum0, get_cram_write_eq3, set_cram_write_eq3),    // '16/03/25
	SOC_ENUM_EXT("CRAM EQ4 Level", ak7755_cram_write_enum0, get_cram_write_eq4, set_cram_write_eq4),    // '16/03/25
	SOC_ENUM_EXT("CRAM EQ5 Level", ak7755_cram_write_enum0, get_cram_write_eq5, set_cram_write_eq5),    // '16/03/25
	SOC_ENUM_EXT("CRAM HPF1 fc", ak7755_cram_write_enum1, get_cram_write_hpf1fc, set_cram_write_hpf1fc),  // '16/03/25
	SOC_ENUM_EXT("CRAM HPF2 fc", ak7755_cram_write_enum1, get_cram_write_hpf2fc, set_cram_write_hpf2fc),  // '16/03/25
	SOC_ENUM_EXT("CRAM Limiter Release Time", ak7755_cram_write_enum2, get_cram_write_limrel, set_cram_write_limrel), // '16/03/25
	SOC_ENUM_EXT("CRAM Limiter Volume", ak7755_cram_write_enum3, get_cram_write_limvol, set_cram_write_limvol),  // '16/03/25
#endif

#ifdef AK7755_DEBUG
	SOC_ENUM_EXT("Reg Read", ak7755_test_enum, get_test_reg, set_test_reg),  // '16/03/25
#endif

	SOC_ENUM_EXT("SELMIX2-0", ak7755_selmix_enum, get_selmix, set_selmix),	//SELMIX2-0 bit setting // '16/03/25

};


/* Clock Event for DAC, ADC */
static int ak7755_clkset_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm); // 170619 for 4.4.XX

	akdbgprt("[AK7755] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (event) {
		case SND_SOC_DAPM_POST_PMU:	/* after widget power up */
			snd_soc_update_bits(codec, AK7755_CF_RESET_POWER_SETTING, 0x08,0x08);	//CRESETN(CODEC ResetN)=1
			break;
	}

	return 0;
}

/* Clock Event */
static int ak7755_clock_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event) //CONFIG_LINF
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm); // 170619 for 4.4.XX

	akdbgprt("[AK7755] %s(%d)\n",__FUNCTION__,__LINE__);

	switch (event) {
		case SND_SOC_DAPM_POST_PMU:	/* after widget power up */
			akdbgprt("[AK7755] %s wait=10msec\n",__FUNCTION__);
			mdelay(10);
			break;
		case SND_SOC_DAPM_PRE_PMD:	/* before widget power down */
			snd_soc_update_bits(codec, AK7755_CF_RESET_POWER_SETTING, 0x08,0x00);	//CODEC Reset
			break;
	}

	return 0;
}

/* SDOUT 1 switch */
static const struct snd_kcontrol_new ak7755_out1e_control =
SOC_DAPM_SINGLE("Switch", AK7755_CA_CLK_SDOUT_SETTING, 0, 1, 0);

/* SDOUT 2 switch */
static const struct snd_kcontrol_new ak7755_out2e_control =
SOC_DAPM_SINGLE("Switch", AK7755_CA_CLK_SDOUT_SETTING, 1, 1, 0);

/* SDOUT 3 switch */
static const struct snd_kcontrol_new ak7755_out3e_control =
SOC_DAPM_SINGLE("Switch", AK7755_CA_CLK_SDOUT_SETTING, 2, 1, 0);

/* LineOut1 Switch */
static const char *ak7755_pmlo1_select_texts[] =
{"Off", "On"};

// '15/10/23
static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_pmlo1_mux_enum, ak7755_pmlo1_select_texts);

static const struct snd_kcontrol_new ak7755_pmlo1_mux_control =
SOC_DAPM_ENUM("OUT1 SW", ak7755_pmlo1_mux_enum);
// *****


/* LineOut2 Switch */
static const char *ak7755_pmlo2_select_texts[] =
{"Off", "On"};

// '15/10/23
static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_pmlo2_mux_enum, ak7755_pmlo2_select_texts);

static const struct snd_kcontrol_new ak7755_pmlo2_mux_control =
SOC_DAPM_ENUM("OUT2 SW", ak7755_pmlo2_mux_enum);
// *****

/* LineOut3 Mixer */
static const struct snd_kcontrol_new ak7755_lo3sw_mixer_controls[] = {
	SOC_DAPM_SINGLE("LOSW1", AK7755_C9_ANALOG_IO_SETTING, 1, 1, 0),
	SOC_DAPM_SINGLE("LOSW2", AK7755_C9_ANALOG_IO_SETTING, 2, 1, 0),
#ifndef DIGITAL_MIC		//LOSW3 used only Analog MIC
	SOC_DAPM_SINGLE("LOSW3", AK7755_C9_ANALOG_IO_SETTING, 3, 1, 0),
#endif
};

/* DSPIn Virt SWITCH */
static const char *ak7755_dspinad_texts[] =
{"Off", "On"};

// '15/10/23
static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_dspinad_enum, ak7755_dspinad_texts);

static const struct snd_kcontrol_new ak7755_dspinad_control =
SOC_DAPM_ENUM("DSPINAD Switch", ak7755_dspinad_enum);

static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_dspinad2_enum, ak7755_dspinad_texts);

static const struct snd_kcontrol_new ak7755_dspinad2_control =
SOC_DAPM_ENUM("DSPINAD2 Switch", ak7755_dspinad2_enum);
// *****


/* LIN MUX */
static const char *ak7755_lin_select_texts[] =
{"IN1", "IN2", "INPN1"};

static const struct soc_enum ak7755_lin_mux_enum =
SOC_ENUM_SINGLE(AK7755_C9_ANALOG_IO_SETTING, 4,
		ARRAY_SIZE(ak7755_lin_select_texts), ak7755_lin_select_texts);

static const struct snd_kcontrol_new ak7755_lin_mux_control =
SOC_DAPM_ENUM("LIN Select", ak7755_lin_mux_enum);

/* RIN MUX */
static const char *ak7755_rin_select_texts[] =
{"IN3", "IN4", "INPN2"};

static const struct soc_enum ak7755_rin_mux_enum =
SOC_ENUM_SINGLE(AK7755_C9_ANALOG_IO_SETTING, 6,
		ARRAY_SIZE(ak7755_rin_select_texts), ak7755_rin_select_texts);

static const struct snd_kcontrol_new ak7755_rin_mux_control =
SOC_DAPM_ENUM("RIN Select", ak7755_rin_mux_enum);

/* DAC MUX */
static const char *ak7755_seldai_select_texts[] =
{"DSP", "MIXOUT", "SDIN2", "SDIN1"};

static const struct soc_enum ak7755_seldai_mux_enum =
SOC_ENUM_SINGLE(AK7755_C8_DAC_IN_SETTING, 6,
		ARRAY_SIZE(ak7755_seldai_select_texts), ak7755_seldai_select_texts);

static const struct snd_kcontrol_new ak7755_seldai_mux_control =
SOC_DAPM_ENUM("SELDAI Select", ak7755_seldai_mux_enum);

/* SDOUT1 MUX */
static const char *ak7755_seldo1_select_texts[] =
{"DSP", "DSP GP0", "SDIN1", "SDOUTAD", "EEST", "SDOUTAD2"};

static const struct soc_enum ak7755_seldo1_mux_enum =
SOC_ENUM_SINGLE(AK7755_CC_VOLUME_TRANSITION, 0,
		ARRAY_SIZE(ak7755_seldo1_select_texts), ak7755_seldo1_select_texts);

static const struct snd_kcontrol_new ak7755_seldo1_mux_control =
SOC_DAPM_ENUM("SELDO1 Select", ak7755_seldo1_mux_enum);

/* SDOUT2 MUX */
static const char *ak7755_seldo2_select_texts[] =
{"DSP", "DSP GP1", "SDIN2", "SDOUTAD2"};

static const struct soc_enum ak7755_seldo2_mux_enum =
SOC_ENUM_SINGLE(AK7755_C8_DAC_IN_SETTING, 2,
		ARRAY_SIZE(ak7755_seldo2_select_texts), ak7755_seldo2_select_texts);

static const struct snd_kcontrol_new ak7755_seldo2_mux_control =
SOC_DAPM_ENUM("SELDO2 Select", ak7755_seldo2_mux_enum);

/* SDOUT3 MUX */
static const char *ak7755_seldo3_select_texts[] =
{"DSP DOUT3", "MIXOUT", "DSP DOUT4", "SDOUTAD2"};

static const struct soc_enum ak7755_seldo3_mux_enum =
SOC_ENUM_SINGLE(AK7755_C8_DAC_IN_SETTING, 4,
		ARRAY_SIZE(ak7755_seldo3_select_texts), ak7755_seldo3_select_texts);

static const struct snd_kcontrol_new ak7755_seldo3_mux_control =
SOC_DAPM_ENUM("SELDO3 Select", ak7755_seldo3_mux_enum);

/* SELMIX Virt SWITCH */
static const char *ak7755_selmix_select_texts[] =
{"Off", "On"};

// '15/10/23
static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_sdoutad_selmix_enum, ak7755_selmix_select_texts);
static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_sdoutad2_selmix_enum, ak7755_selmix_select_texts);
static SOC_ENUM_SINGLE_VIRT_DECL(ak7755_dsp_selmix_enum, ak7755_selmix_select_texts);

static const struct snd_kcontrol_new ak7755_sdoutad_selmix_control =
SOC_DAPM_ENUM("SELMIX Switch", ak7755_sdoutad_selmix_enum);

static const struct snd_kcontrol_new ak7755_sdoutad2_selmix_control =
SOC_DAPM_ENUM("SELMIX Switch", ak7755_sdoutad2_selmix_enum);

static const struct snd_kcontrol_new ak7755_dsp_selmix_control =
SOC_DAPM_ENUM("SELMIX Switch", ak7755_dsp_selmix_enum);
// *****

/* ak7755 dapm widgets */
static const struct snd_soc_dapm_widget ak7755_dapm_widgets[] = {

	// ADC, DAC
#ifdef DIGITAL_MIC
	SND_SOC_DAPM_INPUT("DMICIN1"),
	SND_SOC_DAPM_INPUT("DMICIN2"),
	SND_SOC_DAPM_ADC_E("DMIC1 Left",  NULL, AK7755_CE_POWER_MANAGEMENT, 6, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_ADC_E("DMIC1 Right", NULL, AK7755_CE_POWER_MANAGEMENT, 7, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_ADC_E("DMIC2 Left",  NULL, AK7755_CE_POWER_MANAGEMENT, 5, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_ADC_E("DMIC2 Right", NULL, AK7755_CF_RESET_POWER_SETTING, 1, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_SUPPLY("DMIC1 CLK", AK7755_DE_DMIC_IF_SETTING, 5, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("DMIC2 CLK", AK7755_DE_DMIC_IF_SETTING, 2, 0, NULL, 0),
#else
	SND_SOC_DAPM_ADC_E("ADC Left", NULL, AK7755_CE_POWER_MANAGEMENT, 6, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_ADC_E("ADC Right", NULL, AK7755_CE_POWER_MANAGEMENT, 7, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_ADC_E("ADC2 Left", NULL, AK7755_CE_POWER_MANAGEMENT, 5, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
#endif
	SND_SOC_DAPM_DAC_E("DAC Left", NULL, AK7755_CE_POWER_MANAGEMENT, 0, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_DAC_E("DAC Right", NULL, AK7755_CE_POWER_MANAGEMENT, 1, 0,
			ak7755_clkset_event, SND_SOC_DAPM_POST_PMU ),
	SND_SOC_DAPM_SUPPLY("CLOCK", AK7755_C1_CLOCK_SETTING2, 0, 0,
			ak7755_clock_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	// Analog Output
	SND_SOC_DAPM_OUTPUT("Line Out1"),
	SND_SOC_DAPM_OUTPUT("Line Out2"),
	SND_SOC_DAPM_OUTPUT("Line Out3"),

	SND_SOC_DAPM_MIXER("LineOut Amp3 Mixer", AK7755_CE_POWER_MANAGEMENT, 4, 0,
			&ak7755_lo3sw_mixer_controls[0], ARRAY_SIZE(ak7755_lo3sw_mixer_controls)),
	SND_SOC_DAPM_MUX("LineOut Amp2", AK7755_CE_POWER_MANAGEMENT, 3, 0, &ak7755_pmlo2_mux_control),
	SND_SOC_DAPM_MUX("LineOut Amp1", AK7755_CE_POWER_MANAGEMENT, 2, 0, &ak7755_pmlo1_mux_control),

	// Analog Input
	SND_SOC_DAPM_INPUT("LIN"),
	SND_SOC_DAPM_INPUT("IN1"),
	SND_SOC_DAPM_INPUT("IN2"),
	SND_SOC_DAPM_INPUT("IN3"),
	SND_SOC_DAPM_INPUT("IN4"),
	SND_SOC_DAPM_INPUT("INPN1"),
	SND_SOC_DAPM_INPUT("INPN2"),

	SND_SOC_DAPM_PGA("LineIn Amp", AK7755_CF_RESET_POWER_SETTING, 5, 0, NULL, 0),
	SND_SOC_DAPM_MUX("RIN MUX", SND_SOC_NOPM, 0, 0,	&ak7755_rin_mux_control),
	SND_SOC_DAPM_MUX("LIN MUX", SND_SOC_NOPM, 0, 0,	&ak7755_lin_mux_control),

	// '15/10/23
	SND_SOC_DAPM_MUX("DSPIN SDOUTAD2", SND_SOC_NOPM, 0, 0, &ak7755_dspinad2_control),
	SND_SOC_DAPM_MUX("DSPIN SDOUTAD", SND_SOC_NOPM, 0, 0, &ak7755_dspinad_control),
	// *****

	// Digital Input/Output
	SND_SOC_DAPM_AIF_IN("SDIN1", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SDIN2", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT1", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT2", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("SDOUT3", "Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SWITCH("SDOUT3 Enable", SND_SOC_NOPM, 0, 0,
			&ak7755_out3e_control),
	SND_SOC_DAPM_SWITCH("SDOUT2 Enable", SND_SOC_NOPM, 0, 0,
			&ak7755_out2e_control),
	SND_SOC_DAPM_SWITCH("SDOUT1 Enable", SND_SOC_NOPM, 0, 0,
			&ak7755_out1e_control),

	SND_SOC_DAPM_PGA("SDOUTAD", SND_SOC_NOPM, 0, 0, NULL, 0),		//ADC's Output
	SND_SOC_DAPM_PGA("SDOUTAD2", SND_SOC_NOPM, 0, 0, NULL, 0),		//ADC2's Output
	SND_SOC_DAPM_PGA("DSP", AK7755_CF_RESET_POWER_SETTING, 2, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN("DSP GP0", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DSP GP1", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("EEST", NULL, 0, SND_SOC_NOPM, 0, 0),

	// Multiplexer (selects 1 analog signal from many inputs) & Mixer
	SND_SOC_DAPM_MUX("SDOUT3 MUX", SND_SOC_NOPM, 0, 0, &ak7755_seldo3_mux_control),
	SND_SOC_DAPM_MUX("SDOUT2 MUX", SND_SOC_NOPM, 0, 0, &ak7755_seldo2_mux_control),
	SND_SOC_DAPM_MUX("SDOUT1 MUX", SND_SOC_NOPM, 0, 0, &ak7755_seldo1_mux_control),
	SND_SOC_DAPM_MUX("DAC MUX", SND_SOC_NOPM, 0, 0, &ak7755_seldai_mux_control),

	// '15/10/23
	SND_SOC_DAPM_MUX("SELMIX DSP", SND_SOC_NOPM, 0, 0, &ak7755_dsp_selmix_control),
	SND_SOC_DAPM_MUX("SELMIX AD2", SND_SOC_NOPM, 0, 0, &ak7755_sdoutad2_selmix_control),
	SND_SOC_DAPM_MUX("SELMIX AD", SND_SOC_NOPM, 0, 0, &ak7755_sdoutad_selmix_control),
	// *****

	SND_SOC_DAPM_PGA("SELMIX Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),

};

static const struct snd_soc_dapm_route ak7755_intercon[] =
{

#ifdef DIGITAL_MIC
	{"DMIC1 Left", NULL, "CLOCK"},
	{"DMIC1 Right", NULL, "CLOCK"},
	{"DMIC2 Left", NULL, "CLOCK"},
	{"DMIC2 Right", NULL, "CLOCK"},

	{"DMICIN1", NULL, "DMIC1 CLK"},
	{"DMICIN2", NULL, "DMIC2 CLK"},
	{"DMIC1 Left", NULL, "DMICIN1"},
	{"DMIC1 Right", NULL, "DMICIN1"},
	{"DMIC2 Left", NULL, "DMICIN2"},
	{"DMIC2 Right", NULL, "DMICIN2"},
	{"SDOUTAD", NULL, "DMIC1 Left"},
	{"SDOUTAD", NULL, "DMIC1 Right"},
	{"SDOUTAD2", NULL, "DMIC2 Left"},
	{"SDOUTAD2", NULL, "DMIC2 Right"},
#else
	{"ADC Left", NULL, "CLOCK"},
	{"ADC Right", NULL, "CLOCK"},
	{"ADC2 Left", NULL, "CLOCK"},

	{"LineIn Amp", NULL, "LIN"},
	{"ADC2 Left", NULL, "LineIn Amp"},
	{"SDOUTAD2", NULL, "ADC2 Left"},

	{"LIN MUX", "IN1", "IN1"},
	{"LIN MUX", "IN2", "IN2"},
	{"LIN MUX", "INPN1", "INPN1"},
	{"RIN MUX", "IN3", "IN3"},
	{"RIN MUX", "IN4", "IN4"},
	{"RIN MUX", "INPN2", "INPN2"},
	{"ADC Left", NULL, "LIN MUX"},
	{"ADC Right", NULL, "RIN MUX"},
	{"SDOUTAD", NULL, "ADC Left"},
	{"SDOUTAD", NULL, "ADC Right"},
#endif

	{"DAC Left", NULL, "CLOCK"},
	{"DAC Right", NULL, "CLOCK"},
	{"DSP", NULL, "CLOCK"},

	{"DSP", NULL, "SDIN1"},
	{"DSP", NULL, "SDIN2"},

	{"DSPIN SDOUTAD", "On", "SDOUTAD"},
	{"DSPIN SDOUTAD2", "On", "SDOUTAD2"},
	{"DSP", NULL, "DSPIN SDOUTAD"},
	{"DSP", NULL, "DSPIN SDOUTAD2"},

	{"SELMIX AD", "On", "SDOUTAD"},
	{"SELMIX AD2", "On", "SDOUTAD2"},
	{"SELMIX DSP", "On", "DSP"},
	{"SELMIX Mixer", NULL, "SELMIX AD"},
	{"SELMIX Mixer", NULL, "SELMIX AD2"},
	{"SELMIX Mixer", NULL, "SELMIX DSP"},
	{"DAC MUX", "MIXOUT", "SELMIX Mixer"},
	{"DAC MUX", "DSP", "DSP"},
	{"DAC MUX", "SDIN2", "SDIN2"},
	{"DAC MUX", "SDIN1", "SDIN1"},

	{"DAC Left", NULL, "DAC MUX"},
	{"DAC Right", NULL, "DAC MUX"},

	{"LineOut Amp1", "On", "DAC Left"},
	{"LineOut Amp2", "On", "DAC Right"},
	{"Line Out1", NULL, "LineOut Amp1"},
	{"Line Out2", NULL, "LineOut Amp2"},
	{"LineOut Amp3 Mixer", "LOSW1", "DAC Left"},
	{"LineOut Amp3 Mixer", "LOSW2", "DAC Right"},
#ifndef DIGITAL_MIC		//LOSW3 used only Analog MIC
	{"LineOut Amp3 Mixer", "LOSW3", "LineIn Amp"},
#endif
	{"Line Out3", NULL, "LineOut Amp3 Mixer"},

	{"SDOUT1 MUX", "DSP", "DSP"},
	{"SDOUT1 MUX", "DSP GP0", "DSP GP0"},
	{"SDOUT1 MUX", "SDIN1", "SDIN1"},
	{"SDOUT1 MUX", "SDOUTAD", "SDOUTAD"},
	{"SDOUT1 MUX", "EEST", "EEST"},
	{"SDOUT1 MUX", "SDOUTAD2", "SDOUTAD2"},

	{"SDOUT2 MUX", "DSP", "DSP"},
	{"SDOUT2 MUX", "DSP GP1", "DSP GP1"},
	{"SDOUT2 MUX", "SDIN2", "SDIN2"},
	{"SDOUT2 MUX", "SDOUTAD2", "SDOUTAD2"},

	{"SDOUT3 MUX", "DSP DOUT3", "DSP"},
	{"SDOUT3 MUX", "MIXOUT", "DAC MUX"},
	{"SDOUT3 MUX", "DSP DOUT4", "DSP"},
	{"SDOUT3 MUX", "SDOUTAD2", "SDOUTAD2"},

	{"SDOUT1 Enable", "Switch", "SDOUT1 MUX"},
	{"SDOUT2 Enable", "Switch", "SDOUT2 MUX"},
	{"SDOUT3 Enable", "Switch", "SDOUT3 MUX"},

	{"SDOUT1", NULL, "SDOUT1 Enable"},
	{"SDOUT2", NULL, "SDOUT2 Enable"},
	{"SDOUT3", NULL, "SDOUT3 Enable"},

};

static int ak7755_hw_params_set(struct snd_soc_codec *codec, int nfs)
{
	u8 	fs;
	//	int ret;
	akdbgprt("[AK7755] %s nfs:%d\n",__FUNCTION__,nfs);
	fs = snd_soc_read(codec, AK7755_C0_CLOCK_SETTING1);
	fs &= ~AK7755_FS;

	switch (nfs) {
		case 8000:
			fs |= AK7755_FS_8KHZ;
			break;
		case 11025:
			fs |= AK7755_FS_12KHZ;
			break;
		case 16000:
			fs |= AK7755_FS_16KHZ;
			break;
		case 22050:
			fs |= AK7755_FS_24KHZ;
			break;
		case 32000:
			fs |= AK7755_FS_32KHZ;
			break;
		case 44100:
		case 48000:
			fs |= AK7755_FS_48KHZ;
			break;
		case 88200:
		case 96000:
			fs |= AK7755_FS_96KHZ;
			break;
		default:
			return -EINVAL;
	}
	snd_soc_write(codec, AK7755_C0_CLOCK_SETTING1, fs);
	snd_soc_write(codec, 0xC2, 0x10);
	return 0;
}

static int ak7755_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	ak7755_data->fs = params_rate(params);

	ak7755_hw_params_set(codec, ak7755_data->fs);

	return 0;
}

static int ak7755_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
		unsigned int freq, int dir)
{
	//	struct snd_soc_codec *codec = dai->codec;

	akdbgprt("[AK7755] %s():freq = %d\n",__FUNCTION__,freq);

	ak7755_data->rclk = freq;

	return 0;
}

static int ak7755_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{

	struct snd_soc_codec *codec = dai->codec;
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);  // '15/06/15
	u8 mode, mode2;
	u8 format;

	akdbgprt("[AK7755] %s():fmt = %d\n",__FUNCTION__,fmt);

	/* set master/slave audio interface */
	mode = snd_soc_read(codec, AK7755_C0_CLOCK_SETTING1);		//CKM2-0(M/S)
	mode &= ~AK7755_M_S;
	mode2 = snd_soc_read(codec, AK7755_CA_CLK_SDOUT_SETTING);	//BICKOE,LRCKOE
	format = snd_soc_read(codec, AK7755_C2_SERIAL_DATA_FORMAT);
	format &= ~AK7755_LRIF;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			akdbgprt("[AK7755] %s(Slave)\n",__FUNCTION__);
#ifdef CLOCK_MODE_BICK
			mode |= AK7755_M_S_3;			//CKM mode = 3(Slave, BICK)
#else
			mode |= AK7755_M_S_2;			//CKM mode = 2(Slave, XTI=12.288MHz)
#endif
			//          mode2 &= ~AK7755_BICK_LRCK;		//BICK = LRCK = 0
			break;
		case SND_SOC_DAIFMT_CBM_CFM:
			akdbgprt("\t[AK7755] %s(Master)\n",__FUNCTION__);
#ifdef CLOCK_MODE_18_432
			mode |= AK7755_M_S_1;		//CKM mode = 1(Master, XTI=18.432MHz)
#else
			mode |= AK7755_M_S_0;		//CKM mode = 0(Master, XTI=12.288MHz)
#endif
			mode2 |= AK7755_BICK_LRCK;	//BICK = LRCK = 1
			break;
		case SND_SOC_DAIFMT_CBS_CFM:
		case SND_SOC_DAIFMT_CBM_CFS:
		default:
			dev_err(codec->dev, "Clock mode unsupported");
			return -EINVAL;
	}

	// '15/06/15
	snd_soc_update_bits(codec, AK7755_C1_CLOCK_SETTING2, 0x30, (ak7755->bickfs << 4));
	if ( ak7755->bickfs < 3 ) format &= 0x7F;
	else {
		format |= 0x80;
		format &= 0xF3;
		format |=  AK7755_TDM_INPUT_SOURCE;
	}
	// *****

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			format |= AK7755_LRIF_I2S_MODE;
			// '15/06/15
			snd_soc_update_bits(codec, AK7755_C3_DELAY_RAM_DSP_IO, 0xF0, 0x0); 	// DIF2, DOF2
			snd_soc_update_bits(codec, AK7755_C6_DAC_DEM_SETTING, 0x37, 0x0);		//DIF, DIFDA
			snd_soc_update_bits(codec, AK7755_C7_DSP_OUT_SETTING, 0xFF, 0x0); 	//DOF
			// *****
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			format |= AK7755_LRIF_MSB_MODE;
			// '15/06/15
			if ( ak7755->bickfs == 2 ) {   // 32fs
				snd_soc_update_bits(codec, AK7755_C3_DELAY_RAM_DSP_IO, 0xF0, 0xF0); 	// DIF2, DOF2
				snd_soc_update_bits(codec, AK7755_C6_DAC_DEM_SETTING, 0x37, 0x33);	//DIF, DIFDA
				snd_soc_update_bits(codec, AK7755_C7_DSP_OUT_SETTING, 0xFF, 0xF3); 	//DOF
			}
			else {
				snd_soc_update_bits(codec, AK7755_C3_DELAY_RAM_DSP_IO, 0xF0, 0x0); 	// DIF2, DOF2
				snd_soc_update_bits(codec, AK7755_C6_DAC_DEM_SETTING, 0x37, 0x0);	//DIF, DIFDA
				snd_soc_update_bits(codec, AK7755_C7_DSP_OUT_SETTING, 0xFF, 0x0); 	//DOF
			}
			// *****
			break;
			/*
			   case SND_SOC_DAIFMT_PCM_SHORT:
			   format &= 0xBF;							//BCKP Clear
			   format |= (AK7755_BCKP_BIT << 6);		//BCKP set
			   format |= AK7755_LRIF_PCM_SHORT_MODE;
			   break;
			   case SND_SOC_DAIFMT_PCM_LONG:
			   format &= 0xBF;							//BCKP lear
			   format |= (AK7755_BCKP_BIT << 6);		//BCKP set
			   format |= AK7755_LRIF_PCM_LONG_MODE;
			   break;
			 */
		default:
			return -EINVAL;
	}

	/* set format */
	snd_soc_write(codec, AK7755_C0_CLOCK_SETTING1, mode);
	snd_soc_write(codec, AK7755_CA_CLK_SDOUT_SETTING, mode2);
	snd_soc_write(codec, AK7755_C2_SERIAL_DATA_FORMAT, format);

	return 0;
}


static unsigned int ak7755_i2c_read(
		u8 *reg,
		int reglen,
		u8 *data,
		int datalen)
{
	struct i2c_msg xfer[2];
	int ret;
	struct i2c_client *client = ak7755_data->i2c;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	//	akdbgprt("*****[AK7755] %s (%x,%x)\n",__FUNCTION__, (int)reg[0], (int)data[0]);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return -ret;
	else
		return -EIO;
}

static int ak7755_reads(u8 *tx, size_t wlen, u8 *rx, size_t rlen)
{
	int ret;

	akdbgprt("[AK7755] %s tx[0]=%x, %d, %d\n",__FUNCTION__, tx[0], (int)wlen, (int)rlen);
	if (ak7755_data->control_type == SND_SOC_SPI) {
		ret = spi_write_then_read(ak7755_data->spi, tx, wlen, rx, rlen);
	}
	else {
		ret = ak7755_i2c_read( tx, wlen, rx, rlen);
	}

	return ret;

}

static inline void ak7755_write_reg_cache(
		struct snd_soc_codec *codec,
		u16 reg,
		u16 value)
{
	u8 *cache = codec->reg_cache;
	BUG_ON(reg > ARRAY_SIZE(ak7755_reg));
	cache[reg] = (u8)value;
}


static int ak7755_writes(const u8 *tx, size_t wlen)
{
	int rc;

	akdbgprt("[AK7755W] %s tx[0]=%x tx[1]=%x, len=%d\n",__FUNCTION__, (int)tx[0], (int)tx[1], (int)wlen);

	if (ak7755_data->control_type == SND_SOC_SPI)
		rc = spi_write(ak7755_data->spi, tx, wlen);
	else
		rc = i2c_master_send(ak7755_data->i2c, tx, wlen);

	return rc;
}

/*
 * Write with Mask to  AK7755 register space
 */
// ak7755_writeMask() => snd_soc_update_bits()  // '15/10/23


static int crc_read(void)
{
	int rc;
	u8	tx[1], rx[2];

	tx[0] = CRC_COMMAND_READ_RESULT;

	rc =  ak7755_reads(tx, 1, rx, 2);

	return (rc < 0) ? rc : ((rx[0] << 8) + rx[1]);
}

static int ak7755_set_status(enum ak7755_status status)
{

	switch (status) {
		case RUN:
			snd_soc_update_bits(ak7755_data->codec, AK7755_C1_CLOCK_SETTING2, 0x1, 0x1);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x5, 0x4);  // CRESETN bit = DSPRESETN = 1;
			break;
		case DOWNLOAD:
			snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x5, 0x1);  // DLRDY bit = 1
			mdelay(1);
			break;
		case STANDBY:
			snd_soc_update_bits(ak7755_data->codec, AK7755_C1_CLOCK_SETTING2, 0x1, 0x1);  // CKRESETN bit = 1
			mdelay(10);
			snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x5, 0x0);  // CRESETN bit = DSPRESETN = 0;
			break;
		case SUSPEND:
		case POWERDOWN:
			snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0xD, 0x0);
			snd_soc_update_bits(ak7755_data->codec, AK7755_CE_POWER_MANAGEMENT, 0xFF, 0x0);
			snd_soc_update_bits(ak7755_data->codec, AK7755_C1_CLOCK_SETTING2, 0x0, 0x0);
			break;
		default:
			return -EINVAL;
	}

	ak7755_data->status = status;

	return 0;
}

static int ak7755_ram_download(const u8 *tx_ram, u64 num, u16 crc)
{
	int rc;
	int nDSPRun;
	u16	read_crc;

	akdbgprt("[AK7755] %s num=%ld\n",__FUNCTION__, (long int)num);

	nDSPRun = snd_soc_read(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING);
	akdbgprt("[AK7755] %s nDSPRun=%d tx_ram=0x%02x\n",__FUNCTION__,nDSPRun,*tx_ram);


	ak7755_set_status(DOWNLOAD);

	rc = ak7755_writes(tx_ram, num);
	if (rc < 0) {
		//		ak7755_set_status(STANDBY);
		snd_soc_write(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, nDSPRun);
		return rc;
	}

	if ( ( crc != 0 ) && (rc >= 0) )  {
		read_crc = crc_read();
		akdbgprt("[AK7755] %s CRC Cal=%x Read=%x\n",__FUNCTION__, (int)crc,(int)read_crc);

		if ( read_crc == crc ) rc = 0;
		else rc = 1;
	}

	//	ak7755_set_status(STANDBY);
	snd_soc_write(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, nDSPRun);

	return rc;

}

static int calc_CRC(int length, u8 *data )
{

#define CRC16_CCITT (0x1021)

	unsigned short crc = 0x0000;
	int i, j;

	for ( i = 0; i < length; i++ ) {
		crc ^= *data++ << 8;
		for ( j = 0; j < 8; j++) {
			if ( crc & 0x8000) {
				crc <<= 1;
				crc ^= CRC16_CCITT;
			}
			else {
				crc <<= 1;
			}
		}
	}

	akdbgprt("[AK7755] %s CRC=%x\n",__FUNCTION__, crc);

	return crc;
}

static void ak7755_download_reg_config(void)
{
	clk_disable_unprepare(ak7755_data->mclk);// 关闭主时钟 BICK也会关闭
	msleep(500);
	//set CKRESETN to 0
	snd_soc_update_bits(ak7755_data->codec, AK7755_C1_CLOCK_SETTING2, 0x1, 0x0);  //0XC1
	msleep(100);
	//set CRESETN
	snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x08,0x00); //0XCF
	msleep(100);
	//set DSPRESETN
	snd_soc_update_bits(ak7755_data->codec, AK7755_CD_STO_DLS_SETTING, 0x08,0x00);  //0XCD
	msleep(100);
}

static int ak7755_running_reg_config(void)
{
	int ret;
	ret = clk_prepare_enable(ak7755_data->mclk);
	if (ret){
		dev_err(&ak7755_data->i2c->dev, "Failed to clk_prepare_enable: %d\n", ret);
		return ret;
	}
	msleep(100);
	snd_soc_update_bits(ak7755_data->codec, AK7755_C1_CLOCK_SETTING2, 0x1, 0x1);
	msleep(10);
	snd_soc_update_bits(ak7755_data->codec, AK7755_CD_STO_DLS_SETTING, 0x08,0x08);
	//set CRESETN
	snd_soc_update_bits(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, 0x08,0x08); //0XCF

	return 0;
}


static int ak7755_write_ram(
		int	 nPCRam,  // 0 : PRAM, 1 : CRAM, 2:  OFREG. 3: ACRAM
		u8 	*upRam,
		int	 nWSize)
{
	int n, ret;
	int	wCRC;

	akdbgprt("[AK7755] %s RamNo=%d, len=%d\n",__FUNCTION__, nPCRam, nWSize);

	switch(nPCRam) {
		case RAMTYPE_PRAM:
			if (  nWSize > TOTAL_NUM_OF_PRAM_MAX ) {
				printk("%s: PRAM Write size is over! \n", __func__);
				return(-1);
			}
			break;
		case RAMTYPE_CRAM:
			if (  nWSize > TOTAL_NUM_OF_CRAM_MAX ) {
				printk("%s: CRAM Write size is over! \n", __func__);
				return(-1);
			}
			break;
		case RAMTYPE_OFREG:
			if (  nWSize > TOTAL_NUM_OF_OFREG_MAX ) {
				printk("%s: OFREG Write size is over! \n", __func__);
				return(-1);
			}
			break;
		case RAMTYPE_ACRAM:
			if (  nWSize > TOTAL_NUM_OF_ACRAM_MAX ) {
				printk("%s: ACRAM Write size is over! \n", __func__);
				return(-1);
			}
			break;
		default:
			break;
	}

	wCRC = calc_CRC(nWSize, upRam);

	n = MAX_LOOP_TIMES;
	do {
		ret = ak7755_ram_download(upRam, nWSize, wCRC);
		if ( ret >= 0 ) break;
		n--;
	} while ( n > 0 );

	if ( ret < 0 ) {
		printk("%s: RAM Write Error! RAM No = %d \n", __func__, nPCRam);
		return(-1);
	}


	return(0);

}

static int ak7755_config_dsp_route(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AK7755_D4_LO1_LO2_VOLUME_SETTING, 0xAA); 		//LOVOL1,2=F(0dB)
	snd_soc_write(codec, AK7755_D3_LIN_LO3_VOLUME_SETTING, 0x0F); 		//LOVOL3=F(0dB)
	snd_soc_write(codec, AK7755_D0_FUNCTION_SETTING, 0x40); 			//CRCE=1(CRC Enable)

	//	snd_soc_update_bits(codec, AK7755_C2_SERIAL_DATA_FORMAT, 0x40, AK7755_BCKP_BIT);	//BCKP bit
	snd_soc_update_bits(codec, AK7755_D0_FUNCTION_SETTING, 0x10, AK7755_SOCFG_BIT); //SOFG bit
#ifdef DIGITAL_MIC		//Analog MIC Use
	snd_soc_update_bits(codec, AK7755_DE_DMIC_IF_SETTING, 0x90);					//DMIC1=DMIC2=1(Digital MIC Use)
	snd_soc_update_bits(codec, AK7755_DE_DMIC_IF_SETTING, 0x40, AK7755_DMCLKP1_BIT);	//DMCLKP1 bit
	snd_soc_update_bits(codec, AK7755_DE_DMIC_IF_SETTING, 0x08, AK7755_DMCLKP2_BIT);	//DMCLKP2 bit
#else
	snd_soc_write(codec, AK7755_C0_CLOCK_SETTING1, 0x08); 	//AINE=1(Analog MIC Use)
#endif

	snd_soc_write(codec, 0xC0, 0x3d);
	snd_soc_write(codec, 0xC1, 0x21);

	snd_soc_write(codec, 0xC3, 0xF2);
	snd_soc_write(codec, 0xC4, 0x48);
	snd_soc_write(codec, 0xC5, 0x00);

	snd_soc_write(codec, 0xC6, 0x33);
	snd_soc_write(codec, 0xC7, 0xF3);
	snd_soc_write(codec, 0xC8, 0x00);
	snd_soc_write(codec, 0xC9, 0x00);
	snd_soc_write(codec, 0xCA, 0x07);
	snd_soc_write(codec, 0xCC, 0x00);

	snd_soc_write(codec, 0xCD, 0xC0);
	snd_soc_write(codec, 0xCE, 0xFF);
	snd_soc_write(codec, 0xCF, 0x0c);
	snd_soc_write(codec, 0xD0, 0x40);
	snd_soc_write(codec, 0xC2, 0x10);
	snd_soc_write(codec, 0xD3, 0x0F);

	snd_soc_write(codec, 0xD4, 0xFF);
	snd_soc_write(codec, 0xD5, 0x30);
	snd_soc_write(codec, 0xD6, 0x30);
	snd_soc_write(codec, 0xD7, 0x30);
	snd_soc_write(codec, 0xD8, 0x18);
	snd_soc_write(codec, 0xD9, 0x18);
	snd_soc_write(codec, 0xDA, 0x10);
	snd_soc_write(codec, 0xDB, 0x00);


	return 0;
}



static int ak7755_firmware_write_ram(u16 mode, u16 cmd)
{
	int ret = 0;
	int nNumMode, nMaxLen;
	int nRamSize;
	u8  *ram_basic;
	const struct firmware *fw;
	u8  *fwdn;
	char szFileName[32];

	akdbgprt("%s(),mode=%d, cmd=%d\n",__FUNCTION__, mode, cmd);

	switch(mode) {
		case RAMTYPE_PRAM:
			nNumMode = sizeof(ak7755_firmware_pram) / sizeof(ak7755_firmware_pram[0]);
			break;
		case RAMTYPE_CRAM:
			nNumMode = sizeof(ak7755_firmware_cram) / sizeof(ak7755_firmware_cram[0]);
			break;
		case RAMTYPE_OFREG:
			nNumMode = sizeof(ak7755_firmware_ofreg) / sizeof(ak7755_firmware_ofreg[0]);
			break;
		case RAMTYPE_ACRAM:
			nNumMode = sizeof(ak7755_firmware_acram) / sizeof(ak7755_firmware_acram[0]);
			break;
		default:
			akdbgprt("[AK7755] %s mode Error=%d\n",__FUNCTION__, mode);
			return( -EINVAL);
	}

	if ( cmd == 0 ) return(0);

	if ( cmd >= nNumMode ) {
		pr_err("%s: invalid command %d\n", __func__, cmd);
		return( -EINVAL);
	}

	if ( cmd == 1 ) {
		switch(mode) {
			case RAMTYPE_PRAM:
				ram_basic = ak7755_pram_basic;
				nRamSize = sizeof(ak7755_pram_basic);
				break;
			case RAMTYPE_CRAM:
				ram_basic = ak7755_cram_basic;
				nRamSize = sizeof(ak7755_cram_basic);
				break;
			case RAMTYPE_OFREG:
				ram_basic = ak7755_ofreg_basic;
				nRamSize = sizeof(ak7755_ofreg_basic);
				break;
			case RAMTYPE_ACRAM:
				ram_basic = ak7755_acram_basic;
				nRamSize = sizeof(ak7755_acram_basic);
				break;
			default:
				return( -EINVAL);
		}
		ret = ak7755_write_ram((int)mode, ram_basic, nRamSize);
	}
	else {
		switch(mode) {
			case RAMTYPE_PRAM:
				sprintf(szFileName, "ak7755_pram_%s.bin", ak7755_firmware_pram[cmd]);
				nMaxLen = TOTAL_NUM_OF_PRAM_MAX;
				break;
			case RAMTYPE_CRAM:
				sprintf(szFileName, "ak7755_cram_%s.bin", ak7755_firmware_cram[cmd]);
				nMaxLen = TOTAL_NUM_OF_CRAM_MAX;
				break;
			case RAMTYPE_OFREG:
				sprintf(szFileName, "ak7755_ofreg_%s.bin", ak7755_firmware_cram[cmd]);
				nMaxLen = TOTAL_NUM_OF_OFREG_MAX;
				break;
			case RAMTYPE_ACRAM:
				sprintf(szFileName, "ak7755_acram_%s.bin", ak7755_firmware_cram[cmd]);
				nMaxLen = TOTAL_NUM_OF_ACRAM_MAX;
				break;
			default:
				return( -EINVAL);
		}

		akdbgprt("[%s-%d]filename: %s\n", __func__, __LINE__, szFileName);
		if (ak7755_data->control_type == SND_SOC_SPI) {
			ret = request_firmware(&fw, szFileName, &(ak7755_data->spi->dev));
		}
		else {
			ret = request_firmware(&fw, szFileName, &(ak7755_data->i2c->dev));
		}

		//ak7755_download_reg_config();//下载时寄存器配置
		if (ret) {
			akdbgprt("%s(),could not load firmware=%d\n", szFileName, ret);
			return -EINVAL;
		}

		/*  for(i = 0xc0; i<=0xDE; i++){
		    printk("ak7755_firmware_write_ram start:0x%02x  i2c_value:0x%02x\n",i,snd_soc_read(ak7755_data->codec,i));
		    }*/



		printk("%s(), name=%s size=%d\n",__FUNCTION__, szFileName, (int)fw->size);
		if ( fw->size > nMaxLen ) {
			akdbgprt("[AK7755] %s RAM Size Error : %d\n",__FUNCTION__, (int)fw->size);
			release_firmware(fw);	// 16/03/25
			return -ENOMEM;
		}

		fwdn = kmalloc((unsigned long)fw->size, GFP_KERNEL);
		if (fwdn == NULL) {
			printk(KERN_ERR "failed to buffer vmalloc: %d\n", (int)fw->size);
			release_firmware(fw);	// 16/03/25
			return -ENOMEM;
		}

		memcpy((void *)fwdn, fw->data, fw->size);

		ret = ak7755_write_ram((int)mode, (u8 *)fwdn, (fw->size));

		kfree(fwdn);
		release_firmware(fw);	// 16/03/25
	}

	ak7755_config_dsp_route(ak7755_data->codec);

	//ak7755_running_reg_config();

	/* for(i = 0xc0; i<=0xDE; i++){
	   printk("ak7755_firmware_write_ram end:0x%02x  i2c_value:0x%02x\n",i,snd_soc_read(ak7755_data->codec,i));
	   }*/



	return ret;
}

static int ak7755_write_cram(
		int addr,
		int len,
		unsigned char *cram_data)
{
	int i, n, ret;
	int nDSPRun;
	unsigned char tx[51];

	akdbgprt("[AK7755] %s addr=%d, len=%d\n",__FUNCTION__, addr, len);

	if ( len > 48 ) {
		akdbgprt("[AK7755] %s Length over!\n",__FUNCTION__);
		return(-1);
	}

	nDSPRun = snd_soc_read(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING);

	if ( nDSPRun & 0x4 ) {
		tx[0] = 0x80 + (unsigned char)((len / 3) - 1);
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}
	else {
		ak7755_set_status(DOWNLOAD);
		tx[0] = 0xB4;
		tx[1] = (unsigned char)(0xFF & (addr >> 8));
		tx[2] = (unsigned char)(0xFF & addr);
	}
	printk("%s(), nDSPRun=0x%08x,tx[0]=0x%02x,tx[1]=0x%02x,tx[2]=0x%02x\n",__FUNCTION__,nDSPRun,tx[0],tx[1],tx[2]);
	n = 3;
	for ( i = 0 ; i < len; i ++ ) {
		tx[n++] = cram_data[i];
		printk("%s(), tx[%d]=0x%02x\n",__FUNCTION__, n-1,tx[n-1]);
	}

	ret = ak7755_writes(tx, n);
	if(ret < 0 )
		printk("%s(), ret %d \n",__FUNCTION__, ret);

	if ( nDSPRun & 0x4 ) {
		tx[0] = 0xA4;
		tx[1] = 0;
		tx[2] = 0;
		ret = ak7755_writes(tx, 3);
		if(ret < 0 )
			printk("%s(), 0xA4 ret %d \n",__FUNCTION__, ret);
	}
	else {
		snd_soc_write(ak7755_data->codec, AK7755_CF_RESET_POWER_SETTING, nDSPRun);
	}


	return ret;

}

static unsigned long ak7755_readMIR(
		int nMIRNo)
{
	unsigned char tx[1];
	unsigned char rx[4];
	unsigned long dwMIRData;

	if ( ( nMIRNo < 1 )  ||  ( nMIRNo > 4 ) ) return(-1);

	tx[0] = (unsigned char)(0x74 + (2 * nMIRNo));

	ak7755_reads(tx, 1, rx, 4);

	dwMIRData = ((0xFF & (unsigned long)rx[0]) << 20) + ((0xFF & (unsigned long)rx[1]) << 12) +
		((0xFF & (unsigned long)rx[2]) << 4) + ((0xFF & (unsigned long)rx[3]) >> 4);


	return(dwMIRData);

}

static int ak7755_reg_cmd(REG_CMD *reg_param, int len)
{
	int i;
	int rc;
	int  addr, value;

	akdbgprt("[AK7755] %s len = %d\n",__FUNCTION__, len);

	rc = 0;
	for (i = 0; i < len; i++) {
		addr = (int)(reg_param[i].addr);
		value = (int)(reg_param[i].data);

		if ( addr != 0xFF ) {
			rc = snd_soc_write(ak7755_data->codec, addr, value);
			if (rc < 0) {
				break;
			}
		}
		else {
			mdelay(value);
		}
	}

	if (rc != 0 ) rc = 0;

	return(rc);

}

#ifdef AK7755_IO_CONTROL

static long ak7755_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	struct ak7755_priv *ak7755 = (struct ak7755_priv*)file->private_data;
	struct ak7755_wreg_handle ak7755_wreg;
	struct ak7755_wcram_handle  ak7755_wcram;
	struct ak7755_loadram_handle  ak7755_ram;
	void __user *data = (void __user *)args;
	int *val = (int *)args;
	int i;
	unsigned long dwMIRData;
	int ret = 0;
	REG_CMD      regcmd[MAX_WREG];
	unsigned char cram_data[MAX_WCRAM];

	akdbgprt("[AK7755] %s cmd, val=%x, %x\n",__FUNCTION__, cmd, val[0]);

	switch(cmd) {
		case AK7755_IOCTL_WRITECRAM:
			if (copy_from_user(&ak7755_wcram, data, sizeof(struct ak7755_wcram_handle)))
				return -EFAULT;
			if ( (  ak7755_wcram.len % 3 ) != 0 ) {
				printk(KERN_ERR "[AK7755] %s CRAM len error\n",__FUNCTION__);
				return -EFAULT;
			}
			if ( ( ak7755_wcram.len < 3 ) || ( ak7755_wcram.len > MAX_WCRAM ) ) {
				printk(KERN_ERR "[AK7755] %s CRAM len error2\n",__FUNCTION__);
				return -EFAULT;
			}
			for ( i = 0 ; i < ak7755_wcram.len ; i ++ ) {
				cram_data[i] = ak7755_wcram.cram[i];
			}
			ret = ak7755_write_cram(ak7755_wcram.addr, ak7755_wcram.len, cram_data);
			break;
		case AK7755_IOCTL_WRITEREG:
			if (copy_from_user(&ak7755_wreg, data, sizeof(struct ak7755_wreg_handle)))
				return -EFAULT;
			if ( ( ak7755_wreg.len < 1 ) || ( ak7755_wreg.len > MAX_WREG ) ) {
				printk(KERN_ERR "MAXREG ERROR %d\n", ak7755_wreg.len );
				return -EFAULT;
			}
			for ( i = 0 ; i < ak7755_wreg.len; i ++ ) {
				regcmd[i].addr = ak7755_wreg.regcmd[i].addr;
				regcmd[i].data = ak7755_wreg.regcmd[i].data;
			}
			ak7755_reg_cmd(regcmd, ak7755_wreg.len);
			break;
		case AK7755_IOCTL_LOADRAM:
			if (copy_from_user(&ak7755_ram, data, sizeof(struct ak7755_loadram_handle)))
				return -EFAULT;
			ak7755_firmware_write_ram(ak7755_ram.ramtype, ak7755_ram.mode);
			break;
		case AK7755_IOCTL_SETSTATUS:
			ret = ak7755_set_status(val[0]);
			if (ret < 0) {
				printk(KERN_ERR "ak7755: set_status error: \n");
				return ret;
			}
			break;
		case AK7755_IOCTL_GETSTATUS:
			ret = copy_to_user(data, (const void*)&ak7755->status,
					(unsigned long)sizeof(unsigned int));
			if (ret < 0) {
				printk(KERN_ERR "ak7755: get status error\n");
				return -EFAULT;
			}
			break;
		case AK7755_IOCTL_SETMIR:
			ak7755->MIRNo = val[0];
			if (ret < 0) {
				printk(KERN_ERR "ak7755: set MIR error\n");
				return -EFAULT;
			}
			break;
		case AK7755_IOCTL_GETMIR:
			dwMIRData = ak7755_readMIR(ak7755->MIRNo);
			ret = copy_to_user(data, (const void*)&dwMIRData,
					(unsigned long)sizeof(unsigned int));
			if (ret < 0) {
				printk(KERN_ERR "ak7755: get status error\n");
				return -EFAULT;
			}
			break;

		default:
			printk(KERN_ERR "Unknown command required: %d\n", cmd);
			return -EINVAL;
	}

	return ret;
}

static int init_ak7755_pd(struct ak7755_priv *data)
{
	struct _ak7755_pd_handler *ak7755 = &ak7755_pd_handler;

	if (data == NULL)
		return -EFAULT;

	mutex_init(&ak7755->lock);

	mutex_lock(&ak7755->lock);
	ak7755->data = data;
	mutex_unlock(&ak7755->lock);

	printk("%s() data:%p, ak7755->data:%p\n", __func__, data, ak7755->data);

	return 0;
}

static struct ak7755_priv* get_ak7755_pd(void)
{
	struct _ak7755_pd_handler *ak7755 = &ak7755_pd_handler;

	if (ak7755->data == NULL)
		return NULL;

	mutex_lock(&ak7755->lock);
	ak7755->ref_count++;
	mutex_unlock(&ak7755->lock);

	return ak7755->data;
}

static int rel_ak7755_pd(struct ak7755_priv *data)
{
	struct _ak7755_pd_handler *ak7755 = &ak7755_pd_handler;

	if (ak7755->data == NULL)
		return -EFAULT;

	mutex_lock(&ak7755->lock);
	ak7755->ref_count--;
	mutex_unlock(&ak7755->lock);

	data = NULL;

	return 0;
}

/* AK7755 Misc driver interfaces */
static int ak7755_open(struct inode *inode, struct file *file)
{
	struct ak7755_priv *ak7755;

	ak7755 = get_ak7755_pd();
	file->private_data = ak7755;

	return 0;
}

static int ak7755_close(struct inode *inode, struct file *file)
{
	struct ak7755_priv *ak7755 = (struct ak7755_priv*)file->private_data;

	rel_ak7755_pd(ak7755);

	return 0;
}


static const struct file_operations ak7755_fops = {
	.owner = THIS_MODULE,
	.open = ak7755_open,
	.release = ak7755_close,
	.unlocked_ioctl = ak7755_ioctl,
};

static struct miscdevice ak7755_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ak7755-dsp",
	.fops = &ak7755_fops,
};

#endif


static int ak7755_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *codec_dai)
{
	return 0;
}


static int ak7755_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level)
{
	akdbgprt("%s():level = %d\n",__FUNCTION__,level);

	switch (level) {
		case SND_SOC_BIAS_ON:
		case SND_SOC_BIAS_PREPARE:
		case SND_SOC_BIAS_STANDBY:
			//		ak7755_set_status(STANDBY);
			break;
		case SND_SOC_BIAS_OFF:
			ak7755_set_status(POWERDOWN);
			break;
	}

	return 0;
}
/*
static int ak7755_codec_ctl_gpio(struct ak7755_priv *ak7755,int mute){

	if(mute){
		gpio_set_value(ak7755->amp_mute_gpio, 1);
	}else{
		gpio_set_value(ak7755->amp_mute_gpio, 0);
	}
	msleep(ak7755->spk_mute_delay);
	return 0;
}
*/

static int ak7755_set_dai_mute2(struct snd_soc_codec *codec, int mute)
{
	int ndt;
	struct ak7755_priv *ak7755;
	printk("%s(): mute[%s]\n",__FUNCTION__, mute ? "on":"off");
	ak7755 = snd_soc_codec_get_drvdata(codec);

	if (mute) {	//SMUTE: 1 , MUTE
		//ak7755_codec_ctl_gpio(ak7755,1);
		snd_soc_update_bits(codec, AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0xE0, 0xE0);
	}else {		// SMUTE: 0 ,NORMAL operation
		if(!is_first_unmute){
			// ak7755_codec_ctl_gpio(ak7755,0);
			is_first_unmute = 1;
		}
		snd_soc_update_bits(codec, AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0xE0, 0x00);
	}


	ndt = 1021000 / ak7755_data->fs;
	mdelay(ndt);
	return 0;
}

static int ak7755_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec;
	if(is_ak7755_mute) //if app mute output,driver is on mute
		return-1;
	codec = dai->codec;

	ak7755_set_dai_mute2(codec, mute);

	return 0;
}

#define AK7755_RATES		(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
		SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 |\
		SNDRV_PCM_RATE_96000)

#define AK7755_FORMATS		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE


static struct snd_soc_dai_ops ak7755_dai_ops = {
	.hw_params	= ak7755_hw_params,
	.set_sysclk	= ak7755_set_dai_sysclk,
	.set_fmt	= ak7755_set_dai_fmt,
	.trigger = ak7755_trigger,
	.digital_mute = ak7755_set_dai_mute,
};

struct snd_soc_dai_driver ak7755_dai[] = {
	{
		.name = "ak7755-AIF1",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AK7755_RATES,
			.formats = AK7755_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 4,
			.rates = AK7755_RATES,
			.formats = AK7755_FORMATS,
		},
		.ops = &ak7755_dai_ops,
	},
};


#if 0

static int ak7755_config_route(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, AK7755_D4_LO1_LO2_VOLUME_SETTING, 0xFF, 0xFF);			//LOVOL1,2=F(0dB)
	snd_soc_update_bits(codec, AK7755_D3_LIN_LO3_VOLUME_SETTING, 0x0F, 0x0F);			//LOVOL3=F(0dB)
	snd_soc_update_bits(codec, AK7755_D0_FUNCTION_SETTING, 0x40, 0x40);				//CRCE=1(CRC Enable)
	snd_soc_update_bits(codec, AK7755_C2_SERIAL_DATA_FORMAT, 0x40, AK7755_BCKP_BIT);	//BCKP bit
	snd_soc_update_bits(codec, AK7755_D0_FUNCTION_SETTING, 0x10, AK7755_SOCFG_BIT);	//SOFG bit
#ifdef DIGITAL_MIC		//Analog MIC Use
	snd_soc_update_bits(codec, AK7755_DE_DMIC_IF_SETTING, 0x90);					//DMIC1=DMIC2=1(Digital MIC Use)
	snd_soc_update_bits(codec, AK7755_DE_DMIC_IF_SETTING, 0x40, AK7755_DMCLKP1_BIT);	//DMCLKP1 bit
	snd_soc_update_bits(codec, AK7755_DE_DMIC_IF_SETTING, 0x08, AK7755_DMCLKP2_BIT);	//DMCLKP2 bit
#else
	snd_soc_update_bits(codec, AK7755_C0_CLOCK_SETTING1, 0x08, 0x08);		//AINE=1(Analog MIC Use)
#endif

	snd_soc_update_bits(codec, AK7755_CD_STO_DLS_SETTING, 0x40, 0x40);
	snd_soc_update_bits(codec, AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0x10, 0x10);
	snd_soc_write(codec, 0xE6, 0x01);
	snd_soc_write(codec, 0xEA, 0x80);
	snd_soc_write(codec, 0xc8, 0xc0);
	snd_soc_write(codec, 0xca, 0x00);
	snd_soc_write(codec, 0xce, 0xff);
	snd_soc_write(codec, 0xda, 0x10);
	snd_soc_write(codec, 0xc1, 0x01);
	snd_soc_write(codec, AK7755_CF_RESET_POWER_SETTING, 0x0c);

	return 0;
}
#endif
static int ak7755_init_reg(struct snd_soc_codec *codec)
{
	struct ak7755_priv *ak7755;
	int	devid = 0;
	// int ret;
	ak7755 = snd_soc_codec_get_drvdata(codec);   // '15/06/15
	printk("%s() start...\n",__FUNCTION__);

	// 16/09/27  Add
	devid = ak7755_reg_read(codec, 0x60);
	if ( devid == 0x55 ) {
		printk("%s():device_id =0x%02X\n", __FUNCTION__,devid);
	}
	else {
		printk("%s():Device ID is not 0x55,ID =%X\n", __FUNCTION__,devid);
	}

	//download pram cram
	// ak7755_firmware_write_ram(RAMTYPE_PRAM, 2);
	// msleep(50);
	// ak7755_firmware_write_ram(RAMTYPE_CRAM, 2);
	// msleep(50);
	// ak7755_firmware_write_ram(RAMTYPE_OFREG, 2);
	// ak7755_config_dsp_route(codec);

	// ak7755_config_route(codec);

	return 0;
}



static int ak7755_dts_parse(struct ak7755_priv *ak7755){
	struct device_node *np = ak7755->i2c->dev.of_node;
	int ret;
	printk("%s %d\n",__func__,__LINE__);

	ak7755->pdn_gpio = of_get_named_gpio(np, "pdn-gpio", 0);
	if (ak7755->pdn_gpio < 0) {
		ak7755->pdn_gpio = -1;
		return -1;
	}
	if(!gpio_is_valid(ak7755->pdn_gpio) ) {
		printk(KERN_ERR "failed to pdn-gpio!\n");
		return -1;
	}
	ret = gpio_request(ak7755->pdn_gpio, "ak7755_pdn");
	if (ret) {
		printk("request ak7755_pdn failed!\n");
		return -ENODEV;
	}
	gpio_direction_output(ak7755->pdn_gpio, 1);
	msleep(100);
	gpio_set_value(ak7755->pdn_gpio, 0);
	msleep(20);
	gpio_set_value(ak7755->pdn_gpio, 1);
	mdelay(100);

	ak7755->i2c_sel_gpio = of_get_named_gpio(np, "i2c-sel-gpio", 0); //i2c addr select
	if (ak7755->i2c_sel_gpio < 0) {
		ak7755->i2c_sel_gpio = -1;
		return -1;
	}
	if(!gpio_is_valid(ak7755->i2c_sel_gpio) ) {
		printk(KERN_ERR "failed to i2c_sel_gpio!\n");
		return -1;
	}
	ret = gpio_request(ak7755->i2c_sel_gpio, "ak7755_cad");
	if (ret) {
		printk("request ak7755_cad failed!\n");
		return -ENODEV;
	}
	gpio_direction_output(ak7755->i2c_sel_gpio, 1); //cad:1 i2c_addr:0x18

#if 0
	ak7755->amp_mute_gpio = of_get_named_gpio(np, "amp-mute-gpio", 0);
	if (ak7755->amp_mute_gpio < 0) {
		ak7755->amp_mute_gpio = -1;
		return -1;
	}
	if(!gpio_is_valid(ak7755->amp_mute_gpio) ) {
		printk(KERN_ERR "failed to amp_mute_gpio!\n");
		return -1;
	}
	ret = gpio_request(ak7755->amp_mute_gpio, "amp-mute-gpio");
	if (ret) {
		printk("request amp-mute-gpio failed!\n");
		return -ENODEV;
	}
	gpio_direction_output(ak7755->amp_mute_gpio, 1);	//default:mute amp
	
	ret = of_property_read_u32(np, "spk-mute-delay-ms",&ak7755->spk_mute_delay);
	if (ret < 0) {
		printk(KERN_ERR "%s(),can not read property spk_mute_delay\n",__func__);
		ak7755->spk_mute_delay = 0;
	}
	printk("%s(),spk-mute-delay-ms = %d\n",__func__,ak7755->spk_mute_delay);

	//amp control
	if (vhd_get_hw_version() == NULL) {
		printk("%s: failed to get hw version\n", __func__);
		return -1;
	}
	printk("%s: get hw version[%s]\n", __func__, vhd_get_hw_version());
	if (strcmp(vhd_get_hw_version(), "HW version 1.0") == 0) {
		ak7755->amp_control_gpio = of_get_named_gpio(np, "amp-control-v1-gpio", 0);
	} else {
		ak7755->amp_control_gpio = of_get_named_gpio(np, "amp-control-v2-gpio", 0);
	}
	if (ak7755->amp_control_gpio < 0) {
		return -1;
	}
	if(!gpio_is_valid(ak7755->amp_control_gpio) ) {
		printk(KERN_ERR "failed to amp-control-gpio!\n");
		return -1;
	}
	ret = gpio_request(ak7755->amp_control_gpio, "amp-control-gpio");
	if (ret) {
		printk("request amp-power-gpi failed!\n");
		return -ENODEV;
	}
	gpio_direction_output(ak7755->amp_control_gpio, 0);	//default:close amp power
#endif

	return 0;
}


static int ak7755_probe(struct snd_soc_codec *codec)
{
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	int i;


	printk("%s() start...\n",__FUNCTION__);

	ak7755->codec = codec;
	ak7755_init_reg(codec);

	ak7755->fs = 48000;
	ak7755->rclk = 0;
	ak7755->lign = 7;	//lign:7 = LIGN:0(0dB)
	ak7755->selmix = 0;	//SDOUTAD(L,R)
	ak7755->MIRNo = 1;
	ak7755->status = POWERDOWN;


	ak7755->DSPPramMode = 0;
	ak7755->DSPCramMode = 0;
	ak7755->DSPOfregMode = 0;
	ak7755->DSPAcramMode = 0;

	for ( i = 0 ; i < 5 ; i ++ ) ak7755->EQLevel[i] = 24;
	ak7755->HPFfc[0] = 0;
	ak7755->HPFfc[1] = 0;
	ak7755->LimRel = 2;
	ak7755->LimVol = 0;

	ak7755->bickfs = (AK7755_AUDIO_IF_MODE >> 4);   // '15/06/15

	akdbgprt("%s() end...\n",__FUNCTION__);;

#ifdef AK7755_IO_CONTROL
	init_ak7755_pd(ak7755);
#endif
	return ret;
}

static int ak7755_remove(struct snd_soc_codec *codec)
{
	
	struct ak7755_priv *ak7755;
	akdbgprt("%s()\n",__FUNCTION__);
	ak7755 = snd_soc_codec_get_drvdata(codec);   // '15/06/15

	

	ak7755_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if ( ak7755->pdn_gpio > 0 ) {
		gpio_set_value(ak7755->pdn_gpio, 0);
		gpio_free(ak7755->pdn_gpio);
	}


	return 0;
}

static int ak7755_suspend(struct snd_soc_codec *codec)  // '15/10/23
{
	struct ak7755_priv *ak7755;
	
	ak7755 = snd_soc_codec_get_drvdata(codec);   // '15/06/15


	return 0;
}

static int ak7755_resume(struct snd_soc_codec *codec)
{
	struct ak7755_priv *ak7755 = snd_soc_codec_get_drvdata(codec);
	int i;

	regcache_cache_only(ak7755->regmap, true);
	for ( i = 0 ; i < ARRAY_SIZE(ak7755_reg) ; i++ ) {
		regmap_write(ak7755->regmap, ak7755_reg[i].reg, ak7755_reg[i].def);
	}
	regcache_cache_only(ak7755->regmap, false);

	ak7755_init_reg(codec);

	return 0;
}


struct snd_soc_codec_driver soc_codec_dev_ak7755 = {  // '15/10/23
	.probe = ak7755_probe,
	.remove = ak7755_remove,
	.suspend =	ak7755_suspend,
	.resume =	ak7755_resume,
	.controls = ak7755_snd_controls,
	.num_controls = ARRAY_SIZE(ak7755_snd_controls),
	/*		.dapm_widgets = ak7755_dapm_widgets,
			.num_dapm_widgets = ARRAY_SIZE(ak7755_dapm_widgets),
			.dapm_routes = ak7755_intercon,
			.num_dapm_routes = ARRAY_SIZE(ak7755_intercon),*/

};
EXPORT_SYMBOL_GPL(soc_codec_dev_ak7755);


static bool ak7755_volatile(struct device *dev, unsigned int reg) // '15/10/23
{
	return false;
}

static bool ak7755_writeable(struct device *dev, unsigned int reg) // '15/10/23
{


	if ( ( reg >= AK7755_C0_CLOCK_SETTING1 ) &&( reg <= AK7755_DE_DMIC_IF_SETTING ) ) {
		return true;
	}else {
		return false;
	}
}

static bool ak7755_readable(struct device *dev, unsigned int reg) // '15/10/23
{
	return true;
}


static const struct regmap_config ak7755_regmap = { // '15/10/23
	.reg_bits = 8,
	.reg_stride = 1,
	.val_bits = 8,

	.max_register = AK7755_MAX_REGISTERS,
	.readable_reg = ak7755_readable,
	.writeable_reg = ak7755_writeable,
	.volatile_reg = ak7755_volatile,
	.reg_defaults = ak7755_reg,
	.num_reg_defaults = ARRAY_SIZE(ak7755_reg),
	.cache_type = REGCACHE_RBTREE,
};



static ssize_t ak7755_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value,write_addr,write_value,ret;
	value = simple_strtol(buf, NULL, 16);

	write_addr = (value >> 8) & 0xFF;
	write_value = value & 0xFF;
	printk("ak7755_write(): value = 0x%x write_addr = 0x%x write_value = 0x%x\n",value,write_addr,write_value);

	ret = snd_soc_write(ak7755_data->codec,write_addr,write_value);
	if(ret == 0){
		printk("ak7755_write(): write write_addr:0x%02x success\n",write_addr);
	}else{
		printk("ak7755_write(): write write_addr:0x%02x faild\n",write_addr);
	}


	return count;
}


static ssize_t ak7755_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;

	for(i = 0xc0; i<=0xDE; i++){
		printk("ak7755_read:0x%02x  i2c_value:0x%02x\n",i,snd_soc_read(ak7755_data->codec,i));
	}


	return 0;
}

static ssize_t ak7755_download_firmware(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if(!strncmp(buf, "1", 1)){
		printk("[%s-%d] download start\n", __func__, __LINE__);
		ak7755_download_reg_config();
		ak7755_firmware_write_ram(RAMTYPE_PRAM, 2);
		msleep(50);
		ak7755_firmware_write_ram(RAMTYPE_CRAM, 2);
		msleep(50);
		ak7755_firmware_write_ram(RAMTYPE_OFREG, 2);
		msleep(50);
		ak7755_running_reg_config();
		msleep(20);
		gpio_set_value(ak7755_data->amp_control_gpio,1);  //download success open amp power
		printk("[%s-%d] download end\n", __func__, __LINE__);
	}else{
		printk("invaild cmd\n");
	}

	return count;
}


static ssize_t amp_mute_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int mute;
	if(1 == sscanf(buf, "%d",&mute)){
		printk("amp_mute_control_store,mute[%s]\n",mute?"on":"off");
		if(mute){
			// ak7755_codec_ctl_gpio(ak7755_data,1);
		}else if(mute == 0){
			// ak7755_codec_ctl_gpio(ak7755_data,0);
		}
	}

	return count;
}

void vhd_amp_control(int value)
{
	unsigned char buf = (unsigned char)(value + 0x30);
	amp_mute_control_store(NULL, NULL, &buf, 1);
}

EXPORT_SYMBOL(vhd_amp_control);

static ssize_t amp_power_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if(!strncmp(buf, "off",3)){
		gpio_set_value(ak7755_data->amp_control_gpio,0);
	}else if(!strncmp(buf, "on",2)){
		gpio_set_value(ak7755_data->amp_control_gpio,1);
	}

	return count;
}



static ssize_t show_ak7755_mute_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("audio output status:[%s]\n",is_ak7755_mute?"mute":"unmute");
	return 0;
}


static ssize_t ak7755_mute_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if(!strncmp(buf, "off",3)){
		snd_soc_update_bits(ak7755_data->codec, AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0xE0, 0x00);
		is_ak7755_mute = 0;
	}else if(!strncmp(buf, "on",2)){
		snd_soc_update_bits(ak7755_data->codec, AK7755_DA_MUTE_ADRC_ZEROCROSS_SET, 0xE0, 0xE0);//DAC/ADC mute
		is_ak7755_mute = 1;
	}

	return count;
}


static ssize_t show_ak7755_current_gain(struct device *dev, struct device_attribute *attr, char *buf)
{
	int current_gain;
	current_gain = snd_soc_read(ak7755_data->codec, AK7755_D4_LO1_LO2_VOLUME_SETTING)/17;
	printk("[current volume]:%d\n",current_gain);

	return 0;
}

static uint8_t ak7755_dsp_vol_set(uint8_t vol)
{

	int ret=0;

	if(vol >= AK7755_DSP_GAIN_NUM){
		printk("ak7755_dsp_vol_set failed,vol more then max number, vol = %d\n",vol);
	}

	ret = ak7755_write_cram(0x0008, 3, ak7755_vol_tables[vol]);
	printk("ak7755_dsp_vol_set addr: 0x80 ,vol = %d, ret = %d\n",vol,ret);
	if(ret < 0)
	{
		printk("ak7755_dsp_vol_set error,vol = %d\n",vol);
		return 1;
	}

	return 0;
}
#define AK7755_USE_DSP_GAIN
static ssize_t ak7755_output_gain_adjust(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int gain;
	char gain_buf[10] = {'\0'};
	sprintf(gain_buf,"%s",buf);
	if(gain_buf[count-1]==10){
		gain_buf[count-1]='\0';
	}else{
		gain_buf[count]='\0';
	}
	if((1 == sscanf(gain_buf, "%d", &gain))){

		if(1 == ak7755_dsp_gain_enable || 2 == ak7755_dsp_gain_enable){
			gain=gain+(ak7755_dsp_gain_enable-1)*11;
			ak7755_dsp_vol_set(gain);
		}else{
			if (gain > 10) {
				gain = 10;
			}
			if (gain > 1) {
				gain += 2;
			} else if (gain > 0) {
				gain *= 2;
			} else {
				gain = 0;
			}
			printk("ak7755_output_gain_adjust(),gain = %d\n",gain);
			snd_soc_update_bits(ak7755_data->codec, AK7755_D4_LO1_LO2_VOLUME_SETTING, 0xFF, (16*gain+gain));
		}
	}
	return count;
}


static ssize_t aak7755_dsp_gain_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if(!strncmp(buf, "1", 1)){
		ak7755_dsp_gain_enable = 1;
	}else if(!strncmp(buf, "2", 1)){
		ak7755_dsp_gain_enable = 2;
	}else{
		ak7755_dsp_gain_enable = 0;
	}

	printk("ak7755_dsp_gain_store: buf %s,ak7755_dsp_gain_enable %d \n",buf,ak7755_dsp_gain_enable);

	return count;
}


static ssize_t ak7755_dsp_gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	printk("ak7755_dsp_gain_enable: %d\n",ak7755_dsp_gain_enable);
	return 0;
}



static DEVICE_ATTR(show_ak7755_reg, 0664, ak7755_show, ak7755_store);
static DEVICE_ATTR(download_dsp_firmware, 0664, NULL, ak7755_download_firmware);
static DEVICE_ATTR(amp_mute_control, 0664, NULL, amp_mute_control_store);
static DEVICE_ATTR(amp_power_control, 0664, NULL, amp_power_control_store);
static DEVICE_ATTR(ak7755_mute, 0664, show_ak7755_mute_status, ak7755_mute_control);
static DEVICE_ATTR(output_gain_control, 0664, show_ak7755_current_gain, ak7755_output_gain_adjust);
static DEVICE_ATTR(ak7755_dsp_gain_enable, 0664, ak7755_dsp_gain_show, aak7755_dsp_gain_store);





static struct attribute *ak7755_ctl_sysfs[] = {
	&dev_attr_show_ak7755_reg.attr,
	&dev_attr_download_dsp_firmware.attr,
	&dev_attr_amp_mute_control.attr,
	&dev_attr_amp_power_control.attr,
	&dev_attr_ak7755_mute.attr,
	&dev_attr_output_gain_control.attr,
	&dev_attr_ak7755_dsp_gain_enable.attr,

	NULL,
};

static struct attribute_group ak7755_debug_attr_group = {
	.attrs  = ak7755_ctl_sysfs,
};



static int ak7755_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct ak7755_priv *ak7755;
	int ret=0;

	printk("%s(),i2c_addr:0x%02x name:%s\n",__func__,i2c->addr,i2c->name);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		printk("i2c_check_functionality fail\n");
		return -EINVAL;
	}
	ak7755 = devm_kzalloc(&i2c->dev, sizeof(struct ak7755_priv), GFP_KERNEL);
	if (ak7755 == NULL) return -ENOMEM;

	ak7755->regmap = devm_regmap_init_i2c(i2c, &ak7755_regmap);
	if (IS_ERR(ak7755->regmap)) {
		devm_kfree(&i2c->dev, ak7755);
		dev_err(&i2c->dev, "Failed to create regmap: %d\n", ret);
		return PTR_ERR(ak7755->regmap);
	}

	ak7755->mclk = devm_clk_get(&i2c->dev, "mclk");
	if (IS_ERR(ak7755->mclk))
		return PTR_ERR(ak7755->mclk);

	ret = clk_prepare_enable(ak7755->mclk);
	if (ret){
		printk("Failed to clk_prepare_enable\n");
		return ret;
	}

	i2c_set_clientdata(i2c, ak7755);
	ak7755->control_type = SND_SOC_I2C;
	ak7755->i2c = i2c;

	ak7755_dts_parse(ak7755);
	ak7755_data = ak7755;


#ifdef AK7755_IO_CONTROL
	ret = misc_register(&ak7755_misc);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register AK7755 MISC driver: %d\n",  ret);
	}
#endif

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_ak7755, &ak7755_dai[0], ARRAY_SIZE(ak7755_dai));
	if (ret < 0){
		devm_kfree(&i2c->dev, ak7755);
		akdbgprt("[AK7755 Error!] %s(%d)\n",__FUNCTION__,__LINE__);
	}

	ak7755->ak7755_control_kobj = kobject_create_and_add("ak7755_control", NULL);
	if(!ak7755->ak7755_control_kobj){
		printk("kobject_create_and_add failed!");
		return ret;
	}
	printk("%s %d\n",__func__,__LINE__);

	ret = sysfs_create_group(ak7755->ak7755_control_kobj, &ak7755_debug_attr_group);
	if (ret) {
		pr_err("failed to create ak7755 attr group\n");
	}


	return ret;
}

static int ak7755_i2c_remove(struct i2c_client *client)
{
	struct ak7755_priv *ak7755 = i2c_get_adapdata(client->adapter);

	sysfs_remove_group(ak7755->ak7755_control_kobj, &ak7755_debug_attr_group);
	kobject_put(ak7755->ak7755_control_kobj);
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	misc_deregister(&ak7755_misc);
	return 0;
}


static struct of_device_id ak7755_i2c_dt_ids[] = {
	{ .compatible = "akm,ak7755"},
	{ }
};

static const struct i2c_device_id ak7755_i2c_id[] = {
	{ "ak7755", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ak7755_i2c_id);


static struct i2c_driver ak7755_i2c_driver = {
	.driver = {
		.name = "ak7755",
		.of_match_table = of_match_ptr(ak7755_i2c_dt_ids),
	},
	.probe = ak7755_i2c_probe,
	.remove = ak7755_i2c_remove,
	.id_table = ak7755_i2c_id,
};


static int __init ak7755_init(void)
{
	return i2c_add_driver(&ak7755_i2c_driver);
}

static void __exit ak7755_exit(void)
{
	i2c_del_driver(&ak7755_i2c_driver);
}
late_initcall(ak7755_init);
// module_init(ak7755_init);
module_exit(ak7755_exit);

MODULE_AUTHOR("Junichi Wakasugi <wakasugi.jb@om.asahi-kasei.co.jp>");
MODULE_DESCRIPTION("ASoC ak7755 codec driver for cm800 playback");
MODULE_LICENSE("GPL");
