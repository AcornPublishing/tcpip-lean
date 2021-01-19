/* Web Server for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 20/3/00  Derived from Telnet v0.12
** v0.02 JPB 23/3/00  Used function upcall ptrs in TCP.C and IP.C
** v0.03 JPB 11/4/00  Added HTTP header type selection
** v0.04 JPB 12/4/00  Added clickable HTML directory
** v0.05 JPB 13/4/00  Added state machine to handle large directories
** v0.06 JPB 15/4/00  Return directory only if null filename
**                    Return error response if file not found
** v0.07 JPB 3/7/00   Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
*/

#define VERSION "0.07"

/* HTTP response header: 0=none, 1=simple OK, 2=content-type */
#define HTTP_HEAD 2
/* Directory type: 0=text, 1=simple HTML, 2=table, 3=large dir support */
#define HTML_DIR 3

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

#include "ether.h"
#include "netutil.h"
#include "net.h"
#include "ip.h"
#include "tcp.h"

#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */
#define FILEDIR     ".\\webdocs\\"  /* Default directory */

/* HTTP and HTML text */
#define HTTP_OK     "HTTP/1.0 200 OK\r\n"
#define HTTP_NOFILE "HTTP/1.0 404 Not found\r\n"
#define HTTP_HTM    "Content-type: text/html\r\n"
#define HTTP_TXT    "Content-type: text/plain\r\n"
#define HTTP_GIF    "Content-type: image/gif\r\n"
#define HTTP_XBM    "Content-type: image/x-xbitmap\r\n"
#define HTTP_BLANK  "\r\n"
#define HTTP_MAXLEN 80

GENFRAME genframe;                  /* Frame for network Tx/Rx */
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast Ethernet addr */
NODE  locnode;                      /* My Ethernet and IP addresses */
int breakflag;                      /* Flag to indicate ctrl-break pressed */
char filedir[MAXPATH+1]=FILEDIR;    /* Directory for files */
char filepath[MAXPATH+1];           /* Filename */
char httpreq[HTTP_MAXLEN+1];        /* Buffer for first part of HTTP request */

/* Debug flags */
extern int netdebug;                /* Verbose packet display */
extern int tcpdebug;                /* TCP frame display */
extern int statedebug;              /* TCP state display */
int webdebug;                       /* Web (HTTP) diagnostic display */

/* Socket storage */
#define NSOCKS 2
TSOCK tsocks[NSOCKS] =              /* Initialise index num and buffer sizes */
{
    {1,{_CBUFFLEN_},{_CBUFFLEN_}},
    {2,{_CBUFFLEN_},{_CBUFFLEN_}}
};

/* Application-specific storage */
typedef struct {
    FILE *in;                       /* File I/P pointer */
    long count;                     /* State variable/counter */
} APPDATA;
APPDATA appdata[NSOCKS];

/* Function pointers: upcalls from TCP/IP stack */
extern NODE *(*get_locnode_n)(int n);                   /* Get local node */
extern int (*server_upcall)(TSOCK *ts, CONN_STATE conn);/* TCP server action */

/* Prototypes */
WORD read_netconfig(char *fname, NODE *np);
NODE *locnode_n(int n);
int server_action(TSOCK *ts, CONN_STATE conn);
void http_get(TSOCK *ts, char *fname);
void http_data(TSOCK *ts);
int dir_head(CBUFF *bp, char *path);
int dir_entry(CBUFF *bp, char *name);
int dir_tail(CBUFF *bp);
void do_receive(GENFRAME *gfp);
void do_poll(GENFRAME *gfp);
int buff_inprintf(CBUFF *bp, char *str, ...);
void disp_usage(void);
void break_handler(int sig);

