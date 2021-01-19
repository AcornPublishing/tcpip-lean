/* CCS compiler-sepcific definitions for PIC16F877 or PIC18C452 */

#ID CHECKSUM                // ID bytes are checksum
#ZERO_RAM                   // Wipe RAM for safety

#define WORD unsigned long  // Data type definitions
#define BOOL short          // 'short' is very short (1 bit) in this compiler

#ifndef MIN
#define MIN(a, b) (a>b ? b : a)
#define MAX(a, b) (a>b ? a : b)
#endif

#define COMPILERID  "c"     // String to be displayed after signon

#define HASHBYTE #BYTE      // Other compilers have problem with #byte
#define HASHDEVICE #device  // ..and #device, so wrap them in these macros

#ifdef __PCH__
#include <18C452.H>             // PIC18C452
HASHDEVICE ADC=10               // Put ADC in 10-bit mode
#define _PIC18
#FUSES NOPROTECT,BROWNOUT       // Config fuse settings
#define PORTA_ADDR 0xf80        // Main I/O port addrs for PIC18
#define PORTB_ADDR 0xf81
#define PORTC_ADDR 0xf82
#define PORTD_ADDR 0xf83
#define PORTE_ADDR 0xf84
#byte T1CON=0xfcd
#byte TIMER_1_HIGH=0xfcf
#define LOCATE(var, addr)
#include <string.h>
#else
#include <16F877.h>             // PIC16F877
#device *=16
HASHDEVICE ADC=10               // Put ADC in 10-bit mode
#FUSES HS,NOWDT,NOPROTECT,PUT,NOLVP    // Config fuse settings
#define PORTA_ADDR 5            // Main I/O port addrs for PIC16
#define PORTB_ADDR 6
#define PORTC_ADDR 7
#define PORTD_ADDR 8
#define PORTE_ADDR 9
#byte T1CON=0x10
#ifndef TIMER_1_HIGH
HASHBYTE TIMER_1_LOW=0x0e       // New CCS compiler has no 8-bit defs for timer1
HASHBYTE TIMER_1_HIGH=0x0f
#define LOCATE(var, addr) #LOCATE var=addr
#endif
#endif

#include <stdlib.h>

#byte   PORTA = PORTA_ADDR      // I/O port definitions
#byte   PORTB = PORTB_ADDR
#byte   PORTC = PORTC_ADDR
#byte   PORTD = PORTD_ADDR
#byte   PORTE = PORTE_ADDR

#define SEPARATED #separate
#define BANK1
#define BANK2
#define BANK3
#define FAR

#define ALL_OUT 0
#define ALL_IN  0xff

// Bit I/O definition macros; rather strange, for CCS compatibility!
#define DEFBIT_7(reg, name) #BIT name = reg.7
#define DEFBIT_6(reg, name) #BIT name = reg.6
#define DEFBIT_5(reg, name) #BIT name = reg.5
#define DEFBIT_4(reg, name) #BIT name = reg.4
#define DEFBIT_3(reg, name) #BIT name = reg.3
#define DEFBIT_2(reg, name) #BIT name = reg.2
#define DEFBIT_1(reg, name) #BIT name = reg.1
#define DEFBIT_0(reg, name) #BIT name = reg.0
#define DEFBYTE(reg, name) #BYTE name = reg

#use fast_io(A)                                 // I'll set the direction bits
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)
#use fast_io(E)

#define DELAY_ONE_CYCLE delay_cycles(1)

#use DELAY(CLOCK=CPU_CLK)
#use RS232 (BAUD=SER_BAUD, XMIT=PIN_C6, RCV=PIN_C7, ERRORS)

#ifdef _PIC18
#byte PIE1=0xf9d
#byte PIR1=0xf9e
#byte RCSTA=0xfab
#byte TXSTA=0xfac
#byte TXREG=0xfad
#byte RCREG=0xfae
#byte SPBRG=0xfaf
#byte INTCON=0xff2
#else
#byte PIE1=0x8c
#byte PIR1=0x0c
#byte RCSTA=0x18
#byte TXSTA=0x98
#byte TXREG=0x19
#byte RCREG=0x1a
#byte SPBRG=0x99
#byte INTCON=0x0b
#endif

#bit RCIF=PIR1.5
#bit TXIF=PIR1.4
#bit RCIE=PIE1.5
#bit TXIE=PIE1.4
#bit OERR=RCSTA.1
#bit FERR=RCSTA.2
#bit CREN=RCSTA.4
#bit TXEN=TXSTA.5

#define RX_INT_HANDLER #INT_RDA

#define init_serial()
#define init_i2c()

#define serial_putch(c) putchar(c)
//#define serial_kbhit(c) kbhit(c)

#define putstr(str) printf(putch, str)
#define put_ser(str) printf(str)
#define PRINTF2(str, val) printf(putch, str, val)

#use I2C(MASTER, SDA=PIN_C4, SCL=PIN_C3, RESTART_WDT, FAST)

/* EOF */

