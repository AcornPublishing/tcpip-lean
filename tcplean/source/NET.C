/* Network interface functions for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 19/1/00  Network code split off from TCPFS.C v0.08
** v0.02 JPB 20/1/00  Added support for multiple net drivers
** v0.03 JPB 20/1/00  Improved packet demultiplexing
** v0.04 JPB 28/1/00  Changed 'dispnet' to 'netdebug'
** v0.05 JPB 29/2/00  Fixed bug in non-Ethernet logging
** v0.06 JPB 20/3/00  Added LiteLink and modem support
** v0.07 JPB 23/3/00  Added support for multiple SLIP links
** v0.08 JPB 24/3/00  Fixed multi-channel SLIP interference 
** v0.09 JPB 6/4/00   Fixed type mismatch in SLIP length
** v0.10 JPB 3/7/00   Revised header for book CD
*/

/* Debug option: reverse the order in which IP fragments are sent */
#define SUBFIRST 0  /* Set non-zero to transmit subframes first */
/* Debug options to drop frames on transmit or receive */
#define TXDROP  0   /* Set non-zero to drop 1-in-N transmit frames */
#define RXDROP  0   /* Set non-zero to drop 1-in-N receive frames */

/* All drivers are supported under Borland C
** Packet driver isn't supported under Visual C
** Packet and SLIP drivers aren't supported under DJGPP */
#ifdef __BORLANDC__
#define PKTD_SUPPORT  1
#else
#define PKTD_SUPPORT  0
#endif
#ifdef DJGPP
#define SLIP_SUPPORT 0
#else
#define SLIP_SUPPORT 1
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Override the default circular buffer size used in TCPFS.H */
#define _CBUFFLEN_ 0x2000           /* Buffer size: MUST be power of 2 */

#include "netutil.h"
#include "net.h"
#include "ether.h"
#include "serpc.h"

#if PKTD_SUPPORT
#include "pktd.h"
#else                               /* Dummy functions if PKTD not installed */
#define open_pktd(x)        no_driver("packet")
#define close_pktd(x)       no_driver("packet")
#define put_pktd(x, y, z)   no_driver("packet")
#endif

#if !SLIP_SUPPORT                   /* Dummy functions if SLIP not installed */
#define open_slip(x, y)     no_driver("SLIP")
#define close_slip(x)       no_driver("SLIP")
#define put_slip(x, y, z)   no_driver("SLIP")
#define receive_slip(x)     no_driver("SLIP")
#endif

#define SLIP_END 0xc0               /* SLIP escape codes */
#define SLIP_ESC 0xdb
#define ESC_END  0xdc
#define ESC_ESC  0xdd

CBUFF rxpkts = {_CBUFFLEN_};        /* Rx and Tx circular packet buffers */
CBUFF txpkts = {_CBUFFLEN_};

BYTE slipbuffs[MAXCOMMS][MAXSLIP];  /* SLIP buffers */
WORD sliplens[MAXCOMMS];            /* ..and data lengths */
int netdebug;                       /* Flag to enable net packet display */
FILE *logfile;                      /* Handle for logfile */
int ndrivers;                       /* Number of initialised drivers */
int txfcount, rxfcount;             /* Counts for 'frame drop' options */

/* Private prototypes */
WORD no_driver(char *s);
void do_modem(char c);

/* Get a frame from the network, return length excl. hardware header */
int get_frame(GENFRAME *gfp)
{
    int rxlen;

    if ((rxlen=get_net(gfp)) > 0)           /* If any packet received.. */
    {
        if (is_ether(gfp, rxlen))           /* If Ethernet.. */
            rxlen -= sizeof(ETHERHDR);      /* Subtract header length */
        else if (!is_slip(gfp, rxlen))      /* If not SLIP.. */
            rxlen = 0;                      /* Discard if unrecognised */
    }
    return(rxlen);
}

