const char *config_help_text = "\
Autoboot: If you select this option, Petitboot will automatically choose the \
default option shown in the main menu. Use this option if you want to quickly \
boot your system without changing any boot option settings.\n"
"\n"
"Timeout: Specify the length of time, in seconds, that the main menu will be \
displayed before the first option on the main menu is started by default. \
Timeout is applied only if the Autoboot option is selected.\n"
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
