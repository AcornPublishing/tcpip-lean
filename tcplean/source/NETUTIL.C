/* Network utility functions for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 28/10/99
** v0.02 JPB 15/11/99  Added support for Tx and Rx circular buffers
** v0.03 JPB 17/11/99  Fixed non-reentrant use of min() in circ buffer code
** v0.04 JPB 30/12/99  Added SLIP_SUPPORT option for DJGPP compatibility
** v0.05 JPB 31/12/99  Added support for IEEE 802.3 SNAP
** v0.06 JPB 7/1/00    Added 'maxlen' to net initialisation
** v0.07 JPB 17/1/00   Removed 'maxlen' again!
** v0.08 JPB 18/1/00   Moved get_frame and put_frame into this file
** v0.09 JPB 19/1/00   Added logfile
**                     Split off network functions into NET.C
** v0.10 JPB 21/3/00   Added directory search functions
** v0.11 JPB 23/3/00   Moved 'atoip' here from IP.C
** v0.12 JPB 29/3/00   Moved 'csum' here from IP.C
** v0.13 JPB 11/4/00   Added buff_infile()
** v0.14 JPB 12/4/00   Added find_filesize();
** v0.15 JPB 3/7/00    Revised header for book CD
*/

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "netutil.h"
#include "net.h"
#include "ether.h"

#define CFGDELIMS " \t\r\n"         /* Config item delimiters */
#define MAXCFGLINE 90               /* Max length of line in CFG file */

BYTE bcast[MACLEN] = {BCASTADDR};   /* Broadcast MAC addr */
BYTE zermac[MACLEN];                /* All-zero MAC addr */
extern int netdebug;                /* Net packet display flags */

#if WIN32
long dir_handle=-1L;
struct _finddata_t dir_block;
#else
    struct ffblk dir_block;
#endif

/* Return maximum length of the given frame */
int getframe_maxlen(GENFRAME *gfp)
{
    return(gfp->g.dtype&DTYPE_ETHER ? MAXFRAME : MAXSLIP);
}

/* Return the Maximum Transmission Unit (i.e. data size) for the given frame */
WORD getframe_mtu(GENFRAME *gfp)
{
    WORD mtu;

    mtu = (WORD)getframe_maxlen(gfp);
    if (gfp->g.dtype & DTYPE_ETHER)
    {
        mtu -= sizeof(ETHERHDR);
        if (gfp->g.dtype & DTYPE_SNAP)
            mtu -= sizeof(SNAPHDR);
    }
    return(mtu);
}

/* Return frame header length, given driver type */
WORD dtype_hdrlen(WORD dtype)
{
    return(dtype&DTYPE_ETHER ? sizeof(ETHERHDR) : 0);
}

/* Get pointer to the data area of the given frame */
void *getframe_datap(GENFRAME *gfp)
{
    return(&gfp->buff[dtype_hdrlen(gfp->g.dtype)]);
}

/* Get pointer to the source address of the given frame, 0 if none */
BYTE *getframe_srcep(GENFRAME *gfp)
{
    ETHERHDR *ehp;
    BYTE *srce=0;

    if (gfp->g.dtype & DTYPE_ETHER)         /* Only Ethernet has address */
    {
        ehp = (ETHERHDR *)gfp->buff;
        srce = ehp->srce;
    }
    return(srce);
}

/* Copy the source MAC addr of the given frame; use broadcast if no addr */
BYTE *getframe_srce(GENFRAME *gfp, BYTE *buff)
{
    BYTE *p;

    p = getframe_srcep(gfp);
    if (p)
        memcpy(buff, p, MACLEN);
    else
        memcpy(buff, bcast, MACLEN);
    return(p);
}

/* Get pointer to the destination address of the given frame, 0 if none */
BYTE *getframe_destp(GENFRAME *gfp)
{
    ETHERHDR *ehp;
    BYTE *dest=0;

    if (gfp->g.dtype & DTYPE_ETHER)         /* Only Ethernet has address */
    {
        ehp = (ETHERHDR *)gfp->buff;
        dest = ehp->dest;
    }
    return(dest);
}

