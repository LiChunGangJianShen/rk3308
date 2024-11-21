umount /etc/init.d/.usb_config
cp /etc/init.d/.usb_config /tmp/.usb_config
killall adbd
mount --bind /tmp/.usb_config /etc/init.d/.usb_config
echo usb_adb_en >> /etc/init.d/.usb_config
/etc/init.d/S50usbdevice stop
/etc/init.d/S50usbdevice start