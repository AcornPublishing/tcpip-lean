/* IP functions for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.02 JPB 29/7/97 Added ESLIP packet type
**                   Fixed UDP packet length calculation
** v0.03 JPB 12/8/97 Added incrementing ICMP sequence number (was random!)
** v0.04 JPB 15/8/97 Improved speed of checksum algorithm
** v0.05 JPB 20/8/97 Removed byte-swapping on ICMP seq
** v0.06 JPB 9/1/98  Added assembly-language insert
** v0.07 JPB 18/1/00 Added receive framentation
** v0.08 JPB 24/1/00 Removed subframe ptrs - subframe is now appended to frame
**                   IP headers with options are now resized
** v0.09 JPB 1/3/00  Fixed 'gate_ip' problem when no gateway specified
** v0.10 JPB 23/3/00 Moved 'atoip' to netutil.c
**                   Added 'get_locnode' function pointer
** v0.11 JPB 29/3/00 Moved 'csum' to NETUTIL.C
** v0.12 JPB 3/7/00  Revised header for book CD
*/

/* Debug option to send IP datagrams with a dummy 'options' field */
#define BIGHEAD 0       /* Set non-zero to send datagrams with larger IP hdrs */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ether.h"
#include "netutil.h"
#include "ip.h"

#define IP_TTL          100     /* Time To Live for an outgoing IP datagram */
#define FRAGTRIES       8       /* Number of attempts to match a fragment */
#define NFRAGS          4       /* No of fragments in buffer */

typedef struct {                /* Fragment buffer structure */
    int tries;                      /* Number of times to attempt a match */
    WORD ident;                     /* IP ident field */
    LWORD sip;                      /* Source IP address */
    WORD oset;                      /* Offset in IP data area */
    WORD len;                       /* Length of fragment */
    BYTE data[MAXIP];               /* Fragment data */
} FRAG;

FRAG frags[NFRAGS];             /* Fragment buffer */

extern int netdebug;            /* Debug flag: net packet display */

/* Upcall function pointer: 0 if unused */
NODE *(*get_locnode_n)(int n);  /* Func ptr for local node locator upcall */

/* Private prototypes */
int defrag_ip(IPKT *ip, int dlen);

/* Check ARP packet, swap bytes, return -1, 0 if not ARP */
int is_arp(GENFRAME *gfp, int len)
{
    WORD pcol;
    ARPKT *arp;
    int dlen=0;

    pcol = getframe_pcol(gfp);          /* ARP only on Ether */
    if (pcol==PCOL_ARP && len>=sizeof(ARPKT))
    {                                   /* If protocol OK.. */
        arp = getframe_datap(gfp);
        swap_arp(gfp);                  /* ..check ARP data */
        if (arp->hrd==HTYPE && arp->pro==ARPPRO)
            dlen = -1;                  /* Return non-zero if OK */
        else
        {
            dlen = 0;                   /* Swap back if not OK */
            swap_arp(gfp);
        }
    }
    return(dlen);
}

/* Make an ARP packet, return its total length */
int make_arp(GENFRAME *gfp, NODE *srcep, NODE *destp, WORD code)
{
    ARPKT *arp;

    gfp->g.fragoff = 0;                 /* No fragmentation */
    arp = (ARPKT *)getframe_datap(gfp);
    memcpy(arp->smac, srcep->mac, MACLEN);  /* Srce ARP ether addr */
    memcpy(arp->dmac, destp->mac, MACLEN);  /* Dest ARP ether addr */
    arp->hrd = HTYPE;                   /* Hware & protocol types */
    arp->pro = ARPPRO;
    arp->hln = MACLEN;                  /* Hardware addr len */
    arp->pln = sizeof(LWORD);           /* IP addr len */
    arp->op  = code;                    /* ARP opcode */
    arp->dip = gate_ip(destp, srcep);   /* Dest ip addr (maybe gateway) */
    arp->sip = srcep->ip;               /* Source IP addr */
    swap_arp(gfp);
    return(make_frame(gfp, destp->mac, PCOL_ARP, sizeof(ARPKT)));
}

