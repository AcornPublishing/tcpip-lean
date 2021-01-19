/* PIC Ethernet Web server; runs on the Microchip PICDEM.net board
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */
/*
** v0.01 JPB 1/12/00 First version
** v0.02 JPB 7/12/00 Simple LCD driver added
** v0.04 JPB 10/12/00 Added simple Ethernet transmit
**                    Added simple EEROM check
** v0.05 JPB 17/12/00 Renamed from PICT77 to ETHERPIC
** v0.06 JPB 23/12/00 Got ARP working
** v0.07 JPB 26/12/00 Got Ping working
** v0.08 JPB 26/12/00 Improved Tx throughput
** v0.09 JPB 27/12/00 Added support for 64-byte ping
** v0.10 JPB 27/12/00 Added support for new (v3) PICDEM-NET board
** v0.11 JPB 28/12/00 Added preliminary TCP code
** v0.12 JPB 29/12/00 Added dummy daytime service
** v0.13 JPB 29/12/00 Started boot configuration
**                    Increased NIC RAM size from 20h to 40h pages
**                    Fixed ICMP checksum problem
** v0.14 JPB 30/12/00 Added RX_BUFFERED option (to eliminate Rx buffer!)
** v0.15 JPB 30/12/00 Added first Web page
** v0.16 JPB 30/12/00 Split internal & external Tx length words
** v0.17 JPB 1/1/01   Fixed bugs in TCP transmission and length calculation
**                    Improved debug output
** v0.18 JPB 3/1/01   Added UDP echo capability
**                    Added skeletal FTP for test
** v0.19 JPB 4/1/01   Added TFTP
** v0.20 JPB 5/1/01   Added TFTP receive
** v0.21 JPB 6/1/01   Added EEROM programming to TFTP receive
** v0.22 JPB 6/1/01   Stripped out unnecessary functions
**                    Added user configuration interface
** v0.23 JPB 7/1/01   Added non-volatile config
** v0.24 JPB 7/1/01   Added EGI variable substitution
**                    Reduced Ether RAM size to 8K
** v0.25 JPB 21/1/01  Switched from TFTP to Xmodem for EEROM programming
** v0.26 JPB 25/1/01  Added Fuji display initialisation
** v0.27 JPB 28/1/01  Adapted for new design of Web pages
**                    Sign on as ChipWeb
** v1.02 JPB 11/2/01  Added digital I/P
**                    Up-issued for release
** v1.03 JPB 5/3/01   Fixed problem with blank Web page ROM error msg
*/           

#define SIGNON  "ChipWeb v1.03\n"

#define DEBUG        0          // Set non-zero to enable diagnostic printout
#define OLD_BOARD    0          // Set non-zero if using old PICDEM.NET board
#define RX_BUFFERED  0          // Set non-zero if using Rx buffers 
#define CLOCK16      0          // Set non-zero if using 16 MHz clock */

#include <16F877.H>
#device *=16

#FUSES HS,NOWDT,NOPROTECT,PUT,BROWNOUT,NOLVP // PIC fuse settings
#ID CHECKSUM                                 // ID bytes are checksum
#ZERO_RAM                                    // Wipe RAM for safety
#use fast_io(A)                              // I'll set the direction bits
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)
#use fast_io(E)

#use I2C(MASTER, SDA=PIN_C4, SCL=PIN_C3, RESTART_WDT, FAST)

#define EEROM_ADDR   0xa0   // i2c addr for eerom

// Timer 1 trigger value; tick time = (1024 x DIV) / CPU_CLK
// 50 ms ticks with 7.3728 MHz clock requires divisor 45 prescale 8
#if CLOCK16
#define TIMER1_DIV  98
#use DELAY(CLOCK=16000000)
#else
#define TIMER1_DIV  120
#use DELAY(CLOCK=19660800)
#endif
#define TIMER1_SET  (T1_INTERNAL | T1_DIV_BY_8)

#use RS232 (BAUD=9600, XMIT=PIN_C6, RCV=PIN_C7, ERRORS)

#define WORD unsigned long  // Data type definitions
#define BOOL short          // 'short' is very short (1 bit) in this compiler
typedef union               // Longword definition (not a native type)
{
    BYTE b[4];
    WORD w[2];
    BYTE l;
} LWORD;

#byte   PORTA = 5               // Main I/O ports
#byte   PORTB = 6
#byte   PORTC = 7
#define ALL_OUT 0
#define ALL_IN  0xff

#if OLD_BOARD
#BIT DIAG_LED = PORTA.5
#else
#bit USER_BUTTON=PORTB.5        // User pushbutton
#bit USERLED1 = PORTA.2         // User LEDs
#bit USERLED2 = PORTA.3
#bit SYSLED   = PORTA.4         // System LED
#endif

