
#include "nc-helpscreen.h"

struct help_text main_menu_help_text = define_help_text(
"From the main menu screen, select a boot option. The options displayed are \
available on the system and the network.\n\
\n\
To select a boot option, press Enter.\n\
\n\
To make changes to an existing option, type E (edit).\n\
\n\
To add a new boot option, type N (new).\n\
\n\
To display information about the system, including the MAC addresses of each \
network interface, type I (information).\n\
\n\
To make changes to the system configuration, type C (configure).\n\
\n\
To set the language for the petitboot interface, type L (language).\n\
\n\
To view the log of status messages from the discovery process, type G (log).\n\
\n\
To find new or updated boot options on the system, select the 'Rescan devices' \
option.\n\
\n\
To retreive new boot options from a remote configuration file, select \
the 'Retrieve config from URL' option.\n\
\n\
To restrict petitboot to only autobooting from a specific device type, the \
following keys are available:\n\
\n\
  F10: Only autoboot from disk\n\
  F11: Only autoboot from USB devices\n\
  F12: Only autoboot from network\n\
\n\
Unlike other keys, these do not cancel automatic boot.\n\
\n\
To close the Petitboot interface, type X (exit).\n"
);
