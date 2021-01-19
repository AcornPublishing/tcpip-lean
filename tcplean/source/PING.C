/* Ping utility for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 3/1/00
** v0.02 JPB 7/1/00  Added random data
**                   Added ICMP data size limit
** v0.03 JPB 11/1/00 Fixed divide-by-zero if zero data length specified
** v0.04 JPB 17/1/00 Changed 'netframe' to 'genframe', added fragment offset
** v0.05 JPB 17/1/00 Added transmit fragmentation
** v0.06 JPB 18/1/00 Added receive framentation
** v0.07 JPB 19/1/00 Added logfile capability
**                   Split off network functions from tcpfs.c into net.c
** v0.08 JPB 20/1/00 Added support for multiple networks
** v0.09 JPB 20/1/00 Improved packet demultiplexing
** v0.10 JPB 24/1/00 Removed subframe ptrs - subframe is now appended to frame
**                   IP headers with options are now resized
** v0.11 JPB 25/1/00 Added gateway capability
** v0.12 JPB 27/1/00 Adapted for compatibility with router code
** v0.13 JPB 24/2/00 Changed keyscan to ESC or ctrl-C
** v0.14 JPB 25/2/00 Removed server mode switch (use absence of IP address)
** v0.15 JPB 29/2/00  Fixed bug in non-Ethernet logging
** v0.16 JPB 1/3/00  Fixed 'gate_ip' problem when no gateway specified
** v0.17 JPB 23/3/00  Used upcall pointers in TCP.C
** v0.18 JPB 3/7/00   Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
*/

#define VERSION "0.18"

/* Debug option to use structured data (alphabetic chars 'a'-'w') */
#define ASCDATA 0   /* Set non-zero to use ASCII (not random) data in ping */

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

#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */

#define WAITIME     1000            /* Default delay between pings (msec) */
#define MINWAIT     10              /* Minimum delay time */
#define ARPTIME     500             /* Delay between ARP cycles */
#define DATALEN     32              /* Default ICMP data length */

GENFRAME genframe;                  /* Frame for network Tx/Rx */
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast Ethernet addr */
NODE  locnode;                      /* My Ethernet and IP addresses */
NODE remnode;                       /* Remote Ethernet and IP addresses */
int floodmode;                      /* Flag to enable flood ping mode */
int arped;                          /* Flag to show if remote has been ARPed */
WORD datalen=DATALEN;               /* Length of ICMP data */
WORD txseq, rxseq;                  /* ICMP sequence numbers */
BYTE *testdata;                     /* Block of test data */
LWORD remip;                        /* Remote IP address */
WORD waitime=WAITIME;               /* Waiting time in msec */
LWORD txcount, rxcount, errcount;   /* Transaction counters */
int breakflag;                      /* Flag to indicate ctrl-break pressed */
extern int netdebug;                /* Debug flag: net packet display */

extern NODE *(*get_locnode_n)(int n);   /* Upcall to get local node */

/* Prototypes */
WORD read_netconfig(char *fname, NODE *np);
NODE *locnode_n(int n);
void do_transmit(GENFRAME *gfp);
void do_receive(GENFRAME *gfp);
void do_poll(void);
void disp_usage(void);
void break_handler(int sig);

