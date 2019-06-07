.. _plugins:

Plugins
=======

Petitboot "plugins" provide a convenient way to package and run binaries in the Petitboot shell that would be difficult to distribute in the Linux image due to size, dependency, or licensing constraints.

Plugin usage and the plugin ABI are well documented in the OpenPOWER docs repository here:

`Plugin Usage & Construction <https://github.com/open-power/docs/blob/master/opal/petitboot-plugins.txt>`_

`Plugin Specification & ABI <https://github.com/open-power/docs/blob/master/opal/petitboot-plugin-spec.txt>`_

Petitboot will scan local devices for pb-plugin files, and will also recognise the "plugin" label in PXE network files, eg:

.. code-block:: none

   label Install Ubuntu 18.04
	kernel tftp://server/ubuntu-18.04-ppc64el/vmlinux
	initrd tftp://server/ubuntu-18.04-ppc64el/initrd.gz
	append tasks=standard pkgsel/language-pack-patterns= pkgsel/install-language-support=false

   plugin RAID Setup
        tarball http://server/path/to/plugin-raid-1.0.pb-plugin

Discovered plugins will be listed in the Plugin screen and can be installed and run from there, or run manually from the shell.

