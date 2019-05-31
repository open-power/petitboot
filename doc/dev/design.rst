Design Topics
=============

When considering new functionality or other changes to Petitboot there are a few guidelines we do our best to adhere to:

Be Generic
----------

Avoid adding code that ties Petitboot to a particular platform or functionality by default. Avoid making assumptions about the system that Petitboot may be running on. Platform specific functionality should as much as possible be confined to `platform-` files. Especially when thinking about dependencies on outside utilities or tools consider using the pb-plugin interface instead.

Don't Surprise The User
-----------------------

Communicate clearly to the user what is happening. What the UI says is happening should be what actually happens.
A particular example of this is the kernel command line: there are a few problems that would be easier to solve if we could modify the command line of the kernel to be booted, but this would be something that interferes with the users expectations and depending on their scenario could cause them problems.

Avoid Dependencies On The Target Operating System
-------------------------------------------------

Supporting multiple configuration formats means that Petitboot can be dropped in as the bootloader for most existing systems without any change in the target operating system or its bootloader configuration. Avoid any changes that would require a corresponding update in the target operating system or dependencies on any particular version of Petitboot.

Maintain Server-Client Separation
---------------------------------

As much as possible the server should perform any actions, and the clients should only makes requests to the server. This is particularly true with the introduction of Petitboot "users" and password restrictions.
An exception to this is the running of Petitboot plugins since these need to be run in the visible shell; however these are still subject to user permissions if enabled.

Abstract Dependencies
---------------------

Avoid direct dependencies on certain utilities or versions of utilities. If there are several versions or interfaces to a utility that Petitboot might access, abstract these differences where possible. (See handling of different utilities such as tftp, udhcpc, etc).