/* Copy the destination MAC addr of the given frame; use broadcast if no addr */
BYTE *getframe_dest(GENFRAME *gfp, BYTE *buff)
{
    BYTE *p;

    p = getframe_destp(gfp);
    if (p)
        memcpy(buff, p, MACLEN);
    else
        memcpy(buff, bcast, MACLEN);
    return(p);
}

/* Get the protocol for the given frame; if unknown , return 0 */
WORD getframe_pcol(GENFRAME *gfp)
{
    ETHERHDR *ehp;
    WORD pcol=0;

    if (gfp->g.dtype & DTYPE_ETHER)         /* Only Ethernet has protocol */
    {
        ehp = (ETHERHDR *)gfp->buff;
        pcol = ehp->ptype;
    }
    return(pcol);
}

/* Return non-zero if frame has a broadcast address */
int is_bcast(GENFRAME *gfp)
{
    return(gfp->g.dtype&DTYPE_ETHER && !memcmp(gfp->buff, bcast, MACLEN));
}

/* Check Ethernet frame, given frame pointer & length, return non-0 if OK */
int is_ether(GENFRAME *gfp, int len)
{
    int dlen=0;

    if (gfp && (gfp->g.dtype & DTYPE_ETHER) && len>=sizeof(ETHERHDR))
    {
        dlen = len - sizeof(ETHERHDR);
        swap_ether(gfp);
    }
    return(dlen);
}

/* Make a frame, given data length. Return length of complete frame
** If Ethernet, set dest addr & protocol type; if SLIP, ignore these */
int make_frame(GENFRAME *gfp, BYTE dest[], WORD pcol, WORD dlen)
{
    ETHERHDR *ehp;

    if (gfp->g.dtype & DTYPE_ETHER)
    {
        ehp = (ETHERHDR *)gfp->buff;
        ehp->ptype = pcol;
        memcpy(ehp->dest, dest, MACLEN);
        swap_ether(gfp);
        dlen += sizeof(ETHERHDR);
    }
    return(dlen);
}

/* Byte-swap an Ethernet frame, return header length */
void swap_ether(GENFRAME *gfp)
{
    ETHERFRAME *efp;

    efp = (ETHERFRAME *)gfp->buff;
    efp->h.ptype = swapw(efp->h.ptype);
}

/* Check SLIP frame, return non-zero if OK */
int is_slip(GENFRAME *gfp, int len)
{
    return((gfp->g.dtype & DTYPE_SLIP) && len>0);
}

/* Display SLIP or Ethernet frame (must be in network byte order) */
void disp_frame(GENFRAME *gfp, int dlen, int tx)
{
    char temps[20];
    BYTE *data, pcol;
    WORD type=0x0800;
    int i;

    printf(tx ? "Tx%u /" : "Rx%u \\", gfp->g.dtype & NETNUM_MASK);
    printf("len %u ", dlen);
    data = (BYTE *)getframe_datap(gfp);
    if (netdebug & 1)                       /* Verbose display? */
    {
        if (gfp->g.dtype & DTYPE_ETHER)
        {
            printf("%s ", ethstr(&gfp->buff[tx ? 0 : MACLEN], temps));
            type = swapw(*(WORD *)&gfp->buff[MACLEN*2]);
            if (type == 0x0806)
            {
                printf("ARP %s ", ipstr(swapl(*(long *)&data[14]), temps));
                printf("-> %s ", ipstr(swapl(*(long *)&data[24]), temps));
            }
        }
        else
            printf("------SLIP------- ");
        if (type == 0x0800)
        {
            printf("IP %s ", ipstr(swapl(*(long *)&data[12]), temps));
            printf("-> %s ", ipstr(swapl(*(long *)&data[16]), temps));
            pcol = *(data+9);
            printf(pcol==1 ? "ICMP" : pcol==6 ? "TCP" : pcol==17 ? "UDP" : "");
        }
        printf("\n");
    }
    if (netdebug & 2)                       /* Hex display? */
    {
        for (i=0; i<dlen; i++)
            printf(" %02X", data[i]);
        printf("\n");
    }
}

/* Convert IP address into a string */
char *ipstr(LWORD ip, char *s)
{
    sprintf(s, "%lu.%lu.%lu.%lu",(ip>>24)&255,(ip>>16)&255,(ip>>8)&255,ip&255);
    return(s);
}

/* Convert Ethernet address into a string (max 17 chars plus null) */
char *ethstr(BYTE *addr, char *str)
{
    int i;
    char *s=str;

    if (!memcmp(addr, bcast, MACLEN))
        strcpy(s, "----BROADCAST----");
    else for (i=0; i<MACLEN; i++)
        s += sprintf(s, i>0 ? ":%02x" : "%02x", *addr++);
    return(str);
}

/* Convert string to IP addr: first digits form most-significant byte  */
LWORD atoip(char *str)
{
    LWORD ip=0L;
    int i=4, n;
    char c=1;

    while (--i>=0 && c)
    {
        n = 0;
        while (isdigit(c=*str++))
            n = n*10 + c-'0';
        ip += (LWORD)n << (i*8);
    }
    return(ip);
}

/* Retrieve the integer value of a config item. Return 0 if not found */
int read_cfgval(char *fname, char *item, int *valp)
{
    char str[10];
    int ok=0;

    if ((ok = read_cfgstr(fname, item, str, sizeof(str)-1)) != 0)
    {
        if (valp)
            *valp = atoi(str);
    }
    return(ok);
}

/* Return the config string for the given item. Return 0 if not found */
int read_cfgstr(char *fname, char *item, char *dest, int destlen)
{
    return(read_cfgstr_n(fname, 0, item, dest, destlen));
}

/* Return the config string for the n'th item (1st item if n=0).
** Return 0 if not found */
int read_cfgstr_n(char *fname, int n, char *item, char *dest, int destlen)
{
    FILE *in;
    int ok=-1, len;
    char *s, buff[MAXCFGLINE];

    if ((in=fopen(fname, "rt")) != 0)       /* Open as text file */
    {                                       /* Read a line at a time */
        while (ok<n && fgets(buff, MAXCFGLINE, in))
        {
            strlwr(buff);                   /* Force to lower case */
            len = strcspn(buff, CFGDELIMS); /* Get length of config ID */
            if (len==(int)strlen(item) && !strncmp(buff, item, len))
            {                               /* If it matches my item.. */
                s = skipspace(&buff[len]);  /* ..skip whitespace.. */
                if (dest)
                {                           /* ..get length excl. EOL chars */
                    len = mini(strcspn(s, "\r\n"), destlen);
                    strncpy(dest, s, len);  /* Copy into destination */
                    dest[len] = 0;
                }
                ok++;
            }
        }
        fclose(in);
    }
    return(ok>=n);
}

/* Return non-zero if config item has option set (or 'all' options set) */
int read_cfgopt(char *fname, char *item, char *opt)
{
    char *s, buff[MAXCFGLINE];
    int n, ok=0;

    if (read_cfgstr(fname, item, buff, MAXCFGLINE-1))
    {
        s = buff;
        while (!ok && *s)
        {
            n = strcspn(s, CFGDELIMS);      /* Get length of next option */
            ok = !strncmp(s, opt, n) || !strncmp(s, "all", n);
            s = skippunct(s + n);           /* Match option, skip to next */
        }
    }
    return(ok);
}

/* Check the given token is at the start of the string
** Return pointer to the first char after the token, 0 if token not found */
char *skiptoken(char *str, char *tok)
{
    int n;
    char *s=0;

    n = strlen(tok);
    if (n>0 && str && !strnicmp(str, tok, n))
        s = str + n;
    return(s);
}

/* Return a pointer to the first char after any whitespace */
char *skipspace(char *str)
{
    while (isspace(*str))
        str++;
    return(str);
}

/* Return a pointer to the first char after any whitespace or punctuation */
char *skippunct(char *str)
{
    while (isspace(*str) || ispunct(*str))
        str++;
    return(str);
}

