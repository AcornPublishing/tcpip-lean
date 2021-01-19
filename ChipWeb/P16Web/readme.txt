P16Web v2.02 release notes
--------------------------

Welcome to P16Web, TCP/IP software for the Microchip PIC16xxx and
PIC18xxx series of microcontrollers.

This release is supplied with the 2nd edition of the 'TCP/IP Lean' book
by Jeremy Bentham, and contains all the source code for chapters 12-16
of that book.

The PIC16F877 and PIC18C452 devices are supported, using the Microchip
PICDEM.net (TM) development board for Ethernet communications.

An attempt has been made to support both the PIC16 and PIC18 families, using
both the Hitech and CCS compilers, though this has proved very difficult in
practice. At the time of writing, the PIC18 software tools ar relatively
immature, and it has been impossible to obtain a 'known good' release of
each compiler, with which this software can be tested. Furthermore, the
flash-programmable PIC18F parts are not yet available for testing, so
the UV-erasable PIC18C parts have to be used instead.

The specific compiler versions used are:

CCS PCW IDE version 3.7, which includes
    PCM v3.066 (for PIC16)
    PCH v3.066 (for PIC18)
Hitech PICC v7.87 PL2 (for PIC16)
Hitech PICC18 v8.11 PL1 (for PIC18)

The Hitech PIC18 compiler does compile P16WEB without any errors, but the
resulting binary image does not run correctly; the reason for this is
being investigated.

The following HEX images have been included:

P16WEBC.HEX  Web server for PIC16 (PCM compiler)
P16WEBH.HEX  Web server for PIC16 (PICC compiler)
P16WEB8C.HEX Web server with DHCP for PIC18 (PCH compiler)

Ensure the configuration fuse settings are correct before programming any
devices; see appendix D of 'TCP/IP Lean' 2nd edition.

The PIC18C parts have no EEPROM memory to store non-volatile information
such as IP address and serial number, so these values are fixed for a PIC18;
see PIC18_MYIP at the top of P16WEB.C, and FIXED_SERNUM in P16USR.C. This 
issue will be resolved as soon as flash-programmable PIC18F parts are 
available; until then, DHCP can be used to provide a dynamic IP address
(assuming a DHCP server is available), though the serial number will 
still be fixed, which means that the Ethernet MAC address is fixed,
and will need to be changed in the source code if more than one
board is to be run in the same network.

The PIC18 limitations will be removed as soon as PIC18F parts are available.

The PPP support is essentially 'work in progress', and will be subject to 
revision in the near future; check the Iosoft Ltd. Web site (www.iosoft.co.uk) 
for updates.

JPB 26/2/02

