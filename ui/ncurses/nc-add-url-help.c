#include "nc-helpscreen.h"

struct help_text add_url_help_text = define_help_text("\
Supply a valid URL here to retrieve a remote pxe-boot config file and parse it.\
\n\
\n\
URLs are of the form 'scheme://host/path/to/pxeconffile', \
such as tftp://host/pxeconffile or http://host/pxeconffile");
