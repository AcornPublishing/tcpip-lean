/* IP functions for CHIPWEB  - Copyright (c) Iosoft Ltd 2001
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

#define PCOL_ARP    0x0806  // Protocol type: ARP
#define PCOL_IP     0x0800  //                IP

#define ARPREQ     0x0001   // ARP request & response IDs
#define ARPRESP    0x0002

#define PICMP       1       // IP protocol values: ICMP
#define PTCP        6       //                     TCP
#define PUDP        17      //                     UDP

#define ETHHDR_LEN  6       // Ethernet frame header length
#define IPHDR_LEN   20      // IP, TCP and ICMP header lengths
#define TCPHDR_LEN  20
#define ICMPHDR_LEN 4       // (only include type, code & csum in ICMP hdr)
#define TCPIPHDR_LEN 40     // TCP/IP header length, assuming no options
#define TCPOPT_LEN  4       // Length of TCP MSS option
#define TCPSYN_LEN  24      // TCP header length including MSS option
#define MAXPING_LEN 1476    // Max length of Ping data

#define TFIN        0x01    // Option flags: no more data
#define TSYN        0x02    //           sync sequence nums
#define TRST        0x04    //           reset connection
#define TPUSH       0x08    //           push buffered data
#define TACK        0x10    //           acknowledgement
#define TURGE       0x20    //           urgent

#define ECHOPORT    7       // TCP/UDP Port numbers: echo
#define DAYPORT     13      //                       daytime
#define TELPORT     21      //                       telnet
#define HTTPORT     80      // TCP port number: HTTP
#define VIDPORT     1502    // UDP port number: video capture

#define DAYMSG      "No daytime\r\n"

BYTE ipcol;                 // IP protocol byte
LWORD locip, remip;         // Local & remote IP addresses
WORD locport, remport;      // ..and TCP/UDP port numbers
BYTE remeth[6];
WORD iplen;                 // Incoming/outgoing IP length word
BANK2 BYTE d_checkhi, d_checklo;    // Checksum value for data

BOOL arp_recv(void);
void arp_xmit(BYTE op);
SEPARATED BOOL ip_recv(void);
BOOL icmp_recv(void);
void put_ip(void);
void add_lword(LWORD *lwp, WORD val);

#if INCLUDE_ETH
/* Handle an ARP message */
BOOL arp_recv(void)
{
    WORD op;

    DEBUG_PUTC('a');
    DEBUG_PUTC('>');
    if (match_byte(0x00) && match_byte(0x01) &&     // Hardware type
        match_byte(0x08) && match_byte(0x00) &&     // ARP protocol
        match_byte(6) &&  match_byte(4) &&          // Hardware & IP lengths
        get_word(&op) &&                            // Operation
        get_data(remeth, MACLEN) &&                 // Sender's MAC addr
        get_lword(&remip) &&                        // Sender's IP addr
        skip_data(6) &&                             // Null MAC addr
        match_lword(&myip))                         // Target IP addr (me?)
    {
        DEBUG_PUTC('>');
        if (op == ARPREQ)                           // Received ARP request?
        {
            arp_xmit(ARPRESP);
            DEBUG_PUTC('A');
            return(1);
        }
        else if (op == ARPRESP)                     // Received ARP response?
        {
            if (remip.l == router_ip.l)
                memcpy(router_eth, remeth, MACLEN);
            arp_resp = 1;
        }
    }
    DEBUG_PUTC('?');
    return(0);
}

/* Send an ARP request or response */
void arp_xmit(BYTE op)
{
    init_txbuff(0);
    if (op == ARPREQ)                           // If request, broadcast
        memset(host.eth.srce, 0xff, MACLEN);
    put_word(0x0001);                           // Hardware type
    put_word(0x0800);                           // ARP protocol
    put_byte(6);                                // Hardware & IP lengths
    put_byte(4);
    put_word((WORD)op);                         // ARP req/resp
    put_data(myeth, MACLEN);                    // My MAC addr
    put_lword(&myip);                           // My IP addr
    put_data(host.eth.srce, MACLEN);            // Remote MAC addr
    put_lword(&remip);                          // Remote IP addr
    host.eth.pcol = PCOL_ARP;
    transmit();
    DEBUG_PUTC('A');
}

/* Check if the given host is within my subnet (i.e. router not required) */
BOOL in_subnet(LWORD *hip)
{
    return(((hip->l ^ myip.l) & netmask.l) == 0);
}

/* Send an ARP request for the given host */
void arp_host(LWORD *hip)
{
    if (in_subnet(hip))
        remip.l = hip->l;
    else
        remip.l = router_ip.l;
    arp_xmit(ARPREQ);
}

/* Check if an ARP response has been received for the given host */
BOOL host_arped(LWORD *hip)
{
    if (in_subnet(hip))
        return(arp_resp && remip.l==hip->l);
    else if (router_arp_ok)
        return(1);
    else
        return(arp_resp && remip.l==router_ip.l);
}
#else
void arp_host(LWORD *hip)
{
}
BOOL host_arped(LWORD *hip)
{
    return(ipcp_state == PPP_OPENED);
}
#endif // INCLUDE_ETH

/* Check an IP header, return the protocol in 'ipcol' */
SEPARATED BOOL ip_recv(void)
{
    DEBUG_PUTC('i');
    DEBUG_PUTC('>');
    rx_checkoff = checkflag = 0;                    // Clear checksum
    checkhi = checklo = 0;
    if (match_byte(0x45) && skip_byte() &&          // Version, service
        get_word(&iplen) && skip_word() &&          // Len, ID
        skip_word() &&  skip_byte() &&              // Frags, TTL
        get_byte(&ipcol) && skip_word() &&          // Protocol, checksum
        get_lword(&remip) && get_lword(&locip) &&   // Addresses
        (locip.l==myip.l || locip.l==0xffffffffL) &&
        checkhi==0xff && checklo==0xff &&           // Checksum OK?
        iplen>IPHDR_LEN)                            // IP length OK
    {
        DEBUG_PUTC('>');
        truncate_rxout(iplen-IPHDR_LEN);
        return(1);
    }
    return(0);
}

/* Respond to an ICMP message (e.g. ping) */
BOOL icmp_recv(void)
{
    WORD csum, addr, len;

    DEBUG_PUTC('c');
    DEBUG_PUTC('>');
    checkhi = checklo = checkflag = 0;
    len = iplen - IPHDR_LEN - 4;
    if (locip.l==myip.l && match_byte(8) && match_byte(0) && get_word(&csum))
    {
        addr = rxout;
        skip_data(len);
        setpos_rxout(addr);
        if (checkhi==0xff && checklo==0xff)
        {                                   // If OK and not bcast..
            DEBUG_PUTC('>');
            put_ip();                       // IP header
            put_word(0);                    // ICMP type and code
            csum += 0x0800;                 // Adjust checksum for resp
            if (csum < 0x0800)              // ..including hi-lo carry
                csum++;
            put_word(csum);                 // ICMP checksum
            copy_rx_tx(len);                // Copy data to Tx buffer
            transmit();                     // ..and send it
            DEBUG_PUTC('I');
            return(1);
        }
        DEBUG_PUTC('#');
    }
    return(0);
}

/* Put an IP datagram header in the Tx buffer */
void put_ip(void)
{
    static BYTE id=0;

    checkhi = checklo = 0;          // Clear checksum
    checkflag = 0;
    put_byte(0x45);                 // Version & hdr len */
    put_byte(0);                    // Service
    put_word(iplen);
    put_byte(0);                    // Ident word
    put_byte(++id);
    put_word(0);                    // Flags & fragment offset
    put_byte(100);                  // Time To Live
    put_byte(ipcol);                // Protocol
    check_lword(&myip);             // Include addresses in checksum
    check_lword(&remip);
    put_byte(~checkhi);             // Checksum
    put_byte(~checklo);
    put_lword(&myip);               // Source & destination IP addrs
    put_lword(&remip);
#if INCLUDE_PPP
    ppp_pcol = PPP_IP_DATA;
#endif
}

/* Add a 16-bit value to a longword */
void add_lword(LWORD *lwp, WORD val)
{
    lwp->w[0] += val;
    if (lwp->w[0] < val)
        lwp->w[1]++;
}

/* EOF */
