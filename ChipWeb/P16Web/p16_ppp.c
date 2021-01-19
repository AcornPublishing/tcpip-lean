/* PPP functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define DEBUG_IPCP          0   // Set non-zero to emit IPCP state codes
#define INCLUDE_CLIENT_AUTH 1   // Set non-zero to enable client (outgoing) PAP
#define INCLUDE_SERVER_AUTH 0   // Set non-zero to enable server (incoming) PAP

#ifndef NET_TXBUFFERS
#define NET_TXBUFFERS 1         // Number of network transmit buffers
#endif

DEFBIT_2(PORTE, NIC_RESET)      // NIC I/O Definitions
DEFBIT_1(PORTE, NIC_IOW_)
DEFBIT_0(PORTE, NIC_IOR_)

DEFBIT_2(PORTC, SER_RTS_)       // RTS I/P
DEFBIT_5(PORTC, SER_CTS_)       // CTS O/P
#define TRISC_VAL   0xdf        // CTS is only O/P on port C

// PPP async encapsulation escape codes
#define PPP_END 0x7e
#define PPP_ESC 0x7d
// HDLC marker for start of frame
#define PPP_START 0xff

// PPP protocols
#define PPP_LCP         0xc021  // Link control
#define PPP_IPCP        0x8021  // IP control
#define PPP_IP_DATA     0x0021  // IP data
#define PPP_CCP         0x80fd  // Compression control
#define PPP_PAP         0xc023  // Password authentication

// Values for PPP negotiation code field
#define PPP_CFG_REQ     1
#define PPP_CFG_ACK     2
#define PPP_CFG_NAK     3
#define PPP_CFG_REJ     4
#define PPP_TERM_REQ    5
#define PPP_TERM_ACK    6
#define PPP_CODE_REJ    7
#define PPP_PCOL_REJ    8
#define PPP_ECHO_REQ    9
#define PPP_ECHO_REP    0xa

#define ACTION_MASK     0xfff0
#define STATE_MASK      0x000f

// PPP states (one state machine per protocol) from RFC 1661
#define PPP_INITIAL     0
#define PPP_STARTING    1
#define PPP_CLOSED      2
#define PPP_STOPPED     3
#define PPP_CLOSING     4
#define PPP_STOPPING    5
#define PPP_REQ_SENT    6
#define PPP_ACK_RCVD    7
#define PPP_ACK_SENT    8
#define PPP_OPENED      9

// PPP events (from RFC 1661)
#define EVENT_UP        0
#define EVENT_DOWN      1
#define EVENT_OPEN      2
#define EVENT_CLOSE     3
#define EVENT_TO_OK     4
#define EVENT_TO_ERR    5
#define EVENT_RCR_OK    6
#define EVENT_RCR_ERR   7
#define EVENT_RCA       8
#define EVENT_RCN       9
#define EVENT_RTR       10
#define EVENT_RTA       11
#define EVENT_RUC       12
#define EVENT_RXJ_OK    13
#define EVENT_RXJ_ERR   14
#define EVENT_RXR       15
// Event flag for modem Rx (lo byte is char)
#define EVENT_RXCHAR    0x100

// PPP options
#define LCP_OPT_AUTH    0x3
#define IPCP_OPT_ADDR   0x3

// Data sizes
#define MAXLCP_OPTLEN   256
#define PPP_MRU         1500
#define MAXPPP          1510

// Timeout and retries
#define PPP_TIMEOUT     (2*SECTICKS)
#define MAX_REQS        15
#define MAX_TERMS       2

#define MACLEN   6              /* Dummy MAC address */
BYTE myeth[MACLEN];

#define PPP_HEADLEN 4
//#define PPP_HEADLEN 6

#define RXBUFFLEN 128           // Size of Rx buffer
BANK2 WORD net_rxin;            // Length of current frame
BANK2 WORD mdm_rxin;            // Length of current modem command
BANK2 WORD rxbuffin;            // Incoming Rx buffer pointer
BANK2 BOOL rx_checkoff;         // Flag to disable Rx checksumming
BANK2 WORD rxout;               // Outgoing Rx buffer pointer
BANK2 BOOL atend;
BANK1 BYTE rxbuff[RXBUFFLEN];   // Rx buffer
LOCATE(rxbuff, 0x110)
BOOL inframe;                   // Flag set if PPP frame is being received

WORD pppticks;
BYTE lcp_reqs, lcp_terms;
BYTE lcp_rest;
BYTE lcp_state, lcp_action;
BYTE ipcp_reqs, ipcp_terms;
BYTE ipcp_rest;
BYTE ipcp_state, ipcp_action;