#define LEDTIME 10              // Interval for toggling system LED

WORD adc1, adc2;                // Copy of current ADC values
int tickcount;                  // Timer tick count
BOOL ledon;                     // LED state
unsigned ledticks;              // LED tick count
WORD tpxdlen;                   // Length of external data in Tx frame

/* General prototypes */
void displays(BYTE b);

/* Local prototypes */
BOOL geticks(void);
BOOL timeout(int &var, int tout);
void scan_io(void);
BOOL read_nonvol(void);
BYTE csum_nonvol(void);
void user_config(void);

/* Character O/P for debugging */
#if DEBUG
#define DEBUG_PUTC(c) putchar(c)
#else
#define DEBUG_PUTC(c)
#endif

/* For PCM compiler, library code must be included (can't be linked) */
#include "\chipweb\pcm\p16_eth.h"
#include "\chipweb\pcm\p16_drv.h"
#include "\chipweb\pcm\p16_lcd.h"
#include "\chipweb\pcm\p16_ip.h"
#include "\chipweb\pcm\webrom.h"
#include "\chipweb\pcm\p16_http.h"
#include "\chipweb\pcm\p16_usr.h"

void main()
{
    WORD pcol;

    LCD_E = 0;                          // Disable LCD
    setup_adc_ports(RA0_ANALOG);        // RA0 analog I/P
    setup_adc(ADC_CLOCK_DIV_32);
    SET_ADC_CHANNEL(0);
    port_b_pullups(TRUE);               // Use pullups on port B
#if OLD_BOARD
    set_tris_a(0x01);                   // Set I/O on port A
#else
    set_tris_a(0x03);
#endif
    setup_timer_1(TIMER1_SET);          // Init timer
    timeout(ledticks, 0);
    reset_ether();                      // Reset Ethernet (to free data bus)
    init_lcd();                         // Init LCD
    disp_lcd = disp_serial = TRUE;      // Set display flags
    printf(displays, SIGNON);           // ..and sign on
    while (!read_nonvol() || !USER_BUTTON)  // If csum error, or button
    {           
        printf(displays, "Config ");
        user_config();                      // ..call user config
    }
    init_ether();                       // Init Ethernet
    displays('\n');                     // Display IP address
    disp_decbyte(myip.b[3]);
    displays('.');
    disp_decbyte(myip.b[2]);
    displays('.');
    disp_decbyte(myip.b[1]);
    displays('.');
    disp_decbyte(myip.b[0]);

    while (1)                       // Main loop..
    {
        scan_io();                  // Scan I/O, check timer
        rxin = rxout = 0;
        atend = 0;
        if (get_ether())            // Get Ethernet frame..
        {                           
            if (nicin.eth.pcol == PCOL_ARP)
                arp_recv();         // ..is it ARP?
            else if (nicin.eth.pcol == PCOL_IP)
                ip_recv();          // ..or is it IP?
        }
    }
}

/* Update the current tick count, return non-zero if changed */
BOOL geticks(void)
{
    static BYTE tc, lastc=0;

    tc = TIMER_1_HIGH - lastc;
    if (tc >= TIMER1_DIV)
    {
        tickcount++;
        lastc += TIMER1_DIV;
        return 1;
    }
    return 0;
}

/* Check for timeout using the given tick counter */
BOOL timeout(int &var, int tout)
{
    BOOL ret=0;

    if (!tout || tickcount-var>=tout)
    {
        var = tickcount;
        ret = 1;
    }
    return(ret);
}

/* Read ADC values
** Briefly enable RA1 as analog I/P, as this disables RA3 as digital O/P */
void read_adcs(void)
{
    adc1 = READ_ADC();                      // Get first value
    setup_adc_ports(RA0_RA1_RA3_ANALOG);    // Enable RA1 analog
    SET_ADC_CHANNEL(1);                     // Set multiplexer
    delay_us(10);                           // Allow to setle
    adc2 = READ_ADC();                      // Get 2nd value
    setup_adc_ports(RA0_ANALOG);            // Restore RA1 and multiplexer
    SET_ADC_CHANNEL(0);
}

/* Check timer, scan ADCs, toggle LED if timeout */
void scan_io(void)
{
    WORD w;
    
    restart_wdt();              // Kick watchdog
    geticks();                  // Get tick count
    read_adcs();                // Read ADC values
    if (timeout(ledticks, LEDTIME))   
        SYSLED = !SYSLED;       // Toggle system LED
}

/* EOF */                           
