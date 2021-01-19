/* IP functions for CHIPWEB
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */

#define INCLUDE_DAYTIME  1  // Set non-zero to include 'daytime' service

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
#define TCPOPT_LEN  4       // Length of TCP MSS option
#define TCPSYN_LEN  24      // TCP header length including MSS option  
#define MAXPING_LEN 212     // Max length of Ping data

#define TFIN        0x01    // Option flags: no more data
#define TSYN        0x02    //           sync sequence nums
#define TRST        0x04    //           reset connection
#define TPUSH       0x08    //           push buffered data
#define TACK        0x10    //           acknowledgement
#define TURGE       0x20    //           urgent

#define TCP_MSS     1460    // Max Segment Size for TCP

#define ECHOPORT    7       // TCP Port numbers: echo
#define DAYPORT     13      //                   daytime
#define HTTPORT     80      //                   HTTP

#define DAYMSG      "No daytime msg\r\n"
#define DAYMSG_LEN  16

BYTE ipcol;                 // IP protocol byte
LWORD myip;                 // My IP adress
LWORD locip, remip;         // Local & remote IP addresses
WORD locport, remport;      // ..and TCP port numbers
LWORD rseq, rack;           // TCP sequence & acknowledge values
WORD concount;              // Connection count (for high word of my seq num)
BYTE rflags, tflags;        // Rx and Tx flags
WORD rpdlen, tpdlen;        // Length of user data in Rx, Tx buffer
signed long iplen;          // Incoming/outgoing IP length word
BYTE d_checkhi, d_checklo;  // Checksum value for data

BOOL arp_recv(void);
BOOL ip_recv(void);
BOOL icmp_recv(void);
BOOL tcp_recv(void);
void tcp_handler(void);
void tcp_xmit(void);
#separate
BOOL http_recv(void);
BOOL ftp_handler(void);
BOOL daytime_handler(void);
void put_ip(void);
void put_tcp(void);
void add_lword(LWORD &lw, WORD val);
                                       
/* Handle an ARP message */
BOOL arp_recv(void)
{
    BOOL ret=0;
    
    DEBUG_PUTC('a');                    
    if (match_byte(0x00) && match_byte(0x01) &&     // Hardware type
        match_byte(0x08) && match_byte(0x00) &&     // ARP protocol
        match_byte(6) &&  match_byte(4) &&          // Hardware & IP lengths
        match_word(ARPREQ) &&                       // ARP request
        skip_lword() && skip_word() &&              // Sender's MAC addr
        get_lword(remip.l) &&                       // Sender's IP addr
        skip_lword() && skip_word() &&              // Null MAC addr
        match_lword(myip.l))                        // Target IP addr (me?)
    {
        DEBUG_PUTC('>');
        ret = 1;        
        txin = 0;
        put_word(0x0001);                           // Hardware type
        put_word(0x0800);                           // ARP protocol
        put_byte(6);                                // Hardware & IP lengths
        put_byte(4);                                
        put_word(ARPRESP);                          // ARP response
        put_data(myeth, MACLEN);                    // My MAC addr
        put_lword(myip.l);                          // My IP addr
        put_data(nicin.eth.srce, MACLEN);           // Remote MAC addr
        put_lword(remip.l);                         // Remote IP addr
        put_ether(txbuff, txin);                    // Send to NIC
        xmit_ether(txin);                           // Transmit
        DEBUG_PUTC('A');
    }
    else
        discard_data();
    return(ret);
}

/* Get an IP datagram */
BOOL ip_recv(void)
{
    BYTE b, hi, lo;
    int n=0;
    BOOL ret=1;

    DEBUG_PUTC('i');
    checkflag = 0;                                  // Clear checksum
    checkhi = checklo = 0;
    if (match_byte(0x45) && skip_byte() &&          // Version, service
        get_word(iplen) && skip_word() &&           // Len, ID
        skip_word() &&  skip_byte() &&              // Frags, TTL
        get_byte(ipcol) && skip_word() &&           // Protocol, checksum
        get_lword(remip.l) && match_lword(myip.l) && // Addresses
        checkhi==0xff && checklo==0xff)             // Checksum OK?
    { 
        DEBUG_PUTC('>');
        if (ipcol == PICMP)                         // ICMP?
            icmp_recv();                            // Call ping handler
        else if (ipcol == PTCP)                     // TCP segment?
            tcp_recv();                             // Call TCP handler
        else
            discard_data();                         // Unknown; discard it
    }
    else
        discard_data();
    return(ret);
}

