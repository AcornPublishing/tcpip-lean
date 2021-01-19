/* Router utility for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 21/1/00
** v0.02 JPB 25/1/00  Added IP routing
** v0.03 JPB 26/1/00  Added framestore
** v0.04 JPB 27/1/00  Added SLIP support
** v0.05 JPB 24/2/00 Changed keyscan to ESC or ctrl-C
** v0.06 JPB 23/3/00  Used upcall pointers in TCP.C
** v0.07 JPB 3/7/00   Revised header for book CD
*/

#define VERSION "0.07"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <signal.h>

#include "ether.h"
#include "netutil.h"
#include "net.h"
#include "ip.h"

#define CFGFILE     "router.cfg"    /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */
#define MAXREMS     8               /* Size of remote node address cache */

/* The 'frame buffers' store IP datagrams which can't be sent because the
** destination hasn't been ARPed yet. */
#define NFRAMES     4               /* Number of network frame buffers */
GENFRAME framestore[NFRAMES];       /* Frames for network Tx/Rx */
int frameidx;                       /* Index number of currently-used frame */

/* The 'local nodes' have the IP addresses etc. of each network interface */
NODE locnodes[MAXNETS];             /* My local node addrs for each net */
int nnets;                          /* Number of nets in use */

/* The 'remote node' store is an ARP cache, retaining recently accessed nodes */
NODE remnodes[MAXREMS];             /* Cache of remote node addresses */
int remidx;                         /* Index number of latest entry */

char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast MAC (Ethernet) addr */
extern int netdebug;                /* Flag to enable net packet display */
int breakflag;                      /* Flag to indicate ctrl-break pressed */

extern NODE *(*get_locnode_n)(int n);   /* Upcall to get local node */

/* Private prototypes */
WORD read_netconfig_n(char *fname, int n, NODE *np);
void do_transmit(GENFRAME *gfp);
void do_receive(void);
void do_arp(GENFRAME *gfp, NODE *locp);
void do_icmp(GENFRAME *gfp, int rxlen, NODE *locp);
void do_poll(void);
NODE *find_subnet(NODE *destp);
void add_cache(NODE *np);
int lookup_cache(NODE *np);
void disp_usage(void);
NODE *locnode_n(int n);
void break_handler(int sig);

int main(int argc, char *argv[])
{
    int n, args=0, err=0;
    char *p, temps[20];
    WORD dt;
    NODE *np;

    printf("ROUTER v" VERSION "\n");            /* Sign on */
    get_locnode_n = locnode_n;                  /* Set upcall func ptr */
    signal(SIGINT, break_handler);              /* Trap ctrl-C */
    while (argc > ++args)                       /* Process command-line args */
    {
        if (argv[args][0]=='-')
        {
            switch (toupper(argv[args][1]))
            {
            case 'C':                           /* -C: config filename */
                strncpy(cfgfile, argv[++args], MAXPATH);
                if ((p=strrchr(cfgfile, '.'))==0 || !isalpha(*(p+1)))
                    strcat(cfgfile, CFGEXT);
                break;
            case 'V':                           /* -V: verbose (debug) mode */
                netdebug = 1;
                break;
            default:
                err = 1;
            }
        }
    }
    if (err)                                    /* Erorr: display usage */
    {
        disp_usage();
        exit(1);
    }
    while (nnets<MAXNETS &&                     /* Load net drivers */
           (dt=read_netconfig_n(cfgfile, nnets, &locnodes[nnets]))!=0)
    {
        np = &locnodes[nnets++];                /* Print net addresses etc. */
        np->dtype = dt;
        printf ("Net %u %-15s ", nnets, ipstr(np->ip, temps));
        printf ("mask %-15s ", ipstr(np->mask, temps));
        if (dt & DTYPE_ETHER)
            printf("Ethernet %s\n", ethstr(np->mac, temps));
        else
            printf("SLIP\n");
    }
    if (nnets < 2)
        printf("Insufficient networks for router\n");
    else
    {
        printf("Press ESC or ctrl-C to exit\n");
        while (!breakflag)                      /* Main loop.. */
        {
            do_receive();                       /* ..handle Rx packets */
            do_poll();                          /* ..keep net drivers alive */
            if (kbhit())                        /* If key hit, check for ESC */
                breakflag = getch()==0x1b;
        }
        for (n=0; n<nnets; n++)
            close_net(locnodes[n].dtype);       /* Shut down net drivers */
    }
    return(0);
}

/* Read nth network config item (n=0 for first) to get IP addresses and netmasks
** Return driver type, 0 if error */
WORD read_netconfig_n(char *fname, int n, NODE *np)
{
    char temps[31];
    WORD dtype=0;
    BYTE b;

    if (read_cfgstr_n(fname, n, "net", netcfg, MAXNETCFG))
    {
        if (!read_cfgstr_n(fname, n, "ip", temps, 30) ||
                 (np->ip=atoip(temps))==0)          /* Get IP address */
            printf("No IP address for net %u\n", n);
        else if (!(dtype = open_net(netcfg)))       /* Open net driver */
            printf("Can't open net %u driver '%s'\n", n, netcfg);
        else
        {
            memcpy(np->mac, ether_addr(dtype), MACLEN);
            b = (BYTE)(np->ip >> 24);
            if (read_cfgstr_n(fname, n, "mask", temps, 30))
                np->mask = atoip(temps);            /* Get netmask */
            else
                np->mask = b<128 ? 0xff000000L: b<192 ? 0xffff0000L:0xffffff00L;
            if (read_cfgstr_n(fname, n, "gate", temps, 30))
                np->gate = atoip(temps);            /* Get gateway IP addr */
            else
                np->gate = 0;
        }
    }
    return(dtype);
}

/* Check for incoming packets, send response if required */
void do_receive(void)
{
    int txlen;
    WORD rxlen;
    NODE *locp, srce, dest;
    IPKT *ip, *ip2;
    GENFRAME *gfp;

    gfp = &framestore[frameidx];                    /* Ptr to current frame */
    gfp->g.dtype = 0;                               /* Has any net driver.. */
    if ((rxlen=get_frame(gfp)) > 0)                 /* ..got incoming frame? */
    {
        locp = &locnodes[gfp->g.dtype&NETNUM_MASK]; /* Get my local address */
        if (is_arp(gfp, rxlen))                     /* If ARP.. */
        {
            do_arp(gfp, locp);                      /* ..handle it */
        }
        else if ((rxlen=is_ip(gfp, rxlen))!=0 &&    /* If IP datagram.. */
                 !is_bcast(gfp))                    /* ..but not broadcast.. */
        {
            ip = getframe_datap(gfp);               /* If ICMP  pkt to me.. */
            if (ip->i.dip==locp->ip && (rxlen=is_icmp(ip, rxlen))!=0)
            {
                do_icmp(gfp, rxlen, locp);          /* Send ICMP response */
            }
            else                                    /* Datagram needs routing */
            {
                getip_srce(gfp, &srce);             /* Get source addr */
                add_cache(&srce);                   /* ..and add to cache */
                getip_dest(gfp, &dest);             /* Get dest addr */
                gfp->g.len = gfp->g.fragoff = 0;
                if ((locp=find_subnet(&dest))!=0)   /* Find correct subnet.. */
                {
                    gfp->g.dtype = locp->dtype;     /* ..and its driver type */
                    ip2 = getframe_datap(gfp);      /* Get datagram posn */
                    if (ip != ip2)                  /* If new posn in frame.. */
                        memmove(ip2, ip, rxlen+sizeof(IPHDR));  /* ..move it */
                    ip = ip2;
                    if (locp->dtype & DTYPE_SLIP || /* If SLIP.. */
                        lookup_cache(&dest))        /* ..or dest in cache */
                    {                               /* Make IP datagram */
                        txlen = make_ip(gfp, &srce, &dest, ip->i.pcol, rxlen);
                        put_frame(gfp, txlen);      /* ..and send it */
                    }
                    else
                    {
                        gfp->g.len = rxlen;         /* Save current frame */
                        frameidx = (frameidx+1) % NFRAMES;
                        gfp = &framestore[frameidx];
                        gfp->g.dtype = locp->dtype; /* Broadcast ARP request */
                        memcpy(dest.mac, bcast, MACLEN);
                        txlen = make_arp(gfp, locp, &dest, ARPREQ);
                        put_frame(gfp, txlen);      /* Send packet */
                    }
                }
            }
        }
    }
}

