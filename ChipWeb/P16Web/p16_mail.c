/* Email client functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define MAIL_DEBUG  0   // Set non-zero for diagnostic printout

#if MAIL_DEBUG
#define NETWORK_OUTPUT  NET_SERIAL_OUTPUT
#else
#define NETWORK_OUTPUT  NET_OUTPUT
#endif

#define MAIL_INIT       0
#define MAIL_HELO       1
#define MAIL_FROM       2
#define MAIL_TO         3
#define MAIL_DATA       4

#define POP_INIT        0
#define POP_USER        1
#define POP_PASS        2
#define POP_STAT        3
#define POP_RETR        4
#define POP_DATA        5
#define POP_QUIT        6

BYTE mstate=0;

#if INCLUDE_POP3
#define LBUFFLEN    80
BYTE lbuff[LBUFFLEN];
BYTE lbuff_in=0, lbuff_len=0;
char from_str[]="From:";
char subj_str[]="Subject:";

/* Get a line of text into the line buffer, eliminate CR and LF characters
** If there is already a partial line in the buffer, append the new data
** If line is too long for buffer, truncate it */
BOOL get_lbuff(void)
{
    char c=0;

    while (get_byte(&c) && c!='\r')
    {
        if (lbuff_in<LBUFFLEN-1 && c!='\n')
        {
#if MAIL_DEBUG
            serial_putch(c);
#endif
            lbuff[lbuff_in++] = c;
        }
    }
#if MAIL_DEBUG
    serial_putch('\r');
    serial_putch('\n');
#endif
    if (c == '\r')
    {
        lbuff[lbuff_in] = 0;
        lbuff_len = lbuff_in;
        lbuff_in = 0;
        return(1);
    }
    return(0);
}

/* Handle incoming & outgoing SMTP data while connected
** Return the TCP flags to be transmitted (ACK and/or FIN) */
SEPARATED BYTE pop3_client_data(BYTE rflags)
{
    BYTE tflags=0;
    static BYTE count;

#if MAIL_DEBUG
    NET_SERIAL_OUTPUT;
#else
    NET_OUTPUT;
#endif
    rx_checkoff = 1;
    if (rxleft())
        tflags = TACK;
    if (get_lbuff())
    {
        if (lbuff[0]!='+' && mstate!=POP_DATA)
        {
            putstr("QUIT\r\n");
            mstate = POP_QUIT;
        }
        else if (mstate == POP_RETR)
            mstate = POP_DATA;
        else if (mstate == POP_DATA)
        {
            do
            {
                if (lbuff[0]=='.' && lbuff_len==1)
                {
                    put_ser("\r\n");
                    PRINTF2("RETR %u\r\n", ++count);
                    mstate = POP_RETR;
                }
                if (!strncmp(lbuff, from_str, sizeof(from_str)-1) ||
                    !strncmp(lbuff, subj_str, sizeof(subj_str)-1))
                {
                    ser_puts(lbuff);
                    put_ser("\r\n");
                }
            } while (get_lbuff());
        }
        else if (mstate == POP_INIT)
        {
            putstr("USER test\r\n");
            mstate++;
        }
        else if (mstate == POP_USER)
        {
            putstr("PASS MySecret\r\n");
            mstate++;
        }
        else if (mstate == POP_PASS)
        {
            putstr("STAT\r\n");
            mstate++;
        }
        else if (mstate == POP_STAT)
        {
            count = atoi(&lbuff[4]);
            SERIAL_OUTPUT;
            PRINTF2("\r\n%u messages\r\n\r\n", count);
            NETWORK_OUTPUT;
            putstr("RETR 1\r\n");
            count = 1;
            mstate++;
        }
        else if (mstate == POP_QUIT)
        {
                tflags = TFIN+TACK;
                USERLED1 = USERLEDOFF;
                USERLED2 = USERLEDOFF;
        }
    }
    return(tflags);
}
#endif // INCLUDE_POP3

#if INCLUDE_SMTP
/* Handle incoming & outgoing SMTP data while connected
** Return the TCP flags to be transmitted (ACK and/or FIN) */
SEPARATED BYTE smtp_client_data(BYTE rflags)
{
    BYTE tflags=0;
    char c, e;

    NETWORK_OUTPUT;
    rx_checkoff = 1;
    if (get_byte(&c))
    {
        tflags = TACK;
#if MAIL_DEBUG
        put_ser("  ");
        serial_putch(c);
        while (get_byte(&e) && e!='\n')
            serial_putch(e);
        serial_putch('\n');
#else
        while (get_byte(&e) && e!='\n')
            ;
#endif
        if (e != '\n')
            tflags = 0;
        else if (c == '2')
        {
            if (mstate == MAIL_INIT)
                putstr("HELO localnet\r\n");
            else if (mstate == MAIL_HELO)
                putstr("MAIL FROM: auto@chipweb\r\n");
            else if (mstate == MAIL_FROM)
                putstr("RCPT TO: test@localhost\r\n");
            else if (mstate == MAIL_TO)
                putstr("DATA\r\n");
            else if (mstate == MAIL_DATA+1)
                putstr("QUIT\r\n");
            else if (mstate == MAIL_DATA+2)
            {
                tflags = TFIN+TACK;
                USERLED1 = USERLEDOFF;
                USERLED2 = USERLEDOFF;
            }
            else
                mstate--;
            mstate++;
        }
        else if (c == '3')
        {
            if (mstate == MAIL_DATA)
            {
                putstr("From: \"ChipWeb server\" <auto@chipweb>\r\n");
                putstr("To: <test@localhost>\r\n");
                putstr("Subject: ChipWeb test\r\n");
                putstr("Content-type: text/plain\r\n\r\n");
                putstr("Test message\r\n");
                putstr(".\r\n");
                mstate++;
            }
        }
        else
        {
            tflags = TFIN+TACK;
            USERLED1 = USERLEDOFF;
            USERLED2 = USERLEDON;
        }
    }
    return(tflags);
}
#endif // INCLUDE_SMTP

/* EOF */
