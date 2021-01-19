/* Miniature Web server for PIC 16c76 Copyright (c) Iosoft Ltd 2000
**
** This software is only licensed for distribution with the book 'TCP/IP Lean',
** and may only be used for personal experimentation by the purchaser
** of that book, on condition that this copyright notice is retained.
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */

/* Revisions:
** v0.01 JPB 28/2/00
** v0.02 - v0.27 deleted to save space
** v0.28 JPB 9/5/00  Added run-time variable substitution
** v0.29 JPB 11/5/00 Added temperature variable
** v0.30 JPB 11/5/00 Added time-setting capability
** v0.31 JPB 12/5/00 Added provision for analogue temperature sensor
**                   Added digital O/Ps
** v0.32 JPB 16/5/00 Changed from 100ms to 50ms ticks
** v0.33 JPB 18/5/00 Removed analogue temp sensor, added PORT A digital I/Ps 
** v0.34 JPB 4/6/00  Added PORT B invert capability
*/

#define TXDROP   0          // Set to 4 to drop 1-in-4 Tx frames for test
#define PORTBINV 0          // Set to 1 to invert port B O/Ps

#include <16c76.h>          // CPU definitions, must be followed by..
#device *=16                             // ..enable 16-bit (!) data pointers
#FUSES HS,NOWDT,NOPROTECT,PUT,BROWNOUT   // PIC fuse settings
#ID CHECKSUM                             // ID bytes are checksum
#ZERO_RAM                                // Wipe RAM for safety

#use DELAY(CLOCK=7372800)   // CPU clock frequency 7.3728 MHz
#use RS232 (BAUD=38400, XMIT=PIN_C6, RCV=PIN_C7, RESTART_WDT, ERRORS)
#bit TX_READY =      0x98.1 // Tx ready status bit

#define WORD unsigned long  // Data type definitions
#define BOOL short          // 'short' is very short (1 bit) in this compiler
typedef union               // Longword definition (not a native type)
{
    BYTE b[4];
    WORD w[2];
    BYTE l;
} LWORD;

#define PCOL_ICMP   1       // IP protocol values
#define PCOL_TCP    6
#define IPHDR_LEN   20      // IP, ICMP and TCP header lengths
#define TCPHDR_LEN  20
#define ICMPHDR_LEN 4       // Only include type, code & checksum in ICMP hdr

BYTE ipcol;                 // IP protocol byte
LWORD local, remote;        // Local & remote IP addresses
WORD locport, remport;      // ..and TCP port numbers
LWORD rseq, rack;           // TCP sequence & acknowledge values
WORD concount;              // Connection count (for high word of my seq num)
BYTE rflags, tflags;        // Rx and Tx flags
signed long iplen;          // Incoming IP length word
WORD rpdlen;                // Length of user data in incoming Rx packet
WORD tpdlen;                // Length of user data in outgoing Tx packet
BYTE portval;               // Value of port B O/Ps
BYTE hashmask, barmask;     // Mask values for EGI '#' and '|' variables
BOOL inv_byte;              // Flag to 'invert' the HTML O/P for EGI vars

#include <ctype.h>
#include "\picc\io\picslip.h"   // Include SLIP I/O functions (no linker!)
#include "\picc\io\pictcp.h"    // ..and TCP functions
#include "\picc\io\webrom.h"    // ..and ROM filesystem definitions

#use fast_io(A)             // I'll set the direction bits on I/O ports
#use fast_io(B)
#use fast_io(C)

#byte   PORTA=5             // Main I/O ports
#byte   PORTB=6
#byte   PORTC=7
#define ALL_OUT     0       // Direction (TRIS) values
#define ALL_IN      0xff

// Timer 1 trigger value; tick time = (1024 x DIV) / CPU_CLK
// 50 ms ticks with 7.3728 MHz clock requires divisor 45 prescale 8
#define TIMER1_DIV   45
#define TIMER1_SET   (T1_INTERNAL | T1_DIV_BY_8)

#use I2C (MASTER, SDA=PIN_C4, SCL=PIN_C3, RESTART_WDT, FAST)
#define SENSOR_ADDR  0x9e   // i2c addr for temperature sensor
#define EEROM_ADDR   0xa0   // i2c addr for eerom
#define RTC_ADDR     0xa2   // i2c addr for real-time clock

#define DIAG_LED     PIN_C2 // Diagnostic LED Pin ident

