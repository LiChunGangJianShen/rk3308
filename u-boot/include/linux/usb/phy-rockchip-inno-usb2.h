/*
 * Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#ifndef _PHY_ROCKCHIP_INNO_USB2_H
#define _PHY_ROCKCHIP_INNO_USB2_H

enum usb_linestate {
	PHY_SET_USB_NONE,
	PHY_SET_USB_DP_H_DM_L,
	PHY_SET_USB_DISC,
	PHY_SET_USB_CONNECT,
	PHY_SET_USB_DONE,
	PHY_SET_USB_FS_ENC_DIS,
};

extern int rockchip_chg_get_type(void);

#if defined(CONFIG_PHY_ROCKCHIP_INNO_USB2) || defined(CONFIG_ROCKCHIP_USB2_PHY)
int rockchip_u2phy_vbus_detect(void);
void rockchip_usb2phy_set_linestate(int phy_id,
				    enum usb_linestate set_linestate);
int rockchip_usb2phy_get_linestate(void);
#else
static inline int rockchip_u2phy_vbus_detect(void)
{
	return -ENOSYS;
}

static inline void rockchip_usb2phy_set_linestate(int phy_id,
			enum usb_linestate set_linestate)
{}

static inline int rockchip_usb2phy_get_linestate(void)
{
	return 0;
}
#endif

#endif /* _PHY_ROCKCHIP_INNO_USB2_H */