/* Put frame out onto the network; if sub-frame (fragment), send it as well */
int put_frame(GENFRAME *gfp, int len)
{
    int ret=0, len1, len2;
    GENFRAME *sfp;

    len1 = gfp->g.fragoff ? gfp->g.fragoff : len;   /* Get len of 2 fragments */
    len2 = len - len1;
    sfp = (GENFRAME *)&gfp->buff[gfp->g.fragoff];   /* ..and ptr to 2nd frag */
#if SUBFIRST
    if (len2 > 0)                           /* Send sub-frame first.. */
        ret = put_net(sfp, (WORD)len2);
    if (len1 > 0)                           /* ..then main frame */
        ret += put_net(gfp, (WORD)len1);
#else                                       /* Or send main frame first */
    if (len1 > 0)
        ret = put_net(gfp, (WORD)len1);
    if (len2 > 0)
        ret += put_net(sfp, (WORD)len2);
#endif
    return(ret);
}

/* Open network driver
** Return driver type, which is the driver config number (1st driver is 0)
** plus flags to indicate frame type (Ethernet, SLIP etc.). Return 0 if error */
WORD open_net(char *cfgstr)
{
    WORD dtype=0, etype, stype;
    int append;
    char *s;

    etype = ndrivers + DTYPE_ETHER;                 /* Ethernet driver type */
    stype = ndrivers + DTYPE_SLIP;                  /* ..and SLIP driver type */
    if (ndrivers >= MAXNETS)
        printf("ERROR: exceeded maximum number of network interfaces\n");
    else if ((s = skiptoken(cfgstr, "ether")) != 0) /* Ethernet? */
    {
        cfgstr = skippunct(s);
        if ((s = skiptoken(cfgstr, "snap")) != 0)   /* ..with 802.3 SNAP hdr? */
        {
            etype |= DTYPE_SNAP;
            cfgstr = skippunct(s);
        }                                           /* Direct hardware drive? */
        if ((s = skiptoken(cfgstr, "ne")) != 0)     /* ..NE2000-compatible */
            dtype = open_etherne(s, (WORD)(etype|DTYPE_NE));
        else if ((s = skiptoken(cfgstr, "3c")) != 0)/* ..3COM 3C509 */
            dtype = open_ether3c(s, (WORD)(etype|DTYPE_3C));
        else if ((s = skiptoken(cfgstr, "pktd")) != 0)
            dtype = open_pktd(etype | DTYPE_PKTD);  /* Ether Packet Driver? */
    }
    else if ((s = skiptoken(cfgstr, "slip")) != 0)  /* SLIP interface? */
    {
        cfgstr = skippunct(s);
        if ((s = skiptoken(cfgstr, "pc")) != 0)     /* PC ser (direct/Win32) */
            dtype = open_slip(s, stype);
        else if ((s = skiptoken(cfgstr, "pktd")) != 0)
            dtype = open_pktd(stype | DTYPE_PKTD);  /* Packet driver SLIP */
    }
    if (dtype && (s=strchr(s, '>'))!=0)             /* Log net Tx/Tx to file? */
    {
        if ((append = (*++s == '>'))!=0)            /* If '>>' append to file */
            s++;
        s = skipspace(s);                           /* Open logfile */
        printf("%s logfile '%s'\n", append ? "Appending to" : "Creating",  s);
        if ((logfile=fopen(s, append ? "ab" : "wb"))==0)
            printf("ERROR: can't open logfile\n");
    }
    if (dtype)                                      /* Bump up driver count */
        ndrivers++;
    return(dtype);
}

/* Close network driver */
void close_net(WORD dtype)
{
    if (dtype & DTYPE_NE)                           /* Close NE2000-compat */
        close_etherne(dtype);
    else if (dtype & DTYPE_3C)                      /* ..or 3C509 */
        close_ether3c(dtype);
    else if (dtype == DTYPE_SLIP)                   /* ..or SLIP */
        close_slip(dtype);
    else if (dtype & DTYPE_PKTD)                    /* ..or packet driver */
        close_pktd(dtype);
    if (logfile)                                    /* Close logfile */
    {
        printf("Closing logfile\n");
        if (ferror(logfile))
            printf("\nERROR writing logfile\n");
        fclose(logfile);
        logfile = 0;
    }
}

/* Dummy initialisation function for unsupported drivers */
WORD no_driver(char *s)
{
    printf("ERROR: %s driver not included in this version\n", s);
    return(0);
}

/* Return pointer to my Ethernet addr, given driver type */
BYTE *ether_addr(WORD dtype)
{
    BYTE *addr=(BYTE *)"-SLIP-";

    if (dtype & DTYPE_NE)
        addr = etherne_addr(dtype);
    else if (dtype & DTYPE_3C)
        addr = ether3c_addr(dtype);
#if PKTD_SUPPORT
    else if (dtype & DTYPE_PKTD)
        addr = etherpktd_addr(dtype);
#endif
    return(addr);
}

/* Get frame from receive buffer, given pointer to a frame buffer.
** If driver type field in the frame buffer is non-zero, match that driver type
** If zero, match any driver type. Return data length (0 if no packet) */
WORD get_net(GENFRAME *gfp)
{
    WORD len=0;
    SNAPFRAME *sfp;
    GENHDR gh={0,0,0};
    LWORD mstim;

    if (buff_dlen(&rxpkts) >= sizeof(GENHDR))
    {                                       /* If frame in Rx pkt buffer.. */
        buff_out(&rxpkts, (BYTE *)&gh, sizeof(gh)); /* Check frame hdr */
        len = gh.len;                       /* ..ensure length is sensible */
        if (len<1 || len>MAXFRAME)
        {
            rxpkts.out = rxpkts.trial = rxpkts.in;
            printf("\nERROR Rx frame buffer corrupt!\n");
            printf("Frame len=%u, type=%u\n", len, gh.dtype);
            len = 0;
        }
        else if (gh.dtype==gfp->g.dtype || !gfp->g.dtype)
        {                                   /* Driver type matches or is 0 */
            buff_out(&rxpkts, gfp->buff, len);  /* Store frame in buffer */
            if (!gfp->g.dtype)              /* If no driver type in buffer.. */
                gfp->g = gh;                /* ..load in complete header */
            gfp->g.len = len;               /* Set length in header */
            if (netdebug)                   /* Display raw frame */
                 disp_frame(gfp, len, 0);
            if (logfile)                    /* Log raw frame */
            {
                mstim = mstime();
                fwrite("GET ", 1, 4, logfile);
                fwrite(&mstim, 1, 4, logfile);
                fwrite(gfp, 1, len+sizeof(GENHDR), logfile);
            }
            if (gfp->g.dtype&DTYPE_ETHER && /* If Ethernet frame.. */
                gfp->buff[MACLEN*2] < 6)    /* ..and Pcol less than 600h */
            {                               /* ..might be 802.3 SNAP */
                sfp = (SNAPFRAME *)gfp->buff;
                if (len>sizeof(SNAPHDR) && sfp->s.lsap==0xaaaa)
                {                           /* If so, convert to DIX */
                    len -= sizeof(SNAPHDR);
                    memmove(&sfp->e.ptype, &sfp->s.ptype, len);
                }
                gfp->g.len = len;           /* Re-set length in header */
            }
        }
        else
        {                                   /* Wrong driver type: discard pkt */
            buff_out(&rxpkts, 0, len);
            len = 0;
        }
#if RXDROP
        if (++txfcount % RXDROP == 0)
        {
            printf("     Rx frame dropped for debug\n");
            len = 0;
        }
#endif
    }
    return(len);
}