int main(int argc, char *argv[])
{
    int args=0, err=0, fail, n;
    char *p, temps[18], k=0, c;
    WORD dtype;
    GENFRAME *gfp;
    LWORD mstimer;

    printf("WEBSERVE v" VERSION "\n");          /* Sign on */
    get_locnode_n = locnode_n;                  /* Set upcall func ptrs */
    server_upcall = server_action;
    signal(SIGINT, break_handler);              /* Trap ctrl-C */
    for (n=0; n<NSOCKS; n++)
        tsocks[n].app = &appdata[n];
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
            case 'W':                           /* -W: Web (HTTP) diagnostics */
                webdebug = 1;
                break;
            case 'X':                           /* -X: hex packet display */
                netdebug |= 2;
                break;
            default:
                err = 1;
            }
        }
        else
        {
            *filedir = 0;                       /* File directory */
            if (argv[args][0]!='\\' && argv[args][1]!=':' && argv[args][0]!='.')
                strcpy(filedir, ".\\");
            strcat(filedir, argv[args]);
            if (filedir[strlen(filedir)-1] != '\\')
                strcat(filedir, "\\");
        }
    }
    if (err)                                    /* Prompt user if error */
        disp_usage();
    else if (!(dtype=read_netconfig(cfgfile, &locnode)))
        printf("Invalid configuration '%s'\n", cfgfile);
    else
    {
        genframe.g.dtype = dtype;               /* Set frame driver type */
        gfp = &genframe;                        /* Get pointer to frame */
        printf("IP %s", ipstr(locnode.ip, temps));
        printf(" mask %s", ipstr(locnode.mask, temps));
        if (locnode.gate)
            printf(" gate %s", ipstr(locnode.gate, temps));
        if (dtype & DTYPE_ETHER)
            printf(" Ethernet %s", ethstr(locnode.mac, temps));
        printf("\nPress ESC or ctrl-C to exit\n\n");
        while (!breakflag && k!=0x1b)           /* Main loop.. */
        {
            do_receive(gfp);                    /* Receive frames */
            do_poll(gfp);                       /* Poll net drivers */
            if (kbhit())                        /* Get keypress */
                k = getch();
        }
        if (k)
            printf("Closing...\n");
        mstimeout(&mstimer, 0);
        do
        {
            for (n=fail=0; n<NSOCKS; n++)
                fail += !close_tcp(&tsocks[n]);
            do_receive(gfp);
            do_poll(gfp);
        } while (!mstimeout(&mstimer, 1000) && fail);
        if (fail)
        {
            printf("Resetting connections\n");
            for (n=0; n<NSOCKS; n++)
                reset_tcp(&tsocks[n], gfp);
            while (!mstimeout(&mstimer, 1000))
            {
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
    int ok=1, len;
    WORD port;
    char *s, *name;
    APPDATA *adp;

    port = ts->loc.port;
    adp = (APPDATA *)ts->app;
    if (port != HTTPORT)
        ok = 0;
    else if (conn == TCP_OPEN)
    {
        adp->in = 0;
    }
    else if (conn == TCP_DATA)
    {
        if ((len = buff_chrlen(&ts->rxb, '\n'))!=0) /* Got request? */
        {
            len = mini(len+1, HTTP_MAXLEN);         /* Truncate length */
            buff_out(&ts->rxb, (BYTE *)httpreq, (WORD)len);
            httpreq[len] = 0;
            if (webdebug)                           /* Display if debugging */
                printf("%s", httpreq);
            s = strtok(httpreq, " ");               /* Chop into tokens */
            if (!strcmp(s, "GET"))                  /* 1st token is 'GET'? */
            {
                name = strtok(0, " ?\r\n");         /* 2nd token is filename */
                http_get(ts, name);                 /* Process filename */
            }
        }
    }
    http_data(ts);
    return(ok);
}

/* Process the filepath from an HTTP 'get' method */
void http_get(TSOCK *ts, char *fname)
{
    APPDATA *adp;
    char *s=0;

    adp = (APPDATA *)ts->app;
    strcpy(filepath, filedir);          /* Add on base directory */
    if (*fname)                         /* Copy filename without leading '/' */
        strcat(filepath, fname+1);      
    strlwr(filepath);                   /* Force to lower-case */
    if (strlen(fname) <= 1)
    {                                   /* If name is only a '/'.. */
        if (webdebug)                   /* Display if debugging */
            printf("Sending directory\n", filepath);
        strcpy(filepath, filedir);
        strcat(filepath, "*.*");        /* ..display directory */
#if HTML_DIR == 3
        dir_head(&ts->txb, filedir);    /* Send out directory */
        adp->count = 1;
#elif HTML_DIR > 0
        dir_head(&ts->txb, filedir);    /* Send out directory */
        if ((s = find_first(filepath)) != 0) do {
            dir_entry(&ts->txb, s);
        } while ((s = find_next()) != 0);
        dir_tail(&ts->txb);
        close_tcp(ts);
#else
        buff_instr(&ts->txb, HTTP_OK HTTP_TXT HTTP_BLANK);
        buff_instr(&ts->txb, "Directory\r\n");
        if ((s = find_first(filepath)) != 0) do {
            buff_instr(&ts->txb, s);    /* One file per line */
            buff_instr(&ts->txb, "\r\n");
        } while ((s = find_next()) != 0);
        close_tcp(ts);
#endif
    }                                   /* If file not found.. */
    else if ((adp->in = fopen(filepath, "rb")) == 0)
    {                                   /* ..send message */
        if (webdebug)                   /* Display if debugging */
            printf("File '%s' not found\n", filepath);
        buff_instr(&ts->txb, HTTP_NOFILE HTTP_TXT HTTP_BLANK);
        buff_inprintf(&ts->txb, "Webserve v%s\r\n", VERSION);
        buff_inprintf(&ts->txb, "Can't find '%s'\r\n", filepath);
        close_tcp(ts);
    }
    else                                /* File found OK */
    {
#if HTTP_HEAD == 0
                                                    /* No HTTP header or.. */
#elif HTTP_HEAD == 1                                /* Simple OK header or.. */
        buff_instr(&ts->txb, HTTP_OK HTTP_BLANK);
#else                                               /* Content-type header */
        buff_instr(&ts->txb, HTTP_OK);
        s = strstr(filepath, ".htm") ? HTTP_HTM :
            strstr(filepath, ".txt") ? HTTP_TXT :
            strstr(filepath, ".gif") ? HTTP_GIF :
            strstr(filepath, ".xbm") ? HTTP_XBM : "";
        buff_instr(&ts->txb, s);
        buff_instr(&ts->txb, HTTP_BLANK);
#endif
        if (webdebug)                   /* Display if debugging */
            printf("File '%s' %s", filepath, *s ? s : "\n");
    }
}

/* If there is space in the transmit buffer, send HTTP data */
void http_data(TSOCK *ts)
{
    APPDATA *adp;
    int count=0, ok, len;
    char *s;

    adp = (APPDATA *)ts->app;
    if (adp->count)                     /* If sending a directory.. */
    {                                   /* Skip filenames already sent */
        ok = (s = find_first(filepath)) != 0;
        while (ok && ++count<adp->count)
            ok = (s = find_next()) != 0;
        while (ok && dir_entry(&ts->txb, s))
        {                               /* Send as many entries as will fit */
            adp->count++;
            ok = (s = find_next()) != 0;
        }
        if (!ok && dir_tail(&ts->txb))  /* If no more entries.. */
        {
            adp->count = 0;             /* ..start closing connection */
            close_tcp(ts);
        }
    }                                   /* If sending a file.. */
    if (adp->in && (len = buff_freelen(&ts->txb)) > 0)
    {                                   /* ..put out as much as possible */
        if ((len = buff_infile(&ts->txb, adp->in, (WORD)len)) == 0)
        {                               /* If end of file, close it.. */
            fclose(adp->in);
            adp->in = 0;
            close_tcp(ts);              /* ..and start closing connection */
        }
    }
}

/* Write out head of HTML file dir, given filepath. Return 0 if error */
int dir_head(CBUFF *bp, char *path)
{
#if HTML_DIR > 1
    return(buff_instr(bp, HTTP_OK HTTP_HTM HTTP_BLANK) &&
           buff_instr(bp, "<html><head><title>Directory</title></head>\r\n") &&
           buff_inprintf(bp, "<body><h2>Directory of %s</h2>\r\n", path) &&
           buff_instr(bp, "<table><th>File</th><th>Size</th>\r\n"));
#else
    return(buff_instr(bp, HTTP_OK HTTP_HTM HTTP_BLANK) &&
           buff_instr(bp, "<html><head><title>Directory</title></head>\r\n") &&
           buff_inprintf(bp, "<body><h2>Directory of %s</h2>\r\n", path));
#endif
}

/* Write out HTML file dir entry, given name. Return 0 if buffer full */
int dir_entry(CBUFF *bp, char *name)
{
#if HTML_DIR > 1
    return(buff_inprintf(bp, "<tr><td><a href='%s'>%s</a></td>", name, name) &&
           buff_inprintf(bp, "<td>%lu</td></tr>\r\n", find_filesize()));
#else
    return(buff_inprintf(bp, "<a href='%s'>%s</a><br>\r\n", name, name));
#endif
}

/* Write out tail of HTML file dir. Return length, 0 if error */
int dir_tail(CBUFF *bp)
{
#if HTML_DIR > 1
    return(buff_instr(bp, "</table></body></html>\r\n"));
#else
    return(buff_instr(bp, "</body></html>\r\n"));
#endif
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

/* Version of printf() to write a string into a circular buffer.
** Return string length, or zero if insufficient room in buffer */
int buff_inprintf(CBUFF *bp, char *str, ...)
{
    char temps[200];
    int len;

    va_list argptr;
    va_start(argptr, str);
    len = vsprintf(temps, str, argptr);
    va_end(argptr);
    if (len<=0 || len>buff_freelen(bp))
        len = 0;
    else
        buff_in(bp, (BYTE *)temps, (WORD)len);
    return(len);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    WEBSERVE [ options ] [ directory ]\n");
    printf("          If no directory specified, default is '%s'\n", FILEDIR);
    printf("Options:  -c name   Config filename (default %s)\n", CFGFILE);
    printf("          -s        State display\n");
    printf("          -t        TCP segment display\n");
    printf("          -v        Verbose packet display\n");
    printf("          -w        Web (HTTP) diagnostics\n");
    printf("          -x        Hex packet display\n");
    printf("Example:  WEBSERVE c:\\mydocs\n");
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */

