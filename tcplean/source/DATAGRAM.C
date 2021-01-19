/* UDP datagram utility for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 10/2/00
** v0.02 JPB 10/2/00  Added ICMP message for unreachable ports
** v0.03 JPB 15/2/00  Changed to command-line interface
** v0.04 JPB 7/4/00   Initial TFTP work
** v0.05 JPB 7/4/00   Removed TFTP stuff, added binary mode and file I/P
** v0.06 JPB 3/7/00   Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
**                    Fixed bug in SLIP option (was sending ARP!)
*/

#define VERSION "0.06"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <signal.h>
#include <time.h>

#include "ether.h"
#include "netutil.h"
#include "net.h"
#include "ip.h"
#include "udp.h"

#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */
#define DEFPORT     DAYPORT         /* Default port number */
#define ARPTIME     1000            /* ARP retry timer (msec) */
#define COMMANDLEN  80
#define TRYTIME     1000
#define MAXTRIES    5

#define ARP_TX      1               /* Client state: sending ARP */
#define ARP_RX      2               /*               received ARP */
#define CLIENT_DONE 3               /*               completed */

GENFRAME genframe;                  /* Frame for network Tx/Rx */
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast Ethernet addr */
extern BYTE zermac[MACLEN];         /* All-zero MAC addr */
NODE  locnode;                      /* My Ethernet and IP addresses */
NODE remnode;                       /* Remote node */
WORD remport;                       /* Remote port number */
int breakflag;                      /* Flag to indicate ctrl-break pressed */
int client;                         /* Flag to indicate client is active */
int binmode;                        /* Flag to enable binary mode */
char infile[MAXPATH+1];             /* Filename for I/P data */

/* Debug flags */
extern int netdebug;                /* Net packet display */
extern int udpdebug;                /* UDP frame display */

/* Function pointer: upcall from TCP/IP stack */
extern NODE *(*get_locnode_n)(int n);           /* Get local node */

/* Prototypes */
WORD read_netconfig(char *fname, NODE *np);
NODE *locnode_n(int n);
int do_receive(GENFRAME *gfp);
int udp_receive(GENFRAME *gfp, int len);
void udp_transmit(GENFRAME *gfp, NODE *sp, NODE *dp, void *dat, int len);
void udp_transmit_file(GENFRAME *gfp, NODE *sp, NODE *dp, FILE *in);
void do_poll(GENFRAME *gfp);
void disp_usage(void);
void disp_data(BYTE *data, int len);
WORD str2service(char *str);
void break_handler(int sig);