WORD ppp_pcol;
BYTE ppp_code;
BYTE ppp_rxid, ppp_txid;

BOOL client_auth, client_auth_ok;
BOOL server_auth, server_auth_ok;
char clientname[12]="client";
char clientpass[12]="secret";
char serverid[] = "\7client2\7secret2";

#define TXBUFFLEN 128           // Size of Tx buffer
BANK3 WORD net_txlen;           // Max length of Tx data sent to NIC
BANK3 WORD txin;                // Current I/P pointer for Tx data
BANK3 BYTE txbuff[TXBUFFLEN];   // Tx buffer
LOCATE(txbuff, 0x190)
WORD txcrc;                     // Tx CRC value

#if NET_TXBUFFERS > 1
BYTE txbuffnum;
BANK3 BYTE txbuff2[TXBUFFLEN];  // 2nd Tx buffer
WORD txbuff_lens[NET_TXBUFFERS];// Length of last buffer transmission
#endif

#define MAXNET_DLEN (RXBUFFLEN-6)

// PPP action codes for state table
#define XXX 0x0f
#define TLU 0x10
#define TLD 0x20
#define TLS 0x40
#define TLF 0x80
// PPP action codes for action table
#define IRC 0x01
#define ZRC 0x02
#define SCR 0x04
#define SCA 0x08
#define SCN 0x10
#define STR 0x20
#define STA 0x40
#define SCJ 0x80

#define PPP_EVENTS 16
#define PPP_STATES 10

#define NUM_LCP_CODES 0x10

#define AUTH_FAIL_MSG "Unknown peer-ID or password"

// Modem states
#define MDM_IDLE            0
#define MDM_INITIALISING    1
#define MDM_INITIALISED     2
#define MDM_DIALLING        3
#define MDM_ANSWERING       4
#define MDM_CONNECTED       5
#define MDM_DISCONNECTED    6
#define MDM_FAIL            7

// PPP LCP and IPCP state table, giving action(s) and new state for each state & event
// Taken from RFC1661, with last row (echo request) removed
const BYTE lcp_states[PPP_EVENTS-1][PPP_STATES] =
{
//  0 Initial   1 Starting  2 Closed    3 Stopped   4 Closing   5 Stopping  6 ReqSent   7 AckRcvd   8 AckSent   9 Opened

   {2,          6,          XXX,        XXX,        XXX,        XXX,        XXX,        XXX,        XXX,        XXX         }, // Up
   {XXX,        XXX,        0,          TLS|1,      0,          1,          1,          1,          1,          TLD|1       }, // Down
   {TLS|1,      XXX,        6,          3,          5,          5,          6,          7,          8,          9           }, // Open
   {0,          TLF|0,      2,          2,          4,          4,          4,          4,          4,          TLD|4       }, // Close

   {XXX,        XXX,        XXX,        XXX,        4,          5,          6,          6,          8,          XXX         }, // TO+
   {XXX,        XXX,        XXX,        XXX,        TLF|2,      TLF|3,      TLF|3,      TLF|3,      TLF|3,      XXX         }, // TO-

   {XXX,        XXX,        2,          8,          4,          5,          8,          TLU|9,      8,          TLD|8       }, // RCR+
   {XXX,        XXX,        2,          6,          4,          5,          6,          7,          6,          TLD|8       }, // RCR-
   {XXX,        XXX,        2,          3,          4,          5,          7,          6,          TLU|9,      TLD|6       }, // RCA
   {XXX,        XXX,        2,          3,          4,          5,          6,          6,          8,          TLD|6       }, // RCN

   {XXX,        XXX,        2,          3,          4,          5,          6,          6,          6,          TLD|5       }, // RTR
   {XXX,        XXX,        2,          3,          TLF|2,      TLF|3,      6,          6,          8,          TLD|6       }, // RTA

   {XXX,        XXX,        2,          3,          4,          5,          6,          7,          8,          9           }, // RUC
   {XXX,        XXX,        2,          3,          4,          5,          6,          6,          8,          9           }, // RXJ+
   {XXX,        XXX,        TLF|2,      TLF|3,      TLF|2,      TLF|3,      TLF|3,      TLF|3,      TLF|3,      TLD|5       }  // RXJ-
};
const BYTE lcp_actions[PPP_EVENTS-1][PPP_STATES] =
{
//  0 Initial   1 Starting  2 Closed    3 Stopped   4 Closing   5 Stopping  6 ReqSent   7 AckRcvd   8 AckSent   9 Opened

   {0,          IRC|SCR,    0,          0,          0,          0,          0,          0,          0,          0           }, // Up
   {0,          0,          0,          0,          0,          0,          0,          0,          0,          0           }, // Down
   {0,          0,          IRC|SCR,    0,          0,          0,          0,          0,          0,          0           }, // Open
   {0,          0,          0,          0,          0,          0,          IRC|STR,    IRC|STR,    IRC|STR,    IRC|STR     }, // Close

   {0,          0,          0,          0,          STR,        STR,        SCR,        SCR,        SCR,        0           }, // TO+
   {0,          0,          0,          0,          0,          0,          0,          0,          0,          0           }, // TO-

   {0,          0,          STA,      IRC|SCR|SCA,  0,          0,          SCA,        SCA,        SCA,        SCR|SCA     }, // RCR+
   {0,          0,          STA,      IRC|SCR|SCN,  0,          0,          SCN,        SCN,        SCN,        SCR|SCN     }, // RCR-
   {0,          0,          STA,        STA,        0,          0,          IRC,        SCR,        IRC,        SCR         }, // RCA
   {0,          0,          STA,        STA,        0,          0,          IRC|SCR,    SCR,        IRC|SCR,    SCR         }, // RCN

   {0,          0,          STA,        STA,        STA,        STA,        STA,        STA,        STA,        ZRC|STA     }, // RTR
   {0,          0,          0,          0,          0,          0,          0,          0,          0,          SCR         }, // RTA

   {0,          0,          SCJ,        SCJ,        SCJ,        SCJ,        SCJ,        SCJ,        SCJ,        SCJ         }, // RUC
   {0,          0,          0,          0,          0,          0,          0,          0,          0,          0           }, // RXJ+
   {0,          0,          0,          0,          0,          0,          0,          0,          0,          IRC|STR     }  // RXJ-
};

