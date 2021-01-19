/* User console interface functions for CHIPWEB
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */
                            
#define SOH 0x01
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18

#define ROMPAGE_LEN 32
#define XBLOCK_LEN  128
                            
BOOL escaped;

WORD getnum(void);
void write_nonvol(void);
void xmodem_recv(void);

/* User initialisation code; get serial number and IP address
** Skip if user hits ESC */
void user_config(void)
{                     
    WORD w;
    BYTE t1, t2;
                           
    escaped = 0;                           
    printf("\r\nSerial num? ");
    w = getnum();
    if (!escaped)
    {
        myeth[4] = w >> 8;
        myeth[5] = w;
    }
    escaped = 0;
    printf("\r\nIP addr? ");
    USERLED1 = USERLED2 = 1;
    myip.b[3] = getnum();
    putchar('.');
    if (!escaped)
        myip.b[2] = getnum();
    putchar('.');
    if (!escaped)
        myip.b[1] = getnum();
    putchar('.');
    if (!escaped)
        myip.b[0] = getnum();
    if (!escaped)
        write_nonvol();
    printf("\r\nXmodem? ");
    xmodem_recv();
}

/* Get a 16-bit decimal number from the console
** Return it when any non-numeric key is pressed (except backspace) */
WORD getnum(void)
{
    WORD val=0;
    char c, buff[5];
    int n=0, i=0;
    
    do {
        while (!kbhit())
            scan_io();
        c = getchar();
        if (c == 0x1b)
            escaped = 1;
        else if (c>='0' && c<='9')
        {
            if (n < sizeof(buff))
            {
                buff[n++] = c;
                putchar(c);
            }
        }
        else if (c=='\b' || c==0x7f)
        {
            if (n > 0)
            {
                n--;
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
        }
        else 
            c = 0;
    } while (c && !escaped);
    while (n--)
        val = (val * 10) + buff[i++] - '0';
    return(val);
}

/* Read in the nonvolatile parameters, return 0 if error */
BOOL read_nonvol(void)
{        
    int i;
                      
    myeth[4] = read_eeprom(0);
    myeth[5] = read_eeprom(1);
    myip.b[3] = read_eeprom(2);
    myip.b[2] = read_eeprom(3);
    myip.b[1] = read_eeprom(4);
    myip.b[0] = read_eeprom(5);
    return (csum_nonvol() == read_eeprom(6));
}

/* Write out the nonvolatile parameters */
void write_nonvol(void)
{
    write_eeprom(0, myeth[4]);
    write_eeprom(1, myeth[5]);
    write_eeprom(2, myip.b[3]);
    write_eeprom(3, myip.b[2]);
    write_eeprom(4, myip.b[1]);
    write_eeprom(5, myip.b[0]);
    write_eeprom(6, csum_nonvol());
}

/* Do a 1's omplement checksum of the non-volatile data */
BYTE csum_nonvol(void)
{
    int i;
    BYTE sum=0;
    
    for (i=0; i<6; i++)
        sum += read_eeprom(i);
    return(~sum);
}

/* Handle incoming XMODEM data block */
void xmodem_recv(void)
{     
    BYTE b, len=0, idx, blk, i, oset;
    BOOL rxing=FALSE, b1=FALSE, b2=FALSE, b3=FALSE;
     
    timeout(ledticks, 0);
    while (1) 
    {
        while (!kbhit())
        {
            restart_wdt();              // Kick watchdog
            geticks();                  // Check for timeout
            if (timeout(ledticks, LEDTIME))
            {
                SYSLED = !SYSLED;
                USERLED1 = 1;
                if (!rxing)             // Send NAK if idle
                {
                    len = 0;
                    b1 = b2 = b3 = FALSE;
                    putchar(NAK);
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
                putchar(ACK);
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
            putchar(ACK);
            timeout(ledticks, 0);
            SYSLED = !SYSLED;
            len = 0;
            b1 = b2 = b3 = FALSE;
        }
    }
}

// EOF
