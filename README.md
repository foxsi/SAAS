SAAS Branch to Work with RaspberryPi 5
====

eBUS v6.4.0, ARM processor

Code to control the FOXSI Solar Aspect and Alignment System.

main code executable is display.cpp

Options
-------
Keyboard input

`q` - quit the program

`s` - enable or disable saving image to a FITS file

`arrowkeys` - move crosshair

`+/-` - change exposure by 1000

Input Files
-----------
`calibration_ccd_center.txt` - contains the calibrated center of the CCD.
`camera_settings.txt` - contains the default camera settings.

Error Codes
-----------
30 - Connected to camera, but unable to communicate with it. Turn the camera off and on then run: 
```
sudo nmcli connection delete "Wired connection 1"
sudo nmcli connection add con-name "Wired connection 1" ifname eth0 type ethernet ipv4.method manual ipv4.addresses 192.168.8.8
sudo nmcli connection delete "Wired connection 1"
sudo nmcli connection add con-name "Wired connection 1" ifname eth0 type ethernet ipv4.method manual ipv4.addresses 169.254.1.1/16
```