void save_txbuff(void);
void check_byte(BYTE b);
void poll_net(void);
void lcp_rx_handler(void);
void lcp_event_handler(BYTE event);
void do_lcp_actions(void);
void ipcp_rx_handler(void);
void ipcp_event_handler(BYTE event);
void pap_rx_handler(void);
void pap_event_handler(BYTE event);
void do_ipcp_actions(void);
void send_ppp(BYTE code, BYTE id, BYTE withdata);
void send_ppp_byte(BYTE b);
void transmit(void);
WORD ppp_crc16(WORD crc, BYTE b);

/* Initialise PPP, return 0 if error */
BOOL init_net(void)
{
    SER_CTS_ = 0;
    lcp_event_handler(EVENT_OPEN);
    do_lcp_actions();
#if !INCLUDE_DIAL
    lcp_event_handler(EVENT_UP);
    do_lcp_actions();
#endif
    ipcp_event_handler(EVENT_OPEN);
    return(1);
}

/* Initialise the receive buffer */
void init_rxbuff(void)
{
    atend = rxout = 0;
    rx_checkoff = 0;
}

/* Initialise the transmit buffer */
void init_txbuff(BYTE buffnum)
{
#if NET_TXBUFFERS > 1
    txbuffnum = buffnum;
#endif
    txin = net_txlen = 0;
    checkflag = checkhi = checklo = 0;
}

/* Move the Rx O/P pointer to the given location, return 0 if beyond data */
BOOL setpos_rxout(WORD newpos)
{
    if (newpos > net_rxin)
        return(0);
    rxout = newpos;
    return(1);
}

/* Truncate the remaining Rx data to the given length */
void truncate_rxout(WORD len)
{
    WORD end;

    if ((end = rxout+len) < net_rxin)
        net_rxin = end;
}

/* Move the Rx O/P pointer to the given location, return 0 if beyond data */
BOOL setpos_txin(WORD newpos)
{
    if (newpos > TXBUFFLEN)
        return(0);
    save_txbuff();
    txin = newpos;
    return(1);
}

/* Save the contents of the Tx buffer into the NIC */
void save_txbuff(void)
{
    if (txin > net_txlen)
        net_txlen = txin;
}

/* Get an incoming byte value, return 0 if end of message */
BOOL get_byte(BYTE *bp)
{
    BYTE b;

    if (rxout >= net_rxin)
    {
        atend = 1;
        return(0);
    }
    b = rxbuff[rxout];
    rxout++;
    if (!rx_checkoff)
        check_byte(b);
    *bp = b;
#if DEBUG
    disp_hexbyte(b);
#endif
    return(!atend);
}

