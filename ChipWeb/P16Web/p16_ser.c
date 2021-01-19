/* ChipWeb serial functions - Copyright (c) Iosoft Ltd 2001
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

#if SERIAL_INTERRUPTS
#define SERBUFFLEN  128
BYTE serbuffout=0;
BYTE serbuffin=0;
BYTE serbuff[SERBUFFLEN];
#endif

// Send a string to the serial port
void ser_puts(char *str)
{
    char c;

    while ((c = *str++) != 0)
        serial_putch(c);
}

// Return non-zero if a serial char is available
BOOL serial_kbhit(void)
{
    unsigned char b;
#if SERIAL_INTERRUPTS
    b = serbuffin;
    if (b != serbuffout)
        return(1);
     return(0);
#else
    if (RCIF)
    {
        if (!FERR && !OERR)
            return(1);
        CREN = 0;
        b = RCREG;
        CREN = 1;
    }
    return(0);
#endif
}

// Return serial char (no echo), wait until available
unsigned char serial_getch(void)
{
    unsigned char b;

#if SERIAL_INTERRUPTS
    while (!serial_kbhit())
        ;
    b = serbuff[serbuffout++];
    if (serbuffout > SERBUFFLEN)
        serbuffout = 0;
    return(b);
#else

    while (!RCIF && (FERR || OERR))
    {
        if (FERR || OERR)
        {
            CREN = 0;
            b = RCREG;
            CREN = 1;
            return(0);
        }
        restart_wdt();
    }
    return(RCREG);
#endif
}

#if SERIAL_INTERRUPTS
/* Function to enable serial interrupts */
void enable_serints(void)
{
#ifndef HI_TECH_C
    enable_interrupts(INT_RDA);
#else
    RCIE = 1;
    INTCON |= 0xc0;
#endif
}

/* Function to disable serial interrupts */
void disable_serints(void)
{
    RCIE = 0;
}

RX_INT_HANDLER
rx_handler(void)
{
    BYTE b, in;
#ifndef HI_TECH_C
    if (kbhit())
    {
        b = getch();
#else
    if (OERR || FERR)
    {
        CREN = 0;
        b = RCREG;
        CREN = 1;
    }
    else if (RCIF)
    {
        b = RCREG;
#endif
        in = serbuffin + 1;
        if (in > SERBUFFLEN)
            in = 0;
        if (serbuffout != in)
        {
            serbuff[serbuffin] = b;
            serbuffin = in;
        }
    }
}
#endif

/* EOF */
