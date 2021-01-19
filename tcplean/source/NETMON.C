/* Simple network monitor for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 23/3/00
** v0.02 JPB 24/3/00 Fixed interference problem with multiple SLIP channels
** v0.03 JPB 27/3/00 Added ASCII TCP display
** v0.04 JPB 3/7/00   Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
*/

#define VERSION "0.04"

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
#include "tcp.h"

#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */
#define MAXASCII    70              /* Max num of ASCII chars to be displayed */

extern int netdebug;                /* Flag to enable net packet display */
extern int tcpdebug;                /* TCP frame display */
extern int promisc;                 /* Flag to enable'promiscous' net driver */
int breakflag;                      /* Flag to indicate ctrl-break pressed */

GENFRAME genframe;
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */

WORD dtypes[MAXNETS];               /* Driver types for each network */
int nnets;                          /* Number of nets in use */

WORD read_netconfig_n(char *fname, int n);
void do_receive(void);
void do_poll(void);
void break_handler(int sig);
void disp_usage(void);

int main(int argc, char *argv[])
{
    int n, args=0, err=0;
    char *p;
    WORD dt;

    printf("NETMON v" VERSION "\n");            /* Sign on */
    signal(SIGINT, break_handler);              /* Trap ctrl-C */
    promisc = 1;                                /* Promiscuous mode! */
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
            case 'T':                           /* -T: display TCP segments */
                tcpdebug = 1;
                break;
            case 'V':                           /* -V: verbose packet display */
                netdebug |= 1;
                break;
            case 'X':                           /* -X: hex packet display */
                netdebug |= 2;
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
           (dt=read_netconfig_n(cfgfile, nnets))!=0)
    {
        dtypes[nnets++] = dt;                   /* Save driver type */
        printf ("Net %u ", nnets);
        if (dt & DTYPE_ETHER)
            printf("Ethernet\n");
        else
            printf("SLIP\n");
    }
    if (nnets > 0)
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
            close_net(dtypes[n]);               /* Shut down net drivers */
    }
    return(0);
}

/* Read nth network config item (n=0 for first) to get IP addresses and netmasks
** Return driver type, 0 if error */
WORD read_netconfig_n(char *fname, int n)
{
    WORD dtype=0;

    if (read_cfgstr_n(fname, n, "net", netcfg, MAXNETCFG))
    {
        if (!(dtype = open_net(netcfg)))        /* Open net driver */
            printf("Can't open net %u driver '%s'\n", n, netcfg);
    }
    return(dtype);
}

/* Check for incoming packets, send response if required */
void do_receive(void)
{
    int rxlen, oset, dlen, i;
    char c;
    GENFRAME *gfp;
    IPKT *ip;
    TCPKT *tcp;

    gfp = &genframe;
    gfp->g.dtype = 0;                           /* Has any net driver.. */
    if ((rxlen=get_frame(gfp)) > 0)             /* Any incoming frames? */
    {
        ip = getframe_datap(gfp);
        if (is_arp(gfp, rxlen))                     /* ARP? */
        {
        }
        else if ((rxlen=is_ip(gfp, rxlen))!=0)      /* IP datagram? */
        {
            if (is_icmp(ip, rxlen))                 /* ICMP? */
            {
            }
            else if ((dlen=is_tcp(ip, rxlen)) > 0)  /* TCP? */
            {
                tcp = getframe_datap(gfp);
                oset = gettcp_opt(tcp, 0);
                dlen = maxi(dlen, 0);
                dlen = mini(dlen, MAXASCII);
                for (i=0; i<dlen; i++)
                {
                    c = tcp->data[oset++];
                    if (c >= ' ' && c<0x7e)
                        putchar(c);
                    else if (c=='\r' || c=='\n')
                    {
                        putchar('\\');
                        putchar(c=='\r' ? 'r' : c=='\n' ? 'n' : '?');
                    }
                    else
                        putchar(' ');
                }
                putchar('\n');
            }
        }
    }
}

/* Poll the network interface to keep it alive */
void do_poll(void)
{
    int n;

    for (n=0; n<nnets; n++)
        poll_net(dtypes[n]);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    NETMON [options]\n");
    printf("Options:  -c name   Config filename (default %s)\n", cfgfile);
    printf("          -t        TCP segment display\n");
    printf("          -v        Verbose packet display\n");
    printf("          -x        Hexadecimal data display\n");
    printf("Example:  NETMON -c test.cfg\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */
