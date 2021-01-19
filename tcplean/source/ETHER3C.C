/* 3COM Etherlink III 3C509 ISA drivers for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

WARNING: this driver is in an early stage of development...
It only works with the co-ax network interface at present, not twisted pair.
Use 3COM's 3C5X9CFG utility to select the interface and set the I/O addr

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 1/12/99
** v0.02 JPB 7/1/00   Added 'maxlen' to initialisation
** v0.03 JPB 17/1/00  Removed 'maxlen' again!
** v0.04 JPB 20/1/00  Added support for multiple cards
** v0.05 JPB 23/3/00  Added flag for promiscuous mode
** v0.06 JPB 3/7/00   Revised header for book CD
*/

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#include "ether.h"              /* Typedefs and function prototypes */
#include "netutil.h"
#include "net.h"

#define IDPORT      0x100       /* ID port addr (NOT the card base addr) */

/* Register definitions */
#define CMDS        0xe         /* Command/status       all windows */
#define CCR         0x4         /* Config control       window 0 */
#define FIFO        0x0         /* Tx/Rx FIFO           window 1 */
#define RXSTAT      0x8         /* Receive status       window 1 */
#define TXSTAT      0xb         /* Transmit status      window 1 (byte) */
#define ETHADDR     0x0         /* Ethernet address     window 2 */
#define RXFREE      0xa         /* Rx free space        window 3 */
#define TXFREE      0xc         /* Tx free space        window 1 or 3 */
#define MEDIATYPE   0xa         /* Media type & status  window 4 */

/* Commands */
#define CMD_RESET       0x0000  /* Reset board */
#define CMD_SELWIN      0x0800  /* Select register window */
#define CMD_COAX        0x1000  /* Start coax transceiver */
#define CMD_RXDIS       0x1800  /* Disable Rx */
#define CMD_RXEN        0x2000  /* Enable Rx */
#define CMD_RXDISC      0x4000  /* Discard Rx packet */
#define CMD_TXDIS       0x5000  /* Disable Tx */
#define CMD_TXRESET     0x5800  /* Reset Tx */
#define CMD_TXEN        0x4800  /* Enable Tx */
#define CMD_RXFILT      0x8000  /* Set Rx filter */
#define CMD_TXTHRESH    0x9800  /* Tx threshold */
#define CMD_NOCOAX      0xb800  /* Stop coax transceiver */

/* Network controller register I/O */
#define GETREG(r)       inpw((WORD)(ebase+r))       /* Read word from reg */
#define SETREG(r, x)    outpw((WORD)(ebase+r), x)   /* Write word to reg */
#define CMD(c, x)       SETREG(CMDS, (WORD)(c+x))   /* Send command and data */
#define SELWIN(x)       CMD(CMD_SELWIN, x)          /* Select window number */

typedef struct                  /* Net driver configuration data */
{
    WORD dtype;                     /* Driver type */
    BYTE myeth[MACLEN];             /* MAC (Ethernet) addr */
    WORD ebase;                     /* Card I/O base addr */
} CONFIGNE;

static CONFIGNE configs[MAXNETS];   /* Driver configurations */

static unsigned ebase;              /* Temp I/O base addr (not ID port addr) */
extern int promisc;                 /* Flag to enable promiscuous mode */

/* Private prototypes */
void write_idseq(void);
WORD read_eeword(BYTE oset);

/* Initialise card given base addr, and byte/word mode flag
** Return 0 if initialisation error */
int init_ether3c(WORD dtype, WORD baseaddr)
{
    int ok=0, i;
    WORD w, addr, mfg, prod;
    CONFIGNE *cp;

    cp = &configs[dtype & NETNUM_MASK]; /* Get pointer into driver data */
    cp->dtype = dtype;                  /* Set driver type */
    cp->ebase = ebase = baseaddr;       /* Set card I/O base address */
    outp(IDPORT, 0xc0);                 /* Reset board, wait at least 1 msec */
    delay(10);
    write_idseq();                      /* Do strange ident sequence */
    outp(IDPORT, 0xc0);                 /* Rinse and repeat.. */
    delay(10);
    write_idseq();                 
    mfg = read_eeword(7);               /* Check manufacturer ID */
    prod = read_eeword(3);              /* ..and product ID */
    if (mfg==0x6d50 && (prod&0xf0ff)==0x9050)
    {
        if ((addr=(read_eeword(8) & 0x1f)*0x10 + 0x200) != ebase)
            printf("ERROR: actual card base addr %03Xh\n", addr);
        else
        {
            outp(IDPORT, 0xff);         /* Activate board */
            delay(10);
            SELWIN(0);                  /* Enable adapter */
            SETREG(CCR, 0x01);
            SELWIN(2);
            for (i=0; i<MACLEN; i+=2)   /* Get Ethernet address */
            {
                w = read_eeword((BYTE)(i/2));
                cp->myeth[i] = (BYTE)(w >> 8);
                cp->myeth[i+1] = (BYTE)w;   /* ..and set in controller */
                SETREG(ETHADDR+i, swapw(w));
            }
            CMD(CMD_TXTHRESH, 0x777);   /* Only Tx when whole packet sent */
            CMD(CMD_COAX, 0);           /* Start coax transceiver */
            delay(10);
            SELWIN(3);
            CMD(CMD_TXEN, 0);           /* Enable transmit */
            if (promisc)
                CMD(CMD_RXFILT, 8);     /* Set promiscuous mode */
            else
                CMD(CMD_RXFILT, 5);     /* Accept own & bcast addrs */
            CMD(CMD_RXEN, 0);           /* Enable receive */
            SELWIN(1);
            ok = 1;
        }
    }
    else
        printf("ERROR: card manufacturer %04Xh product %04Xh\n", mfg, prod);
    return(ok);
}

