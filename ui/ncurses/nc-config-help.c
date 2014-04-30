const char *config_help_text = "\
Autoboot: There are three possible options for automatic-boot hehaviour:\n"
"\n"
"Don't autoboot: boot options will be listed in the petitboot menu, but none \
will be booted automatically. User interaction will be required to continue \
past the petitboot menu. Use this option if you want the machine to wait for \
an explicit boot selection, or want to interact with petitboot before \
booting the system\n"
"\n"
"Autoboot from any disk/network device: any boot option that is marked as a \
default (by bootloader configuration) will be booted automatically after a \
timeout. Use this option if you want to quickly boot your system without \
changing any boot option settings. This is the typical configuration.\n"
"\n"
"Autoboot from a specific disk/network device: only boot options \
from a single device (specifed here) will be booted automatically after a \
timeout. Use this option if you have multiple operating system images \
installed.\n"
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
network settings."
;
