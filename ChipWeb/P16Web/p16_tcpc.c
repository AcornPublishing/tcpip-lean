/* TCP client functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define CLIENT_DEBUG    0   // Set non-zero to O/P serial debug info

#define MAILSERVER_ADDR 10,1,1,1
#define SMTP_PORT 25
#define POP3_PORT 110

/* The ephemeral port range for clients; the original values are 1024-4999,
** though newer systems use 49152-65535 */
#define MIN_CLIENT_PORT    1024
#define MAX_CLIENT_PORT    5000

#define CLIENT_ARP_TIMEOUT (2*SECTICKS)
#define CLIENT_ARP_TRIES    4
#define CLIENT_TCP_TIMEOUT (1*SECTICKS)
#define CLIENT_TCP_TRIES    5

#define CLIENT_SEQ_START    9999
#define CLIENT_INIT_PORT    1500

#define CLIENT_INIT     0
#define CLIENT_ARPS     1
#define CLIENT_SYNS     2
#define CLIENT_EST      3
#define CLIENT_LASTACK  4
#define CLIENT_FINWT    5

BYTE cstate=0;              // Client TCP state
WORD cticks;                // Tick count for timeout
WORD ctimeout;              // Timeout value
BYTE ctries;                // Number of retries left
LWORD ctseq;                // Transmit sequence number
LWORD ctack;                // Transmit acknowledgement number
LWORD chostip;              // Remote host IP address
BYTE chosteth[MACLEN];      // Remote host MAC address

void client_xmit(BYTE flags);
void client_rst_xmit(void);
SEPARATED BYTE smtp_client_data(BYTE rflags);
SEPARATED BYTE pop3_client_data(BYTE rflags);
void new_cstate(BYTE news);

/* Receive a response to the TCP client */
SEPARATED void tcp_client_recv(BYTE rflags)
{
    BYTE tflags;

    rflags &= (TFIN+TSYN+TRST+TACK);        // Mask out PUSH & URG flags
    init_txbuff(1);                         // Set up Tx buffer..
    setpos_txin(TCPIPHDR_LEN);              // ..assuming default TCP+IP hdr
    if (cstate<=CLIENT_SYNS || ((ctack.l==rseq.l) && (ctseq.l==rack.l) &&
        remip.l==chostip.l))
    {                                       // Check current TCP state
        switch (cstate)
        {                                   // Closed or sending ARPs?
        case CLIENT_INIT:
        case CLIENT_ARPS:
            client_rst_xmit();
            break;

        case CLIENT_SYNS:                   // SYN sent and SYN ACK value OK?
            if (rflags==TSYN+TACK && rack.l==CLIENT_SEQ_START+1)
            {
#if CLIENT_DEBUG
                put_ser( " CONNECTED\r\n");
#endif
                rpdlen++;                   // Send ACK
                client_xmit(TACK);
                mstate = MAIL_INIT;         // ..init mail state machine
                new_cstate(CLIENT_EST);     // ..and go established
                USERLED2 = USERLEDON;
            }
            else
            {                               // SYN sent and not SYN ACK?
                init_txbuff(0);
                client_xmit(TRST+TACK);     // Send RESET
            }
            break;

        case CLIENT_EST:                    // Established
            if (rpdlen)
                new_cstate(CLIENT_EST);     // Refresh timer
            if (remport == SMTP_PORT)       // SMTP client data?
#if INCLUDE_SMTP
                tflags = smtp_client_data(rflags);
#else
                client_rst_xmit();
#endif
            if (remport == POP3_PORT)       // POP3 client data?
#if INCLUDE_POP3
                tflags = pop3_client_data(rflags);
#else
                client_rst_xmit();
#endif
            if (rflags & TFIN)              // If remote close..
            {
                tflags = TFIN+TACK;         // ..send FIN ACK
                rpdlen++;
                new_cstate(CLIENT_LASTACK);
            }
            else if (tflags & TFIN)         // If local close..
                new_cstate(CLIENT_FINWT);   // ..wait for FIN ACK
            if (tflags)
                client_xmit(tflags);        // If anything to send, do so
            break;

        case CLIENT_LASTACK:
            if (rflags == TACK)             // FIN ACK sent, waiting for ACK
                new_cstate(CLIENT_INIT);    // ..if received, go idle
            break;

        case CLIENT_FINWT:
            if (rflags & TFIN)              // FIN sent, waiting for FIN ACK
            {
                rpdlen++;
                client_xmit(TACK);          // ..if received, ACK and go idle
                new_cstate(CLIENT_INIT);
            }
            else if (rpdlen)                // ..if only data, ACK it
                client_xmit(TACK);
            break;
        }
    }
}