/* Swap byte order of ints in ARP header */
void swap_arp(GENFRAME *gfp)
{
    ARPKT *arp;

    arp = getframe_datap(gfp);
    arp->hrd = swapw(arp->hrd);
    arp->pro = swapw(arp->pro);
    arp->op = swapw(arp->op);
    arp->sip = swapl(arp->sip);
    arp->dip = swapl(arp->dip);
}

/* Check frame is IP/SLIP, checksum & byte-swap, return data len */
int is_ip(GENFRAME *gfp, int len)
{
    int ver, dlen=0, hlen;
    WORD pcol, sum;
    IPKT *ip;

    pcol = getframe_pcol(gfp);
    if ((pcol==PCOL_IP || pcol==0) && len>=sizeof(IPHDR))
    {
        ip = getframe_datap(gfp);           /* Get pointer to IP frame */
        ver = ip->i.vhl >> 4;               /* Get IP version & hdr len */
        hlen = (ip->i.vhl & 0xf) << 2;
        sum = ~csum(&ip->i, (WORD)hlen);    /* Do checksum */
        if (ver==4 && len>=hlen && sum==0)  /* If OK.. */
        {
            swap_ip(gfp);                   /* Do byte-swaps */
            dlen = mini(ip->i.len, len) - hlen;
            if (hlen > sizeof(IPHDR))       /* If IP options present.. */
            {                               /* ..delete them, move data down */
                memmove(ip->data, &ip->data[hlen-sizeof(IPHDR)], len);
                dlen -= hlen-sizeof(IPHDR);
            }
            if ((ip->i.frags & 0x3fff)!=0)  /* If a fragment.. */
                dlen = defrag_ip(ip, dlen); /* ..call defragmenter */
        }
        else if (netdebug)
            printf("Invalid datagram, ver %u len %u sum %u\n", ver, len, sum);
    }
    return(dlen);
}

/* Make an IP packet, if greater than the MTU, also make fragment (subframe) in
** this frame. Return total length of frame and subframes (if any) */
int make_ip(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE pcol, WORD dlen)
{
    IPKT *ip, *ip2;
    int len, sublen=0, fhlen, mtu;
    static WORD ident=1, oset=0;
    GENFRAME *sfp;

    ip = getframe_datap(gfp);           /* Get pointer to IP datagram */
    ip->i.ident = ident;                /* Set datagram ident */
    ip->i.frags = oset >> 3;            /* Frag offset in units of 8 bytes */
    gfp->g.fragoff = 0;                 /* ..assuming no more frags */
    mtu = (getframe_mtu(gfp)-sizeof(IPHDR)) & 0xfff8;   /* MTU rounded down 8 */
    len = mini(dlen, mtu);              /* Size of this frame (this fragment) */
    if (dlen > len)                     /* If fragmentation required.. */
    {                                   /* Create new frag within this frame */
        fhlen = dtype_hdrlen(gfp->g.dtype);             /* Frame hdr len */
        gfp->g.fragoff = len + sizeof(IPHDR) + fhlen;   /* Subframe offset */
        sfp = (GENFRAME *)&gfp->buff[gfp->g.fragoff];   /* Subframe ptr */
        ip->i.frags = (oset>>3)+0x2000;                 /* Show there is frag */
        oset += len;                                    /* New data offset */
        ip2 = (IPKT*)((BYTE*)sfp+sizeof(GENHDR)+fhlen); /* Ptr to 2nd IP frag */
        memmove(ip2->data, &ip->data[oset], dlen-len);  /* Copy data 1st->2nd */
        sfp->g.dtype = gfp->g.dtype;    /* Copy driver type into subframe */
        sublen = make_ip(sfp, srcep, destp, pcol, (WORD)(dlen-len));
    }                                   /* Recursive call to make frag */
    ip->i.vhl = 0x40+(sizeof(IPHDR)>>2);/* Version 4, header len 5 LWORDs */
    ip->i.service = 0;                  /* Routine message */
    ip->i.ttl = IP_TTL;                 /* Time To Live */
    ip->i.pcol = pcol;                  /* Set IP protocol */
    ip->i.sip = srcep->ip;              /* Srce, dest IP addrs */
    ip->i.dip = destp->ip;
#if BIGHEAD
    ip->i.option = 0;                   /* Null options if oversized header */
#endif
    ip->i.len = len + sizeof(IPHDR);    /* Data length */
    swap_ip(gfp);                       /* Do byte-swaps (for checksum) */
    ip->i.check = 0;                    /* Clear checksum */
    ip->i.check = ~csum(ip, sizeof(IPHDR)); /* ..then set to calc value */
    ident++;                            /* Increment datagram ident */
    oset = 0;                           /* Clear fragment offset */
    len += sizeof(IPHDR) + sublen;      /* Bump up length */
    return(make_frame(gfp, destp->mac, PCOL_IP, (WORD)len));
}

