.. _auto-boot:

Auto Boot Order
===============

With autoboot enabled Petitboot will consider the relative priority of each new default boot option it discovers to determine what should be booted automatically.

Note that a boot option must be marked for autoboot ("default") in its own configuration file for Petitboot to autoboot it. For example:

.. code-block:: none

   default linux

   label linux
   kernel ./pxe/de-ad-de-ad-be-ef.vmlinuz
   append command line initrd=./pxe/de-ad-de-ad-be-ef.initrd

Boot Priority
-------------

Petitboot can prioritise boot options based on the following attributes:

* Device type (Disk, Network, USB..)
* Partition (eg, sda2)
* Interface name (eg. eth0)
* LVM logical-volume name (eg. "rhel7-boot-lv")

Once started Petitboot will begin to countdown from the configured timeout (default 10 seconds). During this time any default boot option that is discovered is compared against the configured priority list and the current option with the highest priority. If the new option has a higher priority based on the boot order than the current option then it becomes the current option and will be booted once the countdown completes.

Note that unlike some other bootloaders Petitboot does *not* wait for devices in the boot order. For example if the boot order was "Network, Disk" with a 10 second default and a disk option was found but a hypothetic network option would take longer than 10 seconds to be found (eg. slow network or DHCP server), then Petitboot won't know about it and will boot the disk option. In most cases the appropriate solution if a user runs into this is to increase the timeout value to a suitable length of time for their environment.

Note that :ref:`ipmi` overrides will take precedence over any configured boot order.
