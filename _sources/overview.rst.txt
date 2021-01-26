Petitboot Overview
==================

Petitboot is a set of userspace programs to implement a kexec-based bootloader in a small Linux image. Broadly speaking Petitboot is split into a 'server' half which manages devices and finds bootable images, and a 'client' half which provides a user interface.

Petitboot uses udev to discover block devices and network interfaces and parses a set of known locations for bootloader configurations. Petitboot supports a number of configuration formats including GRUB, SYSLINUX, and Yaboot. Booting is performed via the kexec mechanism.

Booting behaviour can be configured in a number of ways such as boot priority based on device, network-boot behaviour, user permissions, and platform-specific options.

The primary user interface is a ncurses based menu which displays boot options and system information, and provides methods to change configuration options, execute plugins, and drop to a normal shell.

Petitboot is intended to be built as part of a small Linux image, for example one built via tool such as Buildroot. A good example of such an environment is the op-build Buildroot layer used for OpenPOWER: https://github.com/open-power/op-build