int main(int argc, char *argv[])
{
    int i, args=0, len, err=0;
    LWORD mstimer;
    WORD dtype;
    GENFRAME *gfp;
    char *p, c, temps[18];

    printf("PING v" VERSION "\n");              /* Sign on */
    get_locnode_n = locnode_n;                  /* Set upcall ptr to func */
    signal(SIGINT, break_handler);              /* Trap ctrl-C */
    while (argc > ++args)                       /* Process command-line args */
    {
        if ((c=argv[args][0])=='-' || c=='/')
        {
            switch (toupper(argv[args][1]))
            {
            case 'C':                           /* -C: config filename */
                strncpy(cfgfile, argv[++args], MAXPATH);
                if ((p=strrchr(cfgfile, '.'))==0 || !isalpha(*(p+1)))
                    strcat(cfgfile, CFGEXT);
                break;
            case 'L':                           /* -L: length of data */
                datalen = maxi(atoi(argv[++args]), 1);
                break;
            case 'W':                           /* -W: waiting time in msec */
                waitime = maxi(atoi(argv[++args]), MINWAIT);
                break;
            case 'V':                           /* -V: verbose (debug) mode */
                netdebug = 1;
                break;
            case 'F':                           /* -F: flood mode */
                floodmode = 1;
                break;
            default:                            /* Otherwise error */
                err = 1;
            }
        }
        else if isdigit(argv[args][0])          /* Destination IP address */
            remip = atoip(argv[args]);
    }
    if ((testdata=malloc(datalen*2))==0)        /* Allocate mem for test data */
    {
        printf("Can't allocate %u bytes for test data\n", datalen*2);
        exit(1);
    }
    for (i=0; i<datalen*2; i++)                 /* Test block is 2x data size */
#if ASCDATA
        testdata[i] = (BYTE)(i%23 + 'a');       /* ..same data as DOS ping */
#else
        testdata[i] = (BYTE)rand();             /* ..or random test data.. */
#endif
    if (err)                                    /* Prompt user if error */
        disp_usage();                           /* Read net config */
    else if (!(dtype=read_netconfig(cfgfile, &locnode)))
        printf("Invalid configuration '%s'\n", cfgfile);
    else
    {
        remnode.ip = remip;                     /* Set remote addr */
        memcpy(remnode.mac, bcast, MACLEN);     /* ..as broadcast */
        genframe.g.dtype = dtype;               /* Set frame driver type */
        gfp = &genframe;                        /* Get pointer to frame */
        printf("IP %s", ipstr(locnode.ip, temps));
        printf(" mask %s", ipstr(locnode.mask, temps));
        if (locnode.gate)
            printf(" gate %s", ipstr(locnode.gate, temps));
        if (dtype & DTYPE_ETHER)
            printf(" Ethernet %s", ethstr(locnode.mac, temps));
        if (gfp->g.dtype & DTYPE_SLIP)          /* If SLIP.. */
        {
            arped = 1;                          /* ..don't try ARPing! */
            printf(" SLIP");
        }
        if (datalen > (len=icmp_maxdata(gfp)*2))/* Don't exceed 2 frames */
        {
            printf("\nWARNING: data length reduced to %u bytes", len);
            datalen = len;
        }
        if (!remip)
            printf("\nServer mode");
        else
        {
            if (!on_subnet(remip, &locnode) && !locnode.gate)
                printf("\nWARNING: no gateway specified!");
            printf("\n%s ", arped ? "Pinging" : "Resolving");
            printf("%s", ipstr(gate_ip(&remnode, &locnode), temps));
        }
        printf(" - ESC or ctrl-C to exit\n");
        mstimeout(&mstimer, 0);                 /* Refresh timer */
        while (!breakflag)
        {
            if (remip)                          /* If client (not server) */
            {
                if (!arped)                     /* If not ARPed.. */
                {                               /* ..and timeout.. */
                    if (mstimeout(&mstimer, ARPTIME))
                        do_transmit(gfp);       /* ..send ARP */
                }
                else if (floodmode)             /* If flood ping.. */
                {                               /* ..and response or timeout */
                    if (txseq==rxseq || mstimeout(&mstimer, waitime))
                    {
                        mstimeout(&mstimer, 0); /* ..refresh timer */
                        do_transmit(gfp);       /* ..transmit next packet */
                    }
                }
                else                            /* If normal pinging.. */
                {                               /* ..and timeout */
                    if (mstimeout(&mstimer, waitime))
                        do_transmit(gfp);       /* ..transmit next packet */
                }
            }
            do_receive(gfp);                    /* Check responses */
            do_poll();                          /* Poll net drivers */
            if (kbhit())                        /* If user hit a key.. */
                breakflag = getch()==0x1b;      /* ..check for ESC */
        }
        close_net(dtype);                       /* Shut down net driver */
    }
    free(testdata);                             /* Free test data memory */
    printf("ICMP echo: %lu sent, %lu received, %lu errors\n",
           txcount, rxcount, errcount);         /* Print stats */
    return(0);
}

/* Read network config file to get IP address netmask and gateway
** Return driver type, 0 if error */
WORD read_netconfig(char *fname, NODE *np)
{
    char temps[31];
    WORD dtype=0;
    BYTE b;

    if (read_cfgstr(fname, "net", netcfg, MAXNETCFG))
    {                                               /* Get IP address */
        if (!read_cfgstr(fname, "ip", temps, 30) || (np->ip=atoip(temps))==0)
            printf("No IP address\n");
        else if (!(dtype = open_net(netcfg)))       /* Open net driver */
            printf("Can't open net driver '%s'\n", netcfg);
        else
        {                                           /* Save ether address */
            memcpy(np->mac, ether_addr(dtype), MACLEN);
            b = (BYTE)(np->ip >> 24);
            if (read_cfgstr(fname, "mask", temps, 30))
                np->mask = atoip(temps);            /* Get netmask */
            else
                np->mask = b<128 ? 0xff000000L: b<192 ? 0xffff0000L:0xffffff00L;
            if (read_cfgstr(fname, "gate", temps, 30))
                np->gate = atoip(temps);            /* Get gateway IP addr */
            else
                np->gate = 0;
        }
    }
    return(dtype);
}

