/* User console functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define SOH 0x01
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18

#define ROMPAGE_LEN 32
#define XBLOCK_LEN  128

/* If PIC18, use a fixed IP address & serial num (no user configuration)
** until the PIC18F parts with on-chip EEROM are available */
#ifdef _PIC18
#define FIXED_SERNUM 1234
#else
#define FIXED_SERNUM 0
#endif

BOOL escaped;

BYTE str2ip(char *s, LWORD *ip);
BYTE str2w(char *s, WORD *wp);
BYTE get_serln(WORD tout);
SEPARATED BOOL get_ip_num(LWORD *lwp);
void write_nonvol(void);
void xmodem_recv(void);

BOOL user_config(void)
{
#if FIXED_SERNUM || !INCLUDE_CFG
    return(0);
#else
    WORD w;

    put_ser("\r\nSerial num? ");
    init_rxbuff();
    if (get_serln(0))
    {
        if (get_num(&w))
        {
            myeth[4] = w >> 8;
            myeth[5] = w;
        }
    }
    put_ser("\r\nIP addr? ");
    init_rxbuff();
    get_serln(0);
    if (get_ip_num(&myip))
    {
        write_nonvol();
#if INCLUDE_HTTP
        put_ser("\r\nXmodem? ");
        xmodem_recv();
#endif
    }
    else
        put_ser(" Not updated\r\n");
    return(1);
#endif
}

#if INCLUDE_CFG
SEPARATED BOOL get_ip_num(LWORD *lwp)
{
    WORD a, b, c, d;
    if (get_num(&a) && match_byte('.') && get_num(&b) && match_byte('.') &&
        get_num(&c) && match_byte('.') && get_num(&d))
    {
        lwp->b[0] = (BYTE)d;
        lwp->b[1] = (BYTE)c;
        lwp->b[2] = (BYTE)b;
        lwp->b[3] = (BYTE)a;
        return(1);
    }
    return(0);
}

/* Get user input string, if timeout is set, exit after that time */
BYTE get_serln(WORD tout)
{
    BYTE n=0;
    WORD ticks;
    char c;

    timeout(&ticks, 0);
    while (1)
    {
        while (!serial_kbhit())
        {
            if (tout && timeout(&ticks, tout))
            {
                rxbuff[n] = 0;
                return(n);
            }
            scan_io();
        }
        c = serial_getch();
        if (c<' ' || c==0x7f)
        {
            if (c == 0x1b)
            {
                n = 0;
                break;
            }
            else if (c == '\r')
            {
                if (!tout || n>0)
                    break;
            }
            else if (c == '\b' || c == 0x7f)
            {
                if (n > 0)
                {
                    n--;
                    serial_putch('\b');
                    serial_putch(' ');
                    serial_putch('\b');
                }
            }
        }
        else if (n < sizeof(rxbuff)-1)
        {
            rxbuff[n++] = c;
            if (!tout)
                serial_putch(c);
        }
    }
    rxbuff[n] = 0;
    rxbuffin = net_rxin = n;
    return(n);
}
#endif

/* Read in the nonvolatile parameters, return 0 if error */
BOOL read_nonvol(void)
{
#if FIXED_SERNUM
    myeth[4] = FIXED_SERNUM & 0xff;
    myeth[5] = (BYTE)(FIXED_SERNUM >> 8);
    init_ip(&myip, PIC18_MYIP);   // Initialise my (fixed) IP addr
    return(1);
#else
    BYTE sum;

    myeth[4] = read_eeprom(0);    // Get serial num & IP addr from EEPROM
    myeth[5] = read_eeprom(1);
    myip.b[3] = read_eeprom(2);
    myip.b[2] = read_eeprom(3);
    myip.b[1] = read_eeprom(4);
    myip.b[0] = read_eeprom(5);
    sum = read_eeprom(6);
    return(csum_nonvol() == sum);
#endif
}

/* Write out the nonvolatile parameters */
void write_nonvol(void)
{
#if FIXED_SERNUM==0
    write_eeprom(0, myeth[4]);
    write_eeprom(1, myeth[5]);
    write_eeprom(2, myip.b[3]);
    write_eeprom(3, myip.b[2]);
    write_eeprom(4, myip.b[1]);
    write_eeprom(5, myip.b[0]);
    write_eeprom(6, csum_nonvol());
#endif
}

#if FIXED_SERNUM==0
/* Do a 1's omplement checksum of the non-volatile data */
BYTE csum_nonvol(void)
{
    BYTE i, sum=0;

    for (i=0; i<6; i++)
        sum += read_eeprom(i);
    sum = ~sum;
    return(sum);
}
#endif
#if INCLUDE_HTTP
/* Handle incoming XMODEM data block */
void xmodem_recv(void)
{
    BYTE b, len=0, idx, blk, i, oset;
    BYTE rxing=FALSE, b1=FALSE, b2=FALSE, b3=FALSE;

    timeout(&ledticks, 0);
    while (1)
    {
        while (!serial_kbhit())
        {
            restart_wdt();              // Kick watchdog
            geticks();                  // Check for timeout
            if (timeout(&ledticks, LEDTIME))
            {
                SYSLED = !SYSLED;
                USERLED1 = 1;
                if (!rxing)             // Send NAK if idle
                {
                    len = 0;
                    b1 = b2 = b3 = FALSE;
                    serial_putch(NAK);
                }
                rxing = FALSE;
            }
        }
        b = getchar();                  // Get character
        rxing = TRUE;
        if (!b1)                        // Check if 1st char
        {
            if (b == SOH)               // ..if SOH, move on
                b1 = TRUE;
            else if (b == EOT)          // ..if EOT, we're done
            {
                serial_putch(ACK);
                break;
            }
        }
        else if (!b2)                   // Check if 2nd char
        {
            blk = b;                    // ..block num
            b2 = TRUE;
        }
        else if (!b3)                   // Check if 3rd char
        {
            if (blk == ~b)              // ..inverse block num
            {
                b3 = TRUE;
                blk--;
            }
        }
        else if (len < XBLOCK_LEN)      // Rest of chars up to block len
        {                               // Buffer into ROM page
            idx = len & (ROMPAGE_LEN - 1);
            len++;
            txbuff[idx] = b;            // If end of ROM page..
            if (idx == ROMPAGE_LEN-1)   // ..write to ROM
            {
                i2c_start();
                i2c_write(EEROM_ADDR);
                i2c_write(blk >> 1);
                oset = len - ROMPAGE_LEN;
                if (blk & 1)
                    oset += 0x80;
                i2c_write(oset);
                for (i=0; i<ROMPAGE_LEN; i++)
                    i2c_write(txbuff[i]);
                i2c_stop();
            }
        }
        else                            // End of block, send ACK
        {
            serial_putch(ACK);
            timeout(&ledticks, 0);
            SYSLED = !SYSLED;
            len = 0;
            b1 = b2 = b3 = FALSE;
        }
    }
}
#endif
/* EOF */