/* Put packet onto network, given length */
WORD put_net(GENFRAME *gfp, WORD len)
{
    WORD dtype;
    SNAPFRAME *sfp;
    LWORD mstim;

#if TXDROP
    if (++txfcount % TXDROP == 0)
    {
        printf("     Tx frame dropped for debug\n");
        return(len);
    }
#endif
    dtype = gfp->g.dtype;
    len = mini(len, getframe_maxlen(gfp));  /* Truncate if too big */
    gfp->g.len = len;
    if (dtype&DTYPE_SNAP && len+sizeof(SNAPHDR)<=MAXFRAME)
    {                                       /* If 802.3 SNAP.. */
        sfp = (SNAPFRAME *)gfp->buff;       /* Make room for new header */
        memmove(&sfp->s.ptype, &sfp->e.ptype, len);
        len += sizeof(SNAPHDR);             /* Set for 802.3 802.2 & SNAP */
        sfp->e.ptype = swapw((WORD)(len-sizeof(ETHERHDR)));
        sfp->s.lsap = 0xaaaa;
        sfp->s.ctrl = 3;
        memset(sfp->s.oui, 0, 3);
    }
    if (dtype & DTYPE_PKTD)                 /* If pkt drvr, send direct */
        len = put_pktd(dtype, gfp->buff, len);
    else if (dtype & DTYPE_ETHER)           /* If an Ethernet frame.. */
    {                                       /* ..check space in pkt buffer.. */
        if (buff_freelen(&txpkts) >= len+sizeof(GENHDR))
            buff_in(&txpkts, (BYTE *)gfp, (WORD)(len+sizeof(GENHDR)));
    }
    else if (dtype & DTYPE_SLIP)            /* Send SLIP direct to driver */
        len = put_slip(gfp->buff, len, dtype);
    else
        len = 0;
    if (netdebug)
         disp_frame(gfp, len, 1);
    if (len>0 && logfile)
    {
        mstim = mstime();
        fwrite("PUT ", 1, 4, logfile);
        fwrite(&mstim, 1, 4, logfile);
        fwrite(gfp, 1, gfp->g.len+sizeof(GENHDR), logfile);
    }
    return(len);
}

/* Poll the given network interface to keep it alive */
void poll_net(WORD dtype)
{
    if (dtype & DTYPE_NE)
        poll_etherne(dtype);
    else if (dtype & DTYPE_3C)
        poll_ether3c(dtype);
    else if (dtype & DTYPE_SLIP)
        receive_slip(dtype);
}

/* Rx upcall from network driver; store a received frame, return 0 if no room
** If buffer pointer is null, only check if the required space is available
** Beware: this function may be called by an interrupt handler */
WORD receive_upcall(WORD dtype, void *buff, WORD len)
{
    static GENHDR gh;                       /* Static for BC 4.5 interrupts! */

    len = minw(len, MAXFRAME);              /* Truncate to max size */
    if (len>0 && buff_freelen(&rxpkts) >= len+sizeof(GENHDR))
    {                                       /* If space in circ buffer..*/
        if (buff)
        {
            gh.len = len;                   /* Store general frame hdr */
            gh.dtype = dtype;
            buff_in(&rxpkts, (BYTE *)&gh, sizeof(gh));
            buff_in(&rxpkts, buff, len);    /* ..and frame */
        }
    }
    else                                    /* If no space, discard frame */
        len = 0;
    return(len);
}

/* Tx upcall from network driver; get frame for Tx, return len or 0 if none */
WORD transmit_upcall(WORD dtype, void *buff, WORD maxlen)
{
    WORD len=0;
    GENHDR gh;

    if (buff_dlen(&txpkts) >=4)
    {
        buff_try(&txpkts, (BYTE *)&gh, sizeof(gh)); /* Check frame hdr */
        len = minw(gh.len, maxlen);                 /* Truncate as necessary */
        if (gh.dtype==dtype)                        /* If frame type matches..*/
        {
            buff_out(&txpkts, 0, sizeof(GENHDR));   /* ..remove hdr from buff */
            buff_out(&txpkts, buff, len);           /* .. and also the frame */
            if (len < gh.len)                       /* .. discard overbytes */
                buff_out(&txpkts, 0, (WORD)(gh.len-len));
        }
        else                                        /* If wrong driver type */
        {
            buff_retry(&txpkts, sizeof(gh));        /* ..push header back */
            len = 0;
        }
    }
    return(len);
}

/* Open Ethernet hardware driver, given config string. Return frame type */
WORD open_etherne(char *str, WORD dtype)
{
    char *s;
    WORD addr;
                                            /* Address value in hex */
    addr = (WORD)strtoul(skippunct(str), &s, 16);
    if (!addr || !init_etherne(dtype, addr))
         dtype = 0;
    return(dtype);
}

/* Open Ethernet hardware driver, given config string. Return frame type */
WORD open_ether3c(char *str, WORD dtype)
{
    char *s;
    WORD addr;
                                            /* Address value in hex */
    addr = (WORD)strtoul(skippunct(str), &s, 16);
    if (!addr || !init_ether3c(dtype, addr))
        dtype = 0;
    return(dtype);
}

