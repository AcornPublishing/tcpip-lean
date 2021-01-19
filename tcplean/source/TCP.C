/* TCP functions for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 27/1/00
** v0.02 JPB 31/1/00  Added timeout
** v0.03 JPB 2/2/00   Added TCP upcall, and active close
** v0.04 JPB 1/3/00   Fixed problem with SLIP node matching
** v0.05 JPB 23/3/00  Added function pointers to upcalls
** v0.06 JPB 11/4/00  Removed GENFRAME argument from close_tcp()
** v0.07 JPB 21/4/00  Fixed TCP problem receiving FIN + data
** v0.08 JPB 5/5/00   Minor cosmetic improvements to TCP code
** v0.09 JPB 23/5/00  Removed timer refresh on all Rx segments
** v0.10 JPB 3/7/00   Revised header for book CD
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ether.h"
#include "netutil.h"
#include "net.h"
#include "ip.h"
#include "tcp.h"

#define TCP_RETRIES 3
#define TCP_TIMEOUT 2000
#define TCP_RXWIN   3000        /* Default Rx window */

int tcpdebug;                   /* Flag to enable TCP packet display */
int statedebug;
extern BYTE bcast[MACLEN];
char *tcflags[] =               /* Text for TCP option flags */
{
    "<???>","<FIN>","<SYN>","<RST>",
    "<PSH>","<ACK>","<URG>","<???>"
};

char *tstates[] = {TSTATE_STRINGS};

/* Function pointers: upcalls to higher-level code (zero if unused) */
int (*server_upcall)(TSOCK *ts, CONN_STATE conn);   /* TCP server action */
int (*client_upcall)(TSOCK *ts, CONN_STATE conn);   /* TCP client action */

/* Open a TCP socket, given local & remote nodes. Return 0 if error */
int open_tcp(TSOCK *ts, GENFRAME *gfp, NODE *locp, NODE *remp)
{
    int ok=0;

    if ((ok = ts->state==TCP_CLOSED)!=0)
    {
        ts->loc = *locp;
        ts->rem = *remp;
        new_state(ts, TCP_AOPEN);
        put_frame(gfp, tsock_rx(ts, gfp, 0));
    }
    return(ok);
}

/* Close a TCP socket. Return non-0 when closed */
int close_tcp(TSOCK *ts)
{
    if (ts->state==TCP_EST || ts->state==TCP_SYNR)
        new_state(ts, TCP_ACLOSE);
    return(ts->state == TCP_CLOSED);
}

/* Reset a TCP socket */
void reset_tcp(TSOCK *ts, GENFRAME *gfp)
{
    if (ts->state)
        put_frame(gfp, make_sock_tcp(gfp, ts, TRST, 0));
}

/* Return TCP data length (excl. options, -1 if no data), 0 if not TCP */
int is_tcp(IPKT *ip, int len)
{
    TCPKT *tcp;
    WORD sum;
    int dlen=0;
                                        /* Check protocol & minimum length */
    if (ip->i.pcol==PTCP && len>=sizeof(TCPHDR))
    {
        tcp = (TCPKT *)ip;              /* Do checksum */
        sum = check_tcp(tcp, swapl(ip->i.sip), swapl(ip->i.dip), len);
        if (tcp->t.hlen < 0x50)
        {
            if (tcpdebug)
                printf("  ERROR: TCP header len %u\n", (tcp->t.hlen&0xf0)>>2);
        }
        if (sum == 0xffff)              /* If correct.. */
        {
            swap_tcp(tcp);              /* Do byte-swaps */
            len -= sizeof(TCPHDR);      /* Subtract header len */
            if (tcpdebug)               /* Display segment if in debug mode */
                disp_tcp(tcp, len, 0);
            len -= gettcp_opt(tcp, 0);  /* Subtract options len */
            dlen = len>0 ? len : -1;    /* Return -1 if data len=0 */
        }
        else if (tcpdebug)              /* Display error */
            printf("  ERROR: TCP checksum %04X\n", sum);
    }
    return(dlen);
}