/* Swap byte order of ints in IP header */
void swap_ip(GENFRAME *gfp)
{
    IPHDR *iph;

    iph = getframe_datap(gfp);
    iph->len = swapw(iph->len);
    iph->ident = swapw(iph->ident);
    iph->frags = swapw(iph->frags);
    iph->sip = swapl(iph->sip);
    iph->dip = swapl(iph->dip);
}

/* Return the maximum IP data size for a given frame without fragmentation */
int ip_maxdata(GENFRAME *gfp)
{
    return(maxi(getframe_mtu(gfp)-sizeof(IPHDR), 0));
}

/* Defragment an incoming IP datagram by matching with existing fragments
** This function handles a maximum of 2 fragments per datagram
** Return total IP data length, 0 if datagram is incomplete */
int defrag_ip(IPKT *ip, int dlen)
{
    int n=0, match=0;
    WORD oset;
    FRAG *fp, *fp2=0;

    oset = (ip->i.frags & 0x1fff) << 3; /* Get offset for imcoming frag */
    while (n<NFRAGS && !match)          /* Search for matching half */
    {
        fp = &frags[n++];
        if (fp->tries)
        {                               /* ..by checking ident */
            if (!(match = (ip->i.ident==fp->ident && ip->i.sip==fp->sip)))
                fp->tries--;            /* If no match, reduce attempts left */
        }
        else
            fp2 = fp;
    }
    if (match)
    {                                   /* Matched: check isn't a duplicate */
        if ((oset+dlen == fp->oset || fp->oset+fp->len == oset) &&
            dlen+fp->len <= MAXGEN)     /* ..and length is OK */
        {
            if (oset)                   /* Move old data as necessary */
                memmove(&ip->data[oset], ip->data, dlen);
            ip->i.len = dlen += fp->len;/* ..and add in new data */
            memcpy(&ip->data[fp->oset], fp->data, fp->len);
            fp->tries = 0;
        }
        else
        {
            if (netdebug)
                printf("Mismatched frag oset %u buff len %u\n", oset, fp->len);
            match = 0;
        }
    }
    else if (fp2)                       /* No match, but there is spare space */
    {
        fp2->tries = FRAGTRIES;         /* Save frag for matching later */
        fp2->ident = ip->i.ident;
        fp2->sip = ip->i.sip;
        fp2->oset = oset;
        fp2->len = dlen;
        memcpy(fp2->data, ip->data, dlen);
    }
    return(match ? dlen : 0);
}

/* Find local node corresponding to given IP addr, return 0 if not found */
NODE *findloc_ip(LWORD locip)
{
    NODE *np=0;
    int n=0;

    while (get_locnode_n && (np=get_locnode_n(n))!=0 && np->ip!=locip)
        n++;
    return(np);
}