#define TFIN        0x01    // Option flags: no more data
#define TSYN        0x02    //           sync sequence nums
#define TRST        0x04    //           reset connection
#define TPUSH       0x08    //           push buffered data
#define TACK        0x10    //           acknowledgement
#define TURGE       0x20    //           urgent

#define DAYPORT     13      // TCP Port numbers: daytime & HHTP
#define HTTPORT     80

#define LEDONTIME   2       // Ticks for LED on
#define LEDOFFTIME  100     // Ticks for LED off
BOOL ledon;                 // Diagnostic LED state
int ledonticks, ledoffticks;// LED tick counts
int tickcount;              // Timer tick count

#define DAYTIME_LEN     10  // Format string for daytime response
#define DAYTIME_STR     "%02x:%02x:%02x\r\n"

#define HTTP_FAIL_LEN   34  // HTTP string for internal error
#define HTTP_FAIL       "HTTP/ 200 OK\r\n\r\nPICWEB ROM error\r\n"

#define HTML_OK_LEN     44  // HTTP header for HTML text
#define HTML_OK         "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"

#define TEXT_OK_LEN     45  // HTTP header for plain text
#define TEXT_OK         "HTTP/1.0 200 OK\r\nContent-type: text/plain\r\n\r\n"

#define MAXFILES    100     // Limit on ROM file count (to stop runaway)
typedef union               // ROM file directory entry format
{
    ROM_FNAME f;                // Union of filename..
    BYTE b[sizeof(ROM_FNAME)];  // ..with byte values for i2c transfer
} ROM_DIR;

ROM_DIR romdir;             // Storage for one directory entry
int fileidx;                // Index of current file (1=first, 0=error)

typedef struct
{                           // Real-Time Clock (RTC) register format
    BYTE secs:4, sec10:4;       // BCD seconds
    BYTE mins:4, min10:4;       // BCD minutes
    BYTE hours:4, hour10:4;     // BCD hours
} RTC_DATA;
typedef union
{                           // Union of RTC registers..
    RTC_DATA d;
    BYTE b[3];              // ..with byte data for i2c transfer
} RTC_DATA_B;

RTC_DATA_B rtc;             // Storage for current time from RTC
BYTE templo, temphi;        // Storage for current temperature (hi/lo bytes)

/* Protoypes */
void daytime_rx(void);
BOOL http_rx(void);
void check_formargs(void);
BOOL http_tx(void);
#separate
void tcp_rx(void);
#separate
void tx_poll(void);
void rx_poll(void);
void get_rtc_time(void);
void set_rtc_time(void);
void get_temperature(void);
#separate
BOOL find_file(void);
BOOL open_file(void);
void close_file(void);
BOOL tx_file_byte(void);
void tx_byte_inv(BYTE b);
BOOL geticks(void);
BOOL timeout(int &var, int tout);
void setled(BOOL on);

void main(void)
{
    setup_port_a(NO_ANALOGS);           // No analogue I/Ps
    set_tris_a(ALL_IN);
#if PORTBINV
    PORTB = 0xff;                       // Initialise ports
#else
    PORTB = 0;
#endif        
    PORTC = 0xff;
    set_tris_b(ALL_OUT);                // Port B LEDs
    set_tris_c(0xf8);                   // Port C mostly I/Ps
    setup_timer_1(TIMER1_SET);          // Initialise timer
    geticks();
    setled(1);                          // Diagnostic LED flash on
    enable_interrupts(INT_RDA);         // Enable serial Rx interrupts
    enable_interrupts(GLOBAL);
    // ***** MAIN LOOP ***** //
    while (1)
    {
        restart_wdt();                  // Kick watchdog
        geticks();                      // Get timer ticks
        if (rxin || timeout(ledoffticks, LEDOFFTIME))
            setled(1);
        else if (ledon && timeout(ledonticks, LEDONTIME))
            setled(0);
        if (txflag)                     // If transmitting, send next char
            tx_poll();
        rx_poll();                      // Check for Rx modem commands
        if (rxflag)                     // If frame received..
        {
            rxflag = 0;                 // ..prepare for another
            rxout = 0;
            get_ip();                   // ..and process Rx frame
        }
    }
}

/* Get an IP message; if ping, send response */
BOOL get_ip(void)
{
    BYTE b, hi, lo;
    int n=0;
    BOOL ret=1;

    slipend = checkflag = 0;                        // Clear checksum
    checkhi = checklo = 0;
    if (match_byte(0x45) && skip_byte() &&          // Version, service
        get_word(iplen) && skip_word() &&           // Len, ID
        skip_word() &&  skip_byte() &&              // Frags, TTL
        get_byte(ipcol) && skip_word() &&           // Protocol, checksum
        get_lword(remote.l) && get_lword(local.l) &&// Addresses
        checkhi==0xff && checklo==0xff)             // Checksum OK?
    {
        if (ipcol==PCOL_ICMP && get_ping_req())     // Ping request?
        {
            tpdlen = rpdlen;                        // Tx length = Rx length
            if (!txflag)
            {
                put_ip(tpdlen+ICMPHDR_LEN);         // Send ping reply
                put_ping_rep();
            }
        }
        else if (ipcol==PCOL_TCP && get_tcp())      // TCP segment?
            tcp_rx();                               // Call TCP handler
        else
            discard_data();                         // Unknown; discard it
    }
    else
        discard_data();
    return(ret);
}

/* Handle an incoming TCP segment */
#separate
void tcp_rx(void)
{
    BYTE *p, *q;
    BOOL tx=1;

    tpdlen = 0;                         // Assume no Tx data
    tflags = TACK;                      // ..and just sending an ack
    if (txflag || (rflags & TRST))      // RESET received, or busy?
        tx = 0;                         //..do nothing
    else if (rflags & TSYN)             // SYN received?
    {
        inc_lword(rseq.l);              // Adjust Tx ack for SYN
        if (locport==DAYPORT || locport==HTTPORT)
        {                               // Recognised port?
            rack.w[0] = 0xffff;
            rack.w[1] = concount++;
            tflags = TSYN+TACK;         // Send SYN ACK
        }
        else                            // Unrecognised port?
            tflags = TRST+TACK;         // Send reset
    }
    else if (rflags & TFIN)             // Received FIN?
        add_lword(rseq.l, rpdlen+1);    // Ack all incoming data + FIN
    else if (rflags & TACK)             // ACK received?
    {
        if (rpdlen)                     // Adjust Tx ack for Rx data
            add_lword(rseq.l, rpdlen);
        else                            // If no data, don't send ack
            tx = 0;
        if (locport==DAYPORT && rack.w[0]==0)
        {                               // Daytime request?
            daytime_rx();               // Send daytime data
            tx = 0;
        }
        else if (locport==HTTPORT && rpdlen)
        {                               // HTTP 'get' method?
            if (http_rx())              // Send HTTP data & close
                tx = 0;
            else                        // ..or just close connection
                tflags = TFIN+TACK;
        }
    }
    if (tx)                             // If ack to send..
    {
        put_ip(TCPHDR_LEN);             // ..send IP header
        checkhi = checklo = 0;          // ..reset checksum
        put_tcp();                      // ..send TCP header
    }
}

/* Handle an incoming daytime request */
void daytime_rx()
{
    tpdlen = DAYTIME_LEN;               // Data length of response
    get_rtc_time();                     // Read clock
    put_ip(TCPHDR_LEN+tpdlen);          // Send IP header
    checkhi = checklo = 0;              // Reset checksum
    txin = IPHDR_LEN + TCPHDR_LEN;      // O/P data to buffer, calc checksum
    printf(put_byte, DAYTIME_STR, rtc.b[2], rtc.b[1], rtc.b[0]);
    txin = IPHDR_LEN;                   // Go back to end of IP header
    tflags = TFIN+TACK;                 // O/P TCP header
    put_tcp();
}

/* Receive an incoming HTTP request ('method'), return 0 if invalid */
BOOL http_rx(void)
{
    int len, i;
    BOOL ret=0;
    char c;

    tpdlen = 0;                         // Check for 'GET'
    if (match_byte('G') && match_byte('E') && match_byte('T'))
    {
        ret = 1;
        skip_space();
        match_byte('/');                // Start of filename
        if (rxbuff[rxout] == '$')       // If dummy file starting wth '$'
        {
                                        // ..put out simple text string
            tpdlen = TEXT_OK_LEN + DAYTIME_LEN;
            get_rtc_time();             // ..consisting of current time
            put_ip(TCPHDR_LEN+tpdlen);
            checkhi = checklo = 0;
            txin = IPHDR_LEN + TCPHDR_LEN;
            printf(put_byte, TEXT_OK);  // ..with HTTP header
            printf(put_byte, DAYTIME_STR, rtc.b[2], rtc.b[1], rtc.b[0]);
            txin = IPHDR_LEN;
            tflags = TFIN+TPUSH+TACK;
            put_tcp();
        }
        else
        {                               // Get filename into directory buffer
            for (i=0; i<ROM_FNAMELEN; i++)
            {
                c = rxbuff[rxout];
                if (c>' ' && c!='?')    // Name terminated by space or '?'
                    rxout++;
                else
                    c = 0;
                romdir.f.name[i] = c;
            }                           // If file found in ROM
            if (find_file())
            {                           // ..check for form arguments
                check_formargs();
            }
            else                        // File not found, get index.htm
            {
                romdir.f.name[0] = 0;
                find_file();
            }
            if (!fileidx)               // No files at all in ROM - disaster!
            {
                tpdlen = HTTP_FAIL_LEN;     // Inform user of failure..
                put_ip(TCPHDR_LEN+tpdlen);  // Output IP header to buffer
                checkhi = checklo = 0;      // Reset checksum
                strcpy(&txbuff[IPHDR_LEN+TCPHDR_LEN], HTTP_FAIL);
                txin = IPHDR_LEN + TCPHDR_LEN;
                check_txbytes(tpdlen);      // Checksum data
                txin = IPHDR_LEN;           // Go back to end of IP header
                tflags = TFIN+TPUSH+TACK;
                put_tcp();                  // Output TCP header to buffer
            }
            else                        // File found OK
            {
                tpdlen = romdir.f.len;      // Get TCP data length
                put_ip(TCPHDR_LEN+tpdlen);  // Output IP header to buffer
                checkhi = checklo = 0;      // Reset checksum
                check_byte(romdir.f.check); // Add on checksum on ROM file
                check_byte(romdir.f.check >> 8);
                tflags = TFIN+TPUSH+TACK;   // Close connection when sent
                txi2c = 1;                  // Set flag to enable ROM file O/P
                put_tcp();                  // Output TCP header to buffer
            }
        }
    }
    return(ret);
}

/* Check for form arguments in HTTP request string */
void check_formargs(void)
{
    BOOL update=0;
    char c, d, temps[5];
#if PORTBINV
    portval = ~PORTB;               // Read O/P port, just in case
#else
    portval = PORTB;
#endif        
    while (rxout < rxcount)
    {                               // Each arg starts with '?' or '&'
        c = rxbuff[rxout++];
        if (c=='?' || c=='&')
        {                           // Copy string const from ROM to RAM
            strcpy(temps, "hrs=");
            if (match_str(temps))   // ..before matching it
            {
                                    // ..and updating clock value
                update |= get_hexbyte(rtc.b[2]);
                continue;
            }
            strcpy(temps, "min=");  // RTC minutes?
            if (match_str(temps))
            {
                update |= get_hexbyte(rtc.b[1]);
                continue;
            }
            strcpy(temps, "sec=");  // RTC secs?
            if (match_str(temps))
            {
                update |= get_hexbyte(rtc.b[0]);
                continue;
            }                       // O/P port bit change?
            strcpy(temps, "out");
            if (match_str(temps) && get_byte(c) && isdigit(c) &&
                match_byte('=') && get_byte(d) && isdigit(d))
            {
                if (d == '0')       // If off, switch bit on
                    portval |= 1 << (c-'0');
                else                // If on, switch bit off
                    portval &= ~(1 << (c-'0'));  
#if PORTBINV                    
                d = ~portval;       // Update hardware port
                PORTB = d;                                 
#else
                PORTB = portval;
#endif                                
            }
        }
    }
    if (update)                     // Update clock chip if time changed
        set_rtc_time();
}

/* Tx poll: send next SLIP character if possible, adding escape sequences */
#separate
void tx_poll(void)
{
    static BOOL escflag=0;
    BYTE b;

    if (txflag && TX_READY)             // If something to transmit
    {
#if TXDROP
        if (txout==0 && txin>4)         // Check if dropping frames for test
        {
            dropcount++;
            if ((dropcount % TXDROP) == 0)
            {
                txflag = 0;
                txin = txout = 0;
            }
        }
#endif
        if (txout == txin)              // If all RAM headers sent..
        {
            if (txi2c)                  // ..and ROM file to be sent..
            {
                open_file();            // ..open it for O/P
                txout++;
            }
            else                        // All sent: terminate SLIP frame
            {
                putchar(SLIP_END);
                txin = txout = 0;
                txflag = 0;
            }
        }
        else if (txout > txin)          // If sending ROM file
        {
                                        // ..and all sent..
            if (txi2c && !tx_file_byte())
            {                           // ..terminate frame, close file
                putchar(SLIP_END);
                close_file();
                txin = txout = 0;
                txflag = txi2c = 0;
            }
        }
        else                            // If sending RAM header..
        {
            b = read_txbuff(txout);     // ..encode next byte and send it
            if (escflag)
            {                           // Escape char sent, now send value
                putchar(b==SLIP_END ? ESC_END : ESC_ESC);
                txout++;
                escflag = 0;
            }                           // Escape char required
            else if (b==SLIP_END || b==SLIP_ESC)
            {
                putchar(SLIP_ESC);
                escflag = 1;
            }
            else                        // Send byte unmodified
            {
                putchar(b);
                txout++;
            }
        }
    }
}

/* Rx poll: check for modem command, send response */
void rx_poll(void)
{
    if (modemflag && !txflag)
    {
        strcpy(txbuff, "OK\r\n");       // Send OK in response to modem cmd
        txin = 4;
        txout = 0;
        txflag = 1;
        modemflag = 0;
    }                                   // Diagnostic; send index if '?'
    else if (rxbuff[0] == '?' && !txflag)
    {
        rxbuff[0] = rxin = 0;
        romdir.f.name[0] = 0;
        find_file();
        txflag = txi2c = 1;
    }
}

/* Get current time from Real-Time Clock */
void get_rtc_time(void)
{
    int i;

    i2c_start();
    i2c_write(RTC_ADDR);                // i2c address for write cycle
    i2c_write(2);                       // RTC register offset
    i2c_start();
    i2c_write(RTC_ADDR | 1);            // i2c address for read cycle
    for (i=0; i<sizeof(rtc); i++)       // Read bytes
        rtc.b[i] = i2c_read(i<sizeof(rtc)-1);
    i2c_stop();
}

/* Set current time in Real-Time Clock */
void set_rtc_time(void)
{
    int i;

    i2c_start();
    i2c_write(RTC_ADDR);                // i2c address for write cycle
    i2c_write(2);                       // RTC register offset
    for (i=0; i<sizeof(rtc); i++)       // Write bytes
        i2c_write(rtc.b[i]);
    i2c_stop();
}

/* Get current temperature from i2c or sensor */
void get_temperature(void)
{
    i2c_start();
    i2c_write(SENSOR_ADDR | 1);
    temphi = i2c_read(1);
    templo = i2c_read(0);
    i2c_stop();
}

/* Find a filename in ROM filesystem. Return false if not found
** Sets fileidx to 0 if ROM error, 1 if file is first in ROM, 2 if 2nd..
** and leaves directory info in 'romdir'
** If the first byte of name is zero, match first directory entry */
#separate
BOOL find_file(void)
{
    BOOL mismatch=1, end=0;
    int i;
    BYTE b;
    char temps[ROM_FNAMELEN];

    fileidx = 0;                        // Set ROM address pointer to 0
    i2c_start();
    i2c_write(EEROM_ADDR);
    i2c_write(0);
    i2c_write(0);
    i2c_stop();
    do
    {
        i2c_start();                    // Read next directory entry
        i2c_write(EEROM_ADDR | 1);
        if ((romdir.b[0] = i2c_read(1)) == 0xff)
        {                               // Abandon if no entry
            end = 1;
            i2c_read(0);
        }
        else
        {                               // Get file len, ptr, csum and flags
            for (i=1; i<7; i++)
                romdir.b[i] = i2c_read(1);
            mismatch = 0;               // Try matching name
            for (i=0; i<ROM_FNAMELEN; i++)
            {
                temps[i] = b = i2c_read(i<ROM_FNAMELEN-1);
                if (b != romdir.f.name[i])
                    mismatch = 1;
            }
            if (!romdir.f.name[0])      // If null name, match anything
                mismatch = 0;
        }
        i2c_stop();                     // Loop until matched
    } while (!end && fileidx++<MAXFILES && mismatch);
    if (mismatch)
        romdir.f.len = 0;
    return(!mismatch);
}

