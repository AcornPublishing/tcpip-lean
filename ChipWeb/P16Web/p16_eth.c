/* RTL8019AS network driver for ChipWeb - Copyright (c) Iosoft Ltd 2001
**
** This source code is only licensed for distribution in the Iosoft ChipWeb
** package, and the purchaser of that package is granted the non-exclusive
** right to use the software for personal experimentation only, provided
** that this copyright notice is retained. All other rights are retained by
** Iosoft Ltd.
**
** Redistribution of this source code is not permitted. Binary images derived
** from the source code may only be redistributed if a commercial license is
** obtained; see www.iosoft.co.uk or email license@iosoft.co.uk
**
** The software is supplied 'as-is' for development purposes, without warranty
** of any kind, either expressed or implied, including, but not limited to,
** the implied warranties of merchantability and fitness for purpose.
** In no event will Iosoft Ltd. be liable for damages, including any general,
** special, incidental or consequential damages arising out of the use or
** inability to use the software, including, but not limited to, loss or
** corruption of data, or losses sustained by the developer or third parties,
** or failure of the software to operate with any other software or systems.
** This license shall be governed by the laws of England. */

#define DROP_TX 0       /* If non-zero, drop 1 in n Tx packets for debug */

#ifndef NET_TXBUFFERS
#define NET_TXBUFFERS 1         // Number of network transmit buffers
#endif

/* First 3 bytes of MAC address are assigned by the IEEE to a specific
** organisation, last 3 bytes will contain the board serial number */
#define MACBASE_ADDR {0x00, 0x04, 0xa3, 0, 0, 0}

DEFBIT_2(PORTE, NIC_RESET)      /* I/O Definitions */
DEFBIT_1(PORTE, NIC_IOW_)
DEFBIT_0(PORTE, NIC_IOR_)
#define NIC_ADDR PORTB
#define NIC_DATA PORTD
#define DATA_TO_NIC   set_tris_d(ALL_OUT)
#define DATA_FROM_NIC set_tris_d(ALL_IN)
#define TRISB_VAL   0x20        /* Port B, bit 5 pushbutton I/P */

#define PROMISC  0              /* Set non-zero to accept all packets */

/* Ethernet definitions.. */
#define CRCLEN    4             /* Length of Ethernet CRC */
#define MINFRAME  60            /* Min frame size without CRC */
#define MAXFRAME  1514          /* Max frame size without CRC */
#define MINFRAMEC (MINFRAME+CRCLEN) /* Ditto including CRC */
#define MAXFRAMEC (MAXFRAME+CRCLEN)

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
#define TXPAGES  6              /* Pages for one Tx buffer */
#define RXSTART  (TXSTART+(TXPAGES*NET_TXBUFFERS))  /* Rx buffer start page */
#define RXSTOP   (TXSTART+RAMPAGES-1)               /* Rx buffer end page */
#define DCRVAL   0x48           /* Value for data config reg */

#define MACLEN   6              /* My MAC address */
BYTE myeth[MACLEN] = MACBASE_ADDR;

#define RXBUFFLEN 42            // Size of Rx buffer
#define RXMARGIN  10            // Minimum number of pushback bytes
BANK1 BYTE next_page, curr_rx_page;
BANK2 WORD curr_rx_addr;
BANK2 WORD net_rxin;            // Length of incoming packet in NIC
BANK2 BOOL rx_checkoff;         // Flag to disable Rx checksumming
BANK2 WORD rxout;               // Length of packet processed so far
BANK2 BYTE rxbuffin, rxbuffout; // I/O pointers for Rx process buffer
BANK2 BYTE rxbuff[RXBUFFLEN];   // Rx buffer
LOCATE(rxbuff, 0x110)

BANK3 BYTE nic_tx_transfer;     // Flag set if Tx data is being sent to NIC

#define TXBUFFLEN 42            // Size of Tx buffer
#define TXMARGIN  10            // Buffer to be emptied when this space remains
BANK3 WORD net_txlen;           // Max length of Tx data sent to NIC
BANK3 WORD txin;                // Current I/P pointer for Tx data
BANK3 BYTE txbuffin;            // I/P pointer for Tx process buffer
BANK3 BYTE txbuff[TXBUFFLEN];   // Tx buffer
LOCATE(txbuff, 0x190)
#if NET_TXBUFFERS > 1
BANK3 BYTE txbuffnum;           // Number of Tx buffer, if using more than 1
BANK3 WORD txbuff_lens[NET_TXBUFFERS];  // Length of last buffer transmission
#endif

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

#define ETHERHDR_LEN (sizeof(ETHERHEADER))
#define MAXNET_DLEN (MAXFRAME-ETHERHDR_LEN)

