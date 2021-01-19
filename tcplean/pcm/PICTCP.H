/* TCP/IP functions for PIC Web server  Copyright (c) Iosoft Ltd 2000
**
** This software is only licensed for distribution with the book 'TCP/IP Lean',
** and may only be used for personal experimentation by the purchaser
** of that book, on condition that this copyright notice is retained.
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */

/* Revisions:
** v0.01 JPB 30/3/00
*/

BOOL get_tcp(void);
void put_tcp(void);
BOOL get_ping_req(void);
void put_ping_rep(void);
BOOL get_ip(void);
void put_ip(WORD len);
void inc_lword(LWORD &lw);
void add_lword(LWORD &lw, WORD val);

/* Get a TCP seqment, return 0 if error */
BOOL get_tcp(void)
{
    int hlen, n;
    BOOL ret=0;

    checkhi = checklo = 0;
    if (get_word(remport) && get_word(locport) &&   // Source & dest ports
        get_lword(rseq.l) && get_lword(rack.l) &&   // Seq & ack numbers
        get_byte(hlen) && get_byte(rflags) &&       // Header len & flags
        skip_word() && skip_lword())                // Window, csum, urgent ptr
    {
        iplen -= IPHDR_LEN;                         // Get TCP segment length
        check_byte(iplen>>8);                       // Check pseudoheader
        check_byte(iplen);
        check_lword(local.l);
        check_lword(remote.l);
        check_byte(0);
        check_byte(PCOL_TCP);
        rxout = (hlen>>2) + IPHDR_LEN;
        rpdlen = iplen - rxout + IPHDR_LEN;
        checkhi += rdcheckhi;
        checklo += rdchecklo;
        ret = (checkhi==0xff) && (checklo==0xff);
    }
    return(ret);
}

/* Put out a TCP segment. Checksum must be set to correct value for data */
void put_tcp(void)
{
    WORD len;

    checkflag = 0;                  // Ensure we're on an even byte
    put_word(locport);              // Local and remote ports
    put_word(remport);
    put_lword(rack.l);              // Seq & ack numbers
    put_lword(rseq.l);
    put_byte(TCPHDR_LEN*4);         // Header len (no options)
    put_byte(tflags);
    put_byte(0x0b);                 // Window size word
    put_byte(0xb8);
    check_lword(local.l);           // Add pseudoheader to checksum
    check_lword(remote.l);
    check_byte(0);
    check_byte(PCOL_TCP);
    len = tpdlen + TCPHDR_LEN;
    check_byte(len>>8);
    check_byte(len);
    checkflag = 0;
    put_byte(~checkhi);             // Send checksum
    put_byte(~checklo);
    put_nullw();                    // Urgent ptr
    if (!txi2c)                     // If data in RAM (i.e. not in ROM)..
        txin += tpdlen;             // ..update Tx data pointer
    tx_end();                       // Transmit the packet
}

/* Get an ICMP echo request message, return 0 if error */
BOOL get_ping_req(void)
{
    int i, n=0;
    BYTE b;
    BOOL ret=0;

    checkhi = checklo = 0;
    if (match_byte(8) && match_byte(0) && skip_word())
    {
        rpdlen = 0;
        while (skip_byte())
            rpdlen++;
        ret = (checkhi==0xff) && (checklo==0xff);
    }
    return(ret);
}

/* Put out an ICMP echo response message */
void put_ping_rep(void)
{
    int i;

    put_nullw();                    // Type and code
    checkhi = checklo = 0;          // Clear checksum
    checkflag = 0;                  // Reset flag in case odd data len
    check_bytes(&rxbuff[IPHDR_LEN+4], tpdlen);  // Calculate checksum of data
    put_byte(~checkhi);             // Checksum value
    put_byte(~checklo);
    rxout = IPHDR_LEN + 4;
    for (i=0; i<tpdlen; i++)        // Copy data
        put_byte(rxbuff[rxout++]);
    tx_end();
}

/* Send out an IP header, given data length */
void put_ip(WORD len)
{
    static BYTE id=0;

    checkhi = checklo = 0;          // Clear checksum
    checkflag = 0;
    tx_start();
    put_byte(0x45);                 // Version & hdr len */
    put_null();                     // Service
    len += IPHDR_LEN;
    put_byte(len>>8);               // Length word
    put_byte(len);
    put_null();                     // Ident word
    put_byte(++id);
    put_nullw();                    // Flags & fragment offset
    put_byte(100);                  // Time To Live
    put_byte(ipcol);                // Protocol
    check_lword(local.l);           // Include addresses in checksum
    check_lword(remote.l);
    put_byte(~checkhi);             // Checksum
    put_byte(~checklo);
    put_lword(local.l);             // Source & destination IP addrs
    put_lword(remote.l);
}

/* Increment a longword value */
void inc_lword(LWORD &lw)
{
    if (++lw.b[0] == 0)
    {
        if (++lw.b[1] == 0)
        {
            if (++lw.b[2] == 0)
                ++lw.b[3];
        }
    }
}

/* Add a 16-bit value to a longword */
void add_lword(LWORD &lw, WORD val)
{
    if ((lw.w[0] += val) < val)
        lw.w[1]++;
}

/* EOF */

