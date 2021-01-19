/* Embedded Gateway Interface for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/* Simple slider-handling example */
/*
** v0.01 JPB 17/4/00  Derived from webserve.c v0.06
** v0.02 JPB 3/7/00   Revised header for book CD
*/

#define VERSION "0.02"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "ether.h"
#include "netutil.h"
#include "ip.h"
#include "tcp.h"
#include "web_egi.h"
#include "egi_slid.h"

#define PTR_STR     "<img src='clr.gif' height=%u width=10>"
#define PTR_OSET    5
#define PTR_MAX     200
#define NSLIDERS    3

#define RESPFILE    "sliders.htm"

int slidervals[NSLIDERS];

/* Function to handle the user's response to slider clicks */
void sliders_resp(TSOCK *ts, char *str)
{
    APPDATA *adp;
    char *s, temps[10];
    int idx, val;

    adp = (APPDATA *)ts->app;
    if (!str)
    {
        adp->egi = sliders_resp;
        adp->in = url_fopen(RESPFILE);
        if (!adp->in)
        {
            buff_instr(&ts->txb, HTTP_OK HTTP_TXT HTTP_BLANK);
            buff_inprintf(&ts->txb, "File '%s' not found\n", RESPFILE);
            close_tcp(ts);
        }
        else if (*(s = find_connvar(ts, "scale"))!=0)
        {
            idx = *(s+5) - '1';
            sprintf(temps, "scale%u.y", idx+1);
            s = get_connvar(ts, temps);
            printf("Var %s val %s\n", temps, s);
            val = PTR_MAX - atoi(s);
            if (idx>=0 && idx<=3)
                slidervals[idx] = val;
        }
    }
    else
    {
        printf("EGI Tag '%s'\n", str);
        str += EGI_STARTLEN;
        if (!strncmp(str, "$ptr", 4) && *(str+4)>='1' && *(str+4)<='3')
        {
            idx = *(str+4) - '1';
            buff_inprintf(&ts->txb, PTR_STR, slidervals[idx]+PTR_OSET);
        }
    }
}

/* EOF */