typedef struct {                // NIC and Ethernet headers combined
    NICHEADER nic;
    ETHERHEADER eth;
} NICETHERHEADER;

BANK1 NICETHERHEADER host;      // Buffer for incoming NIC & Ether hdrs

/* Prototypes for this file */
void init_rxbuff(void);
void init_txbuff(BYTE buffnum);
BOOL setpos_rxout(WORD newpos);
BOOL setpos_txin(WORD newpos);
BYTE load_rxbuff(BYTE margin, BYTE count);
void save_txbuff(void);
BOOL init_net(void);
void reset_ether(void);
BOOL get_net(void);
void transmit(void);
void setnic_addr(WORD addr);
void setnic_tx(WORD addr);
void setnic_rx(WORD addr);
void getnic_rxbuff(BYTE len);
void putnic_txbuff(void);
BYTE nicwrap(BYTE page);
BYTE innic(BYTE reg);
void outnic(BYTE reg, BYTE b);

/* Prototypes for other files */
WORD get_data(BYTE *ptr, WORD maxlen);
BOOL put_byte(BYTE b);
void put_data(BYTE *data, WORD len);
WORD swapw(WORD val);
void check_byte(BYTE b);

/* Initialise the receive buffer */
void init_rxbuff(void)
{
    rxout = rxbuffin = rxbuffout = 0;
    rx_checkoff = arp_resp = 0;
    setnic_rx(0);
}

/* Initialise the transmit buffer
** If more than one Tx frame, select which */
void init_txbuff(BYTE buffnum)
{
#if NET_TXBUFFERS > 1
    txbuffnum = buffnum;
#endif
    txin = net_txlen = txbuffin = 0;
    checkflag = checkhi = checklo = 0;
    setnic_tx(0);
}

/* Move the Rx O/P pointer to the given location, return 0 if beyond data */
BOOL setpos_rxout(WORD newpos)
{
    if (newpos > net_rxin)
        return(0);
    setnic_rx(newpos);
    rxout = newpos;
    rxbuffin = rxbuffout = 0;
    return(1);
}

/* Truncate the remaining Rx data to the given length */
void truncate_rxout(WORD len)
{
    WORD end;

    if ((end = rxout+len) < net_rxin)
        net_rxin = end;
    if ((end = rxbuffout+len) < rxbuffin)
        rxbuffin = end;
}

/* Return the number of Rx data bytes left in buffer */
WORD rxleft(void)
{
    return(net_rxin - rxout);
}

/* Move the Rx O/P pointer to the given location, return 0 if beyond data */
BOOL setpos_txin(WORD newpos)
{
    if (newpos > MAXFRAME-ETHERHDR_LEN)
        return(0);
    save_txbuff();
    setnic_tx(txin = newpos);
    return(1);
}

/* Return amount the free space left in Tx buffer */
WORD txleft(void)
{
    return(MAXFRAME-ETHERHDR_LEN - txin);
}

/* Load a given number of bytes from the NIC into the Rx buffer
** Remove any processed data from the buffer, but allow a margin
** of data to be retained. Return the actual number of bytes loaded */
BYTE load_rxbuff(BYTE margin, BYTE count)
{
    BYTE in, n;
    WORD rxleft;

    if (rxbuffout > margin)
    {
        in = margin;
        while (rxbuffout<rxbuffin)
            rxbuff[in++] = rxbuff[rxbuffout++];
        rxbuffin = in;
        rxbuffout = margin;
    }
    rxleft = net_rxin - rxout;
    count = (BYTE)MIN(count, rxleft);
    n = count = (BYTE)MIN(count, RXBUFFLEN-rxbuffin);
    if (n)
    {
        if (nic_tx_transfer)
            setnic_rx(rxout);
        getnic_rxbuff(count);
    }
    return(count);
}

/* Save the contents of the Tx buffer into the NIC */
void save_txbuff(void)
{
    if (txbuffin)
    {
        if (!nic_tx_transfer)
            setnic_tx(txin - txbuffin);
        putnic_txbuff();
        txbuffin = 0;
    }
    if (txin > net_txlen)
        net_txlen = txin;
}

/* Get an incoming byte value, return 0 if end of message */
BOOL get_byte(BYTE *bp)
{
    BYTE b;

    if (rxbuffout >= rxbuffin)
        load_rxbuff(RXMARGIN, RXBUFFLEN);
    if (rxbuffout >= rxbuffin)
        return(0);
    b = rxbuff[rxbuffout++];
    rxout++;
    if (!rx_checkoff)
        check_byte(b);
    *bp = b;
#if DEBUG
    disp_hexbyte(b);
#endif
    return(1);
}

/* Push back a byte value, return 0 if no room */
BOOL unget_byte(BYTE b)
{
    if (rxbuffout && rxout)
    {
        rxbuff[--rxbuffout] = b;
        rxout--;
        return(1);
    }
    return(0);
}

/* Send a byte to the network buffer, then add to checksum
** Return 0 if no more data can be accepted */
BOOL put_byte(BYTE b)
{
    if (txin >= MAXFRAME)
        return(0);
    if (txbuffin >= TXBUFFLEN-TXMARGIN)
        save_txbuff();
    txbuff[txbuffin++] = b;
    txin++;
    check_byte(b);
    return(1);
}

/* Copy data from Rx to Tx buffers, return actual byte count */
WORD copy_rx_tx(WORD maxlen)
{
    BYTE n, done=0;
    WORD count=0;

    save_txbuff();
    while (load_rxbuff(0, RXBUFFLEN) || rxbuffin>0)
    {
        n = rxbuffin;
        if (count+n >= maxlen)
        {
            done = 1;
            n = (BYTE)(maxlen - count);
        }
        for (rxbuffout=0; rxbuffout<n; rxbuffout++)
            put_byte(rxbuff[rxbuffout]);
        rxout += n;
        count += n;
        if (done)
            break;
    }
    net_txlen = MAX(txin, net_txlen);
    return(count);
}

/* Initialise NIC, return 0 if error */
BOOL init_net(void)
{
    BYTE i;

    reset_ether();
    delay_ms(2);
    NIC_RESET = 0;
    delay_ms(2);
    outnic(NE_RESET, innic(NE_RESET));  /* Do reset */
    delay_ms(2);
    if ((innic(ISR) & 0x80) == 0)       /* Report if failed */
    {
        put_ser("\nNIC init err ");
        return(0);
    }
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
    return(1);
}

/* Set up the I/O ports, reset the NIC */
void reset_ether(void)
{
    DATA_FROM_NIC;
    NIC_ADDR = 0;
    port_b_pullups(TRUE);
    set_tris_b(TRISB_VAL);
    NIC_IOW_ = NIC_IOR_ = 1;
    NIC_RESET = 1;
    set_tris_e(ALL_OUT);
}

/* Get packet into buffer, return 0 if none available */
BOOL get_net(void)
{
    WORD curr;
    BYTE bound;

    net_rxin = 0;
    if (innic(ISR) & 0x10)              /* If Rx overrun.. */
    {
        put_ser(" NIC Rx overrun ");
        init_net();                     /* ..reset controller (drastic!) */
    }
    outnic(CMDR, 0x60);                 /* DMA abort, page 1 */
    curr = innic(CURRP);                /* Get current page */
    outnic(CMDR, 0x20);                 /* DMA abort, page 0 */
    if (curr != next_page)              /* If Rx packet.. */
    {
        curr_rx_page = next_page;
        curr_rx_addr = (WORD)next_page << 8;
        init_rxbuff();
        getnic_rxbuff(sizeof(host));
        get_data((BYTE *)&host, sizeof(host));
        curr_rx_addr += sizeof(host);
        init_rxbuff();
        net_rxin = host.nic.len;        /* Take length from stored header */
        if ((host.nic.stat&1) && net_rxin>=MINFRAMEC && net_rxin<=MAXFRAMEC)
        {                               /* If hdr is OK, get packet */
            net_rxin -= ETHERHDR_LEN + CRCLEN;
            host.eth.pcol = swapw(host.eth.pcol);
#if DEBUG
            print_serial = TRUE;        /* Diagnostic printout of pkt size */
            print_net = print_lcd = FALSE;
            put_ser(" Rx");             /* Include 14-byte MAC hdr in size */
            disp_decword(net_rxin + ETHERHDR_LEN);
            serial_putch('>');
#endif
        }
        else                            /* If not, no packet data */
        {
            net_rxin = 0;
            put_ser(" NIC pkt err ");
        }                               /* Update next packet ptr */
        if (host.nic.next>=RXSTART && host.nic.next<RXSTOP)
            next_page = host.nic.next;
        else                            /* If invalid, use prev+1 */
        {
            net_rxin = 0;
            put_ser(" NIC ptr err ");
            next_page = nicwrap(next_page + 1);
        }                               /* Update boundary register */
        bound = nicwrap(next_page - 1);
        outnic(BNRY, bound);
    }
    return(net_rxin != 0);
}

/* Transmit the Ethernet frame */
void transmit(void)
{
    WORD dlen;
#if DROP_TX
    static BYTE dropcount=0;
#endif
    dlen = net_txlen;
    save_txbuff();
    setnic_tx(-ETHERHDR_LEN);
    txbuffin = 0;
    put_data(host.eth.srce, MACLEN);    /* Destination addr */
    put_data(myeth, MACLEN);            /* Source addr */
    put_byte((BYTE)(host.eth.pcol>>8)); /* Protocol */
    put_byte((BYTE)host.eth.pcol);
    dlen += ETHERHDR_LEN;               /* Bump up length for MAC header */
    putnic_txbuff();
    if (dlen < MINFRAME)
        dlen = MINFRAME;                /* Constrain length */
#if NET_TXBUFFERS > 1
    txbuff_lens[txbuffnum] = dlen;
    outnic(TPSR, txbuffnum ? (TXSTART+TXPAGES) : TXSTART);
#endif
    outnic(TBCR0, (BYTE)dlen);          /* Set Tx length regs */
    outnic(TBCR1, (BYTE)(dlen >> 8));
#if DROP_TX
    if (++dropcount == DROP_TX)
    {
        dropcount = 0;
        return;
    }
#endif
    outnic(CMDR, 0x24);                 /* Transmit the packet */
}
#if NET_TXBUFFERS > 1
/* Retransmit the Ethernet frame */
void retransmit(BYTE buffnum)
{
    WORD dlen;

    outnic(TPSR, buffnum ? (TXSTART+TXPAGES) : TXSTART);
    dlen = txbuff_lens[buffnum];
    outnic(TBCR0, (BYTE)dlen);          /* Set Tx length regs */
    outnic(TBCR1, (BYTE)(dlen >> 8));
    outnic(CMDR, 0x24);                 /* Transmit the packet */
}
#endif
/* Set the 'remote DMA' address in the NIC's RAM to be accessed */
void setnic_addr(WORD addr)
{
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RSAR0, addr&0xff);           /* Data addr */
    outnic(RSAR1, addr>>8);
}

/* Set the 'remote DMA' address in the NIC Tx packet buffer */
void setnic_tx(WORD addr)
{                                       /* Add on Tx buffer offset */
    addr += (TXSTART << 8) + ETHERHDR_LEN;
#if NET_TXBUFFERS > 1
    if (txbuffnum)                      /* ..additional offset if 2nd buffer */
        addr += (TXPAGES << 8);
#endif
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RSAR0, (BYTE)addr);          /* Remote DMA addr */
    outnic(RSAR1, (BYTE)(addr>>8));
    nic_tx_transfer = 1;
}

/* Set the 'remote DMA' address in the current NIC Rx packet buffer */
void setnic_rx(WORD addr)
{
    addr += curr_rx_addr;
    if (addr >= RXSTOP*256)
        addr += (RXSTART - RXSTOP)*256;
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RSAR0, (BYTE)addr);          /* Data addr */
    outnic(RSAR1, (BYTE)(addr>>8));
    nic_tx_transfer = 0;
}

/* Get data from NIC's RAM into the Rx buffer */
void getnic_rxbuff(BYTE len)
{
    if (len == 0)
        return;
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len);                 /* Byte count */
    outnic(RBCR1, 0);
    outnic(CMDR, 0x0a);                 /* Start, DMA remote read */
    NIC_ADDR = DATAPORT;
    DATA_FROM_NIC;
    while (len--)                       /* Get bytes */
    {
        NIC_IOR_ = 0;
        rxbuff[rxbuffin++] = NIC_DATA;
        NIC_IOR_ = 1;
    }
}

/* Put the given data into the NIC's RAM from the Tx buffer */
void putnic_txbuff(void)
{
    BYTE n=0, len;

    if ((len = txbuffin) == 0)
        return;
    len += len & 1;                     /* Round length up to an even value */
    outnic(ISR, 0x40);                  /* Clear remote DMA interrupt flag */
    outnic(RBCR0, len);                 /* Byte count */
    outnic(RBCR1, 0);
    outnic(CMDR, 0x12);                 /* Start, DMA remote write */
    NIC_ADDR = DATAPORT;
    DATA_TO_NIC;
    while (len--)                        /* O/P bytes */
    {
        NIC_DATA =  txbuff[n++];
        NIC_IOW_ = 0;
        DELAY_ONE_CYCLE;
        NIC_IOW_ = 1;
    }
    DATA_FROM_NIC;
    len = 255;                          /* Done: must ensure DMA complete */
    while (len && (innic(ISR)&0x40)==0)
        len--;
}

/* Wrap an NIC Rx page number */
BYTE nicwrap(BYTE page)
{
   if (page >= RXSTOP)
       page += RXSTART - RXSTOP;
   else if (page < RXSTART)
       page += RXSTOP - RXSTART;
   return(page);
}

/* Input a byte from a NIC register */
BYTE innic(BYTE reg)
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
void outnic(BYTE reg, BYTE b)
{
    NIC_ADDR = reg;
    NIC_DATA = b;
    DATA_TO_NIC;
    NIC_IOW_ = 0;
    DELAY_ONE_CYCLE;
    NIC_IOW_ = 1;
    DATA_FROM_NIC;
}

/* EOF */
