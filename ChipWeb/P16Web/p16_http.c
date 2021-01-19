/* HTTP support for ChipWeb  - Copyright (c) Iosoft Ltd 2001
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

#define HTTP_FAIL       "HTTP/ 200 OK\r\n\r\nNo Web pages!\r\n"

#define MAXFILES    100     // Limit on ROM file count (to stop runaway)
typedef union               // ROM file directory entry format
{
    ROM_FNAME f;                // Union of filename..
    BYTE b[sizeof(ROM_FNAME)];  // ..with byte values for i2c transfer
} ROM_DIR;

ROM_DIR romdir;             // Storage for one directory entry
BYTE fileidx;               // Index of current file (1=first, 0=error)

void check_formargs(void);
BOOL find_file(void);
void open_file(void);
void close_file(void);
BOOL tx_file_byte(void);
void putnic_volts(WORD val);

/* Receive an incoming HTTP request ('method'), return 0 if invalid */
SEPARATED BOOL http_recv(void)
{
    BYTE i, b, idx;
    char c;
    BYTE v;
    WORD val;
    checkflag = checkhi = checklo = 0;
    rx_checkoff = 1;
    DEBUG_PUTC('h');
    if (match_byte('G') && match_byte('E') && match_byte('T'))
    {
        match_byte(' ');
        match_byte('/');            // Start of filename
        DEBUG_PUTC(' ');
        for (i=0; i<ROM_FNAMELEN && get_byte(&c) && c>' ' && c!='?'; i++)
        {                           // Name terminated by space or '?'
            DEBUG_PUTC(c);
            romdir.f.name[i] = c;
        }
        for (; i<ROM_FNAMELEN; i++) // Blank rest of name
        {
            romdir.f.name[i] = 0;
        }                           // If file found in ROM
        if (find_file())
        {                           // ..check for form arguments
            DEBUG_PUTC('>');
            check_formargs();
        }
        else                        // File not found, get index.htm
        {
            DEBUG_PUTC('?');
            romdir.f.name[0] = 0;
            find_file();
        }
        checkhi = checklo = 0;
        checkflag = 0;
        txin = IPHDR_LEN + TCPHDR_LEN;
        if (!fileidx)               // No files at all in ROM - disaster!
        {
//            print_lcd = print_serial = FALSE;
//            print_net = TRUE;
//            setpos_txin(TCPIPHDR_LEN);
            putstr(HTTP_FAIL);
//            tflags = TFIN+TACK;     // Ack & close connection
//            tcp_xmit(tflags);
        }
        else                        // File found OK
        {
            open_file();                // Start i2c transfer
            while (romdir.f.len)
            {
                b = i2c_read(1);                // Get next byte from ROM
                romdir.f.len--;
                if ((romdir.f.flags&EGI_ATVARS) && b=='@' && romdir.f.len)
                {                               // If '@' and EGI var substitution..
                    b = i2c_read(1);            // ..get 2nd byte
                    romdir.f.len--;
                    idx = b - 0x30;
                    val = idx&1 ? adc1 : adc2;
                    if (idx==1 || idx==2)
                    {
                        v = (BYTE)((val * 25) >> 8) + 6 ;
                        if (v > 9)
                        {
                            if (v >= 99)
                                i = v = 9;
                            else for (i=0; i<10 && v>9; i++)
                                v -= 10;
                            put_byte(i + '0');
                        }
                        put_byte(v + '0');
                    }
                    else if (idx==3 || idx==4)  // Voltage value for ADC 1 or 2
                    {
                        v = (BYTE)((val * 25) >> 9);
                        for (i=0; i<10 && v>9; i++)
                            v -= 10;
                        put_byte(i + '0');
                        put_byte('.');
                        put_byte(v + '0');
                    }
                    else if (idx == 5)          // User O/P LED 1 state
                        put_byte(USERLED1 ? '0' : '1');
                    else if (idx == 6)          // User O/P LED 2 state
                        put_byte(USERLED2 ? '0' : '1');
                    else if (idx == 7)          // I/P button state
                        put_byte(USER_BUTTON ? '0' : '1');
                    else                        // Unknown variable
                        put_byte('?');
                }
                else                            // Non-EGI byte; send out unmodified
                    put_byte(b);
            }

            close_file();
//            tflags = TFIN+TPUSH+TACK;   // Close connection when sent
        }
        return(1);
    }
    return(0);
}

/* Check for arguments in HTTP request string
** Simple version: just check last 2 digits of filename, copy to 2 LEDs */
void check_formargs(void)
{
    if (romdir.f.name[6]=='0' || romdir.f.name[6]=='1')
        USERLED1 = (romdir.f.name[6] == '0');
    if (romdir.f.name[7]=='0' || romdir.f.name[7]=='1')
        USERLED2 = (romdir.f.name[7] == '0');
}

/* Find a filename in ROM filesystem. Return false if not found
** Sets fileidx to 0 if ROM error, 1 if file is first in ROM, 2 if 2nd..
** and leaves directory info in 'romdir'
** If the first byte of name is zero, match first directory entry */
BOOL find_file(void)
{
    BYTE mismatch=1, i, b;
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
#if 1
        for (i=0; i<7; i++)
            romdir.b[i] = i2c_read(1);
        if (romdir.b[0]==0xff && romdir.b[1]==0xff)
        {                               // Abandon if no entry
            i2c_read(0);
            break;
        }
        else
        {
#else
        if ((romdir.b[0] = i2c_read(1)) == 0xff)
        {                               // Abandon if no entry
//            end = 1;
            i2c_read(0);
            break;
        }
        else
        {                               // Get file len, ptr, csum and flags
            for (i=1; i<7; i++)
                romdir.b[i] = i2c_read(1);
#endif
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
        fileidx++;
    } while (fileidx<MAXFILES && mismatch);
    if (mismatch)
        romdir.f.len = 0;
    return(!mismatch);
}

/* Open the previously-found file for transmission */
void open_file(void)
{
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

/* EOF */