/* Make a TCP segment given the socket state, flags, data len */
int make_sock_tcp(GENFRAME *gfp, TSOCK *ts, BYTE flags, WORD dlen)
{
    WORD len;
    BYTE st;
    TCPKT *tcp;
    LWORD tseq, tack;

    tcp = getframe_datap(gfp);
    tseq = ts->txb.trial;               /* Seq and ack values if connected */
    tack = ts->rxb.in;
    ts->txflags = flags;
    ts->txdlen = dlen;
    if ((st=ts->state)==TCP_SYNR || st==TCP_SYNS)
        tseq--;                         /* Decrement SEQ if sending SYN */
    else if (st==TCP_CLING || st==TCP_FINWT2 || st==TCP_TWAIT)
        tseq++;                         /* ..or increment if sending FIN */
    if (st==TCP_LASTACK || st==TCP_CLWAIT || st==TCP_CLING || st==TCP_TWAIT)
        tack++;                         /* Increment ACK if FIN received */
    if (dlen > 0)                       /* Get the Tx data */
        dlen = buff_try(&ts->txb, tcp->data, dlen);
    len = make_tcp(gfp, &ts->loc, &ts->rem, flags, tseq, tack, ts->rxwin, dlen);
    return(len);
}

/* Make a TCP segment given the socket, flags, data len */
int make_tcp(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE flags,
    LWORD seq, LWORD ack, WORD window, WORD dlen)
{
    TCPKT *tcp;
    int hlen, tlen, ilen, olen=0;

    tcp = getframe_datap(gfp);
    tcp->t.seq = seq;                   /* Set seq and ack values */
    tcp->t.ack = ack;
    tcp->t.window = window;             /* Window size, srce & dest port nums */
    tcp->t.sport = srcep->port;
    tcp->t.dport = destp->port;
    hlen = sizeof(TCPHDR);              /* TCP header len */
    if (flags&TSYN && dlen==0)          /* Add 4 bytes for options if SYN.. */
    {
        hlen += (olen = 4);
        tcp->data[0] = 2;               /* ..and send max seg size */
        tcp->data[1] = 4;
        *(WORD*)&tcp->data[2] = swapw((WORD)tcp_maxdata(gfp));
    }
    tcp->t.hlen = (BYTE)(hlen<<2);      /* Set TCP header len, and flags */
    tcp->t.flags = flags;
    tcp->t.urgent = tcp->t.check = 0;
    if (tcpdebug)                       /* Display segment if in debug mode */
        disp_tcp(tcp, dlen+olen, 1);
    swap_tcp(tcp);                      /* Do byte-swaps, encapsulate in IP */
    tlen = hlen + dlen;
    ilen = make_ip(gfp, srcep, destp, PTCP, (WORD)(tlen));
    tcp->t.check = ~check_tcp(tcp, tcp->i.sip, tcp->i.dip, tlen);
    return(ilen);                       /* Checksum final packet */
}

/* Make a TCP RESET response to incoming segment */
int make_reset_resp(GENFRAME *gfp, int rdlen)
{
    TCPKT *tcp;
    NODE loc, rem;
    LWORD ack;

    gettcp_srce(gfp, &rem);             /* Get source & dest nodes */
    gettcp_locdest(gfp, &loc);          /* (including port numbers) */
    tcp = getframe_datap(gfp);
    ack = tcp->t.seq + maxi(rdlen, 0);
    if (tcp->t.flags & (TSYN+TFIN))
        ack++;
    return(make_tcp(gfp, &loc, &rem, TRST+TACK, tcp->t.ack, ack, 0, 0));
}

/* Return TCP checksum, given segment (TCP header + data) length.
** The TCP segment and both IP addrs must be in network byte-order */
WORD check_tcp(TCPKT *tcp, LWORD sip, LWORD dip, int tlen)
{
    PHDR tph;
    LWORD sum;

    sum = csum(&tcp->t, (WORD)tlen);            /* Checksum TCP segment */
    tph.len = swapw((WORD)tlen);                /* Make pseudo-header */
    tph.srce = sip;
    tph.dest = dip;
    tph.z = 0;
    tph.pcol = tcp->i.pcol;
    sum += csum(&tph, sizeof(tph));             /* Checksum pseudo-header */
    return(WORD)(sum + (sum>>16));              /* Return total plus carry */
}

/* Swap byte order of ints in TCP header */
void swap_tcp(TCPKT *tcp)
{
    tcp->t.sport = swapw(tcp->t.sport);
    tcp->t.dport = swapw(tcp->t.dport);
    tcp->t.window = swapw(tcp->t.window);
    tcp->t.urgent = swapw(tcp->t.urgent);
    tcp->t.seq = swapl(tcp->t.seq);
    tcp->t.ack = swapl(tcp->t.ack);
}

/* Return the max TCP seg (data) size for a given frame without fragmentation */
int tcp_maxdata(GENFRAME *gfp)
{
    return(maxi(ip_maxdata(gfp)-sizeof(TCPHDR), 0));
}

/* Get the frame driver type, source port, IP and Ethernet addrs */
void gettcp_srce(GENFRAME *gfp, NODE *np)
{
    TCPKT *tcp;

    memset(np, 0, sizeof(NODE));        /* Clear unused fields */
    getip_srce(gfp, np);                /* Get dtype, srce IP and Ether addrs */
    tcp = getframe_datap(gfp);
    np->port = tcp->t.sport;            /* Get source port */
}

/* Get the frame driver type, destination port, IP and Ethernet addrs */
void gettcp_dest(GENFRAME *gfp, NODE *np)
{
    TCPKT *tcp;

    memset(np, 0, sizeof(NODE));        /* Clear unused fields */
    getip_dest(gfp, np);                /* Get dtype, dest IP and Ether addrs */
    tcp = getframe_datap(gfp);
    np->port = tcp->t.dport;            /* Get dest port */
}

/* Get complete TCP local node data corresponding to frame dest IP address
** Return 0 if no matching node */
int gettcp_locdest(GENFRAME *gfp, NODE *np)
{
    TCPKT *tcp;
    int ok;

    ok = getip_locdest(gfp, np);        /* Get addresses, dtype & netmask */
    tcp = getframe_datap(gfp);          /* Get dest port */
    np->port = tcp->t.dport;
    return(ok);
}

/* Return TCP options field length; if Max Seg Size option, get value */
WORD gettcp_opt(TCPKT *tcp, WORD *mssp)
{
    int olen;

    olen = ((tcp->t.hlen & 0xf0) >> 2) - sizeof(TCPHDR);
    if (mssp && olen>=4 && tcp->data[0]==2 && tcp->data[1]==4)
        *mssp = swapw(*(WORD *)&tcp->data[2]);
    return(olen);
}

/* Display TCP segment */
void disp_tcp(TCPKT *tcp, int dlen, int tx)
{
    int i, msk;
    WORD olen, mss=0;

    if (tx)
    {
        printf("     /ack %08lx seq %08lx", tcp->t.ack, tcp->t.seq);
        printf(" port %u->%u ", tcp->t.sport, tcp->t.dport);
    }
    else
    {
        printf("     \\seq %08lx ack %08lx", tcp->t.seq, tcp->t.ack);
        printf(" port %u<-%u ", tcp->t.dport, tcp->t.sport);
    }
    for (i=msk=1; i<8; i++,msk<<=1)
        printf("%s", tcp->t.flags&msk ? tcflags[i] : "");
    olen = gettcp_opt(tcp, &mss);
    if (olen>0 && mss>0)
        printf(" MSS %u", mss);
    printf(" dlen %Xh\n", dlen-olen);
}

