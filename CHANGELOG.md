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
