/* UDP functions for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 15/2/00
** v0.02 JPB 3/7/00   Revised header for book CD
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ether.h"
#include "netutil.h"
#include "net.h"
#include "ip.h"
#include "udp.h"

int udpdebug;                   /* Flag to enable TCP packet display */
extern BYTE bcast[MACLEN];

/* Return UDP data length (-1 if no data), 0 if not UDP */
int is_udp(IPKT *ip, int len)
{
    UDPKT *udp;
    WORD sum;
    int dlen=0;
                                        /* Check protocol & minimum length */
    if (ip->i.pcol==PUDP && len>=sizeof(UDPHDR))
    {
        udp = (UDPKT *)ip;              /* Do checksum */
        sum = check_udp(udp, swapl(ip->i.sip), swapl(ip->i.dip), len);
        if (!udp->u.check || sum==0xffff)
        {                               /* If zero or correct.. */
            swap_udp(udp);              /* Do byte-swaps */
            len -= sizeof(UDPHDR);      /* Subtract header len */
            if (udpdebug)               /* Display segment if in debug mode */
                disp_udp(udp, len, 0);
            dlen = len>0 ? len : -1;    /* Return -1 if data len=0 */
        }
        else if (udpdebug)              /* Display error */
            printf("  ERROR: UDP checksum %04X\n", sum);
    }
    return(dlen);
}

/* Make a UDP datagram given the source & destination, data len */
int make_udp(GENFRAME *gfp, NODE *srcep, NODE *destp, WORD dlen)
{
    UDPKT *udp;
    int ulen, ilen;

    udp = getframe_datap(gfp);
    udp->u.sport = srcep->port;          /* Set ports */
    udp->u.dport = destp->port;
    udp->u.len = ulen = dlen + sizeof(UDPHDR);
    udp->u.check = 0;
    if (udpdebug)                       /* Display datagram if in debug mode */
        disp_udp(udp, dlen, 1);
    swap_udp(udp);                      /* Byte-swap */
    ilen = make_ip(gfp, srcep, destp, PUDP, (WORD)(ulen));
    udp->u.check = ~check_udp(udp, udp->i.sip, udp->i.dip, ulen);
    if (udp->u.check == 0)              /* Change sum of 0 to FFFF */
        udp->u.check = 0xffff;
    return(ilen);                       /* Return IP length */
}

/* Return TCP checksum, given UDP (header + data) length.
** The values must be in network byte-order */
WORD check_udp(UDPKT *udp, LWORD sip, LWORD dip, int ulen)
{
    PHDR tph;
    LWORD sum;

    sum = csum(&udp->u, (WORD)ulen);            /* Checksum TCP segment */
    tph.len = swapw((WORD)ulen);                /* Make pseudo-header */
    tph.srce = sip;
    tph.dest = dip;
    tph.z = 0;
    tph.pcol = udp->i.pcol;
    sum += csum(&tph, sizeof(tph));             /* Checksum pseudo-header */
    return((WORD)(sum + (sum >> 16)));          /* Return total plus carry */
}

/* Swap byte order of ints in TCP header */
void swap_udp(UDPKT *udp)
{
    udp->u.sport = swapw(udp->u.sport);
    udp->u.dport = swapw(udp->u.dport);
    udp->u.len = swapw(udp->u.len);
}

/* Return the max TCP seg (data) size for a given frame without fragmentation */
int udp_maxdata(GENFRAME *gfp)
{
    return(maxi(ip_maxdata(gfp)-sizeof(UDPHDR), 0));
}

/* Get the frame driver type, source port, IP and Ethernet addrs */
void getudp_srce(GENFRAME *gfp, NODE *np)
{
    TCPKT *tcp;

    memset(np, 0, sizeof(NODE));        /* Clear unused fields */
    getip_srce(gfp, np);                /* Get dtype, srce IP and Ether addrs */
    tcp = getframe_datap(gfp);
    np->port = tcp->t.sport;            /* Get source port */
}

/* Get the frame driver type, destination port, IP and Ethernet addrs */
void getudp_dest(GENFRAME *gfp, NODE *np)
{
    TCPKT *tcp;

    memset(np, 0, sizeof(NODE));        /* Clear unused fields */
    getip_dest(gfp, np);                /* Get dtype, dest IP and Ether addrs */
    tcp = getframe_datap(gfp);
    np->port = tcp->t.dport;            /* Get dest port */
}

/* Get complete TCP local node data corresponding to frame dest IP address
** Return 0 if no matching node */
int getudp_locdest(GENFRAME *gfp, NODE *np)
{
    UDPKT *udp;
    int ok;

    ok = getip_locdest(gfp, np);        /* Get addresses, dtype & netmask */
    udp = getframe_datap(gfp);          /* Get dest port */
    np->port = udp->u.dport;
    return(ok);
}

/* Display TCP segment */
void disp_udp(UDPKT *udp, int dlen, int tx)
{
    if (tx)
        printf("     /port %u->%u ", udp->u.sport, udp->u.dport);
    else
        printf("     \\port %u<-%u ", udp->u.dport, udp->u.sport);
    printf(" dlen %u\n", dlen);
}

/* Make a TCP segment given the socket, flags, data len */
int make_tftp_req(GENFRAME *gfp, NODE *sp, NODE *dp, WORD op, char *fname,
    char *mode)
{
    TFTP_REQ *tpr;
    WORD len;

    tpr = getframe_datap(gfp);
    tpr->op = swapw(op);
    len = strlen(strcpy(tpr->data, fname)) + 1;
    len += strlen(strcpy(&tpr->data[len], mode)) + 1;
    return(make_udp(gfp, sp, dp, (WORD)(len+2)));
}

/* EOF */