/* Find socket(s) for incoming ARP response */
void arp_receive(TSOCK tss[], int nsocks, GENFRAME *gfp)
{
    ARPKT *arp;
    TSOCK *ts;
    int n, txlen;

    arp = getframe_datap(gfp);
    for (n=0; n<nsocks; n++)            /* Try matching to socket */
    {
        ts = &tss[n];
        if (ts->state==TCP_ARPS && arp->sip==ts->rem.ip)
        {                               /* If matched, change state */
            memcpy(ts->rem.mac, arp->smac, MACLEN); /* ..copy Ethernet addr */
            new_state(ts, TCP_ARPR);    /* Send SYN */
            if ((txlen = tsock_rx(ts, gfp, 0))>0)
                put_frame(gfp, txlen);
        }
    }
}

/* Find socket for incoming segment; send TCP RESET if none found
** Receive length is non-zero if segment received (-1 if segment has no data) */
void tcp_receive(TSOCK tss[], int nsocks, GENFRAME *gfp, int dlen)
{
    int n, ok=0, txlen=0;
    TSOCK *ts;
    NODE loc, rem;

    if (gettcp_locdest(gfp, &loc))      /* Get local node */
    {
        gettcp_srce(gfp, &rem);         /* Get remote node */
        for (n=0; n<nsocks && !ok; n++) /* Try matching to existing socket */
        {
            ts = &tss[n];
            ok = loc.ip==ts->loc.ip && loc.port==ts->loc.port &&
                 rem.ip==ts->rem.ip && rem.port==ts->rem.port;
        }
        for (n=0; n<nsocks && !ok; n++) /* If not, pick the first idle socket */
        {
            ts = &tss[n];
            if ((ok = ts->state==TCP_CLOSED)!=0)
            {
                ts->loc = loc;
                ts->rem = rem;
            }
        }
        if (ok)                         /* If found, socket gets the segment */
            txlen = tsock_rx(ts, gfp, dlen);
    }
    if (!ok)                            /* If not, send RESET */
    {
        if (statedebug)
            printf("    (?) sending reset\n");
        txlen = make_reset_resp(gfp, dlen);
    }
    if (txlen > 0)
        put_frame(gfp, txlen);
}

/* Poll all the sockets, checking for timeouts or Tx data to be sent */
void tcp_poll(TSOCK tss[], int nsocks, GENFRAME *gfp)
{
    TSOCK *ts;
    int n, txlen;

    for (n=0; n<nsocks; n++)
    {
        ts = &tss[n];
        txlen = 0;
        if (ts->state)
        {
            if (ts->state && ts->timeout && mstimeout(&ts->time, ts->timeout))
            {
                if (tcpdebug)
                    printf("    (%u) Timeout\n", ts->index);
                ts->timeout += ts->timeout;
                if (ts->retries-- >= 0)
                    txlen = remake_tsock(ts, gfp);
                else
                {
                    new_state(ts, TCP_CLOSED);
                    if (ts->state != TCP_ARPS)
                        txlen = make_sock_tcp(gfp, ts, TRST, 0);
                }
            }
            else if (ts->state)                 /* Send Tx data */
                txlen = tsock_rx(ts, gfp, 0);
        }
        if (txlen > 0)
            put_frame(gfp, txlen);
    }
}

/* Change TCP socket state; if closed/opening/closing/established, do upcall
** If upcall returns error, don't change state, and return 0 */
void new_state(TSOCK *ts, int news)
{
    if (news != ts->state)
    {
        if (statedebug)
            printf("    (%u) new state '%s'\n", ts->index, tstates[news]);
        ts->state = news;
    }
    mstimeout(&ts->time, 0);
    ts->retries = TCP_RETRIES;
    ts->timeout = TCP_TIMEOUT;
}

/* Receive an incoming TCP seg into a socket, given data length (-1 if no data)
** Also called with length=0 to send out Tx data when connected.
** Returns transmit length, 0 if nothing to transmit */
int tsock_rx(TSOCK *ts, GENFRAME *gfp, int dlen)
{
    BYTE rflags=0;
    TCPKT *tcp;
    int txlen=0, tx=0;
    static WORD eport=MINEPORT;
    int (*upcall)(TSOCK *ts, CONN_STATE conn);

    tcp = getframe_datap(gfp);
    upcall = ts->server ? server_upcall : client_upcall;
    if (dlen>0 || dlen==DLEN_NODATA)
    {
        rflags = tcp->t.flags & (TFIN+TSYN+TRST+TACK);
        ts->rxseq = tcp->t.seq;
        ts->rxack = tcp->t.ack;
        ts->txwin = tcp->t.window;
        if (rflags & TRST)
            new_state(ts, TCP_RSTR);
        else if (rflags&TACK &&
                 in_limits(ts->rxack, ts->txb.out, ts->txb.trial))
            ts->txb.out = ts->rxack;
    }
    switch (ts->state)
    {
    /* Passive (remote) open a connection */
    case TCP_CLOSED:
        if (rflags == TSYN)                     /* If closed & SYN received.. */
        {
            buff_setall(&ts->rxb, ++ts->rxseq); /* Load my ACK value */
            buff_setall(&ts->txb, mstime()*0x100L); /* ..and my SEQ */
            ts->rxwin = TCP_RXWIN;              /* Default Rx window */
            ts->connflags = 0;                  /* No special flags */
            if (server_upcall && !server_upcall(ts, TCP_OPEN)) /* Upcall err? */
                txlen = make_reset_resp(gfp, dlen); /* ..don't accept SYN */
            else
            {
                new_state(ts, TCP_SYNR);        /* If OK, send SYN+ACK */
                txlen = make_sock_tcp(gfp, ts, TSYN+TACK, 0);
                ts->server = 1;                 /* Identify as server socket */
            }
        }
        else                                    /* If not SYN.. */
            txlen = make_reset_resp(gfp, dlen); /* ..send reset */
        break;

    case TCP_SYNR:                              /* SYN+ACK sent, if ACK Rx.. */
        tsock_estab_rx(ts, gfp, dlen);          /* Fetch Rx data */
        if (rflags==TACK && ts->rxseq==ts->rxb.in && ts->rxack==ts->txb.out)
        {
            if (upcall && !upcall(ts, TCP_CONN))/* If upcall not OK.. */
                new_state(ts, TCP_ACLOSE);      /* ..close after sending data */
            else                                /* If OK.. */
                new_state(ts, TCP_EST);         /* ..go established */
        }
        else if (dlen && ts->rxseq==ts->rxb.in-1)   /* If repeat SYN.. */
            txlen = make_sock_tcp(gfp, ts, TSYN+TACK, 0);
        break;                                      /* ..repeat SYN+ACK */

    /* Connection established */
    case TCP_EST:
        if (rflags)                                 /* Refresh timer if Rx */
            new_state(ts, ts->state);
        if (rflags&TFIN && ts->rxseq==ts->rxb.in)   /* If remote close.. */
        {
            tx = tsock_estab_rx(ts, gfp, dlen);     /* Fetch Rx data */
            if (!upcall || upcall(ts, TCP_CLOSE))
            {
                new_state(ts, TCP_LASTACK);         /* ..FIN+ACK if OK */
                txlen = make_sock_tcp(gfp, ts, TFIN+TACK, 0);
            }
            else
            {
                new_state(ts, TCP_CLWAIT);          /* ..or send data if not */
                txlen = tsock_estab_tx(ts, gfp, 1);
            }
        }
        else
        {
            tx = tsock_estab_rx(ts, gfp, dlen);     /* Fetch Rx data */
            if (upcall && !upcall(ts, dlen>0 ? TCP_DATA : TCP_NODATA))
            {
                new_state(ts, TCP_ACLOSE);      /* If upcall 0, start close */
                tx = 1;
            }
            else if (dlen > 0)                  /* If Rx data, send ack */
                tx = 1;
            txlen = tsock_estab_tx(ts, gfp, tx);/* Send packet, maybe Tx data */
        }
        break;

    /* Passive (remote) close a connection */
    case TCP_CLWAIT:                            /* Do upcall to application */
        if (!upcall || upcall(ts, TCP_CLOSE))   /* If OK, send FIN */
        {
            new_state(ts, TCP_LASTACK);
            txlen = make_sock_tcp(gfp, ts, TFIN+TACK, 0);
        }
        else                                    /* If not, keep open */
            txlen = tsock_estab_tx(ts, gfp, 0);
        break;

    case TCP_LASTACK:                           /* If ACK of my FIN.. */
        if (rflags==TACK && ts->rxseq==ts->rxb.in+1)
            new_state(ts, TCP_CLOSED);          /* ..connection closed */
        break;

    /* Active (local) open a connection */
    case TCP_AOPEN:
        ts->server = 0;                         /* Identify as client socket */
        if (gfp->g.dtype & DTYPE_SLIP)
            new_state(ts, TCP_ARPR);            /* If SLIP, don't ARP */
        else
        {                                       /* If Ether, do ARP */
            memcpy(ts->rem.mac, bcast, MACLEN); /* Set broadcast addr */
            new_state(ts, TCP_ARPS);            /* Send ARP */
            txlen = make_arp(gfp, &ts->loc, &ts->rem, ARPREQ);
        }
        break;

    case TCP_ARPS:                              /* Idle until ARP response */
        break;                                  /* (see ARP_receive function) */

    case TCP_ARPR:
        buff_setall(&ts->txb, mstime()*100L);   /* ARPed: set my SEQ value */
        ts->rxwin = TCP_RXWIN;                  /* Default window size */
        ts->connflags = 0;                      /* No special flags */
        ts->loc.port = ++eport>=MAXEPORT ? MINEPORT : eport;/* New port num */
        if (upcall)                             /* Do upcall */
            upcall(ts, TCP_OPEN);
        new_state(ts, TCP_SYNS);
        txlen = make_sock_tcp(gfp, ts, TSYN, 0);
        break;

    case TCP_SYNS:                              /* Sent SYN, if SYN+ACK Rx */
        if (rflags==TSYN+TACK && ts->rxack==ts->txb.out)
        {
            buff_setall(&ts->rxb, ts->rxseq+1); /* Set my ACK value */
            if (upcall)                         /* Do upcall */
                upcall(ts, TCP_CONN);
            new_state(ts, TCP_EST);             /* ..send ACK, go established */
            txlen = make_sock_tcp(gfp, ts, TACK, 0);
        }
        else if (rflags)                        /* If anything else.. */
            txlen = make_reset_resp(gfp, dlen); /* ..send reset */
        break;

    /* Active (local) close a connection */
    case TCP_ACLOSE:
        tsock_estab_rx(ts, gfp, dlen);          /* Fetch Rx data */
        if (buff_dlen(&ts->txb))                /* If any Tx data left.. */
            txlen = tsock_estab_tx(ts, gfp, 0); /* If unsent, send it */
        else                                    /* All data sent: close conn */
        {
            if (upcall)                         /* Do upcall */
                upcall(ts, TCP_CLOSE);
            new_state(ts, TCP_FINWT1);
            txlen = make_sock_tcp(gfp, ts, TFIN+TACK, 0);
        }
        break;

    case TCP_FINWT1:
        tsock_estab_rx(ts, gfp, dlen);          /* Fetch Rx data */
        if (rflags&TFIN && ts->rxseq==ts->rxb.in)
        {
            if (rflags&TACK && ts->rxack==ts->txb.trial+1)
                new_state(ts, TCP_TWAIT);       /* If ACK+FIN, close */
            else if (!(rflags&TACK) || ts->rxack==ts->txb.trial)
                new_state(ts, TCP_CLING);       /* If FIN, wait for ACK */
            txlen = make_sock_tcp(gfp, ts, TACK, 0);
        }
        else if (rflags&TACK && ts->rxack==ts->txb.trial+1)
            new_state(ts, TCP_FINWT2);          /* If just ACK, half-close */
        break;

    case TCP_FINWT2:                            /* Half-closed: awaiting FIN */
        tsock_estab_rx(ts, gfp, dlen);          /* Fetch Rx data */
        if (rflags&TFIN && ts->rxseq==ts->rxb.in)
        {
            new_state(ts, TCP_TWAIT);           /* Got FIN, close */
            txlen = make_sock_tcp(gfp, ts, TACK, 0);
        }
        break;

    case TCP_CLING:                             /* Closing: need final ACK */
        if (rflags==TACK && ts->rxseq==ts->rxb.in+1)
            new_state(ts, TCP_CLOSED);
        break;

    case TCP_TWAIT:                             /* Timed wait: just close! */
    default:
        new_state(ts, TCP_CLOSED);
        break;
    }
    return(txlen);
}

/* Put Rx data into an established socket, return non-zero if Tx ACK required */
int tsock_estab_rx(TSOCK *ts, GENFRAME *gfp, int dlen)
{
    TCPKT *tcp;
    int oset=0, oldlen, tx=0;
    WORD rdlen=0;
    long rxdiff;

    if (dlen > 0)                               /* If any data received.. */
    {
        tcp = getframe_datap(gfp);
        oset = gettcp_opt(tcp, 0);
        rxdiff = ts->rxseq - ts->rxb.in;        /* Find posn w.r.t. last data */
        if (rxdiff == 0)                        /* If next block, accept it */
            rdlen = (WORD)dlen;
        else if (rxdiff < 0)                    /* If part or all is a repeat */
        {
            oldlen = -(int)rxdiff;              /* ..read in new part */
            if (oldlen<=ts->rxwin && dlen>oldlen)
            {
                rdlen = dlen - oldlen;
                oset += (int)oldlen;
            }
        }
        if (rdlen)                               /* Read the data in */
        {
            if (statedebug)
                printf("    (%u) Rx data %u bytes\n", ts->index, rdlen);
            buff_in(&ts->rxb, &tcp->data[oset], rdlen);
        }                                        /* Tx if repeat or half full */
        tx = tcp->t.flags&TPUSH || rxdiff<0 || buff_dlen(&ts->rxb)>ts->rxwin/2;
    }
    return(tx);
}

/* Prepare Tx frame containing outgoing data for connection, return frame len
** If 'force' is non-zero, make frame even if there is no data */
int tsock_estab_tx(TSOCK *ts, GENFRAME *gfp, int force)
{
    int tdlen, txlen=0;

    tdlen = mini(buff_untriedlen(&ts->txb), tcp_maxdata(gfp));
    tdlen = mini(tdlen, ts->txwin-buff_trylen(&ts->txb));
    if (tdlen>0 && !force)
        force = buff_trylen(&ts->txb)==0 || tdlen<tcp_maxdata(gfp)/2;
    if (force)
    {
        if (tdlen>0 && statedebug)
            printf("    (%u) Tx data %u bytes\n", ts->index, tdlen);
        txlen = make_sock_tcp(gfp, ts, (BYTE)(TACK+ts->connflags), (WORD)tdlen);
    }
    return(txlen);
}

/* Remake the last transmission (TCP or ARP), return frame length */
int remake_tsock(TSOCK *ts, GENFRAME *gfp)
{
    int txlen=0;

    if (ts->state == TCP_ARPS)
        txlen = make_arp(gfp, &ts->loc, &ts->rem, ARPREQ);
    else if (ts->state==TCP_EST||ts->state==TCP_CLWAIT||ts->state==TCP_ACLOSE)
    {
        ts->txb.trial = ts->txb.out;
        txlen = tsock_estab_tx(ts, gfp, 1);
    }
    else if (ts->state)
        txlen = make_sock_tcp(gfp, ts, ts->txflags, 0);
    return(txlen);
}

/* EOF */
