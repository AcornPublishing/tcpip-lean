/* Web Server with Embedded Gateway Interface for 'TCP/IP Lean'
  (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 17/4/00  Derived from webserve.c v0.06
** v0.02 JPB 19/4/00  Added EGI variables
** v0.03 JPB 19/4/00  Improved URL parsing
** v0.04 JPB 20/4/00  Added URL decoding
** v0.05 JPB 24/4/00  Added LEDs & switches, and sliders
** v0.06 JPB 3/7/00   Changed default config file to TCPLEAN.CFG
**                    Revised header for book CD
**                    Added default HTML filename
*/

#define VERSION "0.06"

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
#include "web_egi.h"
#include "egi_form.h"
#include "egi_swit.h"
#include "egi_slid.h"

#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define CFGEXT      ".cfg"          /* Default config extension */
#define MAXNETCFG   40              /* Max length of a net config string */
#define FILEDIR     ".\\webdocs\\"  /* Default directory */
#define DEFAULTFILE "index.htm"     /* Default HTML file (if none requested) */

GENFRAME genframe;                  /* Frame for network Tx/Rx */
char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
char netcfg[MAXNETCFG+1]="??";      /* Network config string */
extern BYTE bcast[MACLEN];          /* Broadcast Ethernet addr */
NODE  locnode;                      /* My Ethernet and IP addresses */
int breakflag;                      /* Flag to indicate ctrl-break pressed */
char filedir[MAXPATH+1]=FILEDIR;    /* Directory for files */
char httpreq[HTTP_MAXLEN+1];        /* Buffer for first part of HTTP request */
char egibuff[EGI_BUFFLEN+1];

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

APPDATA appdata[NSOCKS];

/* EGI function definitions */
EGI_FUNC egifuncs[] = {EGI_FORM_FUNCS, EGI_SWIT_FUNCS, EGI_SLIDER_FUNCS, {0}};

/* Function pointers: upcalls from TCP/IP stack */
extern NODE *(*get_locnode_n)(int n);                   /* Get local node */
extern int (*server_upcall)(TSOCK *ts, CONN_STATE conn);/* TCP server action */

/* Prototypes */
WORD read_netconfig(char *fname, NODE *np);
NODE *locnode_n(int n);
int server_action(TSOCK *ts, CONN_STATE conn);
void http_get(TSOCK *ts, char *fname);
void http_data(TSOCK *ts);
void do_receive(GENFRAME *gfp);
void do_poll(GENFRAME *gfp);
int egi_execstr(TSOCK *ts, char *str);
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
        memset(adp, 0, sizeof(APPDATA));
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
                name = strtok(0, " \r\n");          /* 2nd token is filename */
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
    if (strstr(fname, EGI_EXT))
    {
        url_connvars(ts, fname);
        if (!egi_execstr(ts, s=get_connvar(ts, "fname")))
        {
            buff_instr(&ts->txb, HTTP_NOFILE HTTP_TXT HTTP_BLANK);
            buff_inprintf(&ts->txb, "Can't find EGI function '%s'\r\n", s);
            close_tcp(ts);
        }
    }                                   /* If file not found.. */
    else if ((adp->in = url_fopen(fname)) == 0)
    {                                   /* ..send message */
        if (webdebug)                   /* Display if debugging */
            printf("File '%s' not found\n", fname);
        buff_instr(&ts->txb, HTTP_NOFILE HTTP_TXT HTTP_BLANK);
        buff_inprintf(&ts->txb, "Can't find '%s'\r\n", fname);
        close_tcp(ts);
    }
    else                                /* File found OK */
    {
        strlwr(fname);
        buff_instr(&ts->txb, HTTP_OK);
        s = strstr(fname, ".htm") ? HTTP_HTM :
            strstr(fname, EGI_EXT)? HTTP_HTM :
            strstr(fname, ".txt") ? HTTP_TXT :
            strstr(fname, ".gif") ? HTTP_GIF :
            strstr(fname, ".xbm") ? HTTP_XBM : "";
        buff_instr(&ts->txb, s);
        buff_instr(&ts->txb, HTTP_BLANK);
        if (webdebug)                   /* Display if debugging */
            printf("File '%s' %s", fname, *s ? s : "\n");
    }
}

