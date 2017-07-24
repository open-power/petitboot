#include "nc-helpscreen.h"

struct help_text plugin_help_text = define_help_text("\
This screen lists the details and available commands of an installed plugin.\n"
"To run a plugin command choose it in the list and select the \"Run\" button. \
The Petitboot UI will temporarily exit to run the command, then return to \
this screen.");
