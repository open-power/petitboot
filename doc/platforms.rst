Platforms
=========

Petitboot is indented to be platform agnostic and provide at least basic functionality on any platform that supports the kexec mechanism

The 'platform' interface in ``pb-discover`` allows additional support for specific platforms to be built and dynamically enabled based on the running system. The current implementations of this are:

platform-powerpc
----------------

Intended for "powernv" POWER platforms, which generally means those running the OPAL firmware stack. This implements support for inband IPMI functions (boot overrides, system information, etc), console interface descriptions, and loading and storing parameters from NVRAM flash storage.

platform-arm64
--------------

A fairly general implementation for basic parameter support for ARM64 platforms, but is generally applicable to platforms that provide an efivarfs interface.
