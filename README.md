# Petitboot - A kexec-based bootloader

Petitboot is a bootloader designed to run in a Linux environment for OPAL on PowerPC/POWER machines and the Playstation 3.

Development
-

Petitboot's home is at [ozlabs.org](http://git.ozlabs.org/?p=petitboot); clone it with ` git clone git://git.ozlabs.org/petitboot `

Development and discussion occurs on the Petitboot mailing list: [petitboot@lists.ozlabs.org](https://lists.ozlabs.org/listinfo/petitboot)

Building
-

For an example of building Petitboot for distribution in a Linux image, see [op-build](https://github.com/open-power/op-build/tree/master/openpower/package/petitboot)

To build locally for development/debug (with the ncurses UI for example):
```
./bootstrap
./configure --with-twin-x11=no --with-twin-fbdev=no
make
```