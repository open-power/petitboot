.. _ipmi:

IPMI
====

Petitboot uses inband-IPMI_ on platforms that support it to report information or modify booting behaviour:

.. _inband-IPMI: https://en.wikipedia.org/wiki/Intelligent_Platform_Management_Interface

Platform Information
--------------------

Basic BMC information is read via the "Get Device ID" and "Get LAN Configuration Parameters" commands and shown in the System Information screen. On some platforms with an AMI BMC the OEM command "Get Device ID Golden" will also be issued to get the BMC's secondary side information.

Just before successful boot Petitboot will set the OS boot sensor (command `0x30`) to notify the BMC of a successful boot.

Boot Device Overrides
---------------------

Petitboot supports IPMI boot device overrides. These can be set on the BMC to temporarily override the autoboot behaviour. These are limited to the broad types of "Network", "Disk", "Safe", "CDROM", and "Setup".

If a boot option matches one of these options it receives the highest priority regardless of the boot order. The exceptions are "Setup" which temporarily disables autoboot, and "Safe" which does the same and also enters "safe mode" which prevents setting up any devices until the user issues a "Rescan" command.

Boot Initiator Mailbox
----------------------

Petitboot also supports the "Boot Initiator Mailbox" parameter which behaves similarly to an override but lets a full replacement boot order be set. Support for this on the BMC side is relatively limited so far, but more details can be found here_.

.. _here: https://sthbrx.github.io/blog/2018/12/19/ipmi-initiating-better-overrides/
