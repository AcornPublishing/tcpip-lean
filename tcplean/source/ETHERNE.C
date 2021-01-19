/* NE2000-compatible net card drivers for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 19/6/97
** v0.02 JPB 19/6/97 Added SPIBB code
** v0.03 JPB 20/6/97 Changed I/O address from 340h to 280h
**                   Added mem status byte clear
** v0.04 JPB 31/7/97 Added experimental whole-packet read
** v0.05 JPB 15/8/97 Fixed bug in length calculation for odd-length packets
** v0.06 JPB 9/1/98  Added assembly-language insert
** v0.07 JPB 13/1/97 Poll Rx interrupt flag to see if packet arrived
** v0.08 JPB 11/12/98 Added base addr. and 16-bit flag to 'etinit'
**                   Removed SPIBB driver code
** v0.10 JPB 28/6/99 Significant rework & simplification of the code
**                   Now works OK on fast CPUs!
**                   Reduced RAM size for 8-bit mode (p43 of UM9008 data sheet)
** v0.11 JPB 20/7/99 Added delay between detecting Rx interrupt & polling regs
** v0.12 JPB 21/7/99 Added interrupt capability
** v0.13 JPB 22/7/99 Added 'DMA complete' check at end of 'putnic'
** v0.14 JPB 22/7/99 Minor speed improvement to 'getnic' and 'putnic'
** v0.15 JPB 22/7/99 Removed unused local vars
** v0.16 JPB 29/10/99 Renamed functions for TCPIPFS compatibility
**                    Rxpacket now returns length excl. CRC
** v0.17 JPB 10/11/99 Restored inline I/O code
** v0.18 JPB 15/11/99 Added Tx and Rx circular buffers
** v0.19 JPB 16/11/99 Removed Starlan length hack - does more harm than good!
** v0.20 JPB 16/11/99 Removed interrupt code
** v0.21 JPB 17/11/99 Replaced min() with minw() or mini()
** v0.22 JPB 7/1/00   Added 'maxlen' to initialisation
** v0.23 JPB 17/1/00  Removed 'maxlen' again!
** v0.24 JPB 20/1/00  Added support for multiple cards
** v0.25 JPB 3/7/00   Revised header for CD
*/

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#include "ether.h"              /* Typedefs and function prototypes */
#include "netutil.h"
#include "net.h"

#define WORDMODE 1              /* Set to zero if using 8-bit XT-bus cards */

/* NE2000 definitions */
#define DATAPORT 0x10
#define NE_RESET 0x1f

/* 8390 Network Interface Controller (NIC) page0 register offsets */
#define CMDR    0x00            /* command register for read & write */
#define PSTART  0x01            /* page start register for write */
#define PSTOP   0x02            /* page stop register for write */
#define BNRY    0x03            /* boundary reg for rd and wr */
#define TPSR    0x04            /* tx start page start reg for wr */
#define TBCR0   0x05            /* tx byte count 0 reg for wr */
#define TBCR1   0x06            /* tx byte count 1 reg for wr */
#define ISR     0x07            /* interrupt status reg for rd and wr */
#define RSAR0   0x08            /* low byte of remote start addr */
#define RSAR1   0x09            /* hi byte of remote start addr */
#define RBCR0   0x0A            /* remote byte count reg 0 for wr */
#define RBCR1   0x0B            /* remote byte count reg 1 for wr */
#define RCR     0x0C            /* rx configuration reg for wr */
#define TCR     0x0D            /* tx configuration reg for wr */
#define DCR     0x0E            /* data configuration reg for wr */
#define IMR     0x0F            /* interrupt mask reg for wr */

/* NIC page 1 register offsets */
#define PAR0    0x01            /* physical addr reg 0 for rd and wr */
#define CURR    0x07            /* current page reg for rd and wr */
#define MAR0    0x08            /* multicast addr reg 0 for rd and WR */

