const char *boot_editor_help_text = "\
This screen allows you to edit or create boot options.\n\
\n\
Device: This is a list of block devices available on the system. Select \
the device which contains your boot resources (kernel, initrd and device \
tree), or \"Specify paths/URLs manually\" to use full URLs to the boot \
resources.\n\
\n\
Kernel: enter the path to the kernel to boot. This field is mandatory. \
This should be a kernel image that the kexec utility can handle. Generally, \
this will be a 'vmlinux'-type image.\n\
Example: /boot/vmlinux\n\
\n\
Initrd: enter the path to the initial RAM disk image. This is optional.\n\
Example: /boot/initrd.img\n\
\n\
Device tree: enter the path to the device tree blob file (.dtb). \
This is optional; if not specified, and your platform currently provides \
a device tree, the current one will be used.\n\
Example: /boot/device-tree.dtb\n\
\n\
Boot arguments: enter the kernel command-line arguments. This is optional.\n\
Example: root=/dev/sda1 console=hvc0\n\
\n";
