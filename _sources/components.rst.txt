Petitboot Components
====================

Server
------

The core of Petitboot is the ``pb-discover`` process. This performs initial setup, discovers and configures devices, and handles configuration options or quirks saved or set by the platform.

UI Clients
----------

The ``ui/`` directory contains client processes that implement a user interface. The primary interface is ``petitboot-nc`` which is based on ncurses and provides the full range of options for interaction and configuration.

There is also a twin-based interface under ``ui/twin/`` however this is largely a remnant from the PS3 and does not implement more recent functionality.

Clients generally take no direct action themselves, instead communcating via the "pb-protocol" interface to the ``pb-discover`` server to request actions.

Utilities
---------

A number of smaller utilities exist to perform some specific tasks, including:

pb-console: Initial console setup and UI startup
pb-config: Trimmed down 'client' that can request information from the ``pb-discover`` server.
pb-event: Provides a callable script to send user events to ``pb-discover`` - usually used by other utilities such as ``udhcpc`` to report network information.
pb-exec: Simple wrapper for running programs from the UI
pb-plugin: Implements the petitboot-plugin interface
pb-sos: Collects diagnostic and crash information
boot hooks: Small hooks to be run immediately before boot.
