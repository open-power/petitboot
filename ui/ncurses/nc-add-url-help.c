#include "nc-helpscreen.h"

struct help_text add_url_help_text = define_help_text("\
Supply a valid URL here to retrieve a remote boot config file, \
(eg: petitboot.conf) and parse it.\n\
\n\
URLs are of the form 'scheme://host/path/to/petitboot.conf', \
such as tftp://host/petitboot.conf or http://host/petitboot.conf");
