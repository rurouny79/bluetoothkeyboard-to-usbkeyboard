# bluetohid
a toy project to make blue keyboard to usb keyboard

# usage
1. have a raspberry pi zero w h
2. open "/etc/rc.local" and append two lines below before "exit 0"
  - /root/hid_init.sh
  - /root/hid_start & (or hid_start.sh &)
  
  ※ hid_start.sh is just a script. Very simple code but slow keyboard precessing