/* Start up the TCP client */
BOOL start_client(WORD hport)
{
    if (cstate == CLIENT_INIT)
    {
        init_ip(&chostip, MAILSERVER_ADDR);
        remport = hport;
        USERLED1 = USERLEDON;
        USERLED2 = USERLEDOFF;
#if INCLUDE_DIAL
        do_dial();
        if (!modem_connected)
            return(0);
        USERLED2 = USERLEDON;
        init_net();
        lcp_event_handler(EVENT_UP);
        do_lcp_actions();
#endif
#if CLIENT_DEBUG
        put_ser( "\r\nARP");
#endif
        arp_host(&chostip);
        timeout(&cticks, 0);
        ctries = CLIENT_ARP_TRIES;
        new_cstate(CLIENT_ARPS);
        return(1);
    }
    return(0);
}

/* Start up the TCP client */
void stop_client(void)
{
    new_cstate(CLIENT_INIT);
#if INCLUDE_DIAL
    modem_connected = 0;
#endif
}

/* Check the state of the TCP client */
void check_client(void)
{
    if (cstate == CLIENT_ARPS)              // If sending ARPs
    {
        if (host_arped(&chostip))           // ..and response received
        {
#if CLIENT_DEBUG
            put_ser( " TCP");
#endif                                      // Get MAC address
            memcpy(chosteth, remeth, MACLEN);
            init_txbuff(1);                 // Choose next client port num
            if (++cport<MIN_CLIENT_PORT || cport>=MAX_CLIENT_PORT)
                cport = MIN_CLIENT_PORT;
            locport = cport;
            rseq.l = 0;
            rack.l = CLIENT_SEQ_START;      // Set starting SEQ number
            client_xmit(TSYN);              // Send SYN
            new_cstate(CLIENT_SYNS);
        }
        else if (timeout(&cticks, CLIENT_ARP_TIMEOUT))
        {
            if (ctries--)                   // If ARP timeout..
                arp_host(&chostip);         // ..resend..
            else                            // ..or stop if no more retries
            {
#if CLIENT_DEBUG
                put_ser( "\r\nFAILED\r\n");
#endif
                stop_client();
                USERLED1 = USERLEDON;
            }
        }
    }
    else if (cstate>CLIENT_ARPS && timeout(&cticks, ctimeout))
    {
        if (ctries--)                       // If TCP timeout..
        {
            ctimeout += ctimeout;           // ..resend last packet
            retransmit(1);
        }
        else                                // ..or stop if no more retries
        {
#if CLIENT_DEBUG
            put_ser( "\r\nFAILED\r\n");
#endif
            if (cstate > CLIENT_SYNS)       // ..sending RESET if connected
            {
                init_txbuff(0);
                client_xmit(TRST+TACK);
            }
            stop_client();
            USERLED2 = USERLEDON;
        }
    }
}

/* Transmit on the given socket */
void client_xmit(BYTE flags)
{
    WORD tpdlen=0;

    save_txbuff();
    add_lword(&rseq, rpdlen);               // My ACK = remote SEQ + datalen
    ctack.l = rseq.l;                       // Save my ACK & SEQ values
    ctseq.l = rack.l;
    if (net_txlen > TCPIPHDR_LEN)           // Get my TCP Tx data length
        tpdlen = net_txlen - TCPIPHDR_LEN;
    if (flags & (TSYN+TFIN))                // SYN/FIN flags count as 1 byte
        tpdlen++;
    add_lword(&ctseq, tpdlen);              // My next seq = SEQ + datalen
    tcp_xmit(flags);                        // Transmit the packet
}

/* Send a reset response */
void client_rst_xmit(void)
{
    init_txbuff(0);
    client_xmit(TRST+TACK);
}

/* Change client state, refesh timer */
void new_cstate(BYTE news)
{
    timeout(&cticks, 0);
    ctries = CLIENT_TCP_TRIES;
    ctimeout = CLIENT_TCP_TIMEOUT;
#if CLIENT_DEBUG
    if (news==CLIENT_INIT && cstate!=CLIENT_INIT)
        put_ser( "\r\nDISCONNECTED\r\n");
#endif
    cstate = news;
    flashled1 = (cstate == CLIENT_ARPS);
    flashled2 = (cstate == CLIENT_EST);
}

/* EOF */
