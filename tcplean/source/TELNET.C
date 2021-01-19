/* Telnet utility for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */
/*
** v0.01 JPB 27/1/00
** v0.02 JPB 28/1/00  First steps toward TCP implementation
** v0.03 JPB 31/1/00  Added timeout
** v0.04 JPB 1/2/00   Added duplicate segment handling
** v0.05 JPB 2/2/00   Added TCP upcall, and active close
** v0.06 JPB 3/2/00   Simplified socket closure
** v0.07 JPB 4/2/00   Simplified state machine
** v0.08 JPB 4/2/00   Split upcalls into client & server
** v0.09 JPB 7/2/00   Improved active close
** v0.10 JPB 1/3/00   Fixed 'gate_ip' problem when no gateway specified
**                    Fixed problem with SLIP node matching
** v0.11 JPB 1/3/00   changed command-line interface to include port names
** v0.12 JPB 20/3/00  Added LiteLink and modem support
** v0.13 JPB 23/3/00  Used upcall pointers in TCP.C
** v0.14 JPB 6/4/00   Replaced 'strcmpi' with 'stricmp' for DJGPP compatibility
** v0.15 JPB 21/4/00  Fixed TCP problem receiving FIN + data
** v0.16 JPB 5/5/00   Minor cosmetic improvements to TCP code
** v0.17 JPB 3/7/00   Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
*/

#define VERSION "0.17"

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
#include "tcp.h"

#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */
#define DEFPORT     TELPORT         /* Default port number */

/* Test page for Web server */
#define MYPAGE "HTTP/1.0 200 OK\r\n\r\n\
    <html><body>Test page from telnet server v" VERSION "</body></html>"

GENFRAME genframe;                  /* Frame for network Tx/Rx */
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast Ethernet addr */
NODE  locnode;                      /* My Ethernet and IP addresses */
int breakflag;                      /* Flag to indicate ctrl-break pressed */
char *clientcmd;                    /* Pointer to command-line command */

/* Debug flags */
extern int netdebug;                /* Verbose packet display */
extern int tcpdebug;                /* TCP frame display */
extern int statedebug;              /* TCP state display */

/* Socket storage */
#define NSOCKS 2
TSOCK tsocks[NSOCKS] =              /* Initialise index num and buffer sizes */
{
    {1,{_CBUFFLEN_},{_CBUFFLEN_}}, {2,{_CBUFFLEN_},{_CBUFFLEN_}}
};

/* Telnet options */
BYTE telopts[] =
{
    TEL_IAC, TEL_DO,    TEL_SGA,    /* Do suppress go-ahead */
    TEL_IAC, TEL_WONT,  TEL_ECHO,   /* Won't echo */
    TEL_IAC, TEL_WONT,  TEL_AUTH    /* Won't authenticate */
};

/* Function pointers: upcalls from TCP/IP stack */
extern NODE *(*get_locnode_n)(int n);                   /* Get local node */
extern int (*server_upcall)(TSOCK *ts, CONN_STATE conn);/* TCP server action */
extern int (*client_upcall)(TSOCK *ts, CONN_STATE conn);/* TCP client action */

/* Prototypes */
WORD read_netconfig(char *fname, NODE *np);
NODE *locnode_n(int n);
int server_action(TSOCK *ts, CONN_STATE conn);
int client_action(TSOCK *ts, CONN_STATE conn);
void do_teldisp(TSOCK *ts);
void do_receive(GENFRAME *gfp);
void do_poll(GENFRAME *gfp);
WORD str2service(char *str);
void disp_usage(void);
void break_handler(int sig);

