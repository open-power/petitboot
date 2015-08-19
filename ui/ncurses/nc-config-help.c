#include "nc-helpscreen.h"

struct help_text config_help_text = define_help_text("\
Autoboot: Specify which devices to autoboot from.\n"
"\n"
"By selecting the 'Add Device' button new devices can be added to the autoboot \
list, either by UUID, MAC address, or device type. Once added to the boot \
order, the priority of devices can be changed with the 'left' and 'right' keys \
Devices can be individually removed from the boot order with the minus key. \
Use this option if you have multiple operating system images installed.\n"
"\n"
"To autoboot from any device, select the 'Clear & Boot Any' button. \
In this case, any boot option that is marked as a default \
(by bootloader configuration) will be booted automatically after a \
timeout. Use this option if you want to quickly boot your system without \
changing any boot option settings. This is the typical configuration.\n"
"\n"
"To disable autoboot, select the 'Clear' button, which will clear the boot \
order. \
With autoboot disabled, user interaction will be required to continue past \
the petitboot menu. Use this option if you want the machine to wait for an \
explicit boot selection, or want to interact with petitboot before booting \
the system\n"
"\n"
"Timeout: Specify the length of time, in seconds, that the main menu will be \
displayed before the default boot option is started. This option is only \
displayed if autoboot is enabled.\n"
"\n"
"Network options:\n"
"\n"
"DHCP on all active interfaces: Automatically assigns IP addresses to each \
network interface. Use this option if you have a DHCP server on your \
network.\n"
"\n"
"DHCP on a specific interface: Automatically assigns IP addresses to the \
selected network interface. The other interfaces are not configured. Select \
this option if you have multiple DHCP servers on different interfaces, but \
only want to configure a single interface during boot.\n"
"\n"
"Static IP configuration: Allows you to specify an IPv4 address and network \
mask, gateway, and a DNS server or servers for a network interface. Select \
this option if you do not have a DHCP server, or want explicit control of \
network settings.\n"
"\n"
"Disk R/W: Certain bootloader configurations may request write access to \
disks to save information or update parameters (eg. GRUB2). "
"Use this option to control access to disks.\n");
