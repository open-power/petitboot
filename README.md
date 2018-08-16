# Petitboot - A kexec-based bootloader

Petitboot is an operating system bootloader based on Linux kexec.  It can load any operating system image that supports the Linux kexec re-boot mechanism like Linux and FreeBSD.
Petitboot can load images from any device that can be mounted by Linux, and can also load images from the network using the HTTP, HTTPS, NFS, SFTP, and TFTP protocols.

Current platform support includes PowerPC/POWER with OPAL, the Sony Playstation 3, and ARM64 with ACPI.  Petitboot can be built and run on other platforms, but it will not include all available features.

See the petitboot [man pages](man) for more info.

## Development

Petitboot's home is at [ozlabs.org](http://git.ozlabs.org/?p=petitboot); clone it with `git clone git://git.ozlabs.org/petitboot`.

Development and discussion occurs on the Petitboot mailing list: [petitboot@lists.ozlabs.org](https://lists.ozlabs.org/listinfo/petitboot).

## Building

For an example of building Petitboot for distribution in a Linux image, see [op-build](https://github.com/open-power/op-build/tree/master/openpower/package/petitboot) or [petitboot--buildroot](https://github.com/glevand/petitboot--buildroot).

To build locally for development/debug (with the ncurses UI for example):
```
./bootstrap
./configure
make

./discover/pb-discover --help
./ui/ncurses/petitboot-nc --help
```
