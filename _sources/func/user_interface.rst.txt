User Interface
==============

There are two UI implementations but in practice the ncurses client `petitboot-nc` is the primary interface.

The default view includes a list of discovered boot options and links to other sub screens.
Common shortcuts include:

======== =====================================
Shortcut
======== =====================================
i        Open the System Information screen
e        View/Edit a boot option details
g        View the status update log
l        Open the Language screen
c        Open the System Configuration screen
n        Create/Edit a boot option
h        Show help text for the current screen
x        Exit to the shell
Ctrl-L   Refresh/Redraw the display
======== =====================================

System Information
------------------

Provides an overview of the system, in particular discovered partitions and network interfaces. Depending on the platform can also include information on firmware versions, BMC information, etc.

System Configuration
--------------------

The primary method to configure Petitboot. From here the user can configure the :ref:`auto-boot` order, network interface behaviour, :ref:`snapshots` settings, and handle platform-specific options like :ref:`ipmi` overrides and default consoles.

Boot Option Editor
------------------

The boot option editor displays the source device and boot resources for a given option. All fields are editable; paths are relative to the source device or full URLs for remote resources.

Status Log
----------

The status log keeps a running log of all status updates that normally appear in the bottom line of the interface.

Plugin Menu
-----------

The plugin menu displays a list of all known :ref:`plugins` and whether they are installed or not. "Enter" installs a plugin, and "e" on an installed interface enters a detailed view of the plugin from which a user can run the plugin.

Shell
-----

At any time the user may drop to a shell by exiting the UI, depending on how Petitboot has been built. This is usually a relatively feature-ful ``sh`` shell with Busybox utilities but Petitboot doesn't make any particular guarantees about available tools aside from those it uses itself.
