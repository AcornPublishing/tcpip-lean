/* Video capture functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#if (CPU_CLK>20000000 || INCLUDE_VID==2)
#define VID_BLINES  3           // Picture lines per block
#define VID_WIDTH   412         // Picture width
#define VID_DEPTH   285         // Picture depth
#define TOPMARGIN   22          // Unused margin at top of frame
#define LEFTMARGIN  80          // Unused margin on left of frame
#else
#define VID_BLINES  6           // Picture lines per block
#define VID_WIDTH   207         // Small picture width
#define VID_DEPTH   285         // Small picture depth
#define TOPMARGIN   22          // Unused margin at top of frame
#define LEFTMARGIN  40          // Unused margin on left of frame
#endif

#define VID_BLEN    (VID_WIDTH * VID_BLINES)
#define VID_BLOCKS  (VID_DEPTH / VID_BLINES)

#define VMODE_HALT  1           // Incoming video mode byte

// Capture states
#define CAPSTART    1
#define CAPWAIT     2
#define CAPTURING   3
#define CAPTURED    4
#define CAPFAIL     5

// Timeout value for video capture (waiting for complete field)
#define CAPTIMEOUT (SECTICKS/4) // Allow 0.25 sec

// Capture hardware bit definitions
#if INCLUDE_VID==1      // Video hardware
DEFBIT_1(PORTA, CAP_RSTR)       // Reset read on field memory
DEFBIT_2(PORTA, CAP_IE)         // Input enable
DEFBIT_3(PORTA, CAP_RE)         // Read enable
DEFBIT_0(PORTC, CAP_OE)         // Output enable
DEFBIT_1(PORTC, CAP_BUSY)       // Odd/even field is busy indication
DEFBIT_7(PORTD, CAP_SYNC)       // m.s.bit of data is sync
#endif
#if INCLUDE_VID==2     // Old video hardware
DEFBIT_7(PORTB, CAP_RSTR)       // Reset read on field memory
DEFBIT_2(PORTA, CAP_IE)         // Input enable
DEFBIT_1(PORTC, CAP_RE)         // Read enable
DEFBIT_0(PORTC, CAP_OE)         // Output enable
DEFBIT_2(PORTC, CAP_BUSY)       // Odd/even field is busy indication
DEFBIT_7(PORTD, CAP_SYNC)       // m.s.bit of data is sync
DEFBIT_3(PORTA, CAP_SLOW)       // Select slow clock rate
#endif

#if INCLUDE_VID==1          // Video hardware:
#define TRISA_VAL   0x01        // Port A, bit 0 analog I/P
#define TRISC_VAL   0xfe        // Port C, RS232, i2c, etc
#else                       // Old video hardware:
#define TRISA_VAL   0x01        // Port A, bit 0 analog I/P
#define TRISC_VAL   0xdc        // Port C, RS232, i2c, etc
#endif

BYTE capstate;                  // Capture state variable

void capnic_data(WORD n);
void skiptosync(void);
void skipsync(void);
void capzero(void);
void skiplines(BYTE n);
void skipcap(BYTE n);
void check_cap(void);
void startcap(void);
void stopcap(void);

/* Handler for incoming UDP video request */
void vid_handler(void)
{
    WORD line;
    BYTE n, b, vmode, blocknum, nblocks;

    if (get_byte(&vmode) &&
             get_byte(&blocknum) &&
             get_byte(&nblocks))
    {
        udp_checkoff = 1;                   // Send null checksum
        if (nblocks == 0)                   // If zero block count..
            nblocks = VID_BLOCKS;           // ..return the lot!
        stopcap();                          // Stop video capture
#if INCLUDE_VID==2
        CAP_SLOW = 1;
#endif
//        tpdlen = VID_BLINES * (4 + VID_WIDTH);
        line = 0;
        capzero();
        skiplines(TOPMARGIN);               // Skip top margin
        while (blocknum--)                  // Skip unwanted blocks
        {
            skiplines(VID_BLINES);
            line += VID_BLINES;
        }
        for (b=0; b<nblocks; b++)       // For each network block..
        {
            scan_io();                      // Keep timer alive
            check_cap();
            init_txbuff(0);
            setpos_txin(UDPIPHDR_LEN);
            for (n=0; n<VID_BLINES; n++,line++)
            {                               // For each line in block..
                skiptosync();
                skipcap(LEFTMARGIN);        // Skip left picture margin
                put_byte(line >> 8);        // Put out line number
                put_byte((BYTE)line);
                if (line < VID_DEPTH)       // ..and if any lines left..
                {                           // ..the width of line
                    put_byte(VID_WIDTH >> 8);
                    put_byte(VID_WIDTH & 0xff);
                    save_txbuff();          // Copy hdr to NIC
                    capnic_data(VID_WIDTH); // Copy image data to NIC
                    txin += VID_WIDTH;
                    save_txbuff();          // Update packet length
                }
                else                        // No lines left
                {
                    put_word(0);            // Put out null width
                }
            }
            udp_xmit();
            DEBUG_PUTC('U');
            delay_ms(1);                    // Delay for NIC to start Tx
        }
#if INCLUDE_VID==2
        CAP_SLOW = 0;
#endif
        if (vmode != VMODE_HALT)            // If not halted..
            startcap();                     // ..restart capture
    }
}

/* Copy the given byte count from capture RAM into the NIC RAM */
void capnic_data(WORD n)
{
    outnic(RBCR0, (BYTE)n);         // Byte count
    outnic(RBCR1, n>>8);
    outnic(CMDR, 0x12);             // Start, DMA remote write
    NIC_ADDR = DATAPORT;
    CAP_OE = 1;                     // Enable RAM O/P
    while (n--)
    {
        CAP_RE = 1;                 // Read from RAM
        CAP_RE = 0;
        NIC_IOW_ = 0;               // Write to NIC
        NIC_IOW_ = 1;
    }
    CAP_OE = 0;                     // Disable RAM O/P
}

/* Skip data until sync is reached */
void skiptosync(void)
{
    BYTE n=255;

    CAP_OE = 1;
    while (n-- && !CAP_SYNC)
    {
        CAP_RE = 1;
        CAP_RE = 0;
    }
    CAP_OE = 0;
}

/* Reset RAM O/P address counter */
void capzero(void)
{
    CAP_RSTR = 1;
    DELAY_ONE_CYCLE;
    DELAY_ONE_CYCLE;
    DELAY_ONE_CYCLE;
    CAP_RSTR = 0;
}

/* Skip the given number of video lines (max 255) */
void skiplines(BYTE n)
{
    while (n--)
    {
#if (CPU_CLK>20000000 || INCLUDE_VID==2)
        skipcap(255);
#endif
        skipcap(200);
        skiptosync();
    }
}

/* Skip a given number of capture cycles (max 255) */
void skipcap(BYTE n)
{
    do
    {
        CAP_RE = 1;
        CAP_RE = 0;
    } while (--n);
}

/* Switch on the capture */
void startcap(void)
{
    CAP_IE = 1;
    capstate = CAPSTART;
    check_cap();
}

/* Switch off the capture: ensure at least 1 field was captured */
void stopcap(void)
{
    check_cap();
    while (capstate==CAPSTART || capstate==CAPWAIT || capstate==CAPTURING)
    {
        scan_io();
        check_cap();
    }
    CAP_IE = 0;
}

/* Check the capture state machine; called every timer tick */
void check_cap(void)
{
    static WORD capticks;
    BYTE state=0;

    if (capstate==CAPSTART || capstate==CAPTURED)
    {
        if (!CAP_BUSY)
            state = CAPWAIT;
    }
    else if (capstate == CAPWAIT)
    {
        if (CAP_BUSY)
            state = CAPTURING;
    }
    else if (capstate == CAPTURING)
    {
        if (!CAP_BUSY)
            state = CAPTURED;
    }
    else if (capstate == CAPTURED)
    {
        if (CAP_BUSY)
            state = CAPTURING;
    }
    if (state)                      // Refresh timer if state-change
    {
        capstate = state;
        timeout(&capticks, 0);
    }
    else if (timeout(&capticks, CAPTIMEOUT))
        capstate = CAPFAIL;         // Error if timeout
}

/* EOF */