/* Send a byte to the network buffer, then add to checksum
** Return 0 if no more data can be accepted */
BOOL put_byte(BYTE b)
{
    if (txin >= TXBUFFLEN)
        return(0);
#if NET_TXBUFFERS > 1
    if (txbuffnum)
        txbuff2[txin++] = b;
    else
        txbuff[txin++] = b;
#else
    txbuff[txin++] = b;
#endif
    check_byte(b);
    return(1);
}

/* Copy data from Rx to Tx buffers, return actual byte count */
WORD copy_rx_tx(WORD maxlen)
{
    WORD count=0;

    count = (WORD)min(maxlen, net_rxin-rxout);
    count = (WORD)min(count, TXBUFFLEN-txin);
#if NET_TXBUFFERS > 1
    if (txbuffnum)
        memcpy(&txbuff2[txin], &rxbuff[rxout], count);
    else
        memcpy(&txbuff[txin], &rxbuff[rxout], count);
#else
    memcpy(&txbuff[txin], &rxbuff[rxout], count);
#endif
    txin += count;
    rxout += count;
    save_txbuff();
    return(count);
}

/* Check for incoming PPP frame, return 0 if none */
BOOL get_net(void)
{
    BYTE ok=0;
    WORD mlen;

    if (timeout(&pppticks, PPP_TIMEOUT))        // If PPP timeout..
    {                                           // If LCP is active..
        if (lcp_state>=PPP_CLOSING && lcp_state<=PPP_ACK_SENT)
        {
            if (lcp_rest)                       // ..and any LCP retries left
            {
                lcp_event_handler(EVENT_TO_OK); // ..signal timeout
                lcp_rest--;
            }
            else                                // ..otherwise final timeout
                lcp_event_handler(EVENT_TO_ERR);
            do_lcp_actions();                   // Process the event
        }                                       // If IPCP is active..
        else if (ipcp_state>=PPP_CLOSING && ipcp_state<=PPP_ACK_SENT)
        {
            if (ipcp_rest)                      // ..and any IPCP retries left
            {
                ipcp_event_handler(EVENT_TO_OK);// ..signal timeout
                ipcp_rest--;
            }
            else                                // ..otherwise final timeout
                ipcp_event_handler(EVENT_TO_ERR);
            do_ipcp_actions();
        }
    }
    poll_net();                                 // If incoming frame..
    if (net_rxin)
    {
        init_rxbuff();                          // Prepare Rx buffer
        rx_checkoff = 1;
        if (skip_byte() && skip_byte() &&       // Skip HDLC addr & ctrl bytes
            get_word(&ppp_pcol))                // Get PPP protocol
        {
            init_txbuff(0);
            if (ppp_pcol == PPP_IP_DATA)        // If IP data, just return
            {                                   // (data left for IP handler)
                return(1);
            }
            setpos_txin(PPP_HEADLEN);           // If LCP, IPCP or PAP..
#if INCLUDE_CLIENT_AUTH || INCLUDE_SERVER_AUTH
            if (ppp_pcol==PPP_LCP || ppp_pcol==PPP_IPCP || ppp_pcol==PPP_PAP)
#else
            if (ppp_pcol==PPP_LCP || ppp_pcol==PPP_IPCP)
#endif
            {                                   // ..get header
                if (get_byte(&ppp_code) && get_byte(&ppp_rxid) &&
                    get_word(&mlen) && mlen>=4 && net_rxin>=mlen+2)
                {                               // ..and call handler
                    if (ppp_pcol == PPP_LCP)
                        lcp_rx_handler();
#if INCLUDE_CLIENT_AUTH || INCLUDE_SERVER_AUTH
                    else if (ppp_pcol == PPP_PAP)
                        pap_rx_handler();
#endif
                    else
                        ipcp_rx_handler();
                }
            }
            else
            {
                put_word(ppp_pcol);             // Reject unknown protocol
                ppp_pcol = PPP_LCP;
                send_ppp(PPP_PCOL_REJ, ppp_rxid, 1);
            }
        }
        net_rxin = 0;
    }                                           // Modem emulation: 'AT' cmd?
    if (mdm_rxin>=2  && rxbuff[0]=='A' && rxbuff[1]=='T')
    {
        if (rxbuff[2] == 'D')
            put_ser("\r\nCONNECT\r\n");         // If dialling, 'connect'
        else if (rxbuff[2] == 'H')
            put_ser("\r\nNO CARRIER\r\n");      // If hangup, 'disconnect'
        else
            ok = 1;
    }
    else if (mdm_rxin>=3 && rxbuff[0]=='+')     // '+++' escape sequence?
        ok = 1;
    else if (mdm_rxin>=5  && rxbuff[0]=='C'&& rxbuff[1]=='L')
        put_ser("CLIENTSERVER");                // Msoft CLIENT/SERVER pcol?
    if (ok)
        put_ser("\r\nOK\r\n");
    mdm_rxin = 0;
    return(0);
}

