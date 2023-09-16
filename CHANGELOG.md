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

1. GRUB parser: add `devicetree` command, to set boot options' dtb resource.

### Fixed

1. GRUB parser: spaces are now trimmed from fields, which may result from
   empty-variable expansion
