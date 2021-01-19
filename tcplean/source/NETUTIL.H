/* Network utility definitions for 'TCP/IP Lean' */

/* Define variable sizes: 8, 16, 32 bits */
#ifndef BYTE
#define BYTE unsigned char
#endif
#ifndef WORD
#define WORD unsigned short
#endif
#ifndef LWORD
#define LWORD unsigned long
#endif

/* Ensure network structures aren't padded (DJGPP and VC) */
#pragma pack(1)

#ifdef WIN32
#include "win32def.h"
#else
#include "dosdef.h"
#endif

/* General-purpose frame header, and frame including header */
typedef struct {
    WORD len;                   /* Length of data in genframe buffer */
    WORD dtype;                 /* Driver type */
    WORD fragoff;               /* Offset of fragment within buffer */
} GENHDR;

/* The general-pupose frame buffer is dimensioned to accomodate 2 fragments
** i.e.  a 1514-byte frame, a 2nd GENHDR, and a 2nd 1514-byte frame */
#define MAXGEN 3040
typedef struct {
    GENHDR g;                   /* General-pupose frame header */
    BYTE buff[MAXGEN];          /* Frame itself (2 frames if fragmented) */
} GENFRAME;

/* Default circular buffer size: MUST be power of 2
** This definition is for initialisation only, and may be overriden to
** create a larger or smaller buffer. To determine the actual buffer size, all
** functions MUST use the 'len' value below, not the _CBUFFLEN_ definition */
#ifndef _CBUFFLEN_
#define _CBUFFLEN_ 0x800
#endif
/* Circular buffer structure */
typedef struct
{
    WORD len;                   /* Length of data (must be first) */
    LWORD in;                   /* Incoming data */
    LWORD out;                  /* Outgoing data */
    LWORD trial;                /* Outgoing data 'on trial' */
    BYTE data[_CBUFFLEN_];      /* Buffer */
} CBUFF;

/* Prototypes */
int getframe_maxlen(GENFRAME *gfp);
WORD getframe_mtu(GENFRAME *gfp);
WORD dtype_hdrlen(WORD dtype);
void *getframe_datap(GENFRAME *gfp);
BYTE *getframe_srcep(GENFRAME *gfp);
BYTE *getframe_srce(GENFRAME *gfp, BYTE *buff);
BYTE *getframe_destp(GENFRAME *gfp);
BYTE *getframe_dest(GENFRAME *gfp, BYTE *buff);
WORD getframe_pcol(GENFRAME *gfp);
int is_bcast(GENFRAME *gfp);
int is_ether(GENFRAME *gfp, int len);
int make_frame(GENFRAME *gfp, BYTE dest[], WORD pcol, WORD dlen);
void swap_ether(GENFRAME *gfp);
void disp_ether(GENFRAME *gfp, int dlen);
int is_slip(GENFRAME *gfp, int len);
void disp_frame(GENFRAME *gfp, int dlen, int tx);
char *ipstr(LWORD ip, char *s);
char *ethstr(BYTE *addr, char *str);
LWORD atoip(char *str);
int read_cfgval(char *fname, char *item, int *valp);
int read_cfgstr(char *fname, char *item, char *dest, int destlen);
int read_cfgstr_n(char *fname, int n, char *item, char *dest, int destlen);
int read_cfgopt(char *fname, char *item, char *opt);
char *skiptoken(char *str, char *tok);
char *skipspace(char *str);
char *skippunct(char *str);
int in_limits(LWORD val, LWORD lo, LWORD hi);
void buff_setall(CBUFF *bp, LWORD start);
WORD buff_freelen(CBUFF *bp);
WORD buff_untriedlen(CBUFF *bp);
WORD buff_trylen(CBUFF *bp);
WORD buff_dlen(CBUFF *bp);
WORD buff_retry(CBUFF *bp, WORD len);
WORD buff_preload(CBUFF *bp, LWORD oset, BYTE *data, WORD len);
WORD buff_in(CBUFF *bp, BYTE *data, WORD len);
WORD buff_instr(CBUFF *bp, char *str);
WORD buff_infile(CBUFF *bp, FILE *fp, WORD len);
WORD buff_try(CBUFF *bp, BYTE *data, WORD maxlen);
WORD buff_out(CBUFF *bp, BYTE *data, WORD maxlen);
WORD buff_strlen(CBUFF *bp);
WORD buff_chrlen(CBUFF *bp, char c);
WORD csum(void *dp, WORD count);
WORD minw(WORD a, WORD b);
WORD maxw(WORD a, WORD b);
int mini(int a, int b);
int maxi(int a, int b);
WORD swapw(WORD w);
LWORD swapl(LWORD lw);
int timeout(WORD *timep, int sec);
int mstimeout(LWORD *timep, int msec);
LWORD mstime(void);
void msdelay(WORD millisec);
void hexdump(BYTE *buff, WORD len);
char *find_first(char *path);
char *find_next(void);
long find_filesize(void);

/* EOF */