#if SLIP_SUPPORT
/* Initialise SLIP link */
WORD open_slip(char *str, WORD dtype)
{
    int com, lite;
    long baud;
    char *s;
    WORD ret=0;

    selectuart(dtype & NETNUM_MASK);
    lite = (strstr(str, "litelink") != 0);
    if ((s = strstr(str, "com")) != 0)
    {
        com = (int)strtoul(s+3, &s, 10);        /* COM port number */
        baud = strtoul(skippunct(s), &s, 10);   /* Initialise hardware.. */
        if ((com>=1 || com<=9) && setuart(com, baud, 0))
        {
            ret = dtype;
            if (lite)
                litelink(baud);
        }
    }                                       
    return(ret);
}

/* Shut down SLIP link */
void close_slip(WORD dtype)
{
    selectuart(dtype & NETNUM_MASK);
    clearuart();
}

/* Get a packet from the SLIP link into Rx buffer, return frame count */
int receive_slip(WORD dtype)
{
    WORD w;
    static WORD lastw=0;
    int count=0, chan;
    static int modem=0;
    BYTE *slipbuff;

    selectuart(chan = dtype & NETNUM_MASK);
    slipbuff = slipbuffs[chan];
    while ((w = rxchar()) != NOCHAR)        /* While Rx chars available.. */
    {
        if (lastw == SLIP_ESC)              /* Last char was Escape? */
        {
            lastw = 0;
            w = w==ESC_END ? SLIP_END : w==ESC_ESC ? SLIP_ESC : w;
        }
        else if (w == SLIP_ESC)             /* This char is Escape? */
        {
            lastw = SLIP_ESC;
            w = NOCHAR;
        }
        else if (w == SLIP_END)             /* No escape; maybe End? */
        {
            w = NOCHAR;
            if (sliplens[chan] > 0)         /* Do upcall to save packet */
                receive_upcall(dtype, slipbuff, sliplens[chan]);
            sliplens[chan] = lastw = modem = 0;
            count = 1;
        }
        if (sliplens[chan]==0 && !modem)    /* If start of new message.. */
            modem = (w=='A' || w=='+');     /* ..check if start of modem cmd */
        if (w!=NOCHAR && modem)             /* If in modem mode.. */
            do_modem((char)w);              /* ..send char to modem handler */
        else if (w!=NOCHAR && sliplens[chan]<MAXSLIP)   /* Save in SLIP buff */
            slipbuff[sliplens[chan]++] = (BYTE)w;       /* ..if no overflow! */
    }
    return(count);
}

/* Send packet out of the SLIP link, return zero if failed */
WORD put_slip(BYTE *pack, WORD len, WORD dtype)
{
    int n;

    n = len;
    selectuart(dtype & NETNUM_MASK);
    txchar(SLIP_END);                       /* Start with End */
    while (n--)                             /* (to clear modem line noise) */
    {
        if (*pack == SLIP_END)              /* Trap End code */
        {
            txchar(SLIP_ESC);
            txchar(ESC_END);
        }
        else if (*pack == SLIP_ESC)         /* ..and Escape code */
        {
            txchar(SLIP_ESC);
            txchar(ESC_ESC);
        }
        else                                /* Default; just send char */
            txchar(*pack);
        pack++;
    }
    txchar(SLIP_END);                       /* End with End */
    return(len);
}

/* Handle incoming modem command character */
void do_modem(char c)
{
    static char cmds[41];
    static int cmdlen=0;

    if (c=='\r' || (cmdlen==3 && cmds[0]=='+')) 
    {
        cmds[cmdlen] = 0;
        if (netdebug)
            printf("Modem command: %s\n", cmds);
        txstr("OK\r\n");
        if (cmds[2] == 'D')
            txstr("CONNECT\r\n");
        cmdlen = 0;
    }
    else if (cmdlen<sizeof(cmds)-1)
        cmds[cmdlen++] = c;
}
#endif      // if SLIP_SUPPORT

/* EOF */