/* Check whether a sequence value lies within two others, return 0 if not */
int in_limits(LWORD val, LWORD lo, LWORD hi)
{
    long lodiff, hidiff;

    lodiff = val - lo;
    hidiff = hi - val;
    return(lodiff>=0 && hidiff>=0);
}

/* Return total length of data in buffer */
WORD buff_dlen(CBUFF *bp)
{
    return((WORD)((bp->in - bp->out) & (bp->len - 1)));
}
/* Return length of untried (i.e. unsent) data in buffer */
WORD buff_untriedlen(CBUFF *bp)
{
    return((WORD)((bp->in - bp->trial) & (bp->len - 1)));
}
/* Return length of trial data in buffer (i.e. data sent but unacked) */
WORD buff_trylen(CBUFF *bp)
{
    return((WORD)((bp->trial - bp->out) & (bp->len - 1)));
}
/* Return length of free space in buffer */
WORD buff_freelen(CBUFF *bp)
{
    return(bp->len ? bp->len - 1 - buff_dlen(bp) : 0);
}

/* Set all the buffer pointers to a starting value */
void buff_setall(CBUFF *bp, LWORD start)
{
    bp->out = bp->in = bp->trial = start;
}

/* Rewind the trial pointer by the given byte count, return actual count */
WORD buff_retry(CBUFF *bp, WORD len)
{
    len = minw(len, buff_trylen(bp));
    bp->trial -= len;
    return(len);
}

/* Pre-load data into buffer, i.e. copy into the given buffer location
** Check that existing data isn't overwritten, return byte count if OK.
** If data pointer is null, do check but don't transfer data */
WORD buff_preload(CBUFF *bp, LWORD oset, BYTE *data, WORD len)
{
    WORD in, n=0, n1, n2, free;
    long inoff;

    inoff = oset - bp->in;                  /* Offset of data from I/P ptr */
    in = (WORD)oset & (bp->len-1);          /* Mask I/P ptr to buffer area */
    free = buff_freelen(bp);                /* Free space in buffer */
    if (inoff>=0 && inoff<(free))           /* If start is in free space.. */
    {
        n = minw(len, free);                /* Get max allowable length */
        n1 = minw(n, (WORD)(bp->len - in)); /* Length up to end of buff */
        n2 = n - n1;                        /* Length from start of buff */
        if (n1 && data)                     /* If anything to copy.. */
            memcpy(&bp->data[in], data, n1);/* ..copy up to end of buffer.. */
        if (n2 && data)                     /* ..and maybe also.. */
            memcpy(bp->data, &data[n1], n2);/* ..copy into start of buffer */
    }
    return(n);
}

/* Load data into buffer, return byte count that could be accepted
** If data pointer is null, adjust pointers but don't transfer data */
WORD buff_in(CBUFF *bp, BYTE *data, WORD len)
{
    WORD in, n, n1, n2;

    in = (WORD)bp->in & (bp->len-1);        /* Mask I/P ptr to buffer area */
    n = minw(len, buff_freelen(bp));        /* Get max allowable length */
    n1 = minw(n, (WORD)(bp->len - in));     /* Length up to end of buff */
    n2 = n - n1;                            /* Length from start of buff */
    if (n1 && data)                         /* If anything to copy.. */
        memcpy(&bp->data[in], data, n1);    /* ..copy up to end of buffer.. */
    if (n2 && data)                         /* ..and maybe also.. */
        memcpy(bp->data, &data[n1], n2);    /* ..copy into start of buffer */
    bp->in += n;                            /* Bump I/P pointer */
    return(n);
}

/* Load string into buffer, return num of chars that could be accepted */
WORD buff_instr(CBUFF *bp, char *str)
{
    return(buff_in(bp, (BYTE *)str, (WORD)strlen(str)));
}

/* Load file into buffer, return byte count */
WORD buff_infile(CBUFF *bp, FILE *fp, WORD len)
{
    WORD in, n, n1, n2=0;
    int count=0;

    in = (WORD)bp->in & (bp->len-1);        /* Mask I/P ptr to buffer area */
    n = minw(len, buff_freelen(bp));        /* Get max allowable length */
    n1 = minw(n, (WORD)(bp->len - in));     /* Length up to end of buff */
    if (n1)                                 /* If anything to read.. */
    {                                       /* ..get 1st block from file */
        count = fread(&bp->data[in], 1, n1, fp);
        n2 = len<n1 ? 0 : n-n1;             /* Check for end of file */
    }
    if (n2)                                 /* Maybe also get 2nd block */
        count += fread(bp->data, 1, n2, fp);
    bp->in += count;                        /* Bump I/P pointer */
    return((WORD)count);
}

/* Remove trial data from buffer, return byte count.
** If data pointer is null, adjust pointers but don't transfer data */
WORD buff_try(CBUFF *bp, BYTE *data, WORD maxlen)
{
    WORD trial, n, n1, n2;

    trial = (WORD)bp->trial & (bp->len-1);  /* Mask trial ptr to buffer area */
    n = minw(maxlen, buff_untriedlen(bp));  /* Get max allowable length */
    n1 = minw(n, (WORD)(bp->len - trial));  /* Length up to end of buff */
    n2 = n - n1;                            /* Length from start of buff */
    if (n1 && data)                         /* If anything to copy.. */
        memcpy(data, &bp->data[trial], n1); /* ..copy up to end of buffer.. */
    if (n2 && data)                         /* ..and maybe also.. */
        memcpy(&data[n1], bp->data, n2);    /* ..copy from start of buffer */
    bp->trial += n;                         /* Bump trial pointer */
    return(n);
}

/* Remove data from buffer, return byte count
** If data pointer is null, adjust pointers but don't transfer data */
WORD buff_out(CBUFF *bp, BYTE *data, WORD maxlen)
{
    WORD out, n, n1, n2;

    out = (WORD)bp->out & (bp->len-1);      /* Mask O/P ptr to buffer area */
    n = minw(maxlen, buff_dlen(bp));        /* Get max allowable length */
    n1 = minw(n, (WORD)(bp->len - out));    /* Length up to end of buff */
    n2 = n - n1;                            /* Length from start of buff */
    if (n1 && data)                         /* If anything to copy.. */
        memcpy(data, &bp->data[out], n1);   /* ..copy up to end of buffer.. */
    if (n2 && data)                         /* ..and maybe also.. */
        memcpy(&data[n1], bp->data, n2);    /* ..copy from start of buffer */
    bp->out += n;                           /* Bump O/P pointer */
    if (buff_untriedlen(bp) > buff_dlen(bp))/* ..and maybe trial pointer */
        bp->trial = bp->out;
    return(n);
}
/* Return length of null-delimited string in buffer, 0 if no null terminator */
WORD buff_strlen(CBUFF *bp)
{
    return(buff_chrlen(bp, 0));
}
/* Return length of string in buffer given delimiter char, 0 if no match */
WORD buff_chrlen(CBUFF *bp, char c)
{
    WORD out, n, n1, n2;
    BYTE *p, *q=0;

    out = (WORD)bp->out & (bp->len-1);      /* Mask O/P ptr to buffer area */
    n = buff_dlen(bp);                      /* Get max length */
    n1 = minw(n, (WORD)(bp->len - out));    /* Length up to end of buff */
    n2 = n - n1;                            /* Length from start of buff */
    if (n1)                                 /* Check up to end of buffer */
        q = memchr(p=&bp->data[out], c, n1);
    if (!q && n2)
        q = memchr(p=bp->data, c, n2);      /* ..check data at buffer start */
    else
        n1 = 0;
    return(q ? (WORD)(q - p) + n1 : 0);
}

/* Do TCP-style checksum. Improved algorithm is from RFC 1071 */
WORD csum(void *dp, WORD count)
{
    register LWORD total=0L;
    register WORD n, *p, carries;

    n = count / 2;
    p = (WORD *)dp;
    while (n--)
        total += *p++;
    if (count & 1)
        total += *(BYTE *)p;
    while ((carries=(WORD)(total>>16))!=0)
        total = (total & 0xffffL) + carries;
    return((WORD)total);
}