/* Get the frame driver type, source IP and Ethernet addresses
** Returned data does not include port number, netmask or gateway addr */
void getip_srce(GENFRAME *gfp, NODE *np)
{
    IPHDR *iph;

    np->dtype = gfp->g.dtype;
    getframe_srce(gfp, np->mac);
    iph = getframe_datap(gfp);
    np->ip = iph->sip;
}

/* Get the frame driver type, destination IP and Ethernet addresses
** Returned data does not include port number, netmask or gateway addr */
void getip_dest(GENFRAME *gfp, NODE *np)
{
    IPHDR *iph;

    np->dtype = gfp->g.dtype;
    getframe_dest(gfp, np->mac);
    iph = getframe_datap(gfp);
    np->ip = iph->dip;
}

/* Get local node data corresponding to a frame destination IP address
** Data does not include port number. Return 0 if no matching local node */
int getip_locdest(GENFRAME *gfp, NODE *np)
{
    IPHDR *iph;
    NODE *locp;
    int ok=0;

    iph = getframe_datap(gfp);
    ok = (locp = findloc_ip(iph->dip)) != 0;
    if (ok)
        *np = *locp;
    return(ok);
}

/* Check a remote address to see if it is on the local subnet.
** If so (or no gateway), return it. If not, return the gateway IP address */
LWORD gate_ip(NODE *remp, NODE *locp)
{
    return((locp->gate==0||on_subnet(remp->ip, locp)) ? remp->ip : locp->gate);
}

/* Check an IP address to see if it is on a subnet, return 0 if not */
int on_subnet(LWORD remip, NODE *locp)
{
    return(((remip ^ locp->ip) & locp->mask) == 0);
}

/* Return ICMP data length (-1 if no data), 0 if not ICMP */
int is_icmp(IPKT *ip, int len)
{
    ICMPKT *icmp;
    WORD sum;
    int dlen=0;

    if (ip->i.pcol==PICMP && len>=sizeof(ICMPHDR))
    {
        icmp = (ICMPKT *)ip;
        if ((sum=csum(&icmp->c, (WORD)len)) == 0xffff)
        {
            swap_icmp(icmp);
            dlen = len>sizeof(ICMPHDR) ? len-sizeof(ICMPHDR) : -1;
        }
        else
            printf("\nICMP checksum error: %04X\n", sum);
    }
    return(dlen);
}

/* Make an ICMP packet */
int make_icmp(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE type, BYTE code,
    WORD dlen)
{
    ICMPKT *icmp;
    WORD len;

    icmp = getframe_datap(gfp);
    icmp->c.type = type;
    icmp->c.code = code;
    icmp->c.check = 0;
    swap_icmp(icmp);
    len = (WORD)(dlen + sizeof(ICMPHDR));
    icmp->c.check = ~csum(&icmp->c, len);
    return(make_ip(gfp, srcep, destp, PICMP, len));
}

/* Make ICMP 'destination unreachable' for incoming frame */
int icmp_unreach(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE code)
{
    int len;
    ICMPKT *icmp;

    icmp = getframe_datap(gfp);
    len = ((icmp->i.vhl & 0xf) << 2) + 8;
    swap_ip(gfp);
    memmove(icmp->data, icmp, len);
    return(make_icmp(gfp, srcep, destp, ICUNREACH, code, (WORD)len));
}

/* Swap byte order of ints in ICMP header */
void swap_icmp(ICMPKT *icmp)
{
    icmp->c.ident = swapw(icmp->c.ident);
    icmp->c.seq = swapw(icmp->c.seq);
}

/* Return the maximum ICMP data size for a given frame without fragmentation */
int icmp_maxdata(GENFRAME *gfp)
{
    return(maxi(ip_maxdata(gfp)-sizeof(ICMPHDR), 0));
}

/* EOF */