/* Return pointer to my Ethernet addr, given driver type */
BYTE *ether3c_addr(WORD dtype)
{
    return(configs[dtype & NETNUM_MASK].myeth);
}

/* Close down ethernet controller */
void close_ether3c(WORD dtype)
{
    ebase = configs[dtype & NETNUM_MASK].ebase;
    if (ebase)
    {
        CMD(CMD_TXDIS, 0);           /* Disable transmit & receive */
        CMD(CMD_RXDIS, 0);
        configs[dtype & NETNUM_MASK].ebase = 0;
    }
}

/* Poll network interface to keep it alive; send & receive frames */
void poll_ether3c(WORD dtype)
{
    WORD len;
    BYTE stat;
    static BYTE ebuff[MAXFRAMEC];
    CONFIGNE *cp;

    cp = &configs[dtype & NETNUM_MASK];
    if (cp->ebase)                              /* If net in use.. */
    {
        ebase = cp->ebase;                      /* Set card I/O address */
        /* Receive */
        SELWIN(1);
        while ((len=get_ether3c(ebuff))>0)
        {                               /* Store frames in circ buff */
            receive_upcall(cp->dtype, ebuff, len);
        }
        /* Transmit */
        while ((stat=inp((WORD)(ebase+TXSTAT))) & 0x80)
        {
            if (stat & 0x1c)                    /* Reset Tx if error */
            {
                CMD(CMD_TXRESET, 0);
                CMD(CMD_TXEN, 0);
            }
            outp((WORD)(ebase+TXSTAT), 0);      /* Pop status off stack */
        }
        while (GETREG(TXFREE)>=MAXFRAME &&      /* While space in buff.. */
            (len=transmit_upcall(cp->dtype, ebuff, MAXFRAME))>0)
        {                                       /* ..and frame available.. */
            memcpy(&ebuff[MACLEN], cp->myeth, MACLEN);  /* Set my addr */
            put_ether3c(ebuff, len);            /* ..and transmit frame */
        }
    }
}

/* Get packet into buffer, return length (excl CRC), or 0 if none available */
WORD get_ether3c(void *pkt)
{
    WORD stat, len=0;
    register WORD n, *p;

    if (!((stat = GETREG(RXSTAT)) & 0x8000))/* If complete packet received.. */
    {
        if (!(stat & 0x4000))               /* ..and no errors */
        {
            p = (WORD *)pkt;                /* Get data ptr & length */
            len = minw((WORD)(stat&0x7ff), MAXFRAME);
            n = (len+1) >> 1;
            while (n--)                     /* Read bytes */
                *p++ = GETREG(FIFO);
        }
        CMD(CMD_RXDISC, 0);                 /* Discard received packet */
    }
    return(len);                            /* Return length excl. CRC */
}

/* Send Ethernet packet given len excl. CRC, return length */
WORD put_ether3c(void *pkt, WORD len)
{
    register WORD n, *p;

    p = (WORD *)pkt;                        /* Get length & word ptr */
    len = minw(MAXFRAME, maxw(MINFRAME, len));
    SETREG(FIFO, len);                      /* Send length */
    SETREG(FIFO, 0);                        /* ..and dummy word */
    n = (((len+3) & 0xffc) >> 1);           /* Round len up to DWORD boundary */
    while (n--)                             /* Send data */
        SETREG(FIFO, *p++);
    return(len);
}

/* Do ID sequence to put board in ID command state */
void write_idseq(void)
{
    WORD a=0xff;
    int i;

    outp(IDPORT, 0);
    outp(IDPORT, 0);
    for (i=0; i<255; i++)
    {
        outp(IDPORT, a);
        if ((a <<= 1) & 0x100)
            a ^= 0xcf;
    }
}

/* Read EEPROM data (if already in ID command state) */
WORD read_eeword(BYTE oset)
{
    int i;
    WORD w=0;

    outp(IDPORT, 0x80+oset);
    delay(1);
    for (i=0; i<16; i++)
        w = (w<<1) | (inpw(IDPORT) & 1);
    return(w);
}

/* EOF */
