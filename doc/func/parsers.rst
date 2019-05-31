Configuration Parsers
=====================

Petitboot can read a variety of configuration formats. Generally it does this in one of two ways:

Bison/Flex Parsers
------------------

The "grub2" and "native" parsers are implemented with the Bison parser and Flex lexer combo to define a grammar describing the configuration format. These parsers can hook into other Petitboot or C code as needed but for most scenarios just need a way to resolve resources (resource.h) and create boot options (device-handler.h).

Writing a parser/grammar this way can be a bit more difficult at first but does lend itself to a more robust solution in the long term.

Process-Pair Parsers
--------------------

Other parsers such as for PXE and SYSLINUX use a simpler mechanism provided by `parser-conf.c` In short this provides a way to load the configuration into a buffer and process each line as a key:value format. Where applicable the parser can provide it's own callbacks for these functions.

This method is a lot easier to quickly construct a parser, especially for formats with relatively basic formats.
