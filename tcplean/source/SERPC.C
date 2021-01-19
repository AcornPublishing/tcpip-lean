/* PC serial routines for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v1.00 JPB 2/3/97  Code extracted from TER.C v1.01
** v1.01 JPB 6/1/99  Added COM3/4 support
** v1.02 JPB 10/6/99 Added handshakes and 'initser'
** v1.03 JPB 11/6/99 Added non-interrupt capability to initser
** v1.04 JPB 3/11/99 Minor cosmetic changes for use with TCPIPFS
** v1.05 JPB 11/2/00 Added IRDA 'litelink' setting capability
** v1.06 JPB 17/3/00 Fixed uninitialised variable in 'litelink'
** v1.07 JPB 20/3/00 Changed 'txstr' to use char string (not byte string)
** v1.08 JPB 23/3/00 Added support for multiple serial channels
** v1.09 JPB 9/4/00  Added transmit interrupts
** v1.10 JPB 3/7/00  Revised header for book CD
*/

#include <stdio.h>
#include <dos.h>

#include "serpc.h"

#define TXINTS  1               /* Set non-zero to use transmit interrupts */

int break_handler(void);        /* Control-break handler */

typedef struct {                /* Structure for a circular buffer.. */
    char rbuf[RBUFSIZ];             /* Rx character buffer */
    int  ri, ro;                    /* I/P and O/P pointers */
#if TXINTS
    char tbuf[TBUFSIZ];             /* Tx character buffer */
    int  ti, to;                    /* I/P and O/P pointers */
#endif
    void interrupt (*oldvector)();  /* Save int vector */
    unsigned char oldmask;          /* Saved int mask */
    int irq;                        /* IRQ number */
    unsigned pic;                   /* Int controller addr */
    unsigned uart;                  /* UART addr */
} SERBUFF;

static SERBUFF serbuffs[MAXCOMMS];  /* One circular buffer per channel */
static int serchan;

void interrupt hand1(void), hand2(void), hand3(void), hand4(void);
void ser_handler(SERBUFF *sbp);
int txfree(SERBUFF *sbp);

/* Permissible IRDA baud rates */
long irbauds[] = {115200L, 57600L, 38400L, 19200L, 9600L, 4800L, 2400L, 0};

/* Set up UART to given baud rate, enable receive interrupts
** If 'irq' is zero, use standard IRQ number for COM1 and COM2
** Return base addr, 0 if error */
unsigned setuart(int com, long baud, int irq)
{
    unsigned far *addrp;
    unsigned addr;

    addrp = MK_FP(0x40, (com-1)*2);       /* Read BIOS data area for COM port */
    if (com<=0 || com>4 || *addrp<0x100 || *addrp==0xffff)
        return(0);
    addr = *addrp;
    if ((com==1 && addr!=UART1) || (com==2 && addr!=UART2))
        printf("WARNING: non-standard COM%u address (%03Xh) in BIOS!\n",
            com, addr);
    if (!irq)
    {
        if (com == 1)
            irq = 4;
        else if (com == 2)
            irq = 3;
    }
    ctrlbrk(break_handler);
    return(initser(addr, baud, irq));
}

/* Set up 8250 at given address to given baud rate.
** If 'irq' is non-zero, enable receive interrupts
** Return 0 if error */
unsigned initser(unsigned addr, long baud, int irq)
{
    unsigned divisor, mask, vectnum;
    SERBUFF *sbp;
    void interrupt (*handler)(void);

    sbp = &serbuffs[serchan];
    sbp->uart = addr;                           /* Save addr & irq */
    sbp->irq = irq;
    outportb(addr+1, 0);                        /* Disable Rx ints */
    if (irq)                                    /* Use IRQ if given */
    {
        vectnum = irq>7 ? irq+0x68 : irq+8;
        handler = serchan==0?hand1:serchan==1?hand2:serchan==2?hand3:hand4;
        disable();                              /* Disable interrupts */
        mask = 0xff - (1<<(vectnum & 7));       /* Interrupt mask value */
        sbp->pic  = vectnum>15  ? PIC2  : PIC1; /* Int controller base addr */
        sbp->oldmask = inportb(sbp->pic+1);     /* Enable COM interrupt */
        if (!(sbp->oldmask & (~mask)))          /* Int already enabled? */
        {
            printf("\r\n***WARNING*** serial interrupt already in use!\n");
            printf("Other drivers may be using this port\n");
        }
        sbp->oldvector = getvect(vectnum);      /* Get current int vector */
        setvect(vectnum, handler);              /* Set to new handler */
        outportb(sbp->pic+1, sbp->oldmask & mask);
    }
    outportb(addr+3, 0x80);                     /* Set DLAB bit */
    divisor = (unsigned)(115200L / baud);       /* Write baud rate divisor */
    outportb(addr, divisor&0xff);
    outportb(addr+1, divisor>>8);
    outportb(addr+3, 3);                        /* LCR: 8 bits, 1 stop bit */
    outportb(addr+4, 0xb);                      /* MCR: set RI, RTS, DTR */
    while (inportb(addr+5) & 1)
        inportb(addr);                          /* Clear any Rx chars */
    sbp->ro = sbp->ri = 0;
    if (irq)
    {
        outportb(addr+1, 1);                    /* Enable Rx ints */
        enable();
    }
    return(addr);
}

/* Select a serial channel for initialisation or read/write  */
void selectuart(int n)
{
    serchan = n;
}

/* Set IRDA baud rate using 'LiteLink' protocol; return 0 if invalid rate */
int litelink(long baud)
{
    SERSTATUS st, stat;
    int n=0, ok=0;

    while (irbauds[n] && !(ok = baud==irbauds[n]))
        n++;                                    /* Find baud rate table index */
    if (ok)
    {
        st.rts = st.dtr = 1;                    /* Allow device to warm up */
        setserstatus(stat = st);                /* ..with dtr & rts high */
        delay(50);
        st.rts = 0;                             /* Pulse RTS low to reset */
        setserstatus(st);
        delay(2);
        setserstatus(stat);
        st.rts = 1;
        st.dtr = 0;
        while (n--)                             /* Loop using table index.. */
        {
            delay(2);
            setserstatus(st);                   /* ..pulse DTR low */
            delay(2);
            setserstatus(stat);                 /* ..then high */
        }
        delay(2);
    }
    return(ok);
}

/* Clear down UART */
void clearuart(void)
{
    SERBUFF *sbp;
    unsigned vectnum;

    sbp = &serbuffs[serchan];
    if (sbp->uart)
    {                                           /* Interrupt vector */
        vectnum = sbp->irq>7 ? sbp->irq+0x68 : sbp->irq+8;
        disable();
        outportb(sbp->uart+1, 0);               /* Disable COM interrupts */
        outportb(sbp->uart+3, 0);
        outportb(sbp->uart+4, 0);
        outportb(sbp->pic+1, sbp->oldmask);     /* Restore mask and vector */
        setvect(vectnum, sbp->oldvector);
        enable();
        sbp->uart = 0;
    }
}

/* Handler for serial interrupts */
void interrupt hand1(void)
{
    ser_handler(&serbuffs[0]);
}
void interrupt hand2(void)
{
    ser_handler(&serbuffs[1]);
}
void interrupt hand3(void)
{
    ser_handler(&serbuffs[2]);
}
void interrupt hand4(void)
{
    ser_handler(&serbuffs[3]);
}
void ser_handler(SERBUFF *sbp)
{
    int ip;
    unsigned char istat;
    unsigned uart;

    uart = sbp->uart;
    while (!((istat = inportb(uart+2) & 7) & 1))
    {
        if (istat == 2)                         /* Tx interrupt */
#if TXINTS
            if (sbp->ti == sbp->to)
                outportb(sbp->uart+1, inportb(sbp->uart+1) & ~2);
            else
            {
                outportb(sbp->uart, sbp->tbuf[sbp->to]);
                sbp->to = (sbp->to + 1) & (TBUFSIZ-1);
            }
#else
            ;
#endif
        else if (istat == 4)
        {                                       /* Rx interrupt */
            ip = sbp->ri;
            sbp->rbuf[ip] = inportb(uart);      /* Save character */
            ip = (ip + 1) & (RBUFSIZ-1);
            if (ip != sbp->ro)
                sbp->ri = ip;
        }
        else if (istat == 6)
        {
            inportb(uart+5);                    /* Line status (error) int */
            inportb(uart);
        }
        else
            inportb(uart+6);                    /* Modem status int */
    }
    outportb(sbp->pic, 0x20);                   /* Ack int controller */
    if (sbp->pic != PIC1)                       /* If slave, ack master also */
        outportb(PIC1, 0x20);
}

/* Return a character from circular buffer, NOCHAR if none */
unsigned rxchar(void)
{
    unsigned char c;
    SERBUFF *sbp;

    sbp = &serbuffs[serchan];
    if (sbp->ri == sbp->ro)                     /* Ret if I/P ptr = O/P ptr */
        return(NOCHAR);
    c = sbp->rbuf[sbp->ro];                     /* Get next char */
    sbp->ro = (sbp->ro+1) & (RBUFSIZ-1);        /* Increment & wrap pointer */
    return(c);
}

/* Clear the receive buffer */
void rxclear(void)
{
    SERBUFF *sbp;

    sbp = &serbuffs[serchan];
    disable();
    sbp->ro = sbp->ri;
    enable();
}

/* Send a string out of the serial port */
void txstr(char *s)
{
    while (*s)
        txchar(*s++);
}

/* Send a character out of the serial port */
void txchar(unsigned char c)
{
    SERBUFF *sbp;

    sbp = &serbuffs[serchan];
#if TXINTS
    while (!txfree(sbp))
        ;
    disable();
    sbp->tbuf[sbp->ti] = c;
    sbp->ti = (sbp->ti + 1) & (TBUFSIZ-1);
    outportb(sbp->uart+1, inportb(sbp->uart+1) | 2);
    enable();
#else
    while (! (inportb(sbp->uart+5) & 0x20))
        ;
    outportb(sbp->uart, c);
#endif
}
#if TXINTS
/* Return non-zero if there is space in Tx buffer */
int txfree(SERBUFF *sbp)
{
    return(((sbp->ti+1)&(RBUFSIZ-1)) != sbp->to);
}
#endif
/* Get the state of the handshake lines */
SERSTATUS getserstatus(void)
{
    unsigned char b;
    SERSTATUS hs;
    SERBUFF *sbp;

    sbp = &serbuffs[serchan];
    b = inportb(sbp->uart+6);
    hs.dcd = (b & 0x80) != 0;
    hs.dsr = (b & 0x20) != 0;
    hs.cts = (b & 0x10) != 0;
    b = inportb(sbp->uart+4);
    hs.rts = (b & 0x02) != 0;
    hs.dtr = (b & 0x01) != 0;
    return(hs);
}

/* Set the state of the handshake lines */
void setserstatus(SERSTATUS hs)
{
    unsigned char b;
    SERBUFF *sbp;

    sbp = &serbuffs[serchan];
    b = inportb(sbp->uart+4);
    b = hs.rts ? b | 0x02 : b & ~0x02;
    b = hs.dtr ? b | 0x01 : b & ~0x01;
    outportb(sbp->uart+4, b);
}

#pragma exit clearuart

/* EOF */