/* Respond to an ICMP message (e.g. ping) */
BOOL icmp_recv(void)
{
    BOOL ret=0;
    WORD csum;

    DEBUG_PUTC('c');
    rpdlen = 0;
    if (match_byte(8) && match_byte(0) && get_word(csum))
    {
        while (skip_byte())                 // Check data
            rpdlen++;
        ret = (checkhi==0xff) && (checklo==0xff);
        if (ret && rpdlen<=MAXPING_LEN) 
        {                                   // If OK and not bcast..
            DEBUG_PUTC('>');
            checkhi = checklo = 0;          // Clear checksum
            put_ip();                       // IP header
            put_word(0);                    // ICMP type and code
            csum += 0x0800;                 // Adjust checksum for resp
            if (csum < 0x0800)              // ..including hi-lo carry
                csum++;
            put_word(csum);                 // ICMP checksum
            put_ether(txbuff, txin);        // Send ICMP response
            copy_rx_tx(txin, IPHDR_LEN+ICMPHDR_LEN, rpdlen);
            xmit_ether(IPHDR_LEN+ICMPHDR_LEN+rpdlen);
            DEBUG_PUTC('I');
        }
    }
    return(ret);
}

/* Respond to an TCP segment */
BOOL tcp_recv(void)
{
    int hlen, n;
    BOOL ret=0;
    WORD addr;

    checkhi = checklo = 0;
    DEBUG_PUTC('t');
    if (get_word(remport) && get_word(locport) &&   // Source & dest ports
        get_lword(rseq.l) && get_lword(rack.l) &&   // Seq & ack numbers
        get_byte(hlen) && get_byte(rflags) &&       // Header len & flags
        skip_word() && skip_lword())                // Window, csum, urgent ptr
    {
        DEBUG_PUTC('>');
        iplen -= IPHDR_LEN;                         // Get TCP segment length
        check_word(iplen);                          // Check pseudoheader
        check_lword(myip.l);
        check_lword(remip.l);
        check_byte(0);
        check_byte(PTCP);
        rxout = (hlen>>2) + IPHDR_LEN;              // Skip over options
        rpdlen = iplen - rxout + IPHDR_LEN; 
        addr = getnic_addr();
        check_rxbytes(IPHDR_LEN+TCPHDR_LEN, iplen-TCPHDR_LEN);
        setnic_addr(addr);
        ret = (checkhi==0xff) && (checklo==0xff);
        if (ret)
            tcp_handler();
    }
    else
        DEBUG_PUTC('!');
    return(ret);
}

/* Handle an incoming TCP segment */
void tcp_handler(void)
{
    BOOL tx=1;                          // Set transmission flag

    tpdlen = tpxdlen = 0;               // Assume no Tx data 
    d_checkhi = d_checklo = 0;
    checkflag = 0;
    tflags = TACK;                      // ..and just sending an ack
    if (rflags & TRST)      		// RESET received?
        tx = 0;                         //..do nothing
    else if (rflags & TSYN)             // SYN received?
    {
        add_lword(rseq.l, 1);           // Adjust Tx ack for SYN
#if INCLUDE_DAYTIME        
        if (locport==DAYPORT || locport==HTTPORT)
#else        
        if (locport==HTTPORT)
#endif        
        {                               // Recognised port?
            rack.w[0] = 0xffff;
            rack.w[1] = concount++;
            tflags = TSYN+TACK;         // Send SYN ACK
        }
        else                            // Unrecognised port?
            tflags = TRST+TACK;         // Send reset
    }
    else if (rflags & TFIN)             // Received FIN?
        add_lword(rseq.l, rpdlen+1);    // Ack all incoming data + FIN
    else if (rflags & TACK)             // ACK received?
    {
        if (rpdlen)                     // Adjust Tx ack for Rx data
            add_lword(rseq.l, rpdlen);
        else                            // If no data, don't send ack
            tx = 0;
        if (locport==HTTPORT && rpdlen)
        {                               // HTTP 'get' method?
            http_recv();                // Call handler..
            tx = 0;                     // ..which does its own Tx
        }
#if INCLUDE_DAYTIME
        else if (locport==DAYPORT && rack.w[0]==0)
        {                               // Daytime request?
            daytime_handler();          // Prepare daytime data
            tx = 1;                     // ..and send it
        }
#endif        
    }
    if (tx)                             // If transmission required
        tcp_xmit();                     // ..do it
}

