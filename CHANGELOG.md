# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

Change categories:

- Added
- Changed
- Deprecated
- Removed
- Fixed
- Security

## [Unreleased]

### Added

1. New Vietnamese translation, from Trung Lê <8@tle.id.au>.

2. New documentation for configuration formats

### Fixed

1. Fix builds against ncurses with `NCURSES_OPAQUE_MENU`

2. Fix menu item allocation bug in ncurses UI

## [1.15] - 2024-02-13

### Fixed

1. GRUB parser: bls: Fix NULL pointer dereference on empty values

2. GRUB parser: use new define syntax for yacc source

### Changed

1. pb-console: we now silence the kernel log messages printed to console
   earlier in setup

## [1.14] - 2023-09-20

### Added

1. GRUB parser: add `devicetree` command, to set boot options' dtb resource.

### Fixed

1. GRUB parser: spaces are now trimmed from fields, which may result from
   empty-variable expansion

2. Fix process exit detection; this resolves an issue where multiple payload
   downloads may complete (near-)simultaneously.

3. Network device MACs are now displayed in cases where the MAC address info
   was received after the initial device detection

4. Fix for talloc lifetime bug in the DHCP event handler

5. Fix an issue where the UI would lockup while the countdown runs

6. i18n: zh_CN: better wording for "golden side"

7. Allow missing ID_PATH udev attribute