int main(int argc, char *argv[])
{
    int args=0, err=0, tries=0, st, cstate=ARP_TX, lastcstate=0;
    char c, *p, temps[18], *cmd="";
    WORD dtype;
    GENFRAME *gfp;
    LWORD mstime;
    FILE *in=0;

    printf("DATAGRAM v" VERSION "\n");          /* Sign on */
    get_locnode_n = locnode_n;                  /* Set upcall ptr to func */
    signal(SIGINT, break_handler);              /* Trap ctrl-C */
    remnode.port = DEFPORT;                     /* Preset remote port number */
    locnode.port = MINEPORT;
    while (argc > ++args)                       /* Process command-line args */
    {
        if ((c=argv[args][0]) == '-')
        {
            switch (toupper(argv[args][1]))
            {
            case 'B':                           /* -B: binary mode */
                binmode = 1;
                break;
            case 'C':                           /* -C: config filename */
                strncpy(cfgfile, argv[++args], MAXPATH);
                if ((p=strrchr(cfgfile, '.'))==0 || !isalpha(*(p+1)))
                    strcat(cfgfile, CFGEXT);
                break;
            case 'I':                           /* -I: filename for data in*/
                strncpy(infile, argv[++args], MAXPATH);
                break;
            case 'U':                           /* -U: display UDP segments */
                udpdebug = 1;
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
        else if (client==0 && isdigit(c))       /* If client mode.. */
        {
            remnode.ip = atoip(argv[args]);     /* Get destination IP address */
            client++;
        }
        else if (client==1)
        {
            if ((remport=str2service(argv[args]))!=0)
                remnode.port = remport;         /* ..then port number */
            else
            {
                printf("Unrecognised port number/service '%s'\n", argv[args]);
                err++;
            }
            client++;
        }
        else if (client == 2)
        {
            cmd = argv[args];                   /* ..then command string */
            client++;
        }
    }
    if (err)                                    /* Prompt user if error */
        disp_usage();
    else if (!(dtype=read_netconfig(cfgfile, &locnode)))
        printf("Invalid configuration '%s'\n", cfgfile);
    else if (*infile && (in=fopen(infile, "rb"))==0)
        printf("Can't open data I/P file '%s'\n", infile);
    else
    {
        remnode.dtype = genframe.g.dtype = dtype;   /* Set frame driver type */
        gfp = &genframe;                            /* Get pointer to frame */
        printf("IP %s", ipstr(locnode.ip, temps));
        printf(" mask %s", ipstr(locnode.mask, temps));
        if (locnode.gate)
            printf(" gate %s", ipstr(locnode.gate, temps));
        if (dtype & DTYPE_ETHER)
            printf(" Ethernet %s", ethstr(locnode.mac, temps));
        else if (dtype & DTYPE_SLIP)
            cstate = ARP_RX;
        printf("\nPress ESC or ctrl-C to exit\n");
        if (client)
        {
            printf("Contacting %s...\n\n", ipstr(remnode.ip, temps));
            memcpy(remnode.mac, bcast, MACLEN);
        }
        mstimeout(&mstime, 0);
        while (!breakflag)                      /* Main loop.. */
        {
            if (client && (mstimeout(&mstime, TRYTIME) || cstate!=lastcstate))
            {
                if (tries++ > MAXTRIES)         /* Giving up? */
                    breakflag = 1;
                else if (cstate == ARP_TX)      /* (Re)transmit ARP? */
                    put_frame(gfp, make_arp(gfp, &locnode, &remnode, ARPREQ));
                else if (cstate == ARP_RX)      /* ARP response? */
                {
                    if (in)
                        udp_transmit_file(gfp, &locnode, &remnode, in);
                    else
                        udp_transmit(gfp, &locnode, &remnode, cmd, strlen(cmd));
                }
                else if (cstate == CLIENT_DONE) /* UDP response? */
                    breakflag = 1;
                lastcstate = cstate;            /* Record state-change */
            }
            st = do_receive(gfp);               /* Receive frames */
            cstate = st ? st : cstate;          /* ..maybe change state */
            do_poll(gfp);                       /* Poll net drivers */
            if (kbhit() && getch()==0x1b)       /* Check keyboard */
                breakflag = 1;
        }
        close_net(dtype);                       /* Shut down net driver */
    }
    if (in)
        fclose(in);
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
            np->dtype = dtype;                      /* ..and driver type */
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

/* Check for incoming packets, send response if required
** Return state-change value if ARP response or datagram received */
int do_receive(GENFRAME *gfp)
{
    NODE node;
    ARPKT *arp;
    IPKT *ip;
    ICMPKT *icmp;
    int rxlen, txlen, len, ret=0;

    if ((rxlen=get_frame(gfp)) > 0)                 /* Any incoming frames? */
    {
        ip = getframe_datap(gfp);
        if (is_arp(gfp, rxlen))
        {                                           /* ARP response? */
            arp = getframe_datap(gfp);
            if (arp->op==ARPREQ && arp->dip==locnode.ip)
            {                                       /* ARP request? */
                node.ip = arp->sip;                 /* Make ARP response */
                memcpy(node.mac, arp->smac, MACLEN);
                txlen = make_arp(gfp, &locnode, &node, ARPRESP);
                put_frame(gfp, txlen);              /* Send packet */
            }
            if (arp->op==ARPRESP && arp->dip==locnode.ip)
            {                                       /* ARP response? */
                memcpy(remnode.mac, arp->smac, MACLEN);
                ret = ARP_RX;
            }
        }
        else if ((rxlen=is_ip(gfp, rxlen))!=0 &&    /* IP datagram? */
                 ip->i.dip==locnode.ip || ip->i.dip==BCASTIP)
        {
            getip_srce(gfp, &node);
            if ((len=is_icmp(ip, rxlen))!=0)        /* ICMP? */
            {
                icmp = (ICMPKT *)ip;
                if (icmp->c.type == ICREQ)          /* Echo request? */
                {
                    len = (WORD)maxi(len, 0);       /* Make response */
                    txlen = make_icmp(gfp, &locnode, &node, ICREP,
                                      icmp->c.code, (WORD)len);
                    put_frame(gfp, txlen);          /* Send packet */
                }
                else if (icmp->c.type == ICUNREACH)
                    printf("ICMP: destination unreachable\n");
            }
            else if ((len=is_udp(ip, rxlen))!=0)    /* UDP? */
            {
                ret = udp_receive(gfp, maxi(len, 0));
            }
        }
    }
    return(ret);
}

/* Receive a UDP datagram: return non-0 if client state-change */
int udp_receive(GENFRAME *gfp, int len)
{
    UDPKT *udp;
    int ret=0;
    NODE loc, rem;
    char temps[30];
    time_t t;

    udp = getframe_datap(gfp);
    getudp_srce(gfp, &rem);                         /* Get srce & dest nodes */
    getudp_locdest(gfp, &loc);
    if (loc.port == locnode.port)                   /* Client response */
    {
        disp_data(udp->data, len);                  /* Display data.. */
        ret = CLIENT_DONE;                          /* ..and exit */
    }
    else if (loc.port == ECHOPORT)                  /* Echo req: resend data */
        udp_transmit(gfp, &loc, &rem, udp->data, len);
    else if (loc.port == DAYPORT)                   /* Daytime req: get date */
    {
        time(&t);                                   /* Get standard string */
        strcpy(temps, ctime(&t));
        strcpy(&temps[24], "\r\n");                 /* Patch newline chars */
        udp_transmit(gfp, &loc, &rem, (BYTE *)temps, 26);
    }
    else                                            /* Unreachable: send ICMP */
    {
        swap_udp(udp);
        put_frame(gfp, icmp_unreach(gfp, &loc, &rem, UNREACH_PORT));
    }
    return(ret);
}

/* Send a UDP datagram, given destination node, data and length */
void udp_transmit(GENFRAME *gfp, NODE *sp, NODE *dp, void *dat, int len)
{
    UDPKT *udp;

    udp = getframe_datap(gfp);
    memmove(udp->data, dat, len);
    put_frame(gfp, make_udp(gfp, sp, dp, (WORD)maxi(len, 0)));
}

/* Send a UDP datagram, given destination node, data and length */
void udp_transmit_file(GENFRAME *gfp, NODE *sp, NODE *dp, FILE *in)
{
    UDPKT *udp;
    WORD len;

    udp = getframe_datap(gfp);
    len = fread(udp->data, 1, sizeof(udp->data), in);
    put_frame(gfp, make_udp(gfp, sp, dp, len));
}

/* Poll the network interface to keep it alive */
void do_poll(GENFRAME *gfp)
{
    poll_net(gfp->g.dtype);
}

/* Convert string (numeric or alphabetic) into a port number, 0 if error */
WORD str2service(char *str)
{
    WORD port=0;

    if (isdigit(*str))
        port = atoi(str);
    else if (!stricmp(str, "echo"))
        port = ECHOPORT;
    else if (!stricmp(str, "daytime"))
        port = DAYPORT;
    else if (!stricmp(str, "time"))
        port = TIMEPORT;
    else if (!stricmp(str, "snmp"))
        port = SNMPORT;
    return(port);
}

/* Display the incoming UDP data */
void disp_data(BYTE *data, int len)
{
    BYTE b;
    int i, n, oset=0;

    if (!binmode)
    {
        while (len--)
        {
            b = *data++;
            if ((b>=' '&&b<='~') || b=='\n')
                putchar(b);
            else
                putchar(' ');
        }
    }
    else while (len > 0)
    {
        n = mini(len, 24);
        printf("%04X: ", oset);
        for (i=0; i<n; i++)
            printf("%02X ", data[i]);
        printf("\n      ");
        for (i=0; i<n; i++, data++)
        {
            putchar(' ');
            putchar(*data>=' ' && *data<='~' ? *data : 0);
            putchar(' ');
        }
        len -= n;
        oset += n;
        putchar('\n');
    }
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    DATAGRAM [options] [IP_addr [portnum [data]]]\n");
    printf("                    Default port number is %u\n", DEFPORT);
    printf("Options:  -b        Binary data (hexadecimal data display)\n");
    printf("          -i name   Input data filename\n");
    printf("          -c name   Config filename (default %s)\n", cfgfile);
    printf("          -u        UDP datagram display\n");
    printf("          -v        Verbose (frame) display\n\n");
    printf("Example:  DATAGRAM -c test.cfg 10.1.1.1 echo \"test string\"\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */

