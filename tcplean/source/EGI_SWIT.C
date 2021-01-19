/* Embedded Gateway Interface for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/* Simple switch-and-LED--handling example */
/*
** v0.01 JPB 17/4/00  Derived from webserve.c v0.06
** v0.02 JPB 3/7/00   Revised header for book CD
*/

#define VERSION "0.02"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ether.h"
#include "netutil.h"
#include "ip.h"
#include "tcp.h"
#include "web_egi.h"
#include "egi_swit.h"

#define LEDON       "<img src='ledon.gif'>"
#define LEDOFF      "<img src='ledoff.gif'>"
#define SWOFF       "<input type=image name='switch%u' src='switchu.gif'>"
#define SWON        "<input type=image name='switch%u' src='switchd.gif'>"

#define RESPFILE    "switchfm.htm"

BYTE ledstate;

/* Function to handle the user's response to switch clicks */
void switchfm_resp(TSOCK *ts, char *str)
{
    APPDATA *adp;
    char *s;
    int bit, mask;

    adp = (APPDATA *)ts->app;
    if (!str)
    {
        adp->egi = switchfm_resp;
        adp->in = url_fopen(RESPFILE);
        if (!adp->in)
        {
            buff_instr(&ts->txb, HTTP_OK HTTP_TXT HTTP_BLANK);
            buff_inprintf(&ts->txb, "File '%s' not found\n", RESPFILE);
            close_tcp(ts);
        }
        else if (*(s = find_connvar(ts, "switch"))!=0)
        {
            bit = *(s+6) - '1';
            mask = 1 << bit;
            ledstate ^= mask;
        }
    }
    else
    {
        printf("EGI Tag '%s'\n", str);
        str += EGI_STARTLEN;
        if (!strncmp(str, "$led", 4) && isdigit(*(str+4)))
        {
            bit = *(str+4) - '1';
            mask = 1 << bit;
            buff_instr(&ts->txb, (ledstate & mask) ? LEDON : LEDOFF);
        }
        if (!strncmp(str, "$switch", 7) && isdigit(*(str+7)))
        {
            bit = *(str+7) - '1';
            mask = 1 << bit;
            s = (ledstate & mask) ? SWON : SWOFF;
            buff_inprintf(&ts->txb, s, bit+1);
        }
    }
}

/* EOF */
