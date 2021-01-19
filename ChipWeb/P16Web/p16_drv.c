/* Low-level driver functions for ChipWeb - Copyright (c) Iosoft Ltd 2001
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

#define INCLUDE_STR_MATCH 0     // Set non-zero to include string matching

/* Prototypes for this file */
BOOL get_word(WORD *wp);
BOOL get_lword(LWORD *lwp);
WORD get_data(BYTE *ptr, WORD maxlen);
BOOL unget_data(BYTE n);
BOOL match_byte(BYTE b);
BOOL match_word(WORD w);
BOOL match_lword(LWORD *lwp);
void skip_space(void);
BOOL skip_byte(void);
BOOL skip_data(WORD n);
void check_byte(BYTE b);
void check_word(WORD w);
void check_lword(LWORD *lwp);
void check_data(BYTE *dat, WORD len);
void put_word(WORD w);
void put_lword(LWORD *lwp);
void put_data(BYTE *data, WORD len);
WORD swapw(WORD val);
#define skip_word() skip_data(2)
#define skip_lword() skip_data(4)

/* Other prototypes */
BOOL get_byte(BYTE *bp);
BOOL put_byte(BYTE b);
WORD copy_rx_tx(WORD maxlen);

/* Get an incoming word value, return 0 if end of message */
BOOL get_word(WORD *wp)
{
    BYTE hi, lo;
    WORD w;

    DEBUG_PUTC('.');
    if (get_byte(&hi) && get_byte(&lo))
    {
        w = ((WORD)hi<<8) | (WORD)lo;
        *wp = w;
        return(1);
    }
    return(0);
}

/* Get an incoming lword value, return 0 if end of message */
BOOL get_lword(LWORD *lwp)
{
    BYTE n=4, b;

    DEBUG_PUTC('.');
#ifdef HI_TECH_C
    while (n && get_byte(&b))
        lwp->b[--n] = b;
    return(n==0);
#else
    if (get_byte(&lwp->b[3]) && get_byte(&lwp->b[2]) &&
        get_byte(&lwp->b[1]) && get_byte(&lwp->b[0]))
        return(1);
    return(0);
#endif
}

/* Get the given number of bytes from Rx buffer, return actual number
** If buffer pointer is null, discard the bytes */
WORD get_data(BYTE *ptr, WORD maxlen)
{
    BYTE b;
    WORD n=0;

    DEBUG_PUTC('.');
    while (n<maxlen && get_byte(&b))
    {
        if (ptr)
            *ptr++ = b;
        n++;
    }
    return(n);
}

/* Get a decimal word value, return 0 if no digits */
BOOL get_num(WORD *valp)
{
    WORD val=0;
    BYTE c, n=0;

    while (get_byte(&c))
    {
        if (c>='0' && c<='9')
        {
            n++;
            c -= '0';
            val = (val * 10) + c;
        }
        else
        {
            unget_byte(c);
            break;
        }
    }
    *valp = val;
    return(n > 0);
}

/* Match an incoming byte value, return 0 not matched, or end of message */
BOOL match_byte(BYTE b)
{
    BYTE val;

    if (get_byte(&val) && b==val)
        return(1);
    return(0);
}
/* Match an incoming byte value, return 0 not matched, or end of message */
BOOL match_word(WORD w)
{
    WORD inw;

    if (get_word(&inw) && inw==w)
        return(1);
    return(0);
}

/* Match longword */
BOOL match_lword(LWORD *lwp)
{
    DEBUG_PUTC('.');
    if (match_byte(lwp->b[3]) && match_byte(lwp->b[2]) &&
        match_byte(lwp->b[1]) && match_byte(lwp->b[0]))
        return(1);
    return(0);
}
/* Match incoming data */
BYTE match_data(BYTE *ptr, BYTE len)
{
    BYTE b;

    DEBUG_PUTC('.');
    while (len && get_byte(&b) && *ptr==b)
    {
        ptr++;
        len--;
    }
    if (len==0)
        return(1);
    return(0);
}
/* Skip an incoming byte value, return 0 if end of message */
BOOL skip_byte(void)
{
    BYTE b;

    DEBUG_PUTC('.');
    if (get_byte(&b))
        return(1);
    return(0);
}

/* Skip the given number of incoming bytes */
BOOL skip_data(WORD n)
{
    BYTE b;

    DEBUG_PUTC('.');
    while (n && get_byte(&b))
        n--;
    if (n == 0)
        return(1);
    return(0);
}

/* Add byte to checksum value */
void check_byte(BYTE b)
{
    if (checkflag)
    {
        checklo += b;
        if (checklo < b)
        {
            if (++checkhi == 0)
                checklo++;
        }
    }
    else
    {
        checkhi += b;
        if (checkhi < b)
        {
            if (++checklo == 0)
                checkhi++;
        }
    }
    checkflag = !checkflag;
}

/* Add word to checksum value */
void check_word(WORD w)
{
    check_byte(w>>8);
    check_byte(w);
}
/* Add longword to checksum value */
void check_lword(LWORD *lwp)
{
    check_byte(lwp->b[3]);
    check_byte(lwp->b[2]);
    check_byte(lwp->b[1]);
    check_byte(lwp->b[0]);
}

/* Add array of bytes to checksum value */
void check_data(BYTE *dat, WORD len)
{
    while (len--)
        check_byte(*dat++);
}

/* Put a word in the network buffer, then add to checksum */
void put_word(WORD w)
{
    put_byte((BYTE)(w >> 8));
    put_byte((BYTE)w);
}

/* Put a longword in the network buffer, then add to checksum */
void put_lword(LWORD *lwp)
{
    put_byte(lwp->b[3]);
    put_byte(lwp->b[2]);
    put_byte(lwp->b[1]);
    put_byte(lwp->b[0]);
}

/* Put data block in the network buffer, then add to checksum */
void put_data(BYTE *data, WORD len)
{
    while (len--)
        put_byte(*data++);
}

/* Swap the bytes in a word */
WORD swapw(WORD val)
{
    WORD ret;

    ret = (val >> 8) | (val << 8);
    return(ret);
}

/* EOF */