int main(int argc, char *argv[])
{
    int args=0, err=0, client=0, fail, n;
    char *p, temps[18], k=0, c;
    WORD dtype;
    GENFRAME *gfp;
    LWORD mstimer;
    TSOCK *ts;
    NODE rem;

    printf("TELNET v" VERSION "\n");            /* Sign on */
    get_locnode_n = locnode_n;                  /* Set upcall ptrs to funcs */
    server_upcall = server_action;
    client_upcall = client_action;
    signal(SIGINT, break_handler);              /* Trap ctrl-C */
    ts = &tsocks[0];                            /* Pointer to client socket */
    memset(&rem, 0, sizeof(rem));               /* Preset remote port number */
    rem.port = DEFPORT;
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
            case 'S':                           /* -S: display TCP states */
                statedebug = 1;
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
        else if (client==0 && isdigit(c))       /* If client mode.. */
        {
            rem.ip = atoip(argv[args]);         /* Get destination IP address */
            client++;
        }                                       /* ..then port num/name */
        else if (client==1 && (rem.port=str2service(argv[args]))!=0)
            client++;
        else if (client == 2)
        {
            clientcmd = argv[args];             /* ..then command string */
            client++;
        }

    }
    if (err || rem.port==0)                     /* Prompt user if error */
        disp_usage();
    else if (!(dtype=read_netconfig(cfgfile, &locnode)))
        printf("Invalid configuration '%s'\n", cfgfile);
    else
    {
        rem.dtype = genframe.g.dtype = dtype;   /* Set frame driver type */
        gfp = &genframe;                        /* Get pointer to frame */
        printf("IP %s", ipstr(locnode.ip, temps));
        printf(" mask %s", ipstr(locnode.mask, temps));
        if (locnode.gate)
            printf(" gate %s", ipstr(locnode.gate, temps));
        if (dtype & DTYPE_ETHER)
            printf(" Ethernet %s", ethstr(locnode.mac, temps));
        if (client && !on_subnet(rem.ip, &locnode) && !locnode.gate)
            printf("\nWARNING: no gateway specified!");
        printf("\nPress ESC or ctrl-C to exit\n\n");
        if (client)                             /* If client, open socket */
            open_tcp(ts, gfp, &locnode, &rem);
        while (!breakflag && k!=0x1b)           /* Main loop.. */
        {
            do_receive(gfp);                    /* Receive frames */
            do_poll(gfp);                       /* Poll net drivers */
            if (client)
            {
                do_teldisp(ts);                 /* Telnet client display */
                if (!ts->state)                 /* If closed */
                    breakflag = 1;              /* ..exit from main loop */
                else if (k)                     /* If key pressed.. */
                    buff_in(&ts->txb, (BYTE *)&k, 1);
                k = 0;                          /* ..send it */
            }
            if (kbhit())                        /* Get keypress */
                k = getch();
        }                                       /* Close connection */
        if (k)
            printf("Closing...\n");
        mstimeout(&mstimer, 0);                 /* Refresh timer */
        do
        {                                       /* Loop until conns closed */
            for (n=fail=0; n<NSOCKS; n++)
                fail += !close_tcp(&tsocks[n]);
            do_receive(gfp);
            do_poll(gfp);                       /* ..or timeout */
        } while (!mstimeout(&mstimer, 1000) && fail);
        if (fail)                               /* If still open.. */
        {
            printf("Resetting connections\n");
            for (n=0; n<NSOCKS; n++)            /* ..send reset */
                reset_tcp(&tsocks[n], gfp);
            while (!mstimeout(&mstimer, 1000))
            {                                   /* ..and wait until sent */
                do_receive(gfp);
                do_poll(gfp);
            }
        }
        close_net(dtype);                       /* Shut down net driver */
    }
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

/* Upcall from TCP stack to server when opening, connecting, receiving data
** or closing. Return 0 to prevent connection opening, or close if connected */
int server_action(TSOCK *ts, CONN_STATE conn)
{
    int ok=1;
    WORD port, len;
    BYTE temps[30];
    time_t t;

    port = ts->loc.port;                    /* Connection being opened */
    if (conn == TCP_OPEN)
    {
        ok = port==ECHOPORT || port==DAYPORT || port==HTTPORT;
        if (port == DAYPORT)                    /* Daytime server? */
        {
            time(&t);                           /* ..send date & time string */
            buff_in(&ts->txb, (BYTE *)ctime(&t), 24);
            buff_in(&ts->txb, (BYTE *)"\r\n", 2);
        }
        else if (port == ECHOPORT)              /* Echo server? */
            ts->connflags = TPUSH;              /* ..use PUSH flag */
        else if (port == HTTPORT)               /* HTTP server ? */
        {                                       /* ..load my page */
            buff_in(&ts->txb, (BYTE *)MYPAGE, sizeof(MYPAGE));
        }
    }
    else if (conn == TCP_CONN)              /* Connected */
    {
        if (port==DAYPORT || port==HTTPORT)     /* If daytime or HTTP */
            ok = 0;                             /* ..close straight away */
    }
    else if (conn == TCP_DATA)              /* Received data */
    {
        if (port == ECHOPORT)                   /* If Echo */
        {                                       /* ..echo it back! */
            while ((len=buff_out(&ts->rxb, temps, sizeof(temps)))!=0)
                buff_in(&ts->txb, temps, len);
        }
    }
    return(ok);
}

/* Upcall from TCP stack to client when opening, connecting, receiving data
** or closing. Return 0 to close if connected */
int client_action(TSOCK *ts, CONN_STATE conn)
{
    if (conn == TCP_OPEN)
    {
        if (ts->rem.port == TELPORT)        /* If login, send Telnet opts */
            buff_in(&ts->txb, telopts, sizeof(telopts));
        if (clientcmd)
        {
            buff_instr(&ts->txb, clientcmd);/* Send command-line string */
            buff_instr(&ts->txb, "\r\n");
        }
    }
    return(1);
}

/* Telnet client display */
void do_teldisp(TSOCK *ts)
{
    static BYTE d[3]={0,0,0};

    while (buff_dlen(&ts->rxb))
    {
        if (d[0] == TEL_IAC)
        {
            if (!d[1] && buff_out(&ts->rxb, &d[1], 1))
            {
                if (d[1] == TEL_IAC)
                    d[0] = d[1] = 0;
                else if (!d[2] && buff_out(&ts->rxb, &d[2], 1))
                {
                    d[1] = TEL_WONT;
                    buff_in(&ts->txb, d, 3);
                    d[0] = d[1] = d[2] = 0;
                }

            }
        }
        else while (buff_out(&ts->rxb, d, 1) && d[0]!=TEL_IAC)
            putchar(d[0]);
    }
}

/* Check for incoming packets, send response if required */
void do_receive(GENFRAME *gfp)
{
    NODE node;
    ARPKT *arp;
    IPKT *ip;
    ICMPKT *icmp;
    int rxlen, txlen, len;

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
                arp_receive(tsocks, NSOCKS, gfp);
            }
        }
        else if ((rxlen=is_ip(gfp, rxlen))!=0 &&    /* IP datagram? */
                 ip->i.dip==locnode.ip || ip->i.dip==BCASTIP)
        {
            getip_srce(gfp, &node);
            if ((len=is_icmp(ip, rxlen))!=0)        /* ICMP? */
            {
                icmp = (ICMPKT *)ip;
                if (icmp->c.type==ICREQ)            /* Echo request? */
                {
                    len = (WORD)maxi(len, 0);       /* Make response */
                    txlen = make_icmp(gfp, &locnode, &node, ICREP,
                                      icmp->c.code, (WORD)len);
                    put_frame(gfp, txlen);          /* Send packet */
                }
            }
            else if ((len=is_tcp(ip, rxlen))!=0)    /* TCP? */
            {
                tcp_receive(tsocks, NSOCKS, gfp, len);
            }
        }
    }
}

/* Poll the network interface to keep it alive */
void do_poll(GENFRAME *gfp)
{
    tcp_poll(tsocks, NSOCKS, gfp);
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
    else if (!stricmp(str, "http"))
        port = HTTPORT;
    else
        printf("Unrecognised port '%s'\n", str);
    return(port);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    TELNET [ options ] [ IP_addr [ port [ command ]]]\n");
    printf("Options:  -c name   Config filename (default %s)\n", CFGFILE);
    printf("          -s        State display\n");
    printf("          -t        TCP segment display\n");
    printf("          -v        Verbose packet display\n");
    printf("          -x        Hex packet display\n");
    printf("Example:  TELNET 10.1.1.1 http \"GET /index.html\"\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */

