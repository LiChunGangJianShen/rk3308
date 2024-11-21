#############################################################
#
# pcba test using adb
#
#############################################################

CURRENT_PATH = package/rockchip/pcba_adb/src

define PCBA_ADB_BUILD_CMDS
	$(TARGET_CC) ${CURRENT_PATH}/fa.c -o ${CURRENT_PATH}/bin/fa
	$(TARGET_CC) ${CURRENT_PATH}/Write_SN.c -o ${CURRENT_PATH}/bin/Write_SN
	$(TARGET_CC) ${CURRENT_PATH}/Read_SN.c -o ${CURRENT_PATH}/bin/Read_SN
	$(TARGET_CC) ${CURRENT_PATH}/Write_WiFimac.c -o ${CURRENT_PATH}/bin/Write_WiFimac
	$(TARGET_CC) ${CURRENT_PATH}/Read_WiFimac.c -o ${CURRENT_PATH}/bin/Read_WiFimac
	$(TARGET_CC) ${CURRENT_PATH}/Write_BTmac.c -o ${CURRENT_PATH}/bin/Write_BTmac
	$(TARGET_CC) ${CURRENT_PATH}/Read_BTmac.c -o ${CURRENT_PATH}/bin/Read_BTmac
	$(TARGET_CC) ${CURRENT_PATH}/Version.c -o ${CURRENT_PATH}/bin/Version
	$(TARGET_CC) ${CURRENT_PATH}/Ringmic_test.c  ${CURRENT_PATH}/cJSON/cJSON.c -lm \
	   	-o ${CURRENT_PATH}/bin/Ringmic_test
	$(TARGET_CC) ${CURRENT_PATH}/SIM_test.c -o ${CURRENT_PATH}/bin/SIM_test
	$(TARGET_CC) ${CURRENT_PATH}/Read_Size.c -o ${CURRENT_PATH}/bin/Read_Size
	$(TARGET_CC) ${CURRENT_PATH}/Led_test.c -o ${CURRENT_PATH}/bin/Led_test
	$(TARGET_CC) ${CURRENT_PATH}/Battery_test.c -o ${CURRENT_PATH}/bin/Battery_test
	$(TARGET_CC) ${CURRENT_PATH}/Button_test.c ${CURRENT_PATH}/cJSON/cJSON.c -lm \
	   	-o ${CURRENT_PATH}/bin/Button_test
	$(TARGET_CC) ${CURRENT_PATH}/Aging_test.c -o ${CURRENT_PATH}/bin/Aging_test
	$(TARGET_CC) ${CURRENT_PATH}/Wlan_test.c -o ${CURRENT_PATH}/bin/Wlan_test
	$(TARGET_CC) ${CURRENT_PATH}/Tube_test.c -o ${CURRENT_PATH}/bin/Tube_test
	$(TARGET_CC) ${CURRENT_PATH}/Bt_test.c -o ${CURRENT_PATH}/bin/Bt_test
endef

define PCBA_ADB_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Write_SN \
		$(TARGET_DIR)/usr/bin/Write_SN
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Read_SN \
		$(TARGET_DIR)/usr/bin/Read_SN
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/fa \
		$(TARGET_DIR)/usr/bin/fa
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Write_WiFimac \
		$(TARGET_DIR)/usr/bin/Write_WiFimac
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Read_WiFimac \
		$(TARGET_DIR)/usr/bin/Read_WiFimac
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Write_BTmac \
		$(TARGET_DIR)/usr/bin/Write_BTmac
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Read_BTmac \
		$(TARGET_DIR)/usr/bin/Read_BTmac
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Version \
		$(TARGET_DIR)/usr/bin/Version
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Ringmic_test \
		$(TARGET_DIR)/usr/bin/Ringmic_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/SIM_test \
		$(TARGET_DIR)/usr/bin/SIM_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Read_Size \
		$(TARGET_DIR)/usr/bin/Read_Size
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Led_test \
		$(TARGET_DIR)/usr/bin/Led_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Battery_test \
		$(TARGET_DIR)/usr/bin/Battery_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Button_test \
		$(TARGET_DIR)/usr/bin/Button_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Aging_test \
		$(TARGET_DIR)/usr/bin/Aging_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Wlan_test \
		$(TARGET_DIR)/usr/bin/Wlan_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Tube_test \
		$(TARGET_DIR)/usr/bin/Tube_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/bin/Bt_test \
		$(TARGET_DIR)/usr/bin/Bt_test
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/keytest.json \
		$(TARGET_DIR)/etc/keytest.json
	$(INSTALL) -m 0755 -D ${CURRENT_PATH}/ringmic.json \
		$(TARGET_DIR)/etc/keytest.json
endef
$(eval $(generic-package))
