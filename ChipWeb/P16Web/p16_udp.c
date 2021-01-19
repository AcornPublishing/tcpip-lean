/* UDP functions for PICmicro Web server - Copyright (c) Iosoft Ltd 2001
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

#define UDPHDR_LEN    8         // UDP header length
#define UDPIPHDR_LEN  28        // UDP+IP header length
#define MAXUDP_DLEN (1500-20-8) // Max length of UDP data

#define DHCPSERVER_PORT 67      // UDP ports for DHCP server & client
#define DHCPCLIENT_PORT 68
#define TIMECLIENT_PORT 1501    // Arbitrary source port for time client
#define TIMESERVER_PORT 37

WORD udplen;                    // UDP length (data + header)
WORD ucsum;                     // UDP checksum
BOOL udp_checkoff;              // Flag to disable UDP checksum

SEPARATED BOOL udp_recv(void);
void udp_xmit(void);
void put_udp(void);

void time_handler(void);
void vid_handler(void);
void dhcp_handler(void);

/* Receive an incoming UDP datagram, return 0 if invalid */
SEPARATED BOOL udp_recv(void)
{
    WORD addr;

    checkhi = checklo = 0;
    checkflag = udp_checkoff = 0;
    DEBUG_PUTC('u');
    DEBUG_PUTC('>');
    if (get_word(&remport) && get_word(&locport) && // Source & dest ports
        get_word(&udplen) && get_word(&ucsum) &&    // Dgram length, checksum
        udplen>=UDPHDR_LEN && udplen<=MAXUDP_DLEN)
    {
        DEBUG_PUTC('>');
        if (ucsum)
        {
            check_word(udplen);                 // Check pseudoheader
            check_lword(&locip);
            check_lword(&remip);
            check_byte(0);
            check_byte(PUDP);
            addr = rxout;
            skip_data(rxleft());
            setpos_rxout(addr);
        }                                       // If checksum OK
        if (ucsum==0 || (checkhi==0xff && checklo==0xff))
        {                                       // ..call UDP handler
            init_txbuff(0);
            checkhi = checklo = 0;              // Clear checksum
            checkflag = 0;
            DEBUG_PUTC('*');
            if (locport == ECHOPORT)            // Echo: return copy of data
            {
                setpos_txin(UDPIPHDR_LEN);
                copy_rx_tx(udplen - UDPHDR_LEN);
                udp_xmit();
                DEBUG_PUTC('U');
            }
            else if (locport == DAYPORT)        // Daytime: return string
            {
                print_lcd = print_serial = FALSE;
                print_net = TRUE;
                setpos_txin(UDPIPHDR_LEN);
                putstr(DAYMSG);
                udp_xmit();
            }
#if INCLUDE_DHCP
            else if (locport == DHCPCLIENT_PORT)    // DHCP client
                dhcp_handler();
#endif
#if INCLUDE_TIME
            else if (locport == TIMECLIENT_PORT)    // Time client
                time_handler();
#endif
#if INCLUDE_VID
            else if (locport == VIDPORT)        // Video capture
                vid_handler();
#endif
            return(1);
        }
    }
    DEBUG_PUTC('!');
    return(0);
}

/* Transmit the UDP data that is in the Tx buffer */
void udp_xmit(void)
{
    WORD tpdlen;

    save_txbuff();
    d_checkhi = checkhi;                // Save checksum
    d_checklo = checklo;
    tpdlen = 0;                         // Get data length
    if (txin > UDPIPHDR_LEN)
        tpdlen = txin - UDPIPHDR_LEN;
    udplen = tpdlen + UDPHDR_LEN;       // ..and UDP length
    iplen = udplen + IPHDR_LEN;         // ..and IP length
    setpos_txin(0);                     // Go to start of Tx buffer
    host.eth.pcol = PCOL_IP;            // Ether protocol is IP
    ipcol = PUDP;                       // ..and IP protocol is UDP
    put_ip();                           // Add IP header
    put_udp();                          // Add UDP header
    transmit();                         // Send it all!
    DEBUG_PUTC('U');
}

/* Put out a UDP datagram. Data checksum must have already been computed */
void put_udp(void)
{
    checkflag = 0;                  // Ensure we're on an even byte
    checkhi = d_checkhi;            // Retrieve data checksum
    checklo = d_checklo;
    put_word(locport);              // Local and remote ports
    put_word(remport);
    put_word(udplen);               // UDP length
    check_lword(&myip);             // Add pseudoheader to checksum
    check_lword(&remip);
    check_byte(0);
    check_byte(PUDP);
    check_word(udplen);
    if (udp_checkoff)               // If no checksum computed..
    {                               // ..force to null value
        checkhi = checklo = 0xff;
    }                               // (don't remove the braces!)
    put_byte(~checkhi);             // Send checksum
    put_byte(~checklo);
}

/* EOF */
