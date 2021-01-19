/* Low-level driver functions for PIC Web server 
** Copyright (c) Iosoft Ltd 2001
** This software is only licensed for distribution in the Iosoft ChipWeb package
** and may only be used for personal experimentation by the purchaser 
** of that package, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */

/* Revisions:
** v0.01 JPB 17/12/00 Functions extracted from PIC_SLIP v0.01
*/

/* Globals */
BOOL checkflag;             // Checksum flag & byte values
BYTE checkhi, checklo;

BYTE ungot_byte;
BOOL ungot;

/* Prototypes for this file */
int get_data(BYTE *ptr, int maxlen);
BOOL get_byte(BYTE &b);
void unget_byte(BYTE &b);
BOOL get_word(WORD &w);
BOOL get_lword(LWORD &lw);
void discard_data(void);
BOOL match_byte(BYTE b);
BOOL match_word(WORD w);
void skip_space(void);
BOOL skip_byte();
BOOL skip_word();
BOOL skip_lword();
int check_str(char *s);
void check_byte(BYTE b);
void check_word(WORD w);
void check_lword(LWORD &lw);
void check_bytes(BYTE *dat, int len);
void put_byte(BYTE b);
void put_str(char *s);
void put_word(WORD w);
void put_data(BYTE *data, int len);
void put_lword(LWORD &lw);
void swapw(WORD &val);

/* Get incoming data in buffer; discard any buffer overflow */
int get_data(BYTE *ptr, int maxlen)
{
    BYTE b;
    int n=0;

    while (maxlen-- > 0 && get_byte(b))
    {
        *ptr++ = b;
        n++;
    }
    discard_data();
    return(n);
}

/* Discard incoming data */
void discard_data(void)
{
    while (!atend)
        getch_net();
}

/* Get an incoming byte value, return 0 if end of message */
BOOL get_byte(BYTE &b)
{
    if (ungot)
        b = ungot_byte;
    else
        b = getch_net();
    ungot = 0;
    return(!atend);
}

/* Unget (push back) an incoming byte value */
void unget_byte(BYTE &b)
{
    ungot_byte = b;
    ungot = 1;
}

/* Get an incoming word value, return 0 if end of message */
BOOL get_word(WORD &w)
{
    BYTE hi, lo;

    hi = getch_net();
    lo = getch_net();
    w = ((WORD)hi<<8) | (WORD)lo;
    return(!atend);
}
/* Get an incoming lword value, return 0 if end of message */
BOOL get_lword(LWORD &lw)
{
    lw.b[3] = getch_net();
    lw.b[2] = getch_net();
    lw.b[1] = getch_net();
    lw.b[0] = getch_net();
    return(!atend);
}

/* Match an incoming byte value, return 0 not matched, or end of message */
BOOL match_byte(BYTE b)
{
    return(b==getch_net() && !atend);
}
/* Match an incoming byte value, return 0 not matched, or end of message */
BOOL match_word(WORD w)
{
    WORD inw;

    return(get_word(inw) && inw==w);
}

/* Match longword */
BOOL match_lword(LWORD &lw)
{
    return (match_byte(lw.b[3]) && match_byte(lw.b[2]) && 
        match_byte(lw.b[1]) && match_byte(lw.b[0]));
}

/* Skip whitespace & ctrl chars in incoming data, return 0 if none */
void skip_space(void)
{          
    BYTE b;
    BOOL ok=0;
    
    while (get_byte(b) && b<=' ')
        ok = 1;
    if (ok)
        unget_byte(b);
}

/* Skip an incoming byte value, return 0 if end of message */
BOOL skip_byte(void)
{
    getch_net();
    return(!atend);
}
/* Skip an incoming word value, return 0 if end of message */
BOOL skip_word(void)
{
    getch_net();
    getch_net();
    return(!atend);
}
/* Skip an incoming lword value, return 0 if end of message */
BOOL skip_lword(void)
{
    getch_net();
    getch_net();
    getch_net();
    getch_net();
    return(!atend);
}
 
/* Compute checksum of a string, return its length */
int check_str(char *s)
{
    char c;
    int n=0;

    while ((c = s[n])!=0)
    {
        check_byte(c);
        n++;
    }
    return(n);
}

/* Add byte to checksum value */
void check_byte(BYTE b)
{
    if (checkflag)
    {
        if ((checklo = b+checklo) < b)
        {
            if (++checkhi == 0)
                checklo++;
        }
    }
    else
    {
        if ((checkhi = b+checkhi) < b)
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
void check_lword(LWORD &lw)
{
    check_byte(lw.b[3]);
    check_byte(lw.b[2]);
    check_byte(lw.b[1]);
    check_byte(lw.b[0]);
}

/* Add array of bytes to checksum value */
void check_bytes(BYTE *dat, int len)
{
    while (len--)
        check_byte(*dat++);
}

/* Send a byte to the network buffer */
void put_byte(BYTE b)
{
    putch_net(b);
}

/* Send a string out to the SLIP link, add to checksum */
void put_str(char *s)
{
    while (*s)
        putch_net(*s++);
}

/* Send a word out to the SLIP link, then add to checksum */
void put_word(WORD w)
{
    putch_net(w >> 8);
    putch_net(w);
}

void put_lword(LWORD &lw)
{
    putch_net(lw.b[3]);
    putch_net(lw.b[2]);
    putch_net(lw.b[1]);
    putch_net(lw.b[0]);
}

void put_data(BYTE *data, int len)
{
    while (len--)
        putch_net(*data++);
}

/* Swap the bytes in a word */
void swapw(WORD &val)
{
    val = (val >> 8) | ((val << 8) & 0xff00);
}

/* EOF */