/* Transmit a TCP segment */
void tcp_xmit(void)
{
    iplen = tpdlen + tpxdlen + IPHDR_LEN + TCPHDR_LEN;
    if (tflags & TSYN)                  // If SYN, allow for MSS option
        iplen += TCPOPT_LEN;
    put_ip();                           // IP header
    put_tcp();                          // TCP header
    put_ether(txbuff, txin+tpdlen);     // Send to NIC
    xmit_ether(txin+tpdlen+tpxdlen);    // Transmit
    DEBUG_PUTC('T');
}

/* Respond to an Daytime request */
BOOL daytime_handler(void)
{
    checkhi = checklo = 0;
    txin = IPHDR_LEN + TCPHDR_LEN;      // O/P data to buffer, calc checksum
    printf(put_byte, DAYMSG);
    tpdlen = DAYMSG_LEN;                // Data length of response
    d_checkhi = checkhi;                // Save checksum
    d_checklo = checklo;
    tflags = TFIN+TACK;                 // Ack & close connection
}

/* Put out a TCP segment. Data checksum must have already been computed */
void put_tcp(void)
{
    WORD len;

    checkflag = 0;                  // Ensure we're on an even byte
    checkhi = d_checkhi;            // Retrieve data checksum
    checklo = d_checklo;
    put_word(locport);              // Local and remote ports
    put_word(remport);
    put_lword(rack.l);              // Seq & ack numbers
    put_lword(rseq.l);
    put_byte(tflags&TSYN ? TCPSYN_LEN*4 : TCPHDR_LEN*4);   // Header len
    put_byte(tflags);
    put_byte(0x0b);                 // Window size word
    put_byte(0xb8);
    len = tpdlen + tpxdlen + TCPHDR_LEN;
    if (tflags & TSYN)              // If sending SYN, send MSS option
    {
        txin += 4;                  // Put MSS in buffer after TCP header
        len += TCPOPT_LEN;
        put_byte(2);
        put_byte(4);
        put_word(TCP_MSS);
        txin -= TCPOPT_LEN + 4;     // Go back to checksum in header
    }
    check_lword(myip.l);            // Add pseudoheader to checksum
    check_lword(remip.l);
    check_byte(0);
    check_byte(PTCP);
    check_word(len);
    put_byte(~checkhi);             // Send checksum
    put_byte(~checklo);
    put_word(0);                    // Urgent ptr
    if (tflags & TSYN)              // Adjust Tx ptr if sending MSS option
        txin += TCPOPT_LEN;
}

/* Send out an IP datagram header, given data length */
void put_ip(void)
{
    static BYTE id=0;

    txin = 0;
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
    check_lword(myip.l);            // Include addresses in checksum
    check_lword(remip.l);
    put_byte(~checkhi);             // Checksum
    put_byte(~checklo);
    put_lword(myip.l);              // Source & destination IP addrs
    put_lword(remip.l);
}

/* Add a 16-bit value to a longword */
void add_lword(LWORD &lw, WORD val)
{
    if ((lw.w[0] += val) < val)
        lw.w[1]++;
}

/* EOF */
