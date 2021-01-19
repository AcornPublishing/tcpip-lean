/* RTL8019AS network driver for ChipWeb on PICDEM.net board
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */
/*
** v0.01 JPB 25/11/00  Adapted from ETHERNE.C v0.25
*/

#byte   PORTD       = 8
#byte   PORTE       = 9

#BIT    NIC_RESET   = PORTE.2
#BIT    NIC_IOW_    = PORTE.1
#BIT    NIC_IOR_    = PORTE.0
#BYTE   NIC_ADDR    = PORTB
#BYTE   NIC_DATA    = PORTD
#define DATA_TO_NIC   set_tris_d(ALL_OUT);
#define DATA_FROM_NIC set_tris_d(ALL_IN);

#define PROMISC  0              /* Set non-zero to accept all packets */

/* Ethernet definitions.. */
#define MINFRAME  60
#define MINFRAMEC 64
#define CRCLEN    4
#define MAXFRAME  1514
#define MAXFRAMEC 1518

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
#define CURRP   0x07            /* current page reg for rd and wr */
#define MAR0    0x08            /* multicast addr reg 0 for rd and WR */

/* NIC page 3 register offsets */
#define RTL9346CR 0x01          /* RTL 9346 command reg */ 
#define RTL3    0x06            /* RTL config reg 3 */

/* NIC RAM definitions */
#define RAMPAGES 0x20           /* Total number of 256-byte RAM pages */
#define TXSTART  0x40           /* Tx buffer start page */
#define TXPAGES  6              /* Pages for Tx buffer */
#define RXSTART  (TXSTART+TXPAGES)      /* Rx buffer start page */
#define RXSTOP   (TXSTART+RAMPAGES-1)   /* Last Rx buffer page */
#define DCRVAL   0x48           /* Value for data config reg */

#define MACLEN   6
BYTE myeth[MACLEN] = {0x00, 0x04, 0xa3, 0, 0, 0};
int next_page;
BYTE curr_rx_page;

typedef struct {                // NIC hardware packet header
    BYTE stat;                  //     Error status
    BYTE next;                  //     Pointer to next block
    WORD len;                   //     Length of this frame incl. CRC
} NICHEADER;

typedef struct {                // Ethernet frame header 
    BYTE dest[MACLEN];          //     Dest & srce MAC addresses 
    BYTE srce[MACLEN];
    WORD pcol;                  //     Protocol 
} ETHERHEADER;

typedef struct {                // NIC and Ethernet headers combined
    NICHEADER nic;
    ETHERHEADER eth;
} NICETHERHEADER;

NICETHERHEADER nicin;           // Buffer for incoming NIC & Ether hdrs 

WORD rxin, rxout;               // Rx buffer counts (NIC RAM is buffer)
BOOL atend;

#define TXBUFFLEN 64            
BYTE txbuff[TXBUFFLEN];         // Tx buffer
int txin, txout;

/* Prototypes */
void setnic_addr(WORD addr);
WORD getnic_addr(void);
BYTE getnic_byte(void);
int getnic_data(BYTE *data, int len);
int getnic_rx(WORD addr, WORD len);
void putnic_byte(BYTE b);
void putnic_data(BYTE *data, int len);
BYTE nicwrap(int page);
BYTE innic(int reg);
void outnic(int reg, int b);

/* Prototypes for network driver */
BYTE getch_net(void);
void putch_net(BYTE b);
void check_byte(BYTE b);
void swapw(WORD &val);

/* Return non-zero if MAC address is a broadcast */
BOOL is_mac_bcast(BYTE *addr)
{
    return((addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff);
}

/* Get a byte from network buffer; if end, set flag */
BYTE getch_net(void)
{
    BYTE b=0;

    atend = rxout>=rxin;
    if (!atend)
    {
        b = getnic_byte();
        rxout++;
        check_byte(b);
    }
    return(b);
}

/* Put a byte into the network buffer */
void putch_net(BYTE b)
{
    if (txin < TXBUFFLEN)
        txbuff[txin++] = b;
    check_byte(b);
}
    
/* Set up the I/O ports, reset the NIC */
void reset_ether(void)
{
    DATA_FROM_NIC;
    NIC_ADDR = 0;
    port_b_pullups(TRUE);
    set_tris_b(0xe0);
    NIC_IOW_ = NIC_IOR_ = 1;
    NIC_RESET = 1;
    set_tris_e(ALL_OUT);
}

/* Initialise card given driver type and base addr.
** Return driver type, 0 if error */
int init_ether(void)
{
    int ok=0, i;
    BYTE b;

    reset_ether();
    delay_ms(2);
    NIC_RESET = 0;
    delay_ms(2);
    outnic(NE_RESET, innic(NE_RESET));  /* Do reset */
    delay_ms(2);
    if ((innic(ISR) & 0x80) == 0)       /* Report if failed */
    {
        printf(displays, "\nNIC init err ");
    }
    else
    {
        outnic(CMDR, 0x21);                 /* Stop, DMA abort, page 0 */
        delay_ms(2);                        /* ..wait to take effect */
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
        outnic(CMDR, 0x61);                 /* Stop, DMA abort, page 1 */
        delay_ms(2);
        for (i=0; i<6; i++)                 /* Set Phys addr */
            outnic(PAR0+i, myeth[i]);
        for (i=0; i<8; i++)                 /* Multicast accept-all */
            outnic(MAR0+i, 0xff);
        outnic(CURRP, RXSTART+1);           /* Set current Rx page */
        next_page = RXSTART + 1;
// Set LED 0 to be a 'link' LED, not 'collision' LED
// It would be nice if the following code worked, but the upper bits of the
// RTL config3 register are obstinately read-only, so it doesn't!
//        outnic(CMDR, 0xe0);                 /* DMA abort, page 3 */
//        outnic(RTL9346CR, 0xc0);            /* Write-enable config regs */
//        outnic(RTL3, 0x10);                 /* Enable 'link' LED */
//        outnic(RTL9346CR, 0x00);            /* Write-protect config regs */
        outnic(CMDR, 0x20);                 /* DMA abort, page 0 */
#if PROMISC
        outnic(RCR, 0x14);                  /* Accept broadcasts and all packets!*/
#else
        outnic(RCR, 0x04);                  /* Accept broadcasts */
#endif
        outnic(TCR, 0);                     /* Normal Tx operation */
        outnic(ISR, 0xff);                  /* Clear interrupt flags */
        outnic(CMDR, 0x22);                 /* Start NIC */
        ok = 1;
    }
    return(ok);
}

/* Get packet into buffer, return length (excl CRC), or 0 if none available */
WORD get_ether()
{
    WORD len=0, curr;
    BYTE bound;       
    WORD oset;
    int i;

    if (innic(ISR) & 0x10)              /* If Rx overrun.. */
    {
        printf(" NIC Rx overrun ");
        init_ether();                   /* ..reset controller (drastic!) */
    }
    outnic(CMDR, 0x60);                 /* DMA abort, page 1 */
    curr = innic(CURRP);                /* Get current page */
    outnic(CMDR, 0x20);                 /* DMA abort, page 0 */
    if (curr != next_page)              /* If Rx packet.. */
    {
        curr_rx_page = next_page;
        setnic_addr((WORD)next_page<<8);
        getnic_data((BYTE *)&nicin, sizeof(nicin));
        len = nicin.nic.len;            /* Take length from stored header */
        if ((nicin.nic.stat&1) && len>=MINFRAMEC && len<=MAXFRAMEC)
        {                               /* If hdr is OK, get packet */
            len -= MACLEN+MACLEN+2+CRCLEN;
            rxin = len;
            swapw(nicin.eth.pcol);
#if DEBUG
            printf(" Rx%lu>", len);
#endif            
        }
        else                            /* If not, no packet data */
        {             
            len = 0;
            printf(" NIC pkt err ");
        }                               /* Update next packet ptr */
        if (nicin.nic.next>=RXSTART && nicin.nic.next<RXSTOP)
            next_page = nicin.nic.next;
        else                            /* If invalid, use prev+1 */
        {
            printf(" NIC ptr err ");
            next_page = nicwrap(next_page + 1);
            len = 0;
        }                               /* Update boundary register */
        bound = nicwrap(next_page - 1);
        outnic(BNRY, bound);
    }
    return(len);                        /* Return length excl. CRC */
}

/* Send Ethernet packet given payload len */
void put_ether(void *data, WORD dlen)
{
    outnic(ISR, 0x0a);              /* Clear interrupt flags */
    setnic_addr(TXSTART<<8);
    putnic_data(nicin.eth.srce, MACLEN);
    putnic_data(myeth, MACLEN);
    swapw(nicin.eth.pcol);         
    putnic_data(&nicin.eth.pcol, 2);
    putnic_data(data, dlen);
}

/* Transmit the Ethernet frame */
void xmit_ether(WORD dlen)
{
    dlen += MACLEN+MACLEN+2;
    if (dlen < MINFRAME)
        dlen = MINFRAME;            /* Constrain length */
    outnic(TBCR0, dlen);            /* Set Tx length regs */
    outnic(TBCR1, dlen >> 8);                                  
    outnic(CMDR, 0x24);             /* Transmit the packet */
}

/* Copy a block from NIC Rx to Tx buffers (not crossing page boundaries) */
void copy_rx_tx(BYTE dest, BYTE srce, BYTE len)
{       
    BYTE b;
    
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    dest += sizeof(ETHERHEADER);
    srce += sizeof(NICETHERHEADER);
    while (len--)
    {
        outnic(RSAR0, srce);
        outnic(RSAR1, curr_rx_page);
        b = getnic_byte();
        outnic(RSAR0, dest);
        outnic(RSAR1, TXSTART);
        putnic_byte(b);
        srce++;
        dest++;
    }
}

/* Checksum the given number of bytes in the Rx NIC buffer */
void check_rxbytes(BYTE oset, WORD len)
{       
    BYTE b;
 
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len);                 /* Byte count */
    outnic(RBCR1, len>>8);
    outnic(CMDR, 0x0a);                 /* Start, DMA remote read */
    while (len--)                       /* Get bytes */
    {
        b = innic(DATAPORT);
        check_byte(b);
    }
}    

/* Set the 'remote DMA' address in the NIC's RAM to be accessed */
void setnic_addr(WORD addr)
{
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RSAR0, addr&0xff);           /* Data addr */
    outnic(RSAR1, addr>>8);
}

/* Get the 'remote DMA' address in the NIC's RAM to be accessed */
WORD getnic_addr(void)
{      
    WORD addr;    
    
    addr = innic(RSAR0) + ((WORD)innic(RSAR1) << 8);
    return(addr);
}

/* Return a byte from the NIC RAM */
BYTE getnic_byte(void)
{
    BYTE b;
    
    outnic(RBCR0, 1);                   /* Byte count */
    outnic(RBCR1, 0);
    outnic(CMDR, 0x0a);                 /* Start, DMA remote read */
    b = innic(DATAPORT);
}

/* Get data from NIC's RAM into the given buffer */
void getnic_data(BYTE *data, int len)
{
    BYTE b;
    
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len);                 /* Byte count */
    outnic(RBCR1, 0);
    outnic(CMDR, 0x0a);                 /* Start, DMA remote read */
    while (len--)                       /* Get bytes */
    {
        b = innic(DATAPORT);
        *data++ = b;
    }
}

/* Put the given byte into the NIC's RAM */
void putnic_byte(BYTE b)
{
    outnic(RBCR0, 1);                   /* Byte count */
    outnic(RBCR1, 0);
    outnic(CMDR, 0x12);                 /* Start, DMA remote write */
    outnic(DATAPORT, b);
}

/* Put the given byte into the NIC's RAM */
void putnic_checkbyte(BYTE b)
{
    check_byte(b);
    putnic_byte(b);
    tpxdlen++;
}

/* Put the given data into the NIC's RAM */
void putnic_data(BYTE *data, int len)
{
    len += len & 1;                     /* Round length up to an even value */
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len);                 /* Byte count */
    outnic(RBCR1, 0);
    outnic(CMDR, 0x12);                 /* Start, DMA remote write */
    while (len--)                        /* O/P bytes */
        outnic(DATAPORT, *data++);
    len = 255;                          /* Done: must ensure DMA complete */
    while (len && (innic(ISR)&0x40)==0)
        len--;
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
    BYTE b;

    DATA_FROM_NIC;
    NIC_ADDR = reg;
    NIC_IOR_ = 0;
    b = NIC_DATA;
    NIC_IOR_ = 1;
    return(b);
}

/* Output a byte to a NIC register */
void outnic(int reg, int b)
{
    NIC_ADDR = reg;
    NIC_DATA = b;
    DATA_TO_NIC;
    NIC_IOW_ = 0;
    delay_cycles(1);
    NIC_IOW_ = 1;
    DATA_FROM_NIC;
}

/* EOF */
