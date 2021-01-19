/* ARP address scan utility for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 17/12/99
** v0.02 JPB 30/12/99 Improved DJGPP compatibility - removed MAXPATH definition
** v0.03 JPB 31/12/99 Added support for IEEE 802.3 SNAP
** v0.04 JPB 21/2/00  Updated network interface
** v0.05 JPB 3/7/00   Added 'get_locnode_n' function pointer
**                    Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
*/

#define VERSION "0.05"

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

#define SCANCOUNT   20              /* Default number of addresses to scan */
#define DELTIME     100             /* Delay between transmissions (msec) */

GENFRAME genframe;                  /* Frame for network Tx/Rx */
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast Ethernet addr */
extern BYTE myeth[MACLEN];          /* My Ethernet address */
NODE  locnode;                      /* My Ethernet and IP addresses */
NODE remnode;                       /* Remote Ethernet and IP addresses */
int scancount=SCANCOUNT;            /* Number of addresses to scan */
int breakflag;                      /* Flag to indicate ctrl-break pressed */

extern int netdebug;                /* Debug flag - net packet display */

/* Function pointer: upcall from TCP/IP stack */
extern NODE *(*get_locnode_n)(int n);           /* Get local node */

/* Prototypes */
WORD read_netconfig(char *fname, NODE *np);
NODE *locnode_n(int n);
void disp_usage(void);
void break_handler(int sig);

int main(int argc, char *argv[])
{
    int args=0, err=0;
    LWORD remip=0, mstimer;
    WORD rxlen, txlen, dtype;
    GENFRAME *gfp;
    ARPKT *arp;
    char *p, temps[18];

    printf("ARPSCAN v" VERSION "\n");
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
            case 'N':                           /* -N: num of nodes to scan */
                scancount = atoi(argv[++args]);
                break;
            case 'V':                           /* -V: verbose packet display */
                netdebug = 1;
                break;
            default:
                err = 1;
            }
        }
        else if isdigit(argv[args][0])          /* Starting IP address */
            remip = atoip(argv[args]);
    }
    if (err)                                    /* Prompt user if error */
        disp_usage();
    else if (!(dtype=read_netconfig(cfgfile, &locnode)))
        printf("Invalid configuration '%s'\n", cfgfile);
    else
    {
        remnode.dtype = genframe.g.dtype = dtype;   /* Set frame driver type */
        gfp = &genframe;                            /* Get pointer to frame */
        printf("Press ESC or ctrl-C to exit\n");
        printf("IP %s", ipstr(locnode.ip, temps));
        if (dtype & DTYPE_ETHER)
            printf(" Ethernet %s (local)\n\n", ethstr(locnode.mac, temps));
        mstimeout(&mstimer, 0);                 /* Refresh timer */
        while (!breakflag)
        {                                       /* If scanning & timeout.. */
            if (remip && mstimeout(&mstimer, DELTIME))
            {
                if (!scancount--)               /* ..stop looping if done */
                    break;
                remnode.ip = remip++;           /* Broadcast next IP adr */
                memcpy(remnode.mac, bcast, MACLEN);
                txlen = make_arp(gfp, &locnode, &remnode, ARPREQ);
                put_frame(gfp, txlen);
            }
            poll_net(gfp->g.dtype);             /* Keep network alive */
            if ((rxlen=get_frame(gfp)) > 0)     /* Check for incoming pkts */
            {
                if (is_arp(gfp, rxlen))
                {                               /* ARP response? */
                    arp = getframe_datap(gfp);
                    if (arp->op==ARPRESP && arp->sip==remnode.ip)
                    {
                        printf("IP %s ", ipstr(remnode.ip, temps));
                        printf("Ethernet %s\n", ethstr(arp->smac, temps));
                    }
                    if (arp->op==ARPREQ && arp->dip==locnode.ip)
                    {                           /* ARP request? */
                        remnode.ip = arp->sip;  /* Make ARP response */
                        memcpy(remnode.mac, arp->smac, MACLEN);
                        txlen = make_arp(gfp, &locnode, &remnode, ARPRESP);
                        put_frame(gfp, txlen);
                    }
                }
            }
            if (kbhit())                        /* If user hit a key.. */
                breakflag = getch()==0x1b;      /* ..check for ESC */
        }
        close_net(dtype);                       /* Shut down net driver */
    }
    return(0);
}

/* Read network config file to get IP address
** Return driver type, 0 if error */
WORD read_netconfig(char *fname, NODE *np)
{
    char temps[31];
    WORD dtype=0;

    if (read_cfgstr(fname, "net", netcfg, MAXNETCFG))
    {                                               /* Get IP address */
        if (!read_cfgstr(fname, "ip", temps, 30) || (np->ip=atoip(temps))==0)
            printf("No IP address\n");
        else if (!(dtype = open_net(netcfg)))       /* Open net driver */
            printf("Can't open net driver '%s'\n", netcfg);
        else                                        /* Save ether address */
            memcpy(np->mac, ether_addr(dtype), MACLEN);
    }
    return(dtype);
}

/* Return ptr to local node 'n' (n=0 for first), return 0 if doesn't exist
** Used by IP functions to get my netmask & gateway addresses */
NODE *locnode_n(int n)
{
    return(n==0 ? &locnode : 0);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    ARPSCAN [options] [start_IP_addr]\n");
    printf("          If IP address is omitted, acts as server\n");
    printf("Options:  -c name     Config filename (default %s)\n", cfgfile);
    printf("          -n count    Scan count (default %u)\n", SCANCOUNT);
    printf("Example:  ARPSCAN -c test.cfg 10.1.1.1\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */
