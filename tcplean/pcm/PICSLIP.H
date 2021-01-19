/* SLIP I/O functions for PIC Web server Copyright (c) Iosoft Ltd 2000
**
** This software is only licensed for distribution with the book 'TCP/IP Lean', 
** and may only be used for personal experimentation by the purchaser 
** of that book, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */

/* Revisions:
** v0.01 JPB 30/3/00 Functions extracted from PICWEB v0.14
*/

#define TXBUFFLEN  96       // Tx buffer
#define RXBUFFLEN  80       // Rx buffer: more than enough for a 32-byte Ping
#define RXHDR_LEN  40       // After header, data is checksummed on arrival

#define SLIP_END    0xc0    // SLIP escape codes
#define SLIP_ESC    0xdb
#define ESC_END     0xdc
#define ESC_ESC     0xdd

BYTE txbuff[TXBUFFLEN];     // Transmit buffer
int txin, txout;            // Buffer I/P and O/P counters
WORD txi2c;                 // Count of i2c bytes to be sent
BOOL txflag;                // Flag to start sending out Tx data

BYTE rxbuff[RXBUFFLEN];     // Receive buffer, I/O ptrs, and 'Rx full' flag
int rxin, rxout, rxcount;
BOOL rxflag;
BOOL modemflag;             // Flag to show modem command received

BOOL slipend;               // Flag to mark end of I/P SLIP message
BOOL checkflag;             // Checksum flag & byte values
BYTE checkhi, checklo;
BOOL rdcheckflag;           // Checksum for Rx data (excl. IP & TCP hdrs)
BYTE rdcheckhi, rdchecklo;
int dropcount;

/* Prototypes for this file */
int get_data(BYTE *ptr, int maxlen);
BOOL get_byte(BYTE &b);
BOOL get_word(WORD &w);
BOOL get_lword(LWORD &lw);
BOOL get_hexbyte(BYTE &val);
void discard_data(void);
BYTE getch_slip(void);
BOOL match_byte(BYTE b);
BOOL match_word(WORD w);
BOOL match_str(char *s);
void skip_space(void);
BOOL skip_byte();
BOOL skip_word();
BOOL skip_lword();
int check_str(char *s);
void check_byte(BYTE b);
void check_word(WORD w);
void check_lword(LWORD &lw);
void check_bytes(BYTE *dat, int len);
void put_byte(BYTE b);
void put_str(char *s);
void put_null(void);
void put_word(WORD w);
void put_nullw(void);
void put_lword(LWORD &lw);
void tx_start(void);
void tx_byte(BYTE b);
void tx_end(void);
void check_txbytes(int n);
BYTE read_txbuff(int &oset);
void write_txbuff(int &oset, BYTE &b);

/* Get incoming data in buffer; discard any buffer overflow */
int get_data(BYTE *ptr, int maxlen)
{
    BYTE b;
    int n=0;

    while (maxlen-- > 0 && get_byte(b))
    {
        *ptr++ = b;
        n++;
    }
    discard_data();
    return(n);
}

/* Discard incoming data */
void discard_data(void)
{
    while (!slipend)
        getch_slip();
}

/* Get an incoming byte value, return 0 if end of message */
BOOL get_byte(BYTE &b)
{
    b = getch_slip();
    return(!slipend);
}

/* Get an incoming word value, return 0 if end of message */
BOOL get_word(WORD &w)
{
    BYTE hi, lo;

    hi = getch_slip();
    lo = getch_slip();
    w = ((WORD)hi<<8) | (WORD)lo;
    return(!slipend);
}
/* Get an incoming lword value, return 0 if end of message */
BOOL get_lword(LWORD &lw)
{
    lw.b[3] = getch_slip();
    lw.b[2] = getch_slip();
    lw.b[1] = getch_slip();
    lw.b[0] = getch_slip();
    return(!slipend);
}

/* Get an incoming byte value as 1 or 2 hex characters */
BOOL get_hexbyte(BYTE &val)
{
    BYTE b;
    BOOL ok=0;

    val = 0;
    while (isxdigit(rxbuff[rxout]) && get_byte(b))
    {
        ok = 1;
        val <<= 4;
        if (b <= '9')
            val += b - '0';
        else
            val += (b-'A'+10) & 0xf;
    }
    return(ok);
}

/* Get a SLIP byte from buffer; if end, set flag */
BYTE getch_slip(void)
{
    BYTE b=0;

    slipend = rxout>=rxcount;
    if (!slipend)
    {
        b = rxbuff[rxout++];
        check_byte(b);
    }
    return(b);
}

/* Match an incoming byte value, return 0 not matched, or end of message */
BOOL match_byte(BYTE b)
{
    return(b==getch_slip() && !slipend);
}
/* Match an incoming byte value, return 0 not matched, or end of message */
BOOL match_word(WORD w)
{
    WORD inw;

    return(get_word(inw) && inw==w);
}
/* Match a string value, return 0 (and don't move O/P ptr) if not matched */
BOOL match_str(char *s)
{
    BOOL ok=1;
    int rxo;

    rxo = rxout;
    while (ok && *s)
        ok = match_byte(*s++);
    if (!ok)
        rxout = rxo;
}

/* Skip whitespace & ctrl chars in incoming data */
void skip_space(void)
{
    while (rxbuff[rxout] <= ' ')
        getch_slip();
}
/* Skip an incoming byte value, return 0 if end of message */
BOOL skip_byte()
{
    getch_slip();
    return(!slipend);
}
/* Skip an incoming byte value, return 0 if end of message */
BOOL skip_word()
{
    getch_slip();
    getch_slip();
    return(!slipend);
}
/* Skip an incoming byte value, return 0 if end of message */
BOOL skip_lword()
{
    getch_slip();
    getch_slip();
    getch_slip();
    getch_slip();
    return(!slipend);
}

/* Compute checksum of a string, return its length */
int check_str(char *s)
{
    char c;
    int n=0;

    while ((c = s[n])!=0)
    {
        check_byte(c);
        n++;
    }
    return(n);
}

/* Add byte to checksum value */
void check_byte(BYTE b)
{
    if (checkflag)
    {
        if ((checklo = b+checklo) < b)
        {
            if (++checkhi == 0)
                checklo++;
        }
    }
    else
    {
        if ((checkhi = b+checkhi) < b)
        {
            if (++checklo == 0)
                checkhi++;
        }
    }
    checkflag = !checkflag;
}
/* Add word to checksum value */
void check_word(WORD w)
{
    check_byte(w>>8);
    check_byte(w);
}
/* Add longword to checksum value */
void check_lword(LWORD &lw)
{
    check_byte(lw.b[3]);
    check_byte(lw.b[2]);
    check_byte(lw.b[1]);
    check_byte(lw.b[0]);
}

/* Add array of bytes to checksum value */
void check_bytes(BYTE *dat, int len)
{
    while (len--)
        check_byte(*dat++);
}

/* Start a transmission */
void tx_start(void)
{
    putchar(SLIP_END);
}

/* Encode and transmit a single SLIP byte */
void tx_byte(BYTE b)
{
    if (b==SLIP_END || b==SLIP_ESC)
    {
        putchar(SLIP_ESC);
        putchar(b==SLIP_END ? ESC_END : ESC_ESC);
    }
    else
        putchar(b);
}

/* End a transmission, start sending it out */
void tx_end(void)
{
    txflag = 1;
}

/* Send a byte out to the SLIP link, then add to checksum */
void put_byte(BYTE b)
{
    if (txin < TXBUFFLEN)
    {
        write_txbuff(txin, b);
        txin++;
    }
    check_byte(b);
}

/* Send a string out to the SLIP link, add to checksum */
void put_str(char *s)
{
    while (*s)
        put_byte(*s++);
}

/* Send a null out to the SLIP link */
void put_null(void)
{
    put_byte(0);
}

/* Send a word out to the SLIP link, then add to checksum */
void put_word(WORD w)
{
    put_byte(w >> 8);
    put_byte(w);
}

/* Send a null word out to the SLIP link */
void put_nullw(void)
{
    put_byte(0);
    put_byte(0);
}

void put_lword(LWORD &lw)
{
    put_byte(lw.b[3]);
    put_byte(lw.b[2]);
    put_byte(lw.b[1]);
    put_byte(lw.b[0]);
}

/* Compute checksum of Tx bytes, given count */
void check_txbytes(int n)
{
    while (n--)
    {
        check_byte(read_txbuff(txin));
        txin++;
    }
}

/* Read a byte from the Tx buffer */
BYTE read_txbuff(int &oset)
{
    return(txbuff[oset]);
}

/* Write a byte to the Tx buffer */
void write_txbuff(int &oset, BYTE &b)
{
    txbuff[oset] = b;
}

/* Rx interrupt handler */
#INT_RDA
void rx_handler(void)
{
    BYTE b;
    BOOL escflag;

    if (kbhit())                    // Make sure serial character received
    {
        if ((b = getchar()) == SLIP_ESC)
            escflag = 1;            // SLIP escape character?
        else if (b == SLIP_END)
        {                           // SLIP end-of-frame character?
            if (rxin > 0)
            {                       // If non-zero frame length..
                rxflag = 1;         // ..set Rx frame flag
                rxcount = rxin;     // Reset counter for new frame
                rxin = 0;
            }
        }                           // Modem command 'AT...<CR>'?
        else if (b=='\r' && rxbuff[0]=='A')
        {
            modemflag = 1;          // Set flag (can't be handled on interrupt)
            rxin = 0;
        }
        else                        // Normal SLIP character..
        {
            if (escflag)            // ..following an escape char?
                b = b==ESC_END ? SLIP_END : b==ESC_ESC ? SLIP_ESC : b;
            escflag = 0;
            if (rxin == RXHDR_LEN-1)
            {                       // If nearly at end of hdrs, reset checksum
                rdcheckhi = rdchecklo = 0;
                rdcheckflag = 0;
            }                       // If after headers, calculate checksum
            if (rxin >= RXHDR_LEN)
            {                       // Alternate between checksum bytes..
                if (rdcheckflag)
                {                   // Update lo byte value
                    if ((rdchecklo = b+rdchecklo) < b)
                    {               // If lo byte overflow, increment hi byte
                        if (++rdcheckhi == 0)
                            rdchecklo++;
                    }               // ..and maybe carry back to lo byte!
                }
                else
                {                   // Update hi byte value
                    if ((rdcheckhi = b+rdcheckhi) < b)
                    {               // If hi byte overflow, increment lo byte
                        if (++rdchecklo == 0)
                            rdcheckhi++;
                    }               // ..and maybe carry back to hi byte!
                }
                rdcheckflag = !rdcheckflag; // Next time, check other byte
            }
            if (rxin < RXBUFFLEN)   // Save char if room left in buffer
            {
                rxbuff[rxin++] = b;
            }
        }
    }
}
/* EOF */
