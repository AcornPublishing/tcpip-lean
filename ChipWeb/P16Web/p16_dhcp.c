/* DHCP functions for PICmicro Web server - Copyright (c) Iosoft Ltd 2001
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

#define DHCP_INIT  0            // DHCP states: initial
#define DHCP_DISCOVER   1       //              discovery in progress
#define DHCP_OFFER      2       //              offer received OK
#define DHCP_REQUEST    3       //              sending request
#define DHCP_DECLINE    4       //              declining request
#define DHCP_ACK        5       //              acknowledge
#define DHCP_NAK        6       //              negative ack
#define DHCP_RELEASE    7       //              release bound addr
#define DHCP_INFORM     8       //              info to server
#define DHCP_RENEWING   0x13    // Additional states: renewing
#define DHCP_REBINDING  0x23    //                    rebinding
#define DHCP_BOUND      0x30    //                    bound OK

#define DHCP_TYPE_MASK  0x0f    // Mask value to get msg type from state

#define DHCP_DISCTIME (SECTICKS * 5) // Delay between DHCP discovery requests
#define LEASE_MINSECS 60    // Minimum lease renewal time in seconds (16-bit)
#define LEASE_MAXSECS 43200 // Maximum lease renewal time in seconds (16-bit)
#define DHCP_MAXDISCTICKS (SECTICKS*120)

BYTE dhcp_state=0;              // Current DHCP state
WORD dhcp_delticks;             // Current delay tick count
WORD dhcp_secticks;             // ..and seconds tick counter
WORD dhcp_deltickval;           // Current delay value (in ticks)
WORD dhcp_delbaseval;           // Base value for delay (in ticks)
WORD dhcp_secs;                 // Seconds counter
WORD lease_secs;                // Lease time (in seconds)
WORD renew_secs;                // Renewal time (in seconds)
LWORD dhcp_newip;               // IP address offered
LWORD xid;                      // DHCP transaction ID
BYTE dhcp_hosteth[MACLEN];      // DHCP host Ethernet addr
LWORD dhcp_hostip;              // DHCP host IP addr

LWORD leasetime, newip;

SEPARATED void dhcp_tx(BYTE state);
void put_nulls(BYTE n);
WORD rand_byte(WORD val);
WORD rand_bit(WORD val);

/* Do a DHCP request, return 0 if no DHCP address available yet */
SEPARATED BOOL check_dhcp(void)
{
    WORD sernum;

    if (dhcp_state == DHCP_INIT)
    {
        myip.l = 0;
        sernum = swapw(*(WORD *)&myeth[4]);
        dhcp_secs = 0;
        timeout(&dhcp_delticks, 0);
        timeout(&dhcp_secticks, 0);
        dhcp_delbaseval = dhcp_deltickval =
            SECTICKS + (rand_byte(sernum) & 0x1f);
        xid.w[0] = sernum;
        xid.w[1] = dhcp_delbaseval;
        dhcp_state = DHCP_DISCOVER;
    }
    else if (dhcp_state==DHCP_DISCOVER || dhcp_state==DHCP_REQUEST ||
             dhcp_state==DHCP_RENEWING)
    {
        if (timeout(&dhcp_delticks, dhcp_deltickval))
        {
            checkflag = checkhi = checklo = 0;
            dhcp_tx(dhcp_state);
            if (dhcp_deltickval < DHCP_MAXDISCTICKS/2)
                dhcp_deltickval += dhcp_deltickval;
        }
    }
    if (dhcp_state==DHCP_BOUND || dhcp_state==DHCP_RENEWING)
    {
        if (timeout(&dhcp_secticks, SECTICKS))
        {
            dhcp_secs++;
            if (dhcp_secs == renew_secs)
            {
                dhcp_deltickval = dhcp_delbaseval;
                dhcp_state = DHCP_RENEWING;
            }
            else if (dhcp_secs >= lease_secs)
                dhcp_state = DHCP_INIT;
        }
        return(1);
    }
    return(0);
}

/* Handle an incoming DHCP datagram */
void dhcp_handler(void)
{
    BYTE code, len, type=0;

    rx_checkoff = 1;
    if (match_byte(2) &&                // Boot reply?
        skip_byte() && match_byte(6) && // 6-byte MAC address?
        skip_data(13) &&                // Skip to..
        get_lword(&newip) &&            // ..my new IP address
        skip_data(8) &&
        match_data(myeth, MACLEN) &&
        skip_data(10 + 64 + 128))
    {
        print_lcd = print_net = FALSE;
        print_serial = TRUE;
        if (match_byte(99) && match_byte(130) &&
            match_byte(83) && match_byte(99))
        {
            while (get_byte(&code) && code!=255 && get_byte(&len))
            {
                if (code==53 && len==1)     // Message type?
                    get_byte(&type);
                else if (code==51 && len==4)// Lease time?
                    get_lword(&leasetime);
                else if (code==1 && len==4 && type==DHCP_ACK)
                    get_lword(&netmask);    // Subnet mask?
                else if (code==3 && len>=4 && type==DHCP_ACK)
                    get_lword(&router_ip);  // Router IP addr?
                else
                    skip_data(len);
            }
            if (dhcp_state==DHCP_DISCOVER && type==DHCP_OFFER)
            {
                memcpy(dhcp_hosteth, host.eth.srce, MACLEN);
                dhcp_hostip.l = remip.l;
                dhcp_newip.l = newip.l;
                dhcp_deltickval = dhcp_delbaseval;
                dhcp_state = DHCP_REQUEST;
            }
            else if ((dhcp_state==DHCP_REQUEST || dhcp_state==DHCP_RENEWING) &&
                     type==DHCP_ACK)
            {
                lease_secs = LEASE_MAXSECS;
                if (leasetime.l < LEASE_MAXSECS)
                    lease_secs = leasetime.w[0];
                if (lease_secs < LEASE_MINSECS)
                    lease_secs = LEASE_MINSECS;
                renew_secs = lease_secs >> 1;
                dhcp_secs = 0;
                myip.l = dhcp_newip.l;
                disp_myip();
                put_ser("\r\n");
                dhcp_state = DHCP_BOUND;
                net_ok = 1;
            }
        }
    }
}

/* Send a DHCP message */
SEPARATED void dhcp_tx(BYTE state)
{
    BYTE n;
    WORD sernum;

    print_lcd = print_serial = FALSE;
    print_net = TRUE;
    host.eth.pcol = PCOL_IP;
    ipcol = PUDP;
    locport = DHCPCLIENT_PORT;
    remport = DHCPSERVER_PORT;
    setpos_txin(UDPIPHDR_LEN);
    put_byte(1);                        // DHCP request
    put_byte(1);                        // Ethernet h/w, 6-byte addr
    put_byte(MACLEN);
    put_byte(0);                        // Zero hop count
    put_lword(&xid);
    put_nulls(4);
    put_lword(&myip);
    put_nulls(12);
    put_data(myeth, MACLEN);            // My hardware addr
    while (txin < 236+UDPIPHDR_LEN)
        put_byte(0);                    // Rest of hostname & bootfile name
    put_byte(99);                       // DHCP magic cookie
    put_byte(130);
    put_byte(83);
    put_byte(99);

    put_byte(53);                       // DHCP message type
    put_byte(1);
    put_byte(state & DHCP_TYPE_MASK);
    put_byte(61);                       // Client ID
    put_byte(7);
    put_byte(1);                            // ..addr type 1 (Ethernet)
    put_data(myeth, MACLEN);                // ..my MAC address
    put_byte(12);                       // Host name
    print_net = 0;                          // ..get serial num string length
    sernum = swapw(*(WORD *)&myeth[4]);
    n = disp_decword(sernum);
    print_net = 1;
    put_byte(n+1);                          // ..put out length+1
    put_byte('P');                          // ..put out 'P' prefix
    disp_decword(sernum);                   // ..put out serial number
    if (state == DHCP_REQUEST)
    {                                   // If request..
        put_byte(54);                   // ..send server ID (IP addr)
        put_byte(4);
        put_lword(&dhcp_hostip);
        put_byte(50);                   // .. and requested IP addr
        put_byte(4);
        put_lword(&dhcp_newip);
    }
    put_byte(255);                      // End of DHCP options
    if (state == DHCP_RENEWING)
    {                                   // If renewing, use unicast addr
        memcpy(host.eth.srce, dhcp_hosteth, MACLEN);
        remip.l = dhcp_hostip.l;
    }
    else
    {                                   // ..otherwise use broadcast
        memset(host.eth.srce, 0xff, MACLEN);
        memset(&remip, 0xff, sizeof(remip));
    }
    udp_xmit();
    DEBUG_PUTC('D');
}

/* Put the given number of nulls into the Tx buffer */
void put_nulls(BYTE n)
{
    while (n--)
        put_byte(0);
}

/* Randomise lower 8 bits of 16-bit number */
WORD rand_byte(WORD val)
{
    BYTE n;

    for (n=0; n<8; n++)
        val = rand_bit(val);
    return(val);
}

/* Randomise l.s. bit of 16-bit number */
WORD rand_bit(WORD val)
{
    BYTE n=0;

    if (val & 0x8000) n = !n;
    if (val & 0x4000) n = !n;
    if (val & 0x1000) n = !n;
    if (val & 0x0008) n = !n;
    return(val+val+n);
}

/* EOF */
