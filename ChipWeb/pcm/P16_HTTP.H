/* HTTP support for CHIPWEB on PICDEM.net
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */
/*
** v0.01 10/1/01 JPB
*/

#define HTTP_FAIL       "HTTP/ 200 OK\r\n\r\nNo Web pages!\r\n"

#define MAXFILES    100     // Limit on ROM file count (to stop runaway)
typedef union               // ROM file directory entry format
{
    ROM_FNAME f;                // Union of filename..
    BYTE b[sizeof(ROM_FNAME)];  // ..with byte values for i2c transfer
} ROM_DIR;

ROM_DIR romdir;             // Storage for one directory entry
int fileidx;                // Index of current file (1=first, 0=error)

void check_formargs(void);
BOOL find_file(void);
BOOL open_file(void);
void close_file(void);
BOOL tx_file_byte(void);
void putnic_volts(WORD val);

/* Receive an incoming HTTP request ('method'), return 0 if invalid */
#separate
BOOL http_recv(void)
{
    int len, i;
    BOOL ret=0;
    char c;
    WORD blen;

    tpxdlen = 0;                    // Check for 'GET'
    DEBUG_PUTC('h');
    if (match_byte('G') && match_byte('E') && match_byte('T'))
    {
        ret = 1;
        match_byte(' ');
        match_byte('/');            // Start of filename
        DEBUG_PUTC(' ');
        memset(romdir.f.name, 0, ROM_FNAMELEN);
        for (i=0; i<ROM_FNAMELEN && get_byte(c) && c>' ' && c!='?'; i++)
        {                           // Name terminated by space or '?'
            DEBUG_PUTC(c);
            romdir.f.name[i] = c;
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
            setnic_addr((TXSTART<<8)+sizeof(ETHERHEADER)+IPHDR_LEN+TCPHDR_LEN);
            printf(putnic_checkbyte, HTTP_FAIL);    
            tflags = TFIN+TACK;    
            d_checkhi = checkhi;
            d_checklo = checklo;
            tcp_xmit();
        }
        else                        // File found OK
        {
            open_file();                // Start i2c transfer
            setnic_addr((TXSTART<<8)+sizeof(ETHERHEADER)+IPHDR_LEN+TCPHDR_LEN);
            while (tx_file_byte())      // Copy bytes from ROM to NIC
                ;
            close_file();
            tflags = TFIN+TPUSH+TACK;   // Close connection when sent
            d_checkhi = checkhi;        // Save checksum
            d_checklo = checklo;
            tcp_xmit();                 // Do header, transmit segment
        }
    }
    return(ret);
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

/* Transmit a byte from the current i2c file to the NIC
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
        {                               // If '@' and EGI var substitution..
            b = i2c_read(1);            // ..get 2nd byte
            romdir.f.len--;
            idx = b - 0x30;
            if (idx == 1)               // Scaled ADC value for slider 1
                printf(putnic_checkbyte, "%u", (BYTE)(adc1/11)+6);
            else if (idx == 2)          // Scaled ADC value for slider 2
                printf(putnic_checkbyte, "%u", (BYTE)(adc2/11)+6);
            else if (idx == 3)          // Voltage value for ADC 1
                putnic_volts(adc1);
            else if (idx == 4)          // Voltage value for ADC 2
                putnic_volts(adc2);
            else if (idx == 5)          // User O/P LED 1 state
                putnic_checkbyte(USERLED1 ? '0' : '1');
            else if (idx == 6)          // User O/P LED 2 state
                putnic_checkbyte(USERLED2 ? '0' : '1');
            else if (idx == 7)          // I/P button state
                putnic_checkbyte(USER_BUTTON ? '0' : '1');
            else                        // Unknown variable
                printf(putnic_checkbyte, "??");
        }
        else                            // Non-EGI byte; send out unmodified
            putnic_checkbyte(b);
        romdir.f.len--;                 // Decrement length
        ret = 1;
    }
    return(ret);
}

/* Send the voltage string for the given ADC to the NIC */
void putnic_volts(WORD val)
{             
    BYTE v;
    
    v = (BYTE)(val / 21);
    putnic_checkbyte(v/10 + '0');
    putnic_checkbyte('.');
    putnic_checkbyte(v%10 + '0');
}

/* EOF */