/* Time client functions for PICmicro Web server - Copyright (c) Iosoft Ltd 2001
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

#define TIME_ARP_INTERVAL       (SECTICKS * 2)
#define TIME_REFRESH_INTERVAL   (SECTICKS * 5)

WORD timeticks;
LWORD tserver_ip;           // Time server IP and Ethernet addresses
BYTE tserver_eth[MACLEN];
BOOL tserver_arp_ok=0;
LWORD time;                 // Time value

void request_time(void);

/* Initialise the time client */
void init_time(void)
{
    init_ip(&tserver_ip, TSERVER_ADDR);
}

/* Poll the time client */
void check_time(void)
{
    if (!net_ok)
         timeout(&timeticks, 0);
    else if (!tserver_arp_ok)
    {
        if (host_arped(&tserver_ip))
        {
            memcpy(tserver_eth, remeth, MACLEN);
            tserver_arp_ok = TRUE;
            request_time();
        }
        else if (timeout(&timeticks, TIME_ARP_INTERVAL))
            arp_host(&tserver_ip);
    }
    else if (timeout(&timeticks, TIME_REFRESH_INTERVAL))
        request_time();
}

/* Send a UDP request for the time */
void request_time(void)
{
    init_txbuff(0);
    remip.l = tserver_ip.l;
    memcpy(host.eth.srce, tserver_eth, MACLEN);
    locport = TIMECLIENT_PORT;
    remport = TIMESERVER_PORT;
    checkhi = checklo = 0;
    udp_xmit();
}

/* Handle an incoming time response */
void time_handler(void)
{
    BYTE h, m, s;

    if (get_lword(&time))
    {
        print_lcd = TRUE;
        print_net = print_serial = FALSE;
        lcd_gotoxy(9, 1);
        s = (BYTE)(time.l % 60);
        time.l /= 60;
        m = (BYTE)(time.l % 60);
        time.l /= 60;
        h = (BYTE)(time.l % 24);
#ifdef HI_TECH_C
        printf("%02u:%02u:%02u", h, m, s);
#else
        printf(putch, "%02u:%02u:%02u", h, m, s);
#endif
    }
}

/* EOF */

