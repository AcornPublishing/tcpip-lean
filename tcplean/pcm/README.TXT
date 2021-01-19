PICWEB - miniature Web server for the PIC 16C76 or 16F876

Software copyright (c) Iosoft Ltd. 2000. All rights reserved.

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose.

PCM files
---------
These files are for the Custom Computer Services 'PCM' compiler and the 
Advanced Transdata RICE17A emulator - see the book for details.

The definition 'PORTBINV' will need to be set non-zero for boards which have
active-low outputs.

Use the 'load debugged file' option to load PICWEB.COD into the emulator; after
making any source-code changes, press F12 to compile and download into the
emulator.

The file PICWEB.HEX can be blown directly into a 16C76 device.

The ROM files are in the directory ../ROMDOCS. The file PICWEB.ROM is a pre-built
ROM image, which can be blown directly into a 24256 (or similar) serial ROM.

JPB 13/8/00
