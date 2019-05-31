Linux Image Requirements
========================

Petitboot has a few requirements for the linux image it is built in to.
For an in-practice example of how to build a Petitboot image see op-build_.

For build-time dependencies see configure.ac_.

.. _op-build: https://github.com/open-power/op-build/tree/master/openpower/package/petitboot
.. _configure.ac: https://github.com/open-power/petitboot/blob/master/configure.ac

Core dependencies
-----------------

* pb-discover expects to be run as root, or at least have permission for device management, executing kexec, etc.
* udev: pb-discover discovers devices via libudev enumeration so a udev implementation must be present.
  Following this any udev rules required for certain device types must also be present. Eg. op-build's inclusions_.
* network utilities: pb-discover expects to have ``udhcpc`` available for DHCP, or a call-equivalent version. Similarly it expects ``tftp`` and ``wget`` binaries in order to download boot resources.
* kexec: A kexec binary must be available. This is commonly kexec-lite_ however kexec-tools should also work.
* LVM: Petitboot depends on libdevmapper, and also requires ``vgscan`` and ``vgchange`` to be available if order to setup logical volumes.

.. _inclusions: https://github.com/open-power/op-build/blob/master/openpower/package/petitboot/63-md-raid-arrays.rules
.. _kexec-lite: https://github.com/open-power/kexec-lite

Optional dependencies
---------------------

* ``mdadm`` for software RAID handling.
* libflash: On OPAL platforms Petitboot will use libflash to load firmware version information.
* ipmi: If ``/dev/ipmi`` is available Petitboot will use it to source information from the BMC.
* console setup: unless you have other requirements you probably want to be starting the UI through ``pb-console`` in which case it is useful to have this called by udev.
  For example: petitboot-console-ui.rules_, or depending on if you're using ``agetty -l`` to log in a user, shell_profile_.

.. _petitboot-console-ui.rules: https://github.com/open-power/op-build/blob/master/openpower/package/petitboot/petitboot-console-ui.rules
.. _shell_profile: https://github.com/open-power/op-build/blob/master/openpower/package/petitboot/shell_profile