/* Handle an incoming ARP packet, given local node address */
void do_arp(GENFRAME *gfp, NODE *locp)
{
    ARPKT *arp;
    NODE rem, srce;
    IPKT *ip;
    int txlen, i, n;

    arp = getframe_datap(gfp);
    if (arp->dip == locp->ip)               /* ARP intended for me? */
    {
        rem.ip = arp->sip;                  /* Get remote (source) addrs */
        memcpy(rem.mac, arp->smac, MACLEN);
        rem.dtype = gfp->g.dtype;           /* ..and driver type */
        if (arp->op == ARPREQ)              /* If ARP request.. */
        {                                   /* ..make ARP response */
            txlen = make_arp(gfp, locp, &rem, ARPRESP);
            put_frame(gfp, txlen);          /* ..send packet */
        }
        else if (arp->op == ARPRESP)        /* If ARP response.. */
        {
            add_cache(&rem);                /* ..add to cache */
            for (n=0, i=frameidx; n<NFRAMES-1; n++)
            {                               /* Search framestore */
                gfp = &framestore[i=(i+1)%NFRAMES];
                ip = getframe_datap(gfp);
                if (rem.ip == ip->i.dip)    /* If matching IP addr.. */
                {
                    gfp->g.dtype = rem.dtype;       /* ..make datagram.. */
                    srce = locnodes[rem.dtype & NETNUM_MASK];
                    srce.ip = ip->i.sip;
                    txlen = make_ip(gfp, &srce, &rem, ip->i.pcol, gfp->g.len);
                    put_frame(gfp, txlen);          /* ..and send it */
                    ip->i.dip = 0;
                }
            }
        }
    }
}

/* Handle an incoming ICMP packet, given length and local node address */
void do_icmp(GENFRAME *gfp, int rxlen, NODE *locp)
{
    ICMPKT *icmp;
    NODE rem;
    int len, txlen;

    icmp = getframe_datap(gfp);         /* Get pointer to packet */
    getip_srce(gfp, &rem);              /* Get remote addrs */
    if (icmp->c.type==ICREQ)            /* Echo request? */
    {
        len = (WORD)maxi(rxlen, 0);     /* Make response */
        txlen = make_icmp(gfp, locp, &rem, ICREP, icmp->c.code, (WORD)len);
        put_frame(gfp, txlen);          /* Send packet */
    }
}

/* Poll the network interface to keep it alive */
void do_poll(void)
{
    int n;

    for (n=0; n<nnets; n++)
        poll_net(locnodes[n].dtype);
}

/* Find the local node, given a remote node that should be on the same subnet
** Return pointer to local node, or 0 if remote is unreachable */
NODE *find_subnet(NODE *destp)
{
    int n=0, ok=0;

    while (n<nnets && !(ok=on_subnet(destp->ip, &locnodes[n])))
        n++;
    return(ok ? &locnodes[n] : 0);
}

/* Add a node to the ARP cache */
void add_cache(NODE *np)
{
    char temps[21];

    if (!lookup_cache(np))
    {
        remnodes[remidx] = *np;
        remidx = (remidx+1) % MAXREMS;
        if (netdebug)
            printf ("Adding %s to ARP cache\n", ipstr(np->ip, temps));
    }
}

/* Look up a node in the ARP cache. If found, fill in MAC addr, return non-0 */
int lookup_cache(NODE *np)
{
    int i=remidx, n=MAXREMS, ok=0;

    while (!(ok=remnodes[i=(i-1)%MAXREMS].ip == np->ip) && n>0)
        n--;
    if (ok)
        *np = remnodes[i];
    return(ok);
}

/* Return ptr to local node 'n' (n=0 for first), return 0 if doesn't exist
** Used by IP functions to get my netmask & gateway addresses */
NODE *locnode_n(int n)
{
    return(n<nnets ? &locnodes[n] : 0);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    ROUTER [options]\n");
    printf("Options:  -c name   Config filename (default %s)\n", cfgfile);
    printf("          -v        Verbose mode (displays network packets)\n");
    printf("Example:  ROUTER -v -c test.cfg\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */
