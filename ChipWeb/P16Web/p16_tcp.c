/* TCP functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define TCP_RXCHECK 1       // Set non-zero to enable TCP receive checksum

#define CLIENT_PORT 1500

#define TCP_MSS (MAXNET_DLEN-TCPIPHDR_LEN)  // Maximum segment size

LWORD rseq, rack;           // TCP sequence & acknowledge values
BANK2 WORD concount;        // Connection count (for high word of my seq num)
WORD rpdlen;
WORD cport;

SEPARATED BOOL tcp_recv(void);
BOOL socket_handler(void);
BOOL daytime_handler(void);
void tcp_xmit(BYTE tflags);
void put_tcp(BYTE tflags);

SEPARATED BOOL http_recv(void);
SEPARATED void tcp_client_recv(BYTE rflags);

/* Receive a TCP 'segment' */
SEPARATED BOOL tcp_recv(void)
{
    BYTE hlen, rflags, tflags=TACK;
#if TCP_RXCHECK
    WORD addr;
#endif

    checkflag = 0;                                  // Clear checksum
    checkhi = checklo = 0;
    DEBUG_PUTC('t');
    if (get_word(&remport) && get_word(&locport) && // Source & dest ports
        get_lword(&rseq) && get_lword(&rack) &&     // Seq & ack numbers
        get_byte(&hlen) && get_byte(&rflags) &&     // Header len & flags
        skip_data(6) && iplen>=TCPIPHDR_LEN &&      // Window, csum, urgent ptr
        locip.l==myip.l)                            // Addr match
    {
        DEBUG_PUTC('>');
        check_word(iplen - IPHDR_LEN);                // Check pseudoheader
        check_lword(&myip);
        check_lword(&remip);
        check_byte(0);
        check_byte(PTCP);
        while (hlen>TCPHDR_LEN*4 && iplen>0 && skip_lword())
        {
            iplen -= 4;
            hlen -= 16;
        }
        rpdlen = iplen - TCPIPHDR_LEN;
#if TCP_RXCHECK
        addr = rxout;
        skip_data(rpdlen);
        setpos_rxout(addr);
        if (checkhi==0xff && checklo==0xff)
        {
#endif
            DEBUG_PUTC('>');
            init_txbuff(0);
            setpos_txin(TCPIPHDR_LEN);
            d_checkhi = d_checklo = 0;
            print_lcd = print_serial = FALSE;
            print_net = TRUE;
#if INCLUDE_TCP_CLIENT
            if (locport == cport)
            {
                tcp_client_recv(rflags);
                rflags = tflags = 0;
            }
#endif
            if (rflags & TRST)                  // RESET received?
                tflags = 0;                     //..do nothing
            else if (rflags & TSYN)             // SYN received?
            {
                add_lword(&rseq, 1);            // ..adjust Tx ack for SYN
#if INCLUDE_HTTP
                if (locport==DAYPORT || locport==HTTPORT)
#else
                if (locport==DAYPORT)
#endif
                {                               // Recognised port?
                    rack.w[0] = 0xffff;
                    rack.w[1] = concount++;
                    tflags = TSYN+TACK;         // ..send SYN ACK
                }
                else                            // Unrecognised port?
                    tflags = TRST+TACK;         // ..send reset
            }
            else if (rflags & TFIN)             // Received FIN?
                add_lword(&rseq, rpdlen+1);     // ..ack all incoming data + FIN
            else if (rflags & TACK)             // ACK received?
            {
                if (rpdlen)                     // ..adjust Tx ack for Rx data
                    add_lword(&rseq, rpdlen);
                else                            // If no data, don't send ack
                    tflags = 0;

                if (locport==DAYPORT && rack.w[0]==0)
                {                               // Daytime request?
                    putstr(DAYMSG);
                    tflags = TFIN+TACK;         // Ack & close connection
                }
#if INCLUDE_HTTP
                else if (locport==HTTPORT && rpdlen)
                {                               // HTTP 'get' method?
                    http_recv();                // ..call handler..
                    tflags = TFIN+TACK;
                }
#endif
            }
            if (tflags)                         // If transmission required
                tcp_xmit(tflags);               // ..do it here
            return(1);                          // ..to minimise stack usage
#if TCP_RXCHECK
        }
#endif
    }
    DEBUG_PUTC('!');
    return(0);
}

/* Transmit the TCP data that is in the Tx buffer */
void tcp_xmit(BYTE tflags)
{
    save_txbuff();
    d_checkhi = checkhi;                // Save checksum
    d_checklo = checklo;
    if (net_txlen < TCPIPHDR_LEN)       // If no data payload..
        net_txlen = TCPIPHDR_LEN;       // ..just send IP+TCP headers
    if (tflags & TSYN)                  // If SYN, allow for MSS option
        net_txlen += TCPOPT_LEN;
    iplen = net_txlen;
    setpos_txin(0);                     // Go to start of Tx buffer
    ipcol = PTCP;                       // Set IP protocol field
    put_ip();                           // Add IP header
    put_tcp(tflags);                    // Add TCP header
    transmit();                         // Send it all!
    DEBUG_PUTC('T');
}

/* Put out a TCP segment. Data checksum must have already been computed */
void put_tcp(BYTE tflags)
{
#if INCLUDE_ETH
    host.eth.pcol = PCOL_IP;        // Set Ethernet protocol field
#endif
    checkflag = 0;                  // Ensure we're on an even byte
    checkhi = d_checkhi;            // Retrieve data checksum
    checklo = d_checklo;
    put_word(locport);              // Local and remote ports
    put_word(remport);
    put_lword((LWORD *)&rack);               // Seq & ack numbers
    put_lword((LWORD *)&rseq);
    put_byte(tflags&TSYN ? TCPSYN_LEN*4 : TCPHDR_LEN*4);   // Header len
    put_byte(tflags);
    put_byte(0x0b);                 // Window size word
    put_byte(0xb8);
    if (tflags & TSYN)              // If sending SYN, send MSS option
    {                               // Put MSS in buffer after TCP header
        setpos_txin(TCPIPHDR_LEN);
        put_byte(2);
        put_byte(4);
        put_word(TCP_MSS);
        setpos_txin(TCPIPHDR_LEN-4);
    }                               // Go back to checksum in header
    check_lword(&myip);             // Add pseudoheader to checksum
    check_lword(&remip);
    check_byte(0);
    check_byte(PTCP);
    check_word(iplen - IPHDR_LEN);
    put_byte(~checkhi);             // Send checksum
    put_byte(~checklo);
    put_word(0);                    // Urgent ptr
    if (tflags & TSYN)              // Adjust Tx ptr if sending MSS option
        setpos_txin(TCPIPHDR_LEN+TCPOPT_LEN);
}

/* EOF */

