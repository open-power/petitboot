Configuration File Formats
============================

Petitboot is capable of parsing multiple flavors of configuration files for
gathering the required information for booting the next kernel. `GRUB2` and
`SYSLINUX` configuration formats are both supported along with a "native"
format. Documentation for both the `GRUB2`_ and `SYSLINUX`_
(typically used for PXE configuration) configuration formats can be found
in their respective projects documentation.

.. _GRUB2: https://www.gnu.org/software/grub/manual/grub/grub.html#Configuration
.. _SYSLINUX: https://wiki.syslinux.org/wiki/index.php?title=Config


Native Config Format
---------------------

The native format is made up off multiple sections, one per boot option, that
specify how the kernel is loaded. A section is made up of lines, each line
specifying a boot value to set. A section begins with the name of the option to
specify followed by the values separated by whitespace. Whitespace is defined
as being either a space or tab character. Each paragraph is separated by an
empty line from each other. The following is an example::

        name            linux
        image           /boot/linux
        initrd          /boot/initrd
        dtb             /boot/rk3328.dtb
        args            console=tty0 root=LABEL=root ro
        description     Debian GNU/Linux

Name specifies the name presented on the boot menu. Image specifies the path
to the linux binary. Initrd gives the path to the initial RAM disk. Dtb gives
the path to the Device Tree Binary. Args specifies the command line arguments
given to the kernel. Description provides additional information to display
on the boot menu.

Each configuration format has a list of locations that are searched, in order,
to find the file to load. The following table lists the locations searched for
each configuration file type:

======================  =============================  ======================
GRUB2                   SYSLINUX                        Native
======================  =============================  ======================
/grub.cfg               /boot/syslinux/syslinux.cfg     /boot/petitboot.conf
/menu.lst               /syslinux/syslinux.cfg          /petitboot.conf
/grub/grub.cfg          /syslinux.cfg
/grub2/grub.cfg         /BOOT/SYSLINUX/SYSLINUX.CFG
/grub/menu.lst          /SYSLINUX/SYSLINUX.CFG
/boot/grub/grub.cfg     /SYSLINUX.CFG
/boot/grub2/grub.cfg
/boot/grub/menu.lst
/efi/boot/grub.cfg
/GRUB.CFG
/MENU.LST
/GRUB/GRUB.CFG
/GRUB2/GRUB.CFG
/GRUB/MENU.LST
/BOOT/GRUB/GRUB.CFG
/BOOT/GRUB/MENU.LST
/EFI/BOOT/GRUB.CFG
======================  =============================  ======================