/* Return ptr to local node 'n' (n=0 for first), return 0 if doesn't exist
** Used by IP functions to get my netmask & gateway addresses */
NODE *locnode_n(int n)
{
    return(n==0 ? &locnode : 0);
}

/* Do next transmission cycle */
void do_transmit(GENFRAME *gfp)
{
    ICMPKT *icmp;
    BYTE *data;
    int txlen;

    if (!arped)                                     /* If not arped, send ARP */
    {
        printf("ARP ");                             /* Make packet */
        txlen = make_arp(gfp, &locnode, &remnode, ARPREQ);
    }
    else
    {
        icmp = getframe_datap(gfp);                 /* Send echo req */
        icmp->c.seq = ++txseq;
#if ASCDATA
        data = testdata;                            /* ..using plain data */
#else
        data = &testdata[txseq%datalen];            /* ..or random */
#endif
        memcpy(icmp->data, data, datalen);
        icmp->c.ident = 1;                          /* Make packet */
        txlen = make_icmp(gfp, &locnode, &remnode, ICREQ, 0, datalen);
        txcount++;
    }
    put_frame(gfp, txlen);                          /* Transmit packet */
}

/* Check for incoming packets, send response if required */
void do_receive(GENFRAME *gfp)
{
    NODE node;
    ICMPKT *icmp;
    IPKT *ip;
    ARPKT *arp;
    BYTE *data;
    int rxlen, txlen, len;
    char temps[18];

    if ((rxlen=get_frame(gfp)) > 0)                 /* Any incoming frames? */
    {
        ip = getframe_datap(gfp);
        if (is_arp(gfp, rxlen))
        {                                           /* ARP response? */
            arp = getframe_datap(gfp);
            if (arp->op==ARPRESP && arp->sip==remip)
            {
                memcpy(remnode.mac, arp->smac, MACLEN);
                printf("OK\n");
                arped = 1;
            }
            else if (arp->op==ARPREQ && arp->dip==locnode.ip)
            {                                       /* ARP request? */
                node.ip = arp->sip;                 /* Make ARP response */
                memcpy(node.mac, arp->smac, MACLEN);
                txlen = make_arp(gfp, &locnode, &node, ARPRESP);
                put_frame(gfp, txlen);              /* Send packet */
            }
        }
        else if ((rxlen=is_ip(gfp, rxlen))!=0 &&    /* IP datagram? */
                 ip->i.dip==locnode.ip || ip->i.dip==BCASTIP)
        {
            if ((rxlen=is_icmp(ip, rxlen))!=0)      /* ICMP? */
            {
                icmp = (ICMPKT *)ip;
                if (icmp->c.type == ICREP)          /* Echo response? */
                {
                    printf("Reply from %s seq=%u len=%u ",
                           ipstr(icmp->i.sip, temps), icmp->c.seq, rxlen);
                    rxseq = icmp->c.seq;            /* Check response */
#if ASCDATA
                    data = testdata;
#else
                    data = &testdata[rxseq%datalen];
#endif
                    if (rxlen==datalen && !memcmp(icmp->data, data, rxlen))
                    {
                        printf("OK\n");
                        rxcount++;
                    }
                    else
                    {
                        printf("ERROR\n");
                        errcount++;
                    }
                }
                else if (icmp->c.type==ICREQ)       /* Echo request? */
                {
                    getip_srce(gfp, &node);
                    len = (WORD)maxi(rxlen, 0);     /* Make response */
                    txlen = make_icmp(gfp, &locnode, &node, ICREP,
                                      icmp->c.code, (WORD)len);
                    put_frame(gfp, txlen);          /* Send packet */
                }
            }
        }
    }
}

/* Poll the network interface to keep it alive */
void do_poll(void)
{
    poll_net(genframe.g.dtype);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    PING [options] [IP_addr]\n");
    printf("          If no IP address given, enters server mode\n");
    printf("Options:  -c name   Config filename (default '%s')\n", cfgfile);
    printf("          -v        Verbose (debug) mode\n");
    printf("          -f        Flood mode\n");
    printf("          -l xxx    Length of ICMP data (in bytes)\n");
    printf("          -w xxx    Waiting time (in msec)\n");
    printf("Example:  PING -c test.cfg 10.1.1.1\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */
