TI padconf to dts tool
====================

Tool to parse padconf registers settings from a booted system to produce the
devicetree pinctrl settings.

Relies on booting a current mainline Linux kernel for pinctrl sysfs entries.
To produce the bootloader or Android padconf settings, just boot the mainline
kernel with the pinctrl dts settings temporarily commented out and then run
padconftodts as root user.

Only the SoCs that used to have kernel data are supported, more can be added
as needed. Raw hardware register read could be patched in if needed to allow
the tool to run on older Android kernels.

License
=======

© 2023 Tony Lindgren

GPLv2
