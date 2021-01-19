/* Embedded Gateway Interface for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/* Simple form-handling example */
/*
** v0.01 JPB 17/4/00  Derived from webserve.c v0.06
** v0.02 JPB 3/7/00   Revised header for book CD
*/

#define VERSION "0.02"

#define RESPFILE    "resp.htm"
#define CANCELFILE  "cancel.htm"
#define HAPPYMSG    "I'm glad you are happy."
#define SADMSG      "I'm sorry you are sad."

#include <stdio.h>
#include <string.h>

#include "ether.h"
#include "netutil.h"
#include "ip.h"
#include "tcp.h"
#include "web_egi.h"
#include "egi_form.h"

extern int webdebug;

/* Function to handle the user's response to the form */
void form_resp(TSOCK *ts, char *str)
{
    APPDATA *adp;
    char *s;

    adp = (APPDATA *)ts->app;
    if (!str)
    {
        if (webdebug)
            disp_connvars(ts);
        adp->egi = form_resp;
        s = !stricmp(get_connvar(ts, "send"), "submit") ? RESPFILE : CANCELFILE;
        adp->in = url_fopen(s);
        if (!adp->in)
        {
            buff_instr(&ts->txb, HTTP_OK HTTP_TXT HTTP_BLANK);
            buff_inprintf(&ts->txb, "File '%s' not found\n", s);
            close_tcp(ts);
        }
        else
        {
            buff_instr(&ts->txb, HTTP_OK HTTP_HTM HTTP_BLANK);
            s = !stricmp(get_connvar(ts, "state"), "happy") ? HAPPYMSG : SADMSG;
            put_connvar(ts, "message", s);
        }
    }
    else
    {
        if (webdebug)
            printf("EGI Tag '%s'\n", str);
        str += EGI_STARTLEN;
        if (*str == '$')
        {
            s = strtok(str+1, "-");
            buff_instr(&ts->txb, get_connvar(ts, s));
        }
    }
}

/* EOF */