/* Safe versions of the min() & max() macros for use on re-entrant code
** Ensures that any function arguments aren't called twice */
WORD minw(WORD a, WORD b)
{
    return(a<b ? a : b);
}
WORD maxw(WORD a, WORD b)
{
    return(a>b ? a : b);
}
int mini(int a, int b)
{
    return(a<b ? a : b);
}
int maxi(int a, int b)
{
    return(a>b ? a : b);
}

/* Return byte-swapped word */
WORD swapw(WORD w)
{
    return(((w<<8)&0xff00) | ((w>>8)&0x00ff));
}
/* Return byte-swapped longword */
LWORD swapl(LWORD lw)
{
    return(((lw<<24)&0xff000000L) | ((lw<<8 )&0x00ff0000L) |
           ((lw>>8 )&0x0000ff00L) | ((lw>>24)&0x000000ffL));
}

/* Check for timeout on a given tick counter, return non-zero if true */
int timeout(WORD *timep, int sec)
{

    WORD tim, diff;
    int tout=0;

    tim = (WORD)time(0);
    diff = tim - *timep;
    if (sec==0 || diff>=sec)
    {
        *timep = tim;
        tout = 1;
    }
    return(tout);
}

/* Check for timeout on a given tick counter, return non-zero if true */
int mstimeout(LWORD *timep, int msec)
{

    LWORD tim;
    long diff;
    int tout=0;

    tim = mstime();
    diff = tim - *timep;
    if (msec==0 || diff>=msec)
    {
        *timep = tim;
        tout = 1;
    }
    return(tout);
}

#ifndef WIN32
/* Return approximate millisecond count (DOS only) */
LWORD mstime(void)
{
    return(biostime(0, 0) * 55L);   /* Should be 54.945! */
}
#endif

/* Crude delay in case delay() isn't in the C library
** Derives timing from I/O cycles accessing the interrupt controller
** Slow CPUs (sub-100MHz) will be significantly slower than the given time */
void msdelay(WORD millisec)
{
    int n;

    while (millisec--)
    {
        for (n=0; n<1500; n++)
            inp(0x61);
    }
}

/* Print a hex dump of a buffer */
void hexdump(BYTE *buff, WORD len)
{
    BYTE c, str[17];
    WORD j, n=0;
    while (n < len)                /* For each line of 16 bytes... */
    {
        printf("  %04x:", n);
        for (j=0; j<16; j++)                /* For each byte of 16... */
        {
            printf("%c", j==8 ? '-':' ');   /* Put '-' after 8 bytes */
            if (n++ >= len)                 /* If no bytes left... */
            {
                printf("  ");               /* Fill out space */
                str[j] = 0;
            }
            else                            /* If bytes left... */
            {
                printf("%02x", c = *buff++);/* Print byte value */
                str[j] = c>=' '&&c<='~' ? c : '.';
            }                               /* Save char if valid */
        }
        str[j] = 0;                        /* Print char string */
        printf("  %s\n", str);
    }
}

/* Directory 'findfirst' for both DOS and Win32; returns string, or null */
char *find_first(char *path)
{
#if WIN32
    if (dir_handle != -1L)
        _findclose(dir_handle);
    return((dir_handle=_findfirst(path, &dir_block))!=-1L ? dir_block.name : 0);
#else
    return(findfirst(path, &dir_block, 0)==0 ? dir_block.ff_name : 0);
#endif
}

/* Directory 'findnext' for both DOS and Win32; returns string, or null */
char *find_next(void)
{
#if WIN32
    char *s=0;
    if (_findnext(dir_handle, &dir_block)==0)
        s = dir_block.name;
    else
    {
        _findclose(dir_handle);
        dir_handle = -1L;
    }
    return(s);
#else
    return(findnext(&dir_block)==0 ? dir_block.ff_name : 0);
#endif
}

/* Return the length of a file that has been found */
long find_filesize(void)
{
#if WIN32
    return(dir_block.size);
#else
    return(dir_block.ff_fsize);
#endif
}
/* EOF */

