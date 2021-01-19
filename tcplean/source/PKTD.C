/* PC/TCP packet driver interface for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 17/10/97 Derived from 'pktdrvr.c' in the KA9Q TCP stack
** v0.02 JPB 18/10/97 Improved Rx buffering, dispensed with 'pkta.asm'
** v0.04 JPB 9/7/99   Renamed functions for use with IPTEST.C and EIP.C
** v0.05 JPB 5/11/99  Major revamp to support multiple classes
** v0.06 JPB 12/11/99 Added circular buffer option
** v0.07 JPB 12/11/99 Renamed from PKTD.C to PCPKTD.C
** v0.08 JPB 7/1/00   Added 'maxlen' to initialisation
** v0.09 JPB 17/1/00  Removed 'maxlen' again!
** v0.10 JPB 3/7/00   Revised header for book CD
**                    Fixed SLIP-specific bugs
*/

#include <stdio.h>
#include <string.h>
#include <dos.h>

#include "netutil.h"
#include "net.h"
#include "ether.h"
#include "pktd.h"

#define MINPINT     0x60        /* Minimum and maximum packet interrupt nums */
#define MAXPINT     0x80
#define PKTSIG      "PKT DRVR"  /* Signature string for packet driver */

typedef struct                  /* Net driver configuration data */
{
    WORD dtype;                     /* Driver type */
    BYTE myeth[MACLEN];             /* Ether addr */
    WORD handle;                    /* Driver handle */
    int intnum;                     /* Packet interrupt number */
    WORD rxlen;                     /* Received data length */
    BYTE rxbuff[MAXFRAMEC];         /* Buffer for received data */
} CONFIGPKTD;

static CONFIGPKTD configs[MAXNETS]; /* Driver configurations */

/* Private prototypes */
void interrupt rxhandler(WORD bp, WORD di, WORD si, WORD ds, WORD es, WORD dx,
    WORD cx, WORD bx, WORD ax);
WORD intnum2dtype(int intnum);
WORD handle2dtype(WORD handle);
long access_pktype(int intnum, BYTE iclass, int itype, BYTE *ptype,
    int ptypelen, void interrupt(*handler)());
void release_pktype(int intnum);
int get_address(int intnum, WORD handle, BYTE *buf, int len);

/* Initialise the PC/TCP packet interface, return driver type, 0 if error */
WORD open_pktd(WORD dtype)
{
    int i=-1, used=0;
    long handle=-1L;
    BYTE pclass;
    WORD intnum;
    char far *vec;
    CONFIGPKTD *cp;

    cp = &configs[dtype & NETNUM_MASK];     /* Get pointer into driver data */
    cp->dtype = dtype;                      /* Set driver type */
    pclass = dtype&DTYPE_ETHER ? PKTD_ETHER : dtype&DTYPE_SLIP ? PKTD_SLIP : 0;
    while (pclass && handle<0L && ++i<MAXPINT-MINPINT)
    {                                       /* Scan the interrupts */
        intnum = i + MINPINT;
        vec = (char far *)getvect(intnum);
        used = intnum2dtype(intnum) != 0;   /* Check if already in use */
        if (!used && !_fstrncmp(vec+3, PKTSIG, strlen(PKTSIG)))
            handle = access_pktype(intnum, pclass, ANYTYPE, 0, 0, rxhandler);
    }                                       /* Use interrupt if right type */
    if (handle>=0L)
    {
        cp->intnum = intnum;                /* Save driver interrupt & handle */
        cp->handle = (WORD)handle;
        if (dtype & DTYPE_ETHER)            /* Get Ethernet address */
            get_address(intnum, cp->handle, cp->myeth, MACLEN);
    }
    else
        dtype = 0;
    return(dtype);
}

/* Shut down packet driver */
void close_pktd(WORD dtype)
{
    CONFIGPKTD *cp;

    cp = &configs[dtype & NETNUM_MASK];
    if (cp->intnum)
        release_pktype(cp->intnum);
    cp->intnum = cp->dtype = 0;
}

/* Return pointer to my Ethernet addr, given driver type */
BYTE *etherpktd_addr(WORD dtype)
{
    return(configs[dtype & NETNUM_MASK].myeth);
}

/* Send Ethernet packet: length includes dest addr, srce addr and type */
int put_pktd(WORD dtype, void *pkt, int len)
{
    union REGS regs;
    struct SREGS sregs;
    int ret=0;
    CONFIGPKTD *cp;

    cp = &configs[dtype & NETNUM_MASK];
    if (cp->intnum)
    {
        if (dtype & DTYPE_ETHER)
            memcpy((BYTE *)pkt+MACLEN, cp->myeth, MACLEN);
        segread(&sregs);
        sregs.ds = FP_SEG(pkt);
        sregs.es = FP_SEG(pkt);
        regs.x.si = FP_OFF(pkt);
        regs.x.cx = len;
        regs.h.ah = SEND_PKT;
        int86x(cp->intnum, &regs, &regs, &sregs);
        ret = !regs.x.cflag ? len : 0;
    }
    return(ret);
}

/* Handler for packet Rx interrupt */
#pragma argsused
void interrupt rxhandler(WORD bp, WORD di, WORD si, WORD ds, WORD es, WORD dx,
    WORD cx, WORD bx, WORD ax)
{
    static WORD dtype;                  /* Must be static for BC 4.5 */
    static CONFIGPKTD *cp;

    if ((dtype = handle2dtype(bx))!=0)
    {
        cp = &configs[dtype & NETNUM_MASK];
        if (ax == 0)                        /* Allocate space? */
        {
            if (cx>0 && cx<=MAXFRAMEC)
            {
                es = FP_SEG(cp->rxbuff);    /* Return seg & oset of my buffer */
                di = FP_OFF(cp->rxbuff);
                cp->rxlen = cx;
            }
            else
                es = di = cp->rxlen = 0;    /* Reject if invalid */
        }
        else if (ax==1 && cp->rxlen>0)      /* Packet complete? */
        {
            receive_upcall(dtype, cp->rxbuff, cp->rxlen);
            cp->rxlen = 0;
        }
    }
    else
        es = di = 0;
}

/* Return driver type for a given interrupt number */
WORD intnum2dtype(int intnum)
{
    int n=-1;
    WORD dtype=0;

    while (dtype==0 && ++n<MAXNETS)
        dtype = intnum==configs[n].intnum ? configs[n].dtype : 0;
    return(dtype);
}

/* Return driver type for a given handle */
WORD handle2dtype(WORD handle)
{
    int n=-1;
    WORD dtype=0;

    while (dtype==0 && ++n<MAXNETS)
        dtype = handle==configs[n].handle ? configs[n].dtype : 0;
    return(dtype);
}

/* Access packets of given type, return handle, or -1 if error */
long access_pktype(int intnum, BYTE iclass, int itype, BYTE *ptype,
    int ptypelen, void interrupt(*handler)())
{
    union REGS regs;
    struct SREGS sregs;
    long hand=-1L;

    segread(&sregs);
    regs.h.dl = 0;                  /* Interface 0 */
    sregs.ds = FP_SEG(ptype);       /* Packet type template */
    regs.x.si = FP_OFF(ptype);
    regs.x.cx = ptypelen;           /* Length of type */
    sregs.es = FP_SEG(handler);     /* Address of receive handler */
    regs.x.di = FP_OFF(handler);
    regs.x.bx = itype;              /* Type */
    regs.h.ah = ACCESS_TYPE;        /* Access_type() function */
    regs.h.al = iclass;             /* Class */
    int86x(intnum, &regs, &regs, &sregs);
    if (!regs.x.cflag)              /* Save handle if carry clear */
        hand = regs.x.ax;
    return(hand);
}

/* Relinquish access to the packets */
void release_pktype(int intnum)
{
    union REGS regs;
    WORD dtype;

    if ((dtype=intnum2dtype(intnum))!=0)
    {
        regs.x.bx = configs[dtype & NETNUM_MASK].handle;
        regs.h.ah = RELEASE_TYPE;
        int86(intnum, &regs, &regs);
    }
}

/* Get the Ethernet address */
int get_address(int intnum, WORD handle, BYTE *buf, int len)
{
    union REGS regs;
    struct SREGS sregs;

    segread(&sregs);
    sregs.es = FP_SEG(buf);
    regs.x.di = FP_OFF(buf);
    regs.x.cx = len;
    regs.x.bx = handle;
    regs.h.ah = GET_ADDRESS;
    int86x(intnum, &regs, &regs, &sregs);
    return(!regs.x.cflag);
}

/* EOF */
