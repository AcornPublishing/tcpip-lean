/* ChipWeb microcontroller TCP/IP package - Copyright (c) Iosoft Ltd 2001
**
** This source code is only licensed for distribution in the Iosoft ChipWeb
** package, and the purchaser of that package is granted the non-exclusive
** right to use the software for personal experimentation only, provided
** that this copyright notice is retained. All other rights are retained by
** Iosoft Ltd.
**
** Redistribution of this source code is not permitted. Binary images derived
** from the source code may only be redistributed if a commercial license is
** obtained; see www.iosoft.co.uk or email license@iosoft.co.uk
**
** The software is supplied 'as-is' for development purposes, without warranty
** of any kind, either expressed or implied, including, but not limited to,
** the implied warranties of merchantability and fitness for purpose.
** In no event will Iosoft Ltd. be liable for damages, including any general,
** special, incidental or consequential damages arising out of the use or
** inability to use the software, including, but not limited to, loss or
** corruption of data, or losses sustained by the developer or third parties,
** or failure of the software to operate with any other software or systems.
** This license shall be governed by the laws of England. */
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
** v1.04 JPB 25/4/01  Initial Hitech C version
** v1.05 JPB 1/7/01   Moved toward compatibility with both compilers
** v1.06 JPB 2/7/01   Removed compiler-specific call-by-reference
** v1.07 JPB 3/7/01   Further adaptation for compiler compatibility
** v1.08 JPB 4/7/01   Got Web server running with both Hitech & CCS
** v1.09 JPB 5/7/01   Adapted for PIC18 compatibility
** v1.10 JPB 6/7/01   Added PIC18 serial interface
** v1.11 JPB 6/7/01   Tidied up compiler-specific code
** v1.12 JPB 6/7/01   Reduced Hitech code size by eliminating printf
** v1.13 JPB 13/7/01  Fixed protocol byte-swap bug in Ether Tx
** v1.14 JPB 14/8/01  Tidied up for release
** v1.15 JPB 25/8/01  Re-introduced ChipVid mods
**                    Altered ChipVid I/O to suit Microchip PCB
** v1.16 JPB 26/8/01  Tweaked video resolution for NTSC compatibility
** v1.17 JPB 28/8/01  Simplified LCD driver code
**                    (for compatibility with latest PIC18 compiler)
** v1.18 JPB 27/9/01  Reintroduced PIC16 compatibility
**                    Changed UDP format; use with ChipVid v0.11 or later
** v1.19 JPB 1/10/01  Reintroduced code for old PIC16 board
** v1.20 JPB 4/10/01  Fixed bug in line numbering for single-block download
** v1.30 JPB 9/10/01  Added experimental Rx sliding buffer scheme
** v1.31 JPB 10/10/01 Added string-matching capability
**                    Reinstated video application
** v1.32 JPB 11/10/01 Reinstated Hitech compatibility
** v1.33 JPB 11/10/01 Added sliding Tx buffer
** v1.34 JPB 12/10/01 Reinstated daytime, echo and Web server
** v1.35 JPB 12/10/01 Reinstated video server
** v1.36 JPB 15/10/01 Reinstated Hitech compatibility
** v1.37 JPB 16/10/01 Split off video capture code
** v1.38 JPB 16/10/01 Split off TCP code
** v1.39 JPB 17/10/01 Resolved PIC18 compatibility problems
** v1.40 JPB 8/11/01  First trial of FED compiler
** v1.41 JPB 10/11/01 Added client ARP
**                    Changed timer to 16-bit
** v1.42 JPB 13/11/01 Preliminary TCP client
** v1.43 JPB 15/11/01 Preliminary daytime client working
** v1.50 JPB 21/11/01 Preliminary PPP code
** v1.51 JPB 21/11/01 Preliminary PPP frame transfers
** v1.52 JPB 23/11/01 Added IPCP state machine
** v1.53 JPB 25/11/01 Adapted for Hitech compatibility
**                    Resolved ping problem
** v1.54 JPB 28/11/01 Reintroduced Web server
** v1.55 JPB 28/11/01 Fixed TCP packet corruption
** v1.56 JPB 29/11/01 Adapted Ethernet interface for PPP compatibility
** v1.57 JPB 29/11/01 Fixed PIC18 user config looping
** v1.58 JPB 3/12/01  Incorporated DHCP code
** v1.59 JPB 4/12/01  Fixed unicast/broadcast DHCP problem
** v1.60 JPB 6/12/01  Fixed DHCP rebind timing problem
** v1.61 JPB 7/12/01  Resolved address resolution problems
** v1.62 JPB 8/12/01  Added client TCP capability
** v1.63 JPB 12/12/01 Added TCP echo server
**                    Added Tx drop capability to Ethernet driver
** v1.64 JPB 13/12/01 Added Telnet client
** v1.65 JPB 13/12/01 Simplified packet length computation
** v1.66 JPB 13/12/01 Removed global TCP Tx flags
** v1.67 JPB 14/12/01 Added TCP timeout
** v1.68 JPB 16/12/01 Changed TCP timeout to use 2nd Ethernet buffer
** v1.69 JPB 18/12/01 Initial SMTP tests
** v1.70 JPB 18/12/01 Re-tested HTTP, UDP DHCP and video drivers
** v1.71 JPB 18/12/01 Simplified TCP client
** v1.72 JPB 19/12/01 SMTP client operational
** v1.73 JPB 19/12/01 Fixed problem with TCP simultaneous close
** v1.74 JPB 20/12/01 Fixed bug in dual-buffer Ethernet driver
**                    Added user LED SMTP status display
** v1.75 JPB 22/12/01 Adapted PPP code, and serial interrupt capability
** v1.76 JPB 22/12/01 Interrupts temporarily disabled due to compiler problems
** v1.77 JPB 24/12/01 Added comments to PPP and TCP client code
**                    Added POP3 client
** v1.78 JPB 24/12/01 Tidied up IP address value decoding
**                    Added port number (SMTP/POP3) to TCP client startup
** v1.79 JPB 4/1/02   Increased NIC Tx buffer size when using two buffers
** v2.01 JPB 28/1/02  Removed spurious while(0) from xmodem handler
**                    Renamed from p16data.c to p16web.c
** v2.02 JPB 22/2/02  Removed 'stdlib.h' from Hitech include file
*/

#define SIGNON  "P16Web  v2.02"

/* TCP protocols: enabled if non-zero */
#define INCLUDE_HTTP 1      // Enable HTTP Web server
#define INCLUDE_SMTP 0      // Enable SMTP email client
#define INCLUDE_POP3 0      // Enable POP3 email client

/* UDP protocols: enabled if non-zero */
#define INCLUDE_DHCP 0      // Enable DHCP auto-confiuration
#define INCLUDE_VID  0      // Video demo: 1 to enable, 2 if old video h/w
#define INCLUDE_TIME 0      // Time client: polls server routinely

/* Low-level drivers: enabled if non-zero */
#define INCLUDE_ETH  1      // Ethernet (not PPP) driver
#define INCLUDE_DIAL 0      // Dial out on modem (needs PPP)
#define INCLUDE_LCD  1      // Set non-zero to include LCD driver
#define INCLUDE_CFG  1      // Set non-zero to include IP & serial num config

#define INCLUDE_TCP         (INCLUDE_HTTP || INCLUDE_SMTP || INCLUDE_POP3)
#define INCLUDE_TCP_CLIENT  (INCLUDE_SMTP || INCLUDE_POP3)
#define INCLUDE_UDP         (INCLUDE_DHCP || INCLUDE_VID || INCLUDE_TIME)
#define INCLUDE_PPP         (INCLUDE_ETH == 0)

#define DEBUG        0      // Set non-zero to enable diagnostic printout

#if INCLUDE_TCP_CLIENT      // TCP client needs 2 network Tx buffers
#define NET_TXBUFFERS   2
#else
#define NET_TXBUFFERS   1   // UDP and TCP server need only 1 Tx buffer
#endif

#define PIC18_MYIP   10,1,1,99  // My IP addr if PIC18 (no EEPROM yet!)
#define NETMASK_ADDR 255,0,0,0  // Subnet mask value
#define ROUTER_ADDR  10,1,1,100 // IP address for router
#define TSERVER_ADDR 10,1,1,1       // IP addr for local time server
//#define TSERVER_ADDR 158,152,1,76   // IP addr for ntp.demon.co.uk time server

#if INCLUDE_PPP
#define SER_BAUD     38400      // Serial baud rate for PPP
#define SERIAL_INTERRUPTS 1     // ..with serial interrupts
#else
#define SER_BAUD     9600       // Serial baud rate if Ethernet networking
#define SERIAL_INTERRUPTS 0     // ..with no serial interrupts
#endif

#define TIMER1_RATE  20         // Tick rate, in ticks/sec
#define SECTICKS     TIMER1_RATE// Number of ticks in a second

#define CPU_CLK      19660800   // Standard PICDEM.net clock
//#define CPU_CLK      32000000   // Non-standard PICDEM.net clock for video

#ifdef HI_TECH_C
#include "ht_p16.h"     // Definitions for Hitech C
#include "ht_utils.c"   // Utilities for Hitech C
#endif
#ifdef _FEDPICC
#include "fed_p16.h"    // Defnitions for FED C
#endif
#ifdef __PCM__
#include "ccs_p16.h"    // Definitions for CCS C
#endif
#ifdef __PCH__
#include "ccs_p16.h"    // Definitions for CCS C
#endif

// Timer 1 trigger value; tick time = (1024 x DIV) / CPU_CLK
#define TIMER1_PRESCALE 8
#define TIMER1_DIV      (CPU_CLK / (1024L * TIMER1_PRESCALE * TIMER1_RATE))
#if TIMER1_PRESCALE==1
#define TIMER1_SET  (T1_INTERNAL | T1_DIV_BY_1)
#endif
#if TIMER1_PRESCALE==8
#define TIMER1_SET  (T1_INTERNAL | T1_DIV_BY_8)
#endif

#define LEDTIME (SECTICKS/2)    // Interval to toggle system LED (in ticks)

#define EEROM_ADDR   0xa0       // i2c addr for external EEROM

typedef union                   // Longword (not native for old CCS compiler)
{
    BYTE b[4];
    WORD w[2];
    unsigned INT32 l;
} LWORD_;

#define LWORD BANK1 LWORD_      // Put all Hitech longwords in bank 2

// I/O definitions
#define USERLEDON   0
#define USERLEDOFF  1
DEFBIT_2(PORTA, USERLED1)       // User LED 1, 0 when on
DEFBIT_3(PORTA, USERLED2)       // User LED 2, 0 when on
DEFBIT_4(PORTA, SYSLED)         // System LED, 0 when on
DEFBIT_5(PORTB, USER_BUTTON)    // User pushbutton, 0 when pressed

// Global variables
BANK3 WORD adc1, adc2;          // Copy of current ADC values
BANK3 WORD tickcount;           // Timer tick count
WORD ledticks;                  // LED tick count
BOOL ledon;                     // Status LED state
BOOL flashled1, flashled2;      // Flags to enable flashing of user LEDs
BOOL button_iostate;            // Current I/O state of user button
BOOL button_down;               // Debounced state of user button
BOOL button_in;                 // State variable to detect button change

BOOL arp_resp;                  // Flag indicating ARP response received
BOOL net_ok;                    // Flag signifying network is available
LWORD myip;                     // My IP adress
LWORD hostip;                   // Host IP address
LWORD netmask;                  // Subnet mask value
LWORD router_ip;                // Router IP address

BOOL checkflag;                 // Checksum flag & byte values
BANK1 BYTE checkhi, checklo;

#if INCLUDE_ETH
BYTE router_eth[6];             // Router Ethernet address
BOOL router_arp_ok;             // Flag to show router has been ARPed
#endif

// Flags to enable display O/Ps
BOOL print_lcd, print_serial, print_net;
#define NET_SERIAL_OUTPUT   print_net=print_serial=TRUE, print_lcd=FALSE
#define NET_OUTPUT          print_net=TRUE, print_lcd=print_serial=FALSE
#define SERIAL_OUTPUT       print_serial=TRUE, print_lcd=print_net=FALSE
#define LCD_OUTPUT          print_lcd=TRUE, print_serial=print_net=FALSE
#define LCD_SERIAL_OUTPUT   print_lcd=print_serial=TRUE, print_net=FALSE

/* Prototypes */
SEPARATED void check_event(void);
BOOL geticks(void);
BOOL timeout(WORD *varp, WORD tout);
void scan_io(void);
BOOL read_nonvol(void);
BYTE csum_nonvol(void);
BOOL user_config(void);
void flush_serial(void);
void putch(BYTE b);
void init_ip(LWORD *lwp, BYTE a, BYTE b, BYTE c, BYTE d);
void disp_myip(void);
BYTE disp_decword(WORD w);
void disp_hexbyte(BYTE b);
void disp_hexdig(BYTE b);
SEPARATED BOOL udp_recv(void);

/* Character O/P for debugging */
#if DEBUG
#define DEBUG_PUTC(c) serial_putch(c)
#else
#define DEBUG_PUTC(c)
#endif

/* Include library code (CCS can't link it) */
#include "p16_ser.c"
#if INCLUDE_LCD
#include "p16_lcd.c"
#else
DEFBIT_5(PORTA, LCD_E)
#endif
#if INCLUDE_ETH
#include "p16_eth.c"
#endif // ETH
#include "p16_drv.c"
#if INCLUDE_PPP
#include "p16_ppp.c"
#if INCLUDE_DIAL
#include "p16_dial.c"
#endif // DIAL
#endif // PPP
#include "p16_ip.c"
#if INCLUDE_TCP
#include "p16_tcp.c"
#if INCLUDE_HTTP
#include "webrom.h"
#include "p16_http.c"
#endif // HTTP
#if INCLUDE_TCP_CLIENT
#include "p16_mail.c"
#include "p16_tcpc.c"
#endif // TCP_CLIENT
#endif // TCP
#include "p16_usr.c"
#if INCLUDE_UDP
#include "p16_udp.c"
#if INCLUDE_DHCP
#include "p16_dhcp.c"
#endif // DHCP
#if INCLUDE_TIME
#include "p16_time.c"
#endif // TIME
#if INCLUDE_VID
#include "p16_cap.c"
#endif // VID
#endif // UDP

// Default TRIS values for I/O ports
// May be overriden by included files
#ifndef TRISA_VAL
#define TRISA_VAL   0x03        // Port A, bit 0 and 1 analog I/Ps
#endif
#ifndef TRISB_VAL
#define TRISB_VAL   0x20        // Port B, bit 5 pushbutton I/P
#endif
#ifndef TRISC_VAL
#define TRISC_VAL   (ALL_IN)    // Port C, RS232, i2c, etc
#endif
#ifndef TRISD_VAL
#define TRISD_VAL   (ALL_IN)    // Port D, data bus
#endif
#ifndef TRISE_VAL
#define TRISE_VAL   (ALL_OUT)   // Port E, NIC control lines
#endif

void main(void)
{
    disable_interrupts(GLOBAL);         // Ensure interrupts off
    LCD_E = 0;                          // Disable LCD
    SETUP_ADC_PORTS(RA0_ANALOG);        // RA0 analog I/P
    set_tris_a(TRISA_VAL);
    SETUP_ADC(ADC_CLOCK_DIV_32);
    SET_ADC_CHANNEL(0);

    PORTC = 0;
    set_tris_c(TRISC_VAL);
    set_tris_d(TRISD_VAL);

    NIC_IOW_ = NIC_IOR_ = 1;            // Disable NIC
    NIC_RESET = 1;
    set_tris_e(TRISE_VAL);

    init_serial();                      // Set up serial
#if SERIAL_INTERRUPTS
    enable_serints();
#endif
    init_i2c();                         // ..and i2c bus
    port_b_pullups(TRUE);               // Use pullups on port B

    TIMER_1_HIGH = 0;
    setup_timer_1(TIMER1_SET);          // Init timer
    T1CON &= 0x7f;
    timeout(&ledticks, 0);              // Force timeout of LED timer
    button_iostate = 1;                 // Init button debouce variables
    button_down = button_in = 0;
    USERLED1 = 1;                       // User LEDs off
    USERLED2 = 1;
#if INCLUDE_LCD
    init_lcd();                         // Init LCD
#endif
#if INCLUDE_TIME
    init_time();
#endif
#if INCLUDE_ETH
    LCD_SERIAL_OUTPUT;                  // Set display flags
#else
    LCD_OUTPUT;                         // Suppress serial signon if PPP
#endif
    putstr(SIGNON);                     // Sign on
    putstr(COMPILERID);
    putch('\n');
#ifdef FIXED_CONFIG
    read_nonvol();
    if (!USER_BUTTON)
        putstr("Fixed config");
    while (!USER_BUTTON) ;
#else
    while (!read_nonvol() || !USER_BUTTON)  // If csum error, or button
    {
        putstr("Config ");
        user_config();                      // ..call user config
    }
#endif
    init_net();                         // Init Ethernet or PPP
#if INCLUDE_ETH
#if INCLUDE_DHCP
    myip.l = 0;                         // Null IP addr for DHCP..
    disp_myip();                        // ..display it
    putstr(" DHCP");
#else
    net_ok = 1;                         // If static IP addr, network is OK
    init_ip(&router_ip, ROUTER_ADDR);   // Initialise router addr
    init_ip(&netmask, NETMASK_ADDR);    // ..and subnet mask
    disp_myip();                        // Display my IP addr
#endif
#endif
#if INCLUDE_PPP
    putstr("PPP 38400 baud");
#endif
#if INCLUDE_POP3
    put_ser("\r\nPress any key to run POP3 client..");
#endif
    put_ser("\r\n");

    while (1)                       // Main loop..
    {
        scan_io();                  // Scan I/O, check timer
#if INCLUDE_DHCP
        check_dhcp();               // Check DHCP lease expiry
#endif
#if INCLUDE_TIME
        check_time();               // Check network real-time clock
#endif
#if INCLUDE_VID
        check_cap();
#endif
#if INCLUDE_SMTP
        check_client();
        if (button_down != button_in)
        {
            button_in = button_down;
            if (button_down)
                start_client(SMTP_PORT);
        }
#endif
#if INCLUDE_POP3
        check_client();
        if (kbhit())
        {
            getch();
            start_client(POP3_PORT);
        }
#endif
        if (get_net())              // If frame has arrived..
        {
            init_txbuff(0);         // ..and Tx buffer
#if INCLUDE_ETH
            if (host.eth.pcol == PCOL_ARP)
                arp_recv();         // ..is it ARP?
            if (host.eth.pcol == PCOL_IP && ip_recv())
#else
            if (ip_recv())
#endif
            {                       // ..or is it IP?
// Launch protocol handler here (and not in ip_recv) to keep stack usage low
                if (ipcol == PICMP)         // ICMP?
                    icmp_recv();            // ..call ping handler
#if INCLUDE_UDP
                else if (ipcol == PUDP)     // UDP datagram?
                    udp_recv();             // ..call UDP handler
#endif
#if INCLUDE_TCP
                else if (ipcol == PTCP)     // TCP segment?
                {
                    tcp_recv();             // ..call TCP handler
                }
#endif
            }
            net_rxin = 0;
        }
    }
}

/* Update the current tick count, return non-zero if changed
** Avoid using boolean local variables, to keep Hitech happy */
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