/* Rx handler for Link Control Protocol */
void lcp_rx_handler(void)
{
    BYTE opt, optlen, code;
    BYTE rejects=0;
    WORD auth=0;
    LWORD lw;

    if (ppp_code == PPP_CFG_REQ)                // LCP Config request?
    {                                           // Check option list
        while (get_byte(&opt) && get_byte(&optlen) && optlen>=2)
        {
            if (opt==LCP_OPT_AUTH && optlen>=4) // Authentication option?
            {                                   // (may be accepted)
                get_word(&auth);
                skip_data(optlen - 4);
            }
            else
            {
                put_byte(opt);                  // Skip other options
                put_byte(optlen);               // (will be rejected)
                copy_rx_tx(optlen - 2);
                rejects++;
            }
        }
        if (rejects)                            // If any rejected options..
        {
            lcp_event_handler(EVENT_RCR_ERR);   // ..inform state machine
            if (lcp_action & SCN)               // If OK to respond..
                send_ppp(PPP_CFG_REJ, ppp_rxid, 1); // ..send rejection
        }
        else if (auth)                          // If authentication request..
        {
            client_auth = auth==PPP_PAP;        // ..only PAP is acceptable
            lcp_event_handler(client_auth ? EVENT_RCR_OK : EVENT_RCR_ERR);
            if (lcp_action & (SCA|SCN))         // If OK to respond..
            {
                put_byte(LCP_OPT_AUTH);         // ..send my response
                put_byte(4);                    // (either ACK PAP..
                put_word(PPP_PAP);              // ..or NAK with PAP hint)
                code = (lcp_action & SCA) ? PPP_CFG_ACK : PPP_CFG_NAK;
                send_ppp(code, ppp_rxid, 1);
            }
        }
        else
        {
            lcp_event_handler(EVENT_RCR_OK);    // Request is all OK
            if (lcp_action & SCA)               // If OK to respond, do so
                send_ppp(PPP_CFG_ACK, ppp_rxid, 0);
        }
    }
    else if (ppp_code == PPP_ECHO_REQ)          // LCP echo request?
    {
        if ((lcp_state&0xf) == PPP_OPENED && get_lword(&lw))
        {                                       // Get magic num
            lw.l++;                             // Return magic num + 1
            put_lword(&lw);
            copy_rx_tx(net_rxin-rxout);         // Echo the data
            send_ppp(PPP_ECHO_REP, ppp_rxid, 1);
        }                                       // Others to state machine..
    }
    else if (ppp_code == PPP_TERM_REQ)          // ..terminate request
        lcp_event_handler(EVENT_RTR);
    else if (ppp_code == PPP_CFG_ACK)           // ..config ACK
        lcp_event_handler(EVENT_RCA);
    else if (ppp_code == PPP_CFG_NAK)           // ..config NAK
        lcp_event_handler(EVENT_RCN);
    else if (ppp_code == PPP_TERM_ACK)          // ..terminate ACK
        lcp_event_handler(EVENT_RTA);
    else if (ppp_code == PPP_CODE_REJ)          // ..code reject
        lcp_event_handler(EVENT_RXJ_ERR);
    do_lcp_actions();
}

/* Handle an LCP event */
void lcp_event_handler(BYTE event)
{
    BYTE newstate, state;

    ppp_pcol = PPP_LCP;
    state = newstate = lcp_state & 0xf;         // Get new state & action
    if (state<=PPP_STOPPED || state==PPP_OPENED)
        lcp_reqs = lcp_terms = 0;
    if (state <= PPP_STOPPED)
        client_auth = server_auth = client_auth_ok = server_auth_ok = 0;
    if (state<=PPP_OPENED && event<=EVENT_RXJ_ERR)
    {
        newstate = lcp_states[event][state];
        lcp_action = lcp_actions[event][state];
        if (lcp_action & SCR)                   // Send config req?
        {
            if (lcp_reqs >= MAX_REQS)           // Error if too many requests
            {
                newstate = lcp_states[EVENT_TO_ERR][state];
                lcp_action = lcp_actions[EVENT_TO_ERR][state];
            }
            lcp_reqs++;
        }
        if (lcp_action & STR)                   // Send terminate req?
        {
            if (lcp_terms >= MAX_TERMS)         // Error if too many requests
            {
                newstate = lcp_states[EVENT_TO_ERR][state];
                lcp_action = lcp_actions[EVENT_TO_ERR][state];
            }
            lcp_terms++;
        }
    }
    if (newstate != XXX)
        lcp_state = newstate;                   // Set new state
}