/* Buffer Length and Field Definition Info */
#define TXSTART  0x40           /* Tx buffer start page */
#define TXPAGES  6              /* Pages for Tx buffer */
#define RXSTART  (TXSTART+TXPAGES)  /* Rx buffer start page */
#if WORDMODE
#define RXSTOP   0x7e           /* Rx buffer end page for word mode */
#define DCRVAL   0x49           /* DCR values for word mode */
#else
#define RXSTOP   0x5f           /* Ditto for byte mode */
#define DCRVAL   0x48
#endif
#define STARHACK 0              /* Set non-zero to enable Starlan length hack */

typedef struct                  /* Net driver configuration data */
{
    WORD dtype;                     /* Driver type */
    BYTE myeth[MACLEN];             /* MAC (Ethernet) addr */
    WORD ebase;                     /* Card I/O base addr */
    WORD next_pkt;                  /* Next (current) Rx page */
} CONFIGNE;

static CONFIGNE configs[MAXNETS];   /* Driver configurations */

static WORD ebase;              /* Temp I/O base addr; usually 280h for PC */
int promisc=0;                  /* Flag to enable promiscuous mode */

typedef struct {                /* NIC hardware packet header */
    BYTE stat;                  /*     Error status */
    BYTE next;                  /*     Pointer to next block */
    WORD len;                   /*     Length of this frame incl. CRC */
} NICHDR;
NICHDR nichdr;

/* Private prototypes */
void resetnic(CONFIGNE *cp, char cold);
void getnic(WORD addr, BYTE data[], WORD len);
void putnic(WORD addr, BYTE data[], WORD len);
BYTE nicwrap(int page);
BYTE innic(int reg);
void outnic(int reg, int b);

/* Initialise card given driver type and base addr.
** Return driver type, 0 if error */
int init_etherne(WORD dtype, WORD baseaddr)
{
    int ok=0;
    CONFIGNE *cp;

    cp = &configs[dtype & NETNUM_MASK]; /* Get pointer into driver data */
    cp->dtype = dtype;                  /* Set driver type */
    cp->ebase = ebase = baseaddr;       /* Set card I/O base address */
    outnic(NE_RESET, innic(NE_RESET));  /* Do reset */
    delay(2);
    if ((innic(ISR) & 0x80) == 0)       /* Report if failed */
    {
        printf("  Ethernet card failed to reset!\n");
    }
    else
    {
        resetnic(cp, 1);                /* Reset Ethernet card, get my addr */
        ok = 1;
    }
    return(ok);
}

/* Close down ethernet controller */
void close_etherne(WORD dtype)
{
    ebase = configs[dtype & NETNUM_MASK].ebase;
    if (ebase)
    {
        outnic(CMDR, 0x21);             /* Stop, DMA abort, page 0 */
        configs[dtype & NETNUM_MASK].ebase = 0;
    }
}

/* Return pointer to my Ethernet addr, given driver type */
BYTE *etherne_addr(WORD dtype)
{
    return(configs[dtype & NETNUM_MASK].myeth);
}

/* Poll network interface to keep it alive; send & receive frames */
void poll_etherne(WORD dtype)
{
    WORD len;
    static BYTE ebuff[MAXFRAMEC];
    CONFIGNE *cp;

    cp = &configs[dtype & NETNUM_MASK];
    if (cp->ebase)                          /* If Ether card in use.. */
    {
        ebase = cp->ebase;                  /* Set card I/O address */
        outnic(ISR, 0x01);                  /* Clear interrupt flag */
        /* Receive */
        while ((len=get_etherne(cp->dtype, ebuff))>0)
        {                                   /* Store frames in buff */
            receive_upcall(cp->dtype, ebuff, len);
        }
        /* Transmit */
        while (!(innic(CMDR)&0x04) &&       /* While NIC ready & frame avail */
            (len=transmit_upcall(cp->dtype, ebuff, MAXFRAME))>0)
        {                                   /* ..transmit frame */
            put_etherne(cp->dtype, ebuff, len);
        }
    }
}

/* Get packet into buffer, return length (excl CRC), or 0 if none available */
WORD get_etherne(WORD dtype, void *pkt)
{
    WORD len=0, curr;
    BYTE bnry;
    CONFIGNE *cp;
#if STARHACK
    int hilen, lolen;
#endif
    cp = &configs[dtype & NETNUM_MASK];
    ebase = cp->ebase;
    if (innic(ISR) & 0x10)              /* If Rx overrun.. */
    {
        printf("  NIC Rx overrun\n");
        resetnic(cp, 0);                /* ..reset controller (drastic!) */
    }
    outnic(CMDR, 0x60);                 /* DMA abort, page 1 */
    curr = innic(CURR);                 /* Get current page */
    outnic(CMDR, 0x20);                 /* DMA abort, page 0 */
    if (curr != cp->next_pkt)           /* If Rx packet.. */
    {
        memset(&nichdr, 0xee, sizeof(nichdr));  /* ..get NIC header */
        getnic((WORD)(cp->next_pkt<<8), (BYTE *)&nichdr, sizeof(nichdr));
#if STARHACK
        hilen = nichdr.next - cp->next_pkt - 1;
        lolen = nichdr.len & 0xff;
        if (hilen < 0)                  /* Do len calc from NIC datasheet */
            hilen = RXSTOP - cp->next_pkt + nichdr.next - RXSTART - 1;
        if (lolen > 0xfc)
            hilen++;
        len = (hilen<<8) + lolen;
        if (len != nichdr.len)          /* ..and compare with actual value */
        {
            if (netdebug)
                printf("  NIC length mismatch %Xh - %Xh\n", len, nichdr.len);
        }
#else
        len = nichdr.len;               /* Take length from stored header */
#endif
        if ((nichdr.stat&1) && len>=MINFRAMEC && len<=MAXFRAMEC)
        {                               /* If hdr is OK, get packet */
            len -= CRCLEN;              /* ..without CRC! */
            if (pkt)
                getnic((WORD)((cp->next_pkt<<8)+sizeof(nichdr)), pkt, len);
        }
        else                            /* If not, no packet data */
        {
            printf("  NIC packet error\n");
        }                               /* Update next packet ptr */
        if (nichdr.next>=RXSTART && nichdr.next<RXSTOP)
            cp->next_pkt = nichdr.next;
        else                            /* If invalid, use prev+1 */
        {
            printf("  NIC pointer error\n");
            cp->next_pkt = nicwrap(cp->next_pkt + 1);
        }                               /* Update boundary register */
        bnry = nicwrap(cp->next_pkt - 1);
        outnic(BNRY, bnry);
    }
    return(len);                        /* Return length excl. CRC */
}

/* Send Ethernet packet given len excl. CRC, return 0 if NIC is busy */
WORD put_etherne(WORD dtype, void *pkt, WORD len)
{
    CONFIGNE *cp;

    cp = &configs[dtype & NETNUM_MASK];
    ebase = cp->ebase;
    if (!ebase || innic(CMDR) & 4)      /* If still Txing, return 0 */
        len = 0;
    else if (pkt)
    {                                   /* If last Tx is complete.. */
        len = minw(MAXFRAME, maxw(MINFRAME, len));      /* Constrain length */
        memcpy((BYTE *)pkt+MACLEN, cp->myeth, MACLEN);  /* Set source addr */
        outnic(ISR, 0x0a);              /* Clear interrupt flags */
        outnic(TBCR0, len & 0xff);      /* Set Tx length regs */
        outnic(TBCR1, len >> 8);
        putnic(TXSTART<<8, pkt, len);
        outnic(CMDR, 0x24);             /* Transmit the packet */
    }
    return(len);
}

