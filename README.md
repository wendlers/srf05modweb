SRF05 Ultrasonic Sensor Kernel Module and Web Service
=====================================================
10.05.2012 Stefan Wendler
sw@kaltpost.de

This project provides a kernel module and a web service to access the 
[SRF05 ultra sonic range finder] (http://www.robot-electronics.co.uk/htm/srf05tech.htm) from a Linux based device. 
I use this to measure the water level in my cistern. As a Linux box, I use the in-expensive 
[TP-Link MR3020] (http://www.tp-link.com/lk/products/details/?model=TL-MR3020) box with [OpenWrt] (http://openwrt.org).

The web service uses the excellent [mongoose] (https://code.google.com/p/mongoose/) as a foundation.


Project Directory Layout
------------------------

The top-level directory structure of the project looks something like this:

* `Makefile` 		toplevel Make file
* `Makefile.kmod`	include for building kernel modules
* `README.md`		this README
* `setenv.sh`		source to set cross-compile environment
* `srf05mod`		the kernel module
* `srf05web`		the web service


Prerequisites
-------------

To compile this project the following is needed: 

* OpenWrt checked out and compiled for your target (see [this instructions] (http://wiki.openwrt.org/doc/howto/buildroot.exigence) )
* A freed GPIO on the target hardware (TP-Link MR3020). E.g. [use 7 or 29] (http://wiki.openwrt.org/toh/tp-link/tl-mr3020#gpios). 
* The SRF05 needs to be wired for MODE 2 (shared trigger/data line)


Compilation
------------

**Note:** you need perform a complete OpenWrt build first to succeed with the following steps!

**Check `setenv.sh`**

Edit `setenv.sh`, make sure `OPENWRT_DIR` points to the location of your OpenWrt base directory.

**Set Cross-Compile Environment**

To set the cross-compile environment use the following:

	source setenv.sh

**Compile**

To compile both, the kernel module and the web service:

	make

To create a TGZ including all the files needed on your target device:

	make deploy

This will create an archive called `srf05.tgz` under `deploy`.


Installing on the Target
------------------------

Copy the `srf05.tgz` found in `deploy` to your target an extract it to the root:

	tar -zxvf srf05.tgz -C /

Then make sure the rights are correct:

	chown root:root /opt -R

To start the kernel module and the web service at bootup, add the following to 
`/etc/rc.local` (before the `exit 0` statement):

	insmod /opt/app/modules/srf05.ko
	/opt/app/bin/srf05web &

The module knows the following parameters:

	srf05_gpio		:Number of GPIO to which data line auf SRF05 is connected (default 20). (int)
	srf05_cmdivi	:Divisor used to calculate cm from raw sensor value. (int)
	srf05_updiv		:Divisor used to calculate update freq (devides 10 sec.). (int)


Usage
-----

To access the current sensor values as well as a 24h history, navigate to the following URL:

	http://<your-device-id>/