/* Do the LCP actions (all except config ACK/NAK) */
void do_lcp_actions(void)
{
    ppp_pcol = PPP_LCP;
    if (lcp_action & SCR)                       // Send config request?
    {
        if (server_auth)                        // If incoming authentication
        {
            init_txbuff(0);
            setpos_txin(PPP_HEADLEN);
            put_byte(LCP_OPT_AUTH);             // ..include PAP option in req
            put_byte(4);
            put_word(PPP_PAP);
            send_ppp(PPP_CFG_REQ, ++ppp_txid, 1);
        }
        else                                    // Else no options in request
            send_ppp(PPP_CFG_REQ, ++ppp_txid, 0);
    }
    if (lcp_action & STR)                       // Send termination req?
        send_ppp(PPP_TERM_REQ, ppp_txid, 0);
    if (lcp_action & STA)                       // Send termination ack?
        send_ppp(PPP_TERM_ACK, ppp_rxid, 0);
    if (lcp_action & SCJ)                       // Send code reject?
        send_ppp(PPP_CODE_REJ, ppp_txid, 0);
    if (lcp_action & IRC)                       // Init restart count?
        lcp_rest = lcp_action & STR ? MAX_TERMS : MAX_REQS;
    if (lcp_action & ZRC)                       // Zero restart count?
        lcp_rest = 0;
    if (lcp_state & TLU)                        // This layer up?
        pap_event_handler(EVENT_UP);
    if (lcp_state & TLD)                        // This layer down?
        pap_event_handler(EVENT_DOWN);
}

/* Rx handler for Internet Protocol Control Protocol */
void ipcp_rx_handler(void)
{
    BYTE opt, optlen;
    BYTE rejects=0;

    ppp_pcol = PPP_IPCP;
    if (ppp_code == PPP_CFG_REQ)                // IPCP config request?
    {                                           // Check option list
        while (get_byte(&opt) && get_byte(&optlen) && optlen>=2)
        {
            if (opt==IPCP_OPT_ADDR && optlen==6)// IP address option?
                get_lword(&hostip);             // (will be accepted)
            else
            {
                put_byte(opt);                  // Skip other options
                put_byte(optlen);               // (will be rejected)
                copy_rx_tx(optlen - 2);
                rejects++;
            }
        }
        if (rejects)                            // If any rejected options..
        {
            ipcp_event_handler(EVENT_RCR_ERR);  // ..inform state machine
            if (ipcp_action & SCN)              // If OK to respond..
                send_ppp(PPP_CFG_REJ, ppp_rxid, 1); //..send response
        }
        else if (!hostip.l)                     // If null IP address
        {
            hostip.l = myip.l + 1;              // ..give host my IP addr + 1
            ipcp_event_handler(EVENT_RCR_ERR);  // (for want of anthing else)
            if (ipcp_action & SCN)
            {                                   // If OK to NAK, do so
                put_byte(IPCP_OPT_ADDR);
                put_byte(6);
                put_lword(&hostip);             // ..with new address as hint
                send_ppp(PPP_CFG_NAK, ppp_rxid, 1);
            }
        }
        else                                    // If options OK..
        {
            ipcp_event_handler(EVENT_RCR_OK);
            if (ipcp_action & SCA)              // ..and OK to send, do so
            {
                put_byte(IPCP_OPT_ADDR);
                put_byte(6);
                put_lword(&hostip);
                send_ppp(PPP_CFG_ACK, ppp_rxid, 1);
            }
        }
    }
    else if (ppp_code == PPP_CFG_NAK)           // If NAK received..
    {                                           // ..and IP address hint..
        if (match_byte(IPCP_OPT_ADDR) && match_byte(6))
            get_lword(&myip);                   // ..use it for my address
        ipcp_event_handler(EVENT_RCN);
    }                                           // Others to state machine..
    else if (ppp_code == PPP_TERM_REQ)          // ..terminate Request
        ipcp_event_handler(EVENT_RTR);
    else if (ppp_code == PPP_CFG_ACK)           // ..config ACK
        ipcp_event_handler(EVENT_RCA);
    else if (ppp_code == PPP_TERM_ACK)          // ..terminate ACK
        ipcp_event_handler(EVENT_RTA);
    do_ipcp_actions();
}

