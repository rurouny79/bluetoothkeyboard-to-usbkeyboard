#!/bin/bash

GADGET_PWD=/sys/kernel/config/usb_gadget/isticktoit

mkdir -p $GADGET_PWD
echo 0x1d6b > $GADGET_PWD/idVendor # Linux Foundation
echo 0x0104 > $GADGET_PWD/idProduct # Multifunction Composite Gadget
echo 0x0100 > $GADGET_PWD/bcdDevice # v1.0.0
echo 0x0200 > $GADGET_PWD/bcdUSB # USB2

mkdir -p $GADGET_PWD/strings/0x409
echo "fedcba9876543210"          > $GADGET_PWD/strings/0x409/serialnumber
echo "Tobias Girstmair"          > $GADGET_PWD/strings/0x409/manufacturer
echo "iSticktoit.net USB Device" > $GADGET_PWD/strings/0x409/product

mkdir -p $GADGET_PWD/configs/c.1/strings/0x409
echo "Config 1: ECM network" > $GADGET_PWD/configs/c.1/strings/0x409/configuration
echo 250                     > $GADGET_PWD/configs/c.1/MaxPower

mkdir -p $GADGET_PWD/functions/hid.usb0
echo 1 > $GADGET_PWD/functions/hid.usb0/protocol
echo 1 > $GADGET_PWD/functions/hid.usb0/subclass
echo 8 > $GADGET_PWD/functions/hid.usb0/report_length
echo -ne \\x05\\x01\\x09\\x06\\xa1\\x01\\x05\\x07\\x19\\xe0\\x29\\xe7\\x15\\x00\\x25\\x01\\x75\\x01\\x95\\x08\\x81\\x02\\x95\\x01\\x75\\x08\\x81\\x03\\x95\\x05\\x75\\x01\\x05\\x08\\x19\\x01\\x29\\x05\\x91\\x02\\x95\\x01\\x75\\x03\\x91\\x03\\x95\\x06\\x75\\x08\\x15\\x00\\x25\\x65\\x05\\x07\\x19\\x00\\x29\\x65\\x81\\x00\\xc0 > $GADGET_PWD/functions/hid.usb0/report_desc
ln -s $GADGET_PWD/functions/hid.usb0 $GADGET_PWD/configs/c.1/

ls /sys/class/udc > $GADGET_PWD/UDC
