TCP/IP Lean: Web servers for Embedded Systems
---------------------------------------------

Software copyright (c) Iosoft Ltd. 2000. All rights reserved.

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose.

Structure of the CDROM
----------------------

There is a single top-level directory on the CD, called TCPLEAN. This has
the following subdirectories

BC31    Borland C v3.1 files
BC45    Borland C v4.5 files
DJGPP   DJGPP (GNU compiler plus RHIDE) files
PCM     'C' and project files for the PIC implementation
ROMDOCS Web pages for the PICWEB server
SOURCE  Source code for all PC projects
VC6     Visual C v6 files
WEBDOCS Web pages for the WEBSERVE and WEB_EGI utilities

Please note that these are the only compilers I've used when checking the code,
and not all compilers support all network interfaces - see the book for details.

Installation
------------
Copy the contents of the TCPLEAN directory, and all its subdirectories, to
a directory C:\TCPLEAN on your hard drive. An alternative destination directory
can be used, but some adaptation of the project files may be necessary.

TAKE CARE BEFORE RUNNING ANY OF THE UTILITIES:

1. Study the relevant chapter in the book to discover what the utility does.
2. Check this README file for any update information.
3. Set up an isolated 'scratch' network so your experimentation can do no harm.
4. Set the configuration file entries to match your hardware configuration.
5. Make sure that your operating system isn't accessing that network hardware.

Be aware that any errors in step 4 or 5 could cause a system lock-up, as the
utility might access hardware locations it shouldn't, or might fight the OS
for control of the hardware. The easiest option may be to boot the system into
DOS, or use a DOS-based Single Board Computer for your initial experimentation.

I have deliberately not included a default configuration file 'tcplean.cfg',
but have provided some examples:

ETHER.CFG    NE2000-compatible Ethernet card at 280h (direct-drive)
SLIP.CFG     SLIP link on COM2 at 38400 baud
ETHER3.CFG   3COM 3C509 Ethernet card at 300h (direct-drive, coax i/f only)
ETHERP.CFG   Ethernet packet driver interface
SLIPP.CFG    SLIP packet driver interface
SLIP2.CFG    Twin SLIP interfaces (for NETMON to display bi-directional serial)
ROUTER3.CFG  Triple network interfaces for router

To use these files, modify as necessary and use the '-c' option in the utility
command line, e.g.

    cd \tcplean
    datagram -c ether

The default configuration file can be renamed to TCPLEAN.CFG, in which case it
will be used automatically by all the utilities (except ROUTER) if no other
config file is specified. The router configuration file is generally quite
different, since it must define at least two network interfaces, so the default
name is 'router.cfg'.

The SCRATCHP utility has a dissimilar interface to all the others (e.g. you
don't use the -c option to specify a configuration file); refer to the book
for details.

Running the utilities
---------------------
The home TCPLEAN directory has an executable copy of the Borland versions of
the utilities (the BC31 or BC45 versions with the debug information removed).

The compiler-specific directories also have executable versions, so it is
possible to check serveral versions in quick succession:

    cd \tcplean          (TCP/IP Lean home directory)
    ping 10.1.1.1        (BC31 version)
    bc31\ping 10.1.1.1   (ditto)
    bc45\ping 101.1.1.1  (BC45 version)
    vc6\ping 10.1.1.1    (Visual C version)
    djgpp\ping 10.1.1.1  (DJGPP version)

This assumes that a default configuration file TCPLEAN.CFG has been set up
in the \TCPLEAN directory.

All versions can be launched from a Win95 or Win98 DOS box. The Borland
versions can run on a DOS-only system; the DJGPP version needs extended DOS,
and the Visual C version needs Win32 (e.g. Windows 95 or 98).

The SLIP driver for DOS uses a direct hardware interface, while the Win32
version uses system calls. If using the DOS version under Windows, be sure to
close the DOS box after use, otherwise it will continue to monopolise the
serial hardware, and cause the Windows serial driver initialisation to fail.

If nothing seems to be happening when running a utility, use the '-v' (verbose)
option to check for any transmission or reception.

Rebuilding the utilities
------------------------
Each compiler-specific directory contains the appropriate project files for
that development environment; just load the project file for a utility, and you
should be able to rebuild it. For the first rebuild, it is worth making a
trivial change (e.g. to the version number), and checking that the rebuilt
version still performs the same as the original.

When rebuilding the Visual C executables, be sure to set the 'active
configuration' to 'Win32 release'. If this is set to 'debug', the executable
is generated in a DEBUG sub-directory, so the executable in the VC6 directory
will remain unchanged. If in doubt, check the file timestamps to make sure the
executable has been updated.

Borland C v3.1 compatibility is included for historic reasons; it uses the
'small' memory model, which is too small for the WEB_EGI utility (which compiles
but won't run). If you must recompile this utility under BC31, change to the
'large' memory model, and do a full rebuild.

Packet driver
-------------
The Borland versions have the ability to access the network hardware through
a packet driver, which is usually provided by the hardware manufacturer.
This must be loaded before the utility is run; see the top of ETHERP.CFG
and SLIPP.CFG for examples.

Packet drivers (including source code) are also available from Crynwr Software,
www.crynwr.com

PIC Web server
--------------
The PIC software is completely separate from all the other source code; the
source and project files are in the PCM directory. Due to the absence of a
linker, the libary source code is contained in header files, which are included
into the main source file at compile-time.

The directory ROMDOCS contains the Web page files for this project. See the
README.TXT file in the PCM directory for more information.

Miscellaneous
-------------
The 32-bit CRC utility CRC32.EXE has been included, so that you can compute the
CRC for a source file, and compare it with the value in the project CRC file,
e.g.

    crc32 source\ping.c

currently produces a CRC value of DCC5D3CC, which matches the value in PING.CRC.

If you feel like exploring the world of JavaScript, take a look at
WEBDOCS\JSGRAPH.HTM and  WEBDOCS\JSBAR.HTM. The whole topic of JavaScript didn't
quite make it into the book, but is an interesting way of producing graphics
with very little effort by the server.

Support
-------
I would like to be able to provide on-line support for the code in this book,
but have no free time to do this, so unfortunately am unable to respond
to any requests for further information.

When time permits, I will post bug reports and software updates on the Iosoft
Web site www.iosoft.co.uk. If you find a bug that isn't mentioned there, please
report it to tcplean@iosoft.co.uk, with a concise illustration of the problem,
and I will investigate it as soon as possible.

J.P.Bentham  Iosoft Ltd. July 5, 2000