/* Handle an IPCP event */
void ipcp_event_handler(BYTE event)
{
    BYTE newstate, state;

    ppp_pcol = PPP_IPCP;
    state = newstate = ipcp_state & 0xf;        // Get new state & action
    if (state<=PPP_STOPPED || state==PPP_OPENED)
        ipcp_reqs = ipcp_terms = 0;
    if (state<=PPP_OPENED && event<=EVENT_RXJ_ERR)
    {
        newstate = lcp_states[event][state];
        ipcp_action = lcp_actions[event][state];
        if (ipcp_action & SCR)                  // Send config req?
        {
            if (ipcp_reqs >= MAX_REQS)          // Error if too many requests
            {
                newstate = lcp_states[EVENT_TO_ERR][state];
                ipcp_action = lcp_actions[EVENT_TO_ERR][state];
            }
            ipcp_reqs++;
        }
        if (ipcp_action & STR)                  // Send terminate req?
        {
            if (ipcp_terms >= MAX_TERMS)        // Error if too many requests
            {
                newstate = lcp_states[EVENT_TO_ERR][state];
                ipcp_action = lcp_actions[EVENT_TO_ERR][state];
            }
            ipcp_terms++;
        }
    }
#if DEBUG_IPCP
    serial_putch(newstate & 0xf);
#endif
    if (newstate != XXX)
    {
        ipcp_state = newstate;                  // Set new state
    }
}

/* Do the IPCP actions (all except config ACK/NAK) */
void do_ipcp_actions(void)
{
    ppp_pcol = PPP_IPCP;
    if (ipcp_action & SCR)                      // Send config request?
    {
        init_txbuff(0);
        setpos_txin(PPP_HEADLEN);
        put_byte(IPCP_OPT_ADDR);
        put_byte(6);
        put_lword(&myip);
        send_ppp(PPP_CFG_REQ, ++ppp_txid, 1);
    }                                           // Send terminate request?
    if (ipcp_action & STR)
        send_ppp(PPP_TERM_REQ, ppp_txid, 0);
    if (ipcp_action & STA)                      // Send terminate ACK?
        send_ppp(PPP_TERM_ACK, ppp_rxid, 0);
    if (ipcp_action & SCJ)                      // Send code reject?
        send_ppp(PPP_CODE_REJ, ppp_txid, 0);
    if (ipcp_action & IRC)                      // Init restart count?
        ipcp_rest = ipcp_action & STR ? MAX_TERMS : MAX_REQS;
    if (ipcp_action & ZRC)                      // Zero restart count?
        ipcp_rest = 0;
}

/* Rx handler for Password Authentication Protocol */
void pap_rx_handler(void)
{
    ppp_pcol = PPP_PAP;
    if (ppp_code == PPP_CFG_REQ)                // Config request?
    {                                           // (incoming authentication)
        if (match_data(serverid, strlen(serverid)))
        {
            send_ppp(PPP_CFG_ACK, ppp_rxid, 0); // ACK if ID and password OK
            pap_event_handler(EVENT_RCR_OK);
        }
        else
        {
            send_ppp(PPP_CFG_NAK, ppp_rxid, 0); // NAK if wrong
            pap_event_handler(EVENT_RCR_ERR);
        }
    }
    else if (ppp_code == PPP_CFG_ACK)           // Config ACK, client auth OK
        pap_event_handler(EVENT_RCA);
    else if (ppp_code == PPP_CFG_NAK)           // Config NAK, client auth fail
        pap_event_handler(EVENT_RCN);
}

/* Handle an PAP event */
void pap_event_handler(BYTE event)
{
    ppp_pcol = PPP_PAP;
    init_txbuff(0);                             // PAP layer up, or timeout?
    if (event == EVENT_UP || event==EVENT_TO_OK)
    {
        if (client_auth)                        // If client authentication..
        {
            setpos_txin(PPP_HEADLEN);           // ..send ID and password
            put_byte(strlen(clientname));
            put_ser(clientname);
            put_byte(strlen(clientpass));
            put_ser(clientpass);
            send_ppp(PPP_CFG_REQ, ppp_txid, 1);
        }
    }
    else if (event == EVENT_RCR_OK)             // Received config req
        server_auth_ok = 1;
    else if (event == EVENT_RCA)                // ..or config ACK
        client_auth_ok = 1;
    if ((!client_auth || client_auth_ok) && (!server_auth || server_auth_ok))
    {
        ipcp_event_handler(EVENT_UP);           // If authentication OK
        do_ipcp_actions();                      // ..start up IPCP
    }
}

