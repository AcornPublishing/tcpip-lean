/* HiTech compiler-sepcific definitions for PIC16 and PIC18 */

#ifdef _PIC18
#include "pic18.h"
#if _HTC_VER_MAJOR_ == 8 && _HTC_VER_MINOR_ == 1
#define OLD_HT18    1       // Set non-zero if using HT-PIC18 v8.01
#endif
#else
#include "pic.h"
#endif
#include <stdio.h>
#include <string.h>

#define WORD unsigned int   // Data type definitions
#define BYTE unsigned char
#define INT8 signed char
#define BOOL bit
#define INT32 long

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

#ifndef MIN
#define MIN(a, b) (a>b ? b : a)
#define MAX(a, b) (a>b ? a : b)
#endif

#define COMPILERID  "h"     // String to be displayed after signon

#ifdef _PIC18
#if OLD_HT18                // Old Hitech PIC18 Config settings
__CONFIG(1, OSCSEN|FOSC1|UNPROTECT);
__CONFIG(2, BOREN | BORV0);
#else                       // New Hitech PIC18 Config settings
__CONFIG(1, OSCSEN & HSPLL & UNPROTECT);
__CONFIG(2, BOREN & BORV42 & WDTDIS);
#endif
__CONFIG(3, 0);
__CONFIG(4, STVREN);
#define PORTA_ADDR  0xF80   // I/O port addresses for PIC18
#define PORTB_ADDR  0xF81
#define PORTC_ADDR  0xF82
#define PORTD_ADDR  0xF83
#define PORTE_ADDR  0xF84
#define SSPCON      SSPCON1
#define BANK1               // No RAM banks for PIC18
#define BANK2
#define BANK3
#define FAR
#if OLD_HT18
static volatile bit TRISC4  @ (unsigned)&TRISC*8+4;
static volatile bit TRISC3  @ (unsigned)&TRISC*8+3;
#endif
static volatile bit STAT_RW @ (unsigned)&SSPSTAT*8+2;

#else                       // PIC16 config settings
__CONFIG(BODEN|FOSC1|CP0|CP1|CPD);
#define PORTA_ADDR 5        // I/O port addresses for PIC16
#define PORTB_ADDR 6
#define PORTC_ADDR 7
#define PORTD_ADDR 8
#define PORTE_ADDR 9
#define BANK1 bank1         // RAM banks for PIC16
#define BANK2 bank2
#define BANK3 bank3
#define FAR   far
#endif

#define SEPARATED           // No separation - functions always separate
#define LOCATE(var, addr)   // No LOCATE

#define ALL_OUT 0
#define ALL_IN  0xff

// Bit I/O definition macros; rather strange, for CCS compatibility!
#define DEFBIT_0(reg, name) static volatile bit name @ (unsigned)&reg*8 + 0;
#define DEFBIT_1(reg, name) static volatile bit name @ (unsigned)&reg*8 + 1;
#define DEFBIT_2(reg, name) static volatile bit name @ (unsigned)&reg*8 + 2;
#define DEFBIT_3(reg, name) static volatile bit name @ (unsigned)&reg*8 + 3;
#define DEFBIT_4(reg, name) static volatile bit name @ (unsigned)&reg*8 + 4;
#define DEFBIT_5(reg, name) static volatile bit name @ (unsigned)&reg*8 + 5;
#define DEFBIT_6(reg, name) static volatile bit name @ (unsigned)&reg*8 + 6;
#define DEFBIT_7(reg, name) static volatile bit name @ (unsigned)&reg*8 + 7;
// Byte I/O definition
#define DEFBYTE(name, reg) static volatile BYTE name @ reg;

#define port_b_pullups(on) RBPU = !on

#define set_tris_a(val) TRISA = val
#define set_tris_b(val) TRISB = val
#define set_tris_c(val) TRISC = val
#define set_tris_d(val) TRISD = val
#define set_tris_e(val) TRISE = val

// Ensure that EEPROM write cycle is complete..
#define write_eeprom(addr, val) eeprom_write(addr, val); while (WR)
#define read_eeprom(addr) eeprom_read(addr)

#define disable_interrupts(mode) GIE = 0; PEIE = 0
#define GLOBAL 0xff

#define SETUP_ADC(val)          ADCON0 = (ADCON0 & 0x38) | val
#define ADC_CLOCK_DIV_32        0x81
#define ADC_CLOCK_DIV_8         0x41
#define SET_ADC_CHANNEL(num)    ADCON0 = (ADCON0 & 0xc7) | (num << 3)
#define SETUP_ADC_PORTS(val)    ADCON1 = val
#define NO_ANALOGS              0x07
#define RA0_ANALOG              0x8E
#define RA0_RA1_RA3_ANALOG      0x84

// standard printf automatically uses putch()
#define PRINT(str) printf(str)
#define PRINTF2(str, val) printf(str, val)

// change getchar() so it doesn't echo serial char
#undef getchar
#define getchar getch
#define getch serial_getch
#define kbhit serial_kbhit

#define restart_wdt() asm("clrwdt")

#define TIMER_1_HIGH    TMR1H
#define T1_INTERNAL     5
#define T1_DIV_BY_2     0x10
#define T1_DIV_BY_4     0x20
#define T1_DIV_BY_8     0x30
#define setup_timer_1(val) T1CON = val

#define DELAY_ONE_CYCLE asm("nop")

// Rough time delay assuming 16 MHz clock
#if CPU_CLK < 20000000L
#define delay_us(x)  { unsigned char _del; _del = x>>1; while(_del--) ; }
#else
#define delay_us(x)  { unsigned char _del; _del = x; while(_del--) ; }
#endif

#define RX_INT_HANDLER static void interrupt

/* EOF */