/* Open the previously-found file for transmission */
BOOL open_file(void)
{
    if (romdir.f.flags & EGI_ATVARS)    // If EGI '@' substitution
    {
        get_rtc_time();                 // ..get time and temperature
        get_temperature();
    }
    if (romdir.f.flags & EGI_HASHVARS)  // If EGI boolean '#' substitution
    {
        hashmask = barmask = 0x80;      // ..reset mask values
#if PORTBINV        
        portval = ~PORTB;               // ..and fetch port value
#else
        portval = PORTB;
#endif                
    }
    i2c_start();
    i2c_write(EEROM_ADDR);              // Write start pointer to eerom
    i2c_write(romdir.f.start >> 8);
    i2c_write(romdir.f.start);
    i2c_stop();
    i2c_start();
    i2c_write(EEROM_ADDR | 1);          // Restart ROM access as read cycle
}

/* Close the previously-opened file */
void close_file(void)
{
    i2c_read(0);                        // Dummy read cycle without ACK
    i2c_stop();
}

/* Transmit a byte from the current i2c file to the SLIP link
** Return 0 when complete file is sent
** If file has EGI flag set, perform run-time variable substitution */
BOOL tx_file_byte(void)
{
    int ret=0, idx;
    BYTE b;

    if (romdir.f.len)                   // Check if any bytes left to send
    {
        b = i2c_read(1);                // Get next byte from ROM
        if ((romdir.f.flags&EGI_ATVARS) && b=='@')
        {                               // If EGI var substitution..
            b = i2c_read(1);            // ..get 2nd byte
            romdir.f.len--;
            inv_byte = b > '@';         // Get variable index
            idx = inv_byte ? 0x50-b : b-0x30;
            if (idx>=0 && idx<=2)       // Index 0-2 are secs, mins, hrs
                printf(tx_byte_inv, "%02X", rtc.b[idx]);
            else if (idx == 3)          // Index 3 is temperature
            {
                printf(tx_byte_inv, "%2u", temphi);
                if (templo & 0x80)
                    printf(tx_byte_inv, ".5");
                else
                    printf(tx_byte_inv, ".0");
                i2c_read(1);            // Discard padding in ROM
                i2c_read(1);
                romdir.f.len -= 2;
            }
            else if (b == '?')          // @? - read input port
            {
                tx_byte('@');
                tx_byte(b);
                hashmask = barmask = 0x20;
                portval = ~PORTA;
            }
            else if (b == '!')          // @! - read output port
            {
                tx_byte('@');
                tx_byte(b);
                hashmask = barmask = 0x80;      
#if PORTBINV                
                portval = ~PORTB;
#else
                portval = PORTB;
#endif  
            }
            else                        // Unrecognised variable
                printf(tx_byte_inv, "??");
        }                               // '#' and '|' are for boolean values
        else if (romdir.f.flags&EGI_HASHVARS && (b=='#' || b=='|'))
        {
            if (b=='#')                 // Replace '|' with '1' or '0'
            {
                tx_byte(portval&hashmask ? '1' : '0');
                hashmask >>= 1;
            }
            else                        // Replace '|' with inverse
            {
                tx_byte(portval&barmask ? '|'+'#'-'1' : '|'+'#'-'0');
                barmask >>= 1;
            }
        }
        else                            // Non-EGI byte; send out unmodified
            tx_byte(b);
        romdir.f.len--;
        ret = 1;
    }
    return(ret);
}

/* Transmit a SLIP byte or its 'inverse' i.e. 80h minus the value
** Use by EGI functions to ensure data and its inverse add up to 80h */
void tx_byte_inv(BYTE b)
{
    tx_byte(inv_byte ? 0x80-b : b);
}

/* Update the current tick count, return non-zero if changed */
BOOL geticks(void)
{
    static unsigned tc, lastc=0;
    BOOL changed=0;

    tc = TIMER_1_HIGH;
    if (tc - lastc >= TIMER1_DIV)
    {
        tickcount++;
        lastc += TIMER1_DIV;
        changed = 1;
    }
    return(changed);
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

/* Set the diagnostic LED on or off, refresh its timers */
void setled(BOOL on)
{
    ledon = on;
    output_bit(DIAG_LED, !on);
    timeout(ledonticks, 0);
    timeout(ledoffticks, 0);
}
/* EOF */