/* Check for timeout using the given tick counter and timeout value
** Force timeout if timeout value is zero */
BOOL timeout(WORD *varp, WORD tout)
{
    WORD diff;

    diff = tickcount - *varp;
    if (!tout || diff >= tout)
    {
        *varp = tickcount;
        return 1;
    }
    return 0;
}

/* Read ADC values
** Briefly enable RA1 as analog I/P, as this disables RA3 as digital O/P */
void get_adcvals(void)
{
    adc1 = read_adc();                      // Get first value
#if !INCLUDE_VID
    SETUP_ADC_PORTS(RA0_RA1_RA3_ANALOG);    // Enable RA1 analog
    SET_ADC_CHANNEL(1);                     // Set multiplexer
    delay_us(10);                           // Allow to setle
    adc2 = read_adc();                      // Get 2nd value
    SETUP_ADC_PORTS(RA0_ANALOG);            // Restore RA1 and multiplexer
    SET_ADC_CHANNEL(0);
#endif
}

/* Check timer, scan ADCs, toggle LED if timeout */
void scan_io(void)
{
    restart_wdt();              // Kick watchdog
    if (geticks())              // Get tick count
    {                           // Debounce user pushutton
        if (USER_BUTTON == button_iostate)
            button_down = !button_iostate;
        else
            button_iostate = !button_iostate;
        if (tickcount & 1)
        {
            if (flashled1)      // Fast flash user LEDs
                USERLED1 = !USERLED1;
            if (flashled2)
                USERLED2 = !USERLED2;
        }
    }
    get_adcvals();              // Read ADC values
    if (timeout(&ledticks, LEDTIME))
        SYSLED = !SYSLED;       // Toggle system LED
}

/* Display handler; redirects O/P char to LCD, serial, network */
void putch(BYTE b)
{
#if INCLUDE_LCD
    if (print_lcd)              // O/P to LCD
    {
        if (b == '\r')
            lcd_cmd(LCD_SETPOS);
        else if (b == '\n')
            lcd_cmd(LCD_SETPOS + LCD_LINE2);
        else
            lcd_char(b);
    }
#endif
    if (print_serial)           // O/P to serial
    {
        if (b == '\n')
            serial_putch('\r');
        serial_putch(b);
    }
    if (print_net)
    {
        put_byte(b);            // O/P to network Tx buffer, with checksum
    }
}

/* Flush the serial buffer */
void flush_serial(void)
{
    while (serial_kbhit())
        serial_getch();
}

/* Initialise an IP address using the given constants */
void init_ip(LWORD *lwp, BYTE a, BYTE b, BYTE c, BYTE d)
{
    lwp->b[0] = d;
    lwp->b[1] = c;
    lwp->b[2] = b;
    lwp->b[3] = a;
}

/* Display my IP address */
void disp_myip(void)
{
    print_lcd = print_serial = TRUE;    // Set display flags
    print_net = FALSE;
    putch('\n');                        // Display IP address
#if INCLUDE_LCD
    lcd_clearline(2);
#endif
    disp_decword(myip.b[3]);
    putch('.');
    disp_decword(myip.b[2]);
    putch('.');
    disp_decword(myip.b[1]);
    putch('.');
    disp_decword(myip.b[0]);
}

/* Display a word in unsigned decimal format
** Return the digit count */
BYTE disp_decword(WORD w)
{
    BYTE count, n=0, d[5];

    do
    {
        d[n++] = (BYTE)(w % 10) + '0';
        w /= 10;
    } while(w);
    count = n;
    while (n)
        putch(d[--n]);
    return(count);
}
#if DEBUG
/* Display a byte in hexadecimal format */
void disp_hexbyte(BYTE b)
{
    disp_hexdig(b >> 4);
    disp_hexdig(b);
}

/* Display a hexadecimal digit */
void disp_hexdig(BYTE b)
{
    b &= 0x0f;
    if (b <= 9)
        putch(b + '0');
    else
        putch(b + '7');
}
#endif
/* EOF */