/* Put a PPP header in the Tx buffer and transmit the frame */
void send_ppp(BYTE code, BYTE id, BYTE withdata)
{
    if (withdata)
        setpos_txin(0);
    else
        init_txbuff(0);
    put_byte(code);
    put_byte(id);
    if (withdata)
    {
        save_txbuff();
        put_word(net_txlen);
    }
    else
        put_word(4);
    save_txbuff();
    transmit();
}

/* Put out a byte using PPP escape codes, update the CRC */
void send_ppp_byte(BYTE b)
{
    poll_net();
    if (b==PPP_END || b==PPP_ESC || b<0x20)
    {
        serial_putch(PPP_ESC);
        poll_net();
        serial_putch(b ^ 0x20);
    }
    else
        serial_putch(b);
    poll_net();
    txcrc = ppp_crc16(txcrc, b);
}

/* Transmit the PPP frame */
void transmit(void)
{
    WORD n;
    BYTE hi, lo;

    txcrc = 0xffff;
    serial_putch(PPP_END);                  // Start flag
    send_ppp_byte(0xff);                    // HDLC address
    send_ppp_byte(3);                       // ..and control byte
    send_ppp_byte((BYTE)(ppp_pcol >> 8));   // PPP protocol word
    send_ppp_byte((BYTE)ppp_pcol);
#if NET_TXBUFFERS > 1                       // If more than 1 Tx buffer
    txbuff_lens[txbuffnum] = net_txlen;     // ..save Tx length
#endif
    for (n=0; n<net_txlen; n++)             // Transmit PPP data
#if NET_TXBUFFERS > 1
    {
        if (txbuffnum)
            send_ppp_byte(txbuff2[n]);
        else
            send_ppp_byte(txbuff[n]);
    }
#else
        send_ppp_byte(txbuff[n]);
#endif
    hi = ~ (BYTE)(txcrc >> 8);              // Append CRC
    lo = ~ (BYTE)txcrc;
    send_ppp_byte(lo);
    send_ppp_byte(hi);
    serial_putch(PPP_END);                  // End flag
}
#if NET_TXBUFFERS > 1
/* Retransmit the Ethernet frame */
void retransmit(BYTE buffnum)
{
    txbuffnum = buffnum;
    net_txlen = txbuff_lens[txbuffnum];
    transmit();
}
#endif

/* Poll the serial interface for Rx and Tx characters */
void poll_net(void)
{
    BYTE b, saver=1;
    static BYTE lastb=0, pluscount=0;
    static WORD crc;

    if (serial_kbhit())                     // Incoming serial byte?
    {
        b = serial_getch();
        if (lastb == PPP_ESC)               // Last char was Escape?
        {
            lastb = 0;
            b ^= 0x20;
        }
        else if (b == PPP_ESC)              // This char is Escape?
        {
            lastb = PPP_ESC;
            saver = 0;
        }
        else if (b == PPP_END)              // No escape; maybe End?
        {
            if (inframe)                    // If currently in a frame..
            {                               // Check CRC & save length
                if (crc==0xf0b8  && rxbuffin>2)
                    net_rxin = rxbuffin - 2;
                inframe = saver = 0;
            }
            else
                saver = 0;
            rxbuffin = 0;
        }
        if (!inframe)                       // If not in PPP frame..
        {
            if (b == PPP_START)             // ..check for start of frame
            {
                crc = 0xffff;
                rxbuffin = 0;
                inframe = 1;
            }
            else if (b=='\r' || b=='\n')    // ..or control char
            {
                mdm_rxin = rxbuffin;
                saver = rxbuffin = pluscount = 0;
            }                               // ..or 'CONNECT'
            else if (rxbuff[0]== 'C' && rxbuffin==5 && b=='T')
            {
                mdm_rxin = rxbuffin;
                saver = rxbuffin = pluscount = 0;
            }
            else if (b=='+')                // ..or '+++'
            {
                if (++pluscount >= 3)
                {
                    mdm_rxin = rxbuffin;
                    saver = rxbuffin = pluscount = 0;
                }
            }
            else
                pluscount = 0;
        }
        if (saver)                          // If saving the new byte
        {
            if (inframe)
                crc = ppp_crc16(crc, b);    // ..do CRC as well
            if (rxbuffin < RXBUFFLEN)
            {
                rxbuff[rxbuffin] = b;
                rxbuffin++;
            }
        }
    }
}

/* Return new PPP CRC value, given previous CRC value and new byte */
WORD ppp_crc16(WORD crc, BYTE b)
{
    BYTE i;

    for (i=0; i<8; i++)
    {
        if ((crc ^ b) & 1)
            crc = (crc >> 1) ^ 0x8408;
        else
            crc >>= 1;
        b >>= 1;
    }
    return(crc);
}

/* EOF */