/* Reset the Ethernet card, if 'cold' start, get my 6-byte address */
void resetnic(CONFIGNE *cp, char cold)
{
    int i;
    BYTE temp[MACLEN*2];

    outnic(CMDR, 0x21);                 /* Stop, DMA abort, page 0 */
    delay(2);                           /* ..wait to take effect */
    outnic(DCR, DCRVAL);
    outnic(RBCR0, 0);                   /* Clear remote byte count */
    outnic(RBCR1, 0);
    outnic(RCR, 0x20);                  /* Rx monitor mode */
    outnic(TCR, 0x02);                  /* Tx internal loopback */
    outnic(TPSR, TXSTART);              /* Set Tx start page */
    outnic(PSTART, RXSTART);            /* Set Rx start, stop, boundary */
    outnic(PSTOP, RXSTOP);
    outnic(BNRY, (BYTE)(RXSTOP-1));
    outnic(ISR, 0xff);                  /* Clear interrupt flags */
    outnic(IMR, 0);                     /* Mask all interrupts */
    if (cold)
    {
        outnic(CMDR, 0x22);             /* Start NIC, DMA abort */
        getnic(0, temp, 12);            /* Get 6-byte addr */
        for (i=0; i<MACLEN; i++)        /* Convert addr words to bytes */
            cp->myeth[i] = temp[WORDMODE ? i+i : i];
    }
    outnic(CMDR, 0x61);                 /* Stop, DMA abort, page 1 */
    delay(2);
    for (i=0; i<6; i++)                 /* Set Phys addr */
        outnic(PAR0+i, cp->myeth[i]);
    for (i=0; i<8; i++)                 /* Multicast accept-all */
        outnic(MAR0+i, 0xff);
    outnic(CURR, RXSTART+1);            /* Set current Rx page */
    cp->next_pkt = RXSTART + 1;
    outnic(CMDR, 0x20);                 /* DMA abort, page 0 */
    outnic(RCR, promisc ? 0x14 : 0x04); /* Allow broadcasts, maybe all pkts */
    outnic(TCR, 0);                     /* Normal Tx operation */
    outnic(ISR, 0xff);                  /* Clear interrupt flags */
    outnic(CMDR, 0x22);                 /* Start NIC */
}

/* Get a packet from a given address in the NIC's RAM */
void getnic(WORD addr, BYTE data[], WORD len)
{
    register int count;
    register WORD *dataw, dataport;

    count = WORDMODE ? len>>1 : len;    /* Halve byte count if word I/P */
    dataport = ebase + DATAPORT;        /* Address of NIC data port */
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len&0xff);            /* Byte count */
    outnic(RBCR1, len>>8);
    outnic(RSAR0, addr&0xff);           /* Data addr */
    outnic(RSAR1, addr>>8);
    outnic(CMDR, 0x0a);                 /* Start, DMA remote read */
#if WORDMODE
    dataw = (WORD *)data;               /* Use pointer for speed */
    while(count--)                      /* Get words */
        *dataw++ = inpw(dataport);
    if (len & 1)                        /* If odd length, do last byte */
        *(BYTE *)dataw = inp(dataport);
#else
    while(count--)                      /* Get bytes */
        *data++ = inp(dataport);
#endif
}

/* Put a packet into a given address in the NIC's RAM */
void putnic(WORD addr, BYTE data[], WORD len)
{
    register int count;
    register WORD *dataw, dataport;

    len += len & 1;                     /* Round length up to an even value */
    count = WORDMODE ? len>>1 : len;    /* Halve byte count if word O/P */
    dataport = ebase + DATAPORT;        /* Address of NIC data port */
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len&0xff);            /* Byte count */
    outnic(RBCR1, len>>8);
    outnic(RSAR0, addr&0xff);           /* Data addr */
    outnic(RSAR1, addr>>8);
    outnic(CMDR, 0x12);                 /* Start, DMA remote write */
#if WORDMODE                            /* Word transfer? */
    dataw = (WORD *)data;
    while(count--)
        outpw(dataport, *dataw++);  /* O/P words */
#else
    while(count--)                  /* O/P bytes */
        outp(dataport, *data++);
#endif
    count = 10000;                      /* Done: must ensure DMA complete */
    while(count && (innic(ISR)&0x40)==0)
        count--;
}

/* Wrap an NIC Rx page number */
BYTE nicwrap(int page)
{
   if (page >= RXSTOP)
       page += RXSTART - RXSTOP;
   else if (page < RXSTART)
       page += RXSTOP - RXSTART;
   return(page);
}

/* Input a byte from a NIC register */
BYTE innic(int reg)
{
    return(inp((WORD)(ebase+reg)));
}
/* Output a byte to a NIC register */
void outnic(int reg, int b)
{
    outp((WORD)(ebase+reg), b);
}

/* EOF */
