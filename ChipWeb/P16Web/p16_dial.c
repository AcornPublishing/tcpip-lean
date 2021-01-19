/* Modem dialling functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define DIALSTR         "ATDT 123\r"
#define MODEM_TIMEOUT   (5*SECTICKS)
#define DIAL_TIMEOUT    (60*SECTICKS)

BOOL modem_connected;

BYTE get_kbuff(WORD tout);

/* Start up the phone dialling state machine */
BOOL do_dial(void)
{
    print_serial = TRUE;
    print_lcd = print_net = FALSE;
    SER_CTS_ = 0;
    if (!modem_connected)
    {
        USERLED1 = USERLEDON;
        flashled1 = 1;
        put_ser("ATE0\r");
        delay_ms(10);
        flush_serial();
        put_ser("AT&FE0\r");
        get_kbuff(MODEM_TIMEOUT);
        if (kbuff[0]=='O' && kbuff[1]=='K')
        {
            put_ser(DIALSTR);
            flush_serial();
            get_kbuff(DIAL_TIMEOUT);
            if (kbuff[0]=='C' && kbuff[1]=='O')
                modem_connected = TRUE;
        }
        USERLED1 = USERLEDON;
        flashled1 = 0;
    }
    return(0);
}

/* EOF */