/* If there is space in the transmit buffer, send HTTP data */
void http_data(TSOCK *ts)
{
    APPDATA *adp;
    int len, n;
    char *start, *end;

    adp = (APPDATA *)ts->app;
    if (adp->in && (len = buff_freelen(&ts->txb)) >= EGI_BUFFLEN)
    {
        if (adp->egi)                   /* If EGI is active.. */
        {
            len = fread(egibuff, 1, EGI_BUFFLEN, adp->in);
            egibuff[len] = 0;
            if (len <= 0)
            {                           /* If end of file, close it.. */
                fclose(adp->in);
                adp->in = 0;
                close_tcp(ts);          /* ..and start closing connection */
            }
            else
            {                           /* Check for start of EGI tag */
                if ((start = strstr(egibuff, EGI_STARTS)) == 0)
                    start = strchr(&egibuff[len-EGI_STARTLEN], '<');
                if (start==egibuff && (end=strstr(egibuff, EGI_ENDS))!=0)
                {                       /* If tag is at start of buffer.. */
                    n = (int)(end - start) + sizeof(EGI_ENDS) - 1;
                    fseek(adp->in, n-len, SEEK_CUR);
                    egibuff[n] = 0;     /* ..call handler */
                    adp->egi(ts, egibuff);
                    len = 0;
                }                       /* If tag not at start of buffer.. */
                else if (start)
                {                       /* ..send file up to tag */
                    n = (int)(start - egibuff);
                    fseek(adp->in, n-len, SEEK_CUR);
                    len = n;
                }
                if (len > 0)            /* Send next chunk of file */
                    buff_in(&ts->txb, (BYTE *)egibuff, (WORD)len);
            }
        }
        else if (buff_infile(&ts->txb, adp->in, (WORD)len) == 0)
        {                               /* If end of file, close it.. */
            fclose(adp->in);
            adp->in = 0;
            close_tcp(ts);              /* ..and start closing connection */
        }
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

/* Add a variable to the connection variable space, return 0 if no room */
int put_connvar(TSOCK *ts, char *name, char *val)
{
    return(put_connvarlen(ts, name, strlen(name), val, strlen(val)));
}

/* Add a variable to the connection variable space, return 0 if no room
** String aren't necessarily null-terminated; they are pointers & lengths */
int put_connvarlen(TSOCK *ts, char *name, int namlen, char *val, int valen)
{
    int ok=0;
    APPDATA *adp;

    adp = (APPDATA *)ts->app;
    if (adp->vlen+namlen+valen+3<VARSPACE &&
        namlen<MAX_EGINAME && valen<MAX_EGIVAL)
    {
        adp->vars[adp->vlen++] = (char)(namlen | 0x80);
        strncpy(&adp->vars[adp->vlen], name, namlen);
        adp->vlen += namlen;
        adp->vars[adp->vlen++] = 0;
        strncpy(&adp->vars[adp->vlen], val, valen);
        adp->vlen += valen;
        adp->vars[adp->vlen++] = 0;
    }
    return(ok);
}

/* Get variable from the connection space, return null string if not found */
char *get_connvar(TSOCK *ts, char *name)
{
    int n, len;
    APPDATA *adp;
    char *s=0, *end;

    adp = (APPDATA *)ts->app;
    end = &adp->vars[adp->vlen];
    n = strlen(name);
    if (n < MAX_EGINAME)
    {
        s = memchr(adp->vars, (char)(n | 0x80), adp->vlen-3);
        while (s && strncmp(s+1, name, n) && (len=(int)(end-s))>3)
            s = memchr(s+1, (char)(n | 0x80), len);
    }
    return(s ? s+n+2 : "");
}

/* Find variable in the connection space, by matching first few chars
** Return full name string, null string if not found */
char *find_connvar(TSOCK *ts, char *name)
{
    int n;
    APPDATA *adp;
    char *s=0, *end;

    adp = (APPDATA *)ts->app;
    end = &adp->vars[adp->vlen];
    n = strlen(name);
    if (n < MAX_EGINAME)
    {
        s = adp->vars;
        while (*s && strncmp(s+1, name, n) && (int)(end-s)>2)
        {
            do {
                s++;
            } while (s<end-2 && !(*s & 0x80));
        }
    }
    return(*s & 0x80 ? s+1 : "");
}

/* Display all vars in the connection variable space, for debugging */
void disp_connvars(TSOCK *ts)
{
    int len=0, n;
    APPDATA *adp;

    adp = (APPDATA *)ts->app;
    while (len < adp->vlen)
    {
        n = (int)adp->vars[len++] & 0x7f;
        printf("Var %s", &adp->vars[len]);
        len += n + 1;
        printf("=%s\n", &adp->vars[len]);
        len += strlen(&adp->vars[len]) + 1;
    }
}

/* Get connection variable values from URL */
void url_connvars(TSOCK *ts, char *str)
{
    int nlen, vlen, n;

    if (*str == '/')
        str++;
    vlen = strcspn(str, " .?");
    put_connvarlen(ts, "fname", 5, str, vlen);
    str += vlen;
    if (*str == '.')
    {
        str++;
        vlen = strcspn(str, " ?");
        put_connvarlen(ts, "fext", 4, str, vlen);
        str += vlen;
    }
    if (*str++ == '?')
    {
        while ((nlen=strcspn(str, "="))!=0 && (vlen=strcspn(str+nlen, "&"))!=0)
        {
            n = url_decode(str+nlen+1, vlen-1);
            put_connvarlen(ts, str, nlen, str+nlen+1, n);
            str += nlen + vlen;
            if (*str == '&')
                str++;
        }
    }
}

/* Decode a URL-encoded string (with length), return new length */
int url_decode(char *str, int len)
{
    int n=0, d=0;
    char c, c2, *dest;

    dest = str;
    while (n < len)
    {
        if ((c = str[n++]) == '+')
            c = ' ';
        else if (c=='%' && n+2<=len &&
                 isxdigit(c=str[n++]) && isxdigit(c2=str[n++]))
        {
            c = c<='9' ? (c-'0')*16 : (toupper(c)-'A'+10)*16;
            c += c2<='9' ? c2-'0' : toupper(c2)-'A'+10;
        }
        dest[d++] = c;
    }
    return(d);
}

/* Open a file, given the URL filename. Return file handle, 0 if error */
FILE *url_fopen(char *str)
{
    char fpath[MAXPATH+1];
    int n;

    if (*str == '/')                        /* Strip leading '/' */
        str++;
    strcpy(fpath, filedir);                 /* Copy base directory */
    if (*str <= ' ')                        /* No filename left? */
    {
        strcat(fpath, DEFAULTFILE);         /* Use default if none */
    }
    else
    {
        n = strlen(fpath);                  /* Copy requested filename */
        while (n<MAXPATH && *str>' ' && *str!='?')
            fpath[n++] = *str++;
        fpath[n] = 0;
    }
    return(fopen(fpath, "rb"));             /* Open file */
}

/* Execute the EGI function corresponding to a string, return 0 if not found */
int egi_execstr(TSOCK *ts, char *str)
{
    int ok=0, n=0;

    while (egifuncs[n].func && !(ok=!stricmp(str, egifuncs[n].name)))
        n++;
    if (ok)
        egifuncs[n].func(ts, 0);
    return(ok);
}

/* Display usage help */
void disp_usage(void)
{
    printf("Usage:    WEB_EGI [ options ] [ directory ]\n");
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

