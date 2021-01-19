/* Scratch Protocol for 'TCP/IP Lean' (c) Iosoft Ltd. 2000

This software is only licensed for distribution with the book 'TCP/IP Lean',
and may only be used for personal experimentation by the purchaser
of that book, on condition that this copyright notice is retained.
For commercial licensing, contact license@iosoft.co.uk

This is experimental software; use it entirely at your own risk. The author
offers no warranties of any kind, including its fitness for purpose. */

/*
** v0.01 JPB 28/10/99
** v0.02 JPB 1/11/99  Removed 'flags' from packet format
** v0.03 JPB 2/11/99  Reintroduced sequencing flags
** v0.04 JPB 3/11/99  Added SLIP support
** v0.05 JPB 4/11/99  Added config-file support
** v0.06 JPB 5/11/99  Revamped packet driver to support multiple classes
** v0.07 JPB 8/1/99   Updated packet format to use command string
** v0.08 JPB 8/1/99   Adapted Tx, Rx so txlen and rxlen include header length
** v0.09 JPB 9/11/99  Added sequencing
** v0.10 JPB 9/11/99  Added data queues
** v0.11 JPB 11/11/99 Improved sequence logic
** v0.12 JPB 12/11/99 Added circular buffer option to packet driver
** v0.13 JPB 15/11/99 Added network Tx and Rx circular buffers
** v0.14 JPB 16/11/99 Improved support for direct-drive of NE2000 card
** v0.15 JPB 16/11/99 Added SCRATCHE for use with DJGPP compiler
** v0.16 JPB 17/11/99 Fixed reentrancy problem on receive
** v0.17 JPB 18/11/99 Removed references to min() and max()
** v0.18 JPB 19/11/99 Completely revamped state machine
** v0.19 JPB 22/11/99 Added timeouts
** v0.20 JPB 23/11/99 Added WIN32 support
** v0.21 JPB 25/11/99 Fixed SLIP interface
** v0.22 JPB 26/11/99 Improved SCRATCHP state-machine
** v0.23 JPB 29/11/99 Added 'dir' command
** v0.24 JPB 30/11/99 Added 'get' and 'put'
** v0.25 JPB 1/12/99  Promoted 'seq' and 'ack' to 32-bit values
** v0.26 JPB 6/12/99  Added 3COM driver
** v0.27 JPB 30/12/99 Improved DJGPP compatibility - removed MAXPATH definition
** v0.28 JPB 6/4/00   Updated to use latest version of net drivers
** v0.29 JPB 13/8/00  Updated copyright notice
*/

#define VERSION "0.29"              /* This software version */

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <ctype.h>
#include <string.h>

/* Override the default circular buffer size used in TCPFS.H */
#define _CBUFFLEN_ 0x1000           /* Circ buffer size: MUST be power of 2 */

#include "ether.h"
#include "netutil.h"
#include "net.h"
#include "scratchp.h"

#define SCRATCHPVER 1               /* Protocol version number */
#define TESTLEN     SCRATCHPDLEN    /* Max size of 1 block of test data */
#define TESTBUFFLEN (TESTLEN*2)     /* Size of test data buffer */
#define TESTINC     1               /* Increment value for test data */
#define CFGFILE     "tcplean.cfg"   /* Default config filename */
#define NDIAGS      40              /* Number of diagnostic header entries */
#define DIAG_TX     1               /* Identifiers for diagnostic Tx, Rx hdrs */
#define DIAG_RX     2
#define ERRTIME     2               /* Error timeout in sec */
#define RETRIES     2               /* Number of error retries */
#define BLOCKLEN    255             /* Len of file xfer blk (excl. len byte) */
#define FILENAMELEN 40              /* Max length of get/put filename */

char cfgfile[MAXPATH+5]=CFGFILE;    /* Config filename */
BYTE testdata[TESTBUFFLEN];         /* Test data buffer */
LWORD testlen;
int txoff, rxoff;                   /* Tx, Rx offsets into test data */
GENFRAME genframe;                  /* Receive/transmit frame */
long txcount;                       /* Count of Rx bytes */
CBUFF txbuff={_CBUFFLEN_};          /* Circular buffer for transmit */
CBUFF rxbuff={_CBUFFLEN_};          /* Circular buffer for receive */
int statedebug, pktdebug, sigdebug; /* Flags to enable diagnostic printout */
char locid[IDLEN+1];                /* My (local) ident string */
char remid[IDLEN+1];                /* Remote node ident string */
BYTE remaddr[MACLEN];               /* Remote node address */
extern BYTE bcast[MACLEN];          /* Broadcast address */
char netcfg[40];                    /* Network config string */
int connstate;                      /* Network connection state */
int appstate;                       /* Application state */
char *connstates[]={STATENAMES};    /* Strings for connection states */
char *appstates[]={APPNAMES};       /* Diagnostic strings for states */
char *signames[] = {SIGNAMES};
BYTE apptemp[TESTLEN+1];            /* Temporary storage for app. data */
SCRATCHPHDR diaghdrs[NDIAGS];       /* Diagnostic storage of SCRATCHP headers */
int diagidx;                        /* Index into dignostic storage */
char kbuff[81];                     /* Keyboard buffer */
WORD errtimer, errcount;            /* Timer & counter for errors */
char *offon[] = {"off", "on"};      /* Off/on strings */
FILE *fhandle;                      /* File handle for GET and PUT */
char filename[FILENAMELEN+1];       /* File name for GET and PUT */
long filelen;                       /* File length for GET and PUT */
int breakflag;                      /* Flag to indicate ctrl-break pressed */

/* Prototypes */
int get_pkts(GENFRAME *nfp);
void prompt_user(void);
int do_scratchp(GENFRAME *nfp, int rxlen, int signal);
int do_apps(CBUFF *rxb, CBUFF *txb, int sig);
int do_dir(CBUFF *txb);
void newconnstate(int state);
void newappstate(int state);
void disp_sig(int sig);
int make_scratchp(GENFRAME *nfp, BYTE *dest, char *cmd, BYTE flags,
                  void *data, int dlen);
int make_scratchpds(GENFRAME *nfp, BYTE *dest, char *cmd, BYTE flags, char *str);
int put_scratchp(GENFRAME *nfp, WORD txlen);
int is_scratchp(GENFRAME *nfp, int len);
int swap_scratchp(GENFRAME *nfp);
void disp_scratchphdr(SCRATCHPHDR *sph);
void disp_scratchp(GENFRAME *nfp);
void dump_diags(void);
int read_cfg(char *fname);
int mygets(char *buff, int maxlen);

int main(int argc, char *argv[])
{
    char k, cmdkey=0;
    int i, keysig, connsig, sstep=0;
    WORD frametype, txlen=0;
    GENFRAME *nfp;

    printf("SCRATCHP v" VERSION "  ");      /* Sign on */
    if (!read_cfg(argc>1 ? argv[1] : 0))    /* Read config file */
    {
        printf("ERROR: invalid configuration file\n");
        exit(1);
    }
    printf("\n");                           /* Make random test data */
    for (i=0; i<TESTLEN; i++)
        testdata[i] = testdata[i+TESTLEN] = (BYTE)rand()&0xff;
    nfp = &genframe;                        /* Open net driver.. */
    nfp->g.dtype = frametype = open_net(netcfg);    /* ..get frame type */
    if (!frametype)
    {
        printf("ERROR: can't open network driver\n");
        exit(1);
    }
    else
    {
        newappstate(STATE_IDLE);            /* Set default states */
        newconnstate(APP_IDLE);
        timeout(&errtimer, 0);              /* Refresh timeout timer */
        while (!breakflag && cmdkey!='Q')   /* Main command loop.. */
        {
            txlen = keysig = connsig = 0;
            prompt_user();                  /* Prompt user on state change */
            if (sstep || kbhit())           /* If single-step or keypress..*/
            {
                k = getch();                /* ..get key */
                if (sstep)
                    timeout(&errtimer, 0);  /* If single-step, refresh timer */
                cmdkey = toupper(k);        /* Decode keystrokes.. */
                switch (cmdkey)             /* ..and generate signals */
                {
                case 'I':                   /* 'I': broadcast ident */
                    if (connstate != STATE_CONNECTED)
                        printf("Broadcast ident request\n");
                    keysig = SIG_USER_IDENT;
                    break;

                case 'O':                   /* 'O': open connection */
                    printf("Open connection: remote ident (RETURN if any)? ");
                    mygets(remid, IDLEN);
                    if (kbuff[0])
                        printf("Contacting '%s'..\n", remid);
                    else
                        printf("Contacting any node..\n");
                    keysig = SIG_USER_OPEN;
                    break;

                case 'D':                   /* 'D': directory */
                    printf("Directory of remote node\n");
                    if (connstate != STATE_CONNECTED)
                        printf("Error: not connected\n");
                    else
                        keysig = SIG_USER_DIR;
                    break;

                case 'E':                   /* 'E': echo data test */
                    if (connstate != STATE_CONNECTED)
                        printf("Error: not connected\n");
                    else
                    {
                        printf("Echo test\n");
                        keysig = SIG_USER_ECHO;
                    }
                    break;

                case 'G':                   /* 'G': get file from rmeote */
                    if (connstate != STATE_CONNECTED)
                        printf("Error: not connected\n");
                    else
                    {
                        printf("Filename to get? ");
                        if (mygets(filename, FILENAMELEN))
                            keysig = SIG_USER_GET;
                    }
                    break;

                case 'P':                   /* 'P': put file into remote */
                    if (connstate != STATE_CONNECTED)
                        printf("Error: not connected\n");
                    else
                    {
                        printf("Filename to put? ");
                        if (mygets(filename, FILENAMELEN))
                            keysig = SIG_USER_PUT;
                    }
                    break;

                case 'C':                   /* 'C': close connection */
                    printf("Closing connection..\n");
                    keysig = SIG_USER_CLOSE;
                    break;

                case 'S':                   /* 'S': single-step */
                    sstep = !sstep;
                    printf("Single-step %s\n", sstep ? "on" : "off");
                    break;

                case '?':                   /* '?': dump dignostic log */
                    printf("\n");
                    dump_diags();
                    break;
                }
            }                               /* Feed kbd signal to application */
            connsig = do_apps(&rxbuff, &txbuff, keysig);
            if (!connsig && connstate!=STATE_IDLE &&
                timeout(&errtimer, ERRTIME))/* If idle and timeout.. */
            {                               /* ..check error counter.. */
                if (errcount++ < RETRIES)
                    connsig = do_apps(&rxbuff, &txbuff, SIG_TIMEOUT);
                else                        /* ..signal 'timeout' or 'fail' */
                    connsig = do_apps(&rxbuff, &txbuff, SIG_FAIL);
            }                               /* Keep SCRATCHP alive */
            txlen = do_scratchp(nfp, 0, connsig);
            put_scratchp(nfp, txlen);       /* Transmit packet (if any) */
            txlen = get_pkts(nfp);          /* Check receive packets */
            put_scratchp(nfp, txlen);       /* Transmit response (if any) */
            poll_net(nfp->g.dtype);         /* Keep net drivers alive */
        }
        if (connstate)                      /* Shutdown: still connected? */
        {
            printf("Closing connection..");
            while (get_pkts(nfp))           /* Discard all receive packets.. */
                putchar(',');               /* ..then send 'stop' ..*/
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_STOP, 0, 0);
            put_scratchp(nfp, txlen);
            while (get_pkts(nfp))           /* ..then discard any more.. */
                putchar('.');               /* (to ensure Tx buffer flushed) */
            putchar('\n');
            poll_net(nfp->g.dtype);         /* Keep net drivers alive */
        }
    }
    close_net(frametype);                   /* Close network drver */
    return(0);
}

/* Demultiplex incoming packets */
int get_pkts(GENFRAME *nfp)
{
    int rxlen, txlen=0;

    if ((rxlen=get_frame(nfp)) > 0)         /* If any packet received.. */
    {
        if (is_scratchp(nfp, rxlen))        /* If SCRATCHP.. */
        {
            swap_scratchp(nfp);                 /* ..do byte-swaps.. */
            txlen = do_scratchp(nfp, rxlen, 0); /* ..action it.. */
        }
    }                                       /* ..and maybe return a response */
    return(txlen);                          /* (using the same pkt buffer) */
}

/* Prompt user depending on connection & application states, & kbd signal */
void prompt_user(void)
{
    static int lastappstate=0, lastconnstate=-1;
    static long lastfilelen=0;

    if (lastfilelen != filelen)
        printf("%lu bytes     \r", filelen);
    lastfilelen = filelen;
    if (appstate != lastappstate)
    {
        if (appstate == APP_FILE_RECEIVER)
            printf("Receiving '%s'..\n", filename);
        if (appstate == APP_FILE_SENDER)
            printf("Sending '%s'..\n", filename);
        else if (lastappstate==APP_FILE_SENDER ||
                 lastappstate==APP_FILE_RECEIVER)
        {
            if (!filelen)
                printf("ERROR: file not found\n");
            else
                printf("%lu bytes transferred\n", filelen);
        }
        if (appstate == APP_ECHO_SERVER)
            printf("Echo server running... [C]lose connection?\n");
        else if (appstate == APP_ECHO_CLIENT)
            printf("Echo client running... [C]lose connection?\n");
        lastappstate = appstate;
    }
    else if (connstate != lastconnstate)
    {
        if (connstate == STATE_IDLE)
            printf("Connection closed: [I]dent, [O]pen, [Q]uit?\n");
        if (connstate == STATE_CONNECTED)
        {
            printf("Connected");
            if (*remid)
                printf(" to '%s'", remid);
            printf(": [D]ir [G]et [P]ut [E]cho [C]lose?\n");
        }
        lastconnstate = connstate;
    }
}

/* SCRATCHP connection state machine; given packet buffer and Rx len
** If Rx len is non-zero, process the incoming SCRATCHP packet,
** Otherwise, only check for state chamges or timeouts,
** Return non-zero packet length (incl. SCRATCHP hdr) if responding */
int do_scratchp(GENFRAME *nfp, int rxlen, int sig)
{
    WORD n, trylen, tx=0, txlen=0, crlen=0, dlen=0, txw;
    LWORD oldrx, rxw, acked=0;
    static LWORD txack;
    char *errstr=0, temps[22];
    BYTE rxflags=0, *p;
    SCRATCHPKT *sp;

    sp = getframe_datap(nfp);               /* Get pointer to frame data */
    if (rxlen)                              /* If packet received.. */
    {
        rxflags = sp->h.flags;              /* Decode command & data areas */
        if (rxflags&FLAG_CMD || rxflags&FLAG_RESP)
            crlen = strlen((char *)sp->data) + 1;
        dlen = sp->h.dlen - crlen;          /* Actual data is after command */
        diaghdrs[diagidx] = sp->h;          /* Store hdr in diagnostic log */
        diaghdrs[diagidx].ver = DIAG_RX;
        diagidx = (diagidx + 1) % NDIAGS;
        if (rxflags & FLAG_ERR)             /* Convert flags into signals */
            sig = SIG_ERR;
        else if (rxflags & FLAG_STOP)
            sig = SIG_STOP;
        else if (rxflags & FLAG_CMD)
            sig = SIG_CMD;
        else if (rxflags & FLAG_RESP)
            sig = SIG_RESP;
        else if (rxflags & FLAG_START)
            sig = SIG_START;
        else if (rxflags & FLAG_CONN)
            sig = SIG_CONN;
    }
    if (sigdebug && sig && sig!=SIG_CONN && sig<USER_SIGS)
        printf("Signal %s ", signames[sig]);
    if (connstate == STATE_IDLE)            /* If idle state.. */
    {
        timeout(&errtimer, 0);              /* Refresh timer */
        switch (sig)                        /* Check signals */
        {
        case SIG_USER_IDENT:                 /* User IDENT request? */
            txlen = make_scratchpds(nfp, bcast, CMD_IDENT, FLAG_CMD, "");
            break;

        case SIG_USER_OPEN:                 /* User OPEN request? */
            txlen = make_scratchpds(nfp, bcast, CMD_IDENT, FLAG_CMD, remid);
            buff_setall(&txbuff, 1);        /* My distinctive SEQ value */
            newconnstate(STATE_IDENT);      /* Start ident cycle */
            break;

        case SIG_CMD:                       /* Command signal? */
            if (!strcmp((char *)sp->data, CMD_IDENT))
            {                               /* IDENT cmd with my ID or null? */
                if (dlen<2 || !strncmp((char *)&sp->data[crlen], locid, dlen))
                {                           /* Respond to sender */
                    txlen = make_scratchp(nfp, getframe_srcep(nfp), CMD_IDENT,
                                          FLAG_RESP, locid, strlen(locid)+1);
                }
            }
            break;

        case SIG_RESP:                      /* Response signal? */
            if (!strcmp((char *)sp->data, CMD_IDENT))
            {                               /* IDENT response? */
                printf("Ident '%s'", (char *)&sp->data[crlen]);
                if ((p=getframe_srcep(nfp)) !=0 )
                    printf(" address  %s", ethstr(p, temps));
                printf("\n");
            }
            break;

        case SIG_START:                     /* START signal? */
            getframe_srce(nfp, remaddr);
            buff_setall(&txbuff, 0x8001);   /* My distinctive SEQ value */
            txack = sp->h.seq;              /* My ack is his SEQ */
            buff_setall(&rxbuff, txack);
            *remid = 0;                     /* Clear remote ID */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_CONN, 0, 0);
            newconnstate(STATE_CONNECTED);  /* Go connected */
            break;

        case SIG_CONN:                      /* CONNECTED or STOP signal? */
        case SIG_STOP:
            txlen = make_scratchp(nfp, getframe_srcep(nfp), 0, FLAG_ERR, 0, 0);
            break;                          /* Send error */
        }
    }
    else if (connstate == STATE_IDENT)      /* If in identification cycle.. */
    {
        switch (sig)                        /* Check signals */
        {
        case SIG_RESP:                      /* Got IDENT response? */
            if (!strcmp((char *)sp->data, CMD_IDENT) && dlen<=IDLEN)
            {
                if (!remid[0] || !strcmp((char *)&sp->data[crlen], remid))
                {                           /* Get remote addr and ID */
                    getframe_srce(nfp, remaddr);
                    strcpy(remid, (char *)&sp->data[crlen]);
                    txlen = make_scratchp(nfp, remaddr, 0, FLAG_START, 0, 0);
                    newconnstate(STATE_OPEN);
                }                           /* Open up the connection */
            }
            break;

        case SIG_ERR:                       /* Error response? */
            newconnstate(STATE_IDLE);       /* Go idle */
            break;

        case SIG_TIMEOUT:                   /* Timeout on response? */
            n = strlen(remid) + 1;          /* Resend IDENT command */
            txlen = make_scratchp(nfp, bcast, CMD_IDENT, FLAG_CMD, remid, n);
            break;

        case SIG_FAIL:                      /* Failed? */
            newconnstate(STATE_IDLE);       /* Go idle */
            break;
        }
    }
    else if (connstate == STATE_OPEN)       /* If I requested a connection.. */
    {
        switch (sig)                        /* Check signals */
        {
        case SIG_START:
        case SIG_CONN:                      /*  Response OK? */
            buff_setall(&rxbuff, sp->h.seq);
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_CONN, 0, 0);
            newconnstate(STATE_CONNECTED);  /* Send connect, go connected */
            break;

        case SIG_STOP:                      /* Stop already? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_STOP, 0, 0);
            newconnstate(STATE_IDLE);       /* Send stop, go idle */
            break;

        case SIG_ERR:                       /* Error response? */
            newconnstate(STATE_IDLE);
            break;                          /* Go idle */

        case SIG_TIMEOUT:                   /* Timeout on response? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_START, 0, 0);
            break;                          /* Resend request */

        case SIG_FAIL:                      /* Failed? */
            newconnstate(STATE_IDLE);       /* Go idle */
            break;

        }
    }
    else if (connstate == STATE_CONNECTED)  /* If connected.. */
    {
        switch (sig)                        /* Check signals */
        {
        case SIG_START:                     /* Duplicate START? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_CONN, 0, 0);
            break;                          /* Still connected */

        case SIG_TIMEOUT:                   /* Timeout on acknowledge? */
            buff_retry(&txbuff, buff_trylen(&txbuff));
                                            /* Rewind data O/P buffer */
            /* Fall through to normal connect.. */
        case SIG_CONN:                      /* If newly connected.. */
        case SIG_NULL:                      /* ..or still connected.. */
            /* Check received packet */
            if (rxlen > 0)                  /* Received packet? */
            {
                newconnstate(connstate);    /* Refresh timeout timer */
                /* Rx seq shows how much of his data he thinks I've received */
                oldrx = rxbuff.in - sp->h.seq;  /* Check for his repeat data */
                if (oldrx == 0)                 /* Accept up-to-date data */
                    buff_in(&rxbuff, &sp->data[crlen], dlen);
                else if (oldrx <= WINDOWSIZE)   /* Respond to repeat data.. */
                    tx = 1;                     /* ..with forced (repeat) ack */
                else                            /* Reject out-of-window data */
                    errstr = "invalid SEQ";
                /* Rx ack shows how much of my data he's actually received */
                acked = sp->h.ack - txbuff.out; /* Check amount acked */
                if (acked <= buff_trylen(&txbuff))
                    buff_out(&txbuff, 0, (WORD)acked);  /* My Tx data acked */
                else
                    errstr = "invalid ACK";
                rxw = rxbuff.in - txack;        /* Check Rx window.. */
                if (rxw >= WINDOWSIZE/2)        /* ..force Tx ack if 1/2 full */
                    tx = 1;
                if (errstr)                     /* If error, close connection */
                {
                    printf("Protocol error: %s\n", errstr);
                    txlen = make_scratchp(nfp, remaddr, 0, FLAG_ERR, 0, 0);
                    newconnstate(STATE_IDLE);
                }
            }
            /* Check whether a transmission is needed */
            txw = WINDOWSIZE - buff_trylen(&txbuff);/* Check Tx window space */
            trylen = minw(buff_untriedlen(&txbuff), /* ..size of data avail */
                          minw(SCRATCHPDLEN, txw)); /* ..and max packet len */
            if (trylen>0 || sig==SIG_TIMEOUT || tx) /* If >0, or timeout.. */
            {                                       /* ..or forced Tx.. */
                txlen = make_scratchp(nfp, remaddr, 0, FLAG_CONN, 0, trylen);
                buff_try(&txbuff, sp->data, trylen);/* ..do a transmission */
                txack = rxbuff.in;
            }
            if (buff_trylen(&txbuff) == 0)  /* If all data acked.. */
                newconnstate(connstate);    /* refresh timer (so no timeout) */
            break;

        case SIG_USER_CLOSE:                /* User closing connection? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_STOP, 0, 0);
            newconnstate(STATE_CLOSE);      /* Send stop command, go close */
            break;

        case SIG_STOP:                      /* STOP command? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_STOP, 0, 0);
            newconnstate(STATE_IDLE);       /* Send ack, go idle */
            break;

        case SIG_ERR:                       /* Error command? */
            newconnstate(STATE_IDLE);       /* Go idle */
            break;

        case SIG_FAIL:                      /* Application failed? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_ERR, 0, 0);
            newconnstate(STATE_IDLE);       /* Send stop command, go idle */
            break;

        }
    }
    else if (connstate == STATE_CLOSE)      /* If I'm closing connection.. */
    {
        switch (sig)                        /* Check signals */
        {
        case SIG_STOP:                      /* Stop or error command? */
        case SIG_ERR:
            newconnstate(STATE_IDLE);       /* Go idle */
            break;

        case SIG_TIMEOUT:                   /* Timeout on response? */
            txlen = make_scratchp(nfp, remaddr, 0, FLAG_STOP, 0, 0);
            break;                          /* Resend stop command */
        }
    }
    return(txlen);
}

/* Do application-specific tasks, given I/P and O/P buffers, and command string
** Return a connection signal value, 0 if no signal */
int do_apps(CBUFF *rxb, CBUFF *txb, int usersig)
{
    WORD len;
    BYTE lenb;
    int connsig=0;
    char cmd[CMDLEN+1];

    if (sigdebug && usersig && usersig>=USER_SIGS)
        printf("Signal %s ", signames[usersig]);
    connsig = usersig;                      /* Send signal to connection */
    if (connstate != STATE_CONNECTED)       /* If not connected.. */
        ;                                   /* Do nothing! */
    else if (appstate == APP_IDLE)          /* If application is idle.. */
    {
        if (usersig == SIG_USER_DIR)        /* User requested directory? */
        {                                   /* Send command */
            buff_in(txb, (BYTE *)CMD_DIR, sizeof(CMD_DIR));
        }
        else if (usersig == SIG_USER_GET)   /* User 'GET' command? */
        {
            filelen = 0;                    /* Open file */
            if ((fhandle = fopen(filename, "wb"))==0)
                printf("Can't open file\n");
            else
            {                               /* Send command & name to remote */
                buff_instr(txb, CMD_GET " ");
                buff_in(txb, (BYTE *)filename, (WORD)(strlen(filename)+1));
                newappstate(APP_FILE_RECEIVER); /* Become receiver */
            }
        }
        else if (usersig == SIG_USER_PUT)   /* User 'PUT' command? */
        {
            filelen = 0;                    /* Open file */
            if ((fhandle = fopen(filename, "rb"))==0)
                printf("Can't open file\n");
            else
            {                               /* Send command & name to remote */
                buff_instr(txb, CMD_PUT " ");
                buff_in(txb, (BYTE *)filename, (WORD)(strlen(filename)+1));
                newappstate(APP_FILE_SENDER);   /* Become sender */
            }
        }
        else if (usersig == SIG_USER_ECHO)  /* User equested echo? */
        {
            buff_in(txb, (BYTE *)CMD_ECHO, sizeof(CMD_ECHO));
            txoff = rxoff = 0;              /* Send echo command */
            newappstate(APP_ECHO_CLIENT);   /* Become echo client */
        }
        else if ((len=buff_strlen(rxb))>0 && len<=CMDLEN)
        {
            len++;                          /* Possible command string? */
            buff_out(rxb, (BYTE *)cmd, len);
            if (!strcmp(cmd, CMD_ECHO))     /* Echo command? */
                newappstate(APP_ECHO_SERVER);   /* Become echo server */
            else if (!strcmp(cmd, CMD_DIR)) /* DIR command? */
                do_dir(txb);                    /* Send DIR O/P to buffer */
            else if (!strncmp(cmd, CMD_GET, 3)) /* GET command? */
            {                                   /* Try to open file */
                filelen = 0;
                strcpy(filename, &cmd[4]);
                if ((fhandle = fopen(filename, "rb"))!=0)
                    newappstate(APP_FILE_SENDER);   /* If OK, become sender */
                else                            /* If not, respond with null */
                    buff_in(txb, (BYTE *)"\0", 1);
            }
            else if (!strncmp(cmd, CMD_PUT, 3)) /* PUT command? */
            {
                filelen = 0;
                strcpy(filename, &cmd[4]);      /* Try to open file */
                fhandle = fopen(filename, "wb");
                newappstate(APP_FILE_RECEIVER); /* Become receiver */
            }
        }
        else                                /* Default: show data from remote */
        {
            len = buff_out(rxb, apptemp, TESTLEN);
            apptemp[len] = 0;
            printf("%s", apptemp);
        }
    }
    else if (appstate == APP_ECHO_CLIENT)   /* If I'm an echo client.. */
    {
        if (usersig==SIG_USER_CLOSE)        /* User closing connection? */
            newappstate(APP_IDLE);
        else
        {                                   /* Generate echo data.. */
            if ((len = minw(buff_freelen(txb), TESTLEN)) > TESTLEN/2)
            {
                len = rand() % len;             /* ..random data length */
                buff_in(&txbuff, &testdata[txoff], len);
                txoff = (txoff + len) % TESTLEN;/* ..move & wrap data pointer */
            }
            if ((len = buff_out(rxb, apptemp, TESTLEN)) > 0)
            {                               /* Check response data */
                if (!memcmp(apptemp, &testdata[rxoff], len))
                {                               /* ..match with data buffer */
                    rxoff = (rxoff + len) % TESTLEN;/* ..move & wrap data ptr */
                    testlen += len;
                    printf("%lu bytes OK      \r", testlen);
                }
                else
                {
                    printf("\nEcho response incorrect!\n");
                    connsig = SIG_STOP;     /* If error, close connection */
                }
            }
        }
    }
    else if (appstate == APP_ECHO_SERVER)   /* If I'm an echo server.. */
    {
        if (usersig == SIG_USER_CLOSE)      /* User closing connection? */
            newappstate(APP_IDLE);
        else if ((len = minw(buff_freelen(txb), TESTLEN))>0 &&
                 (len = buff_out(rxb, apptemp, len)) > 0)
            buff_in(txb, apptemp, len);     /* Else copy I/P data to O/P */
    }
    else if (appstate == APP_FILE_RECEIVER) /* If I'm receiving a file.. */
    {
        while (buff_try(rxb, &lenb, 1))     /* Get length byte */
        {                                   /* If rest of block absent.. */
            if (buff_untriedlen(rxb) < lenb)
            {
                buff_retry(rxb, 1);         /* .. push length byte back */
                break;
            }
            else
            {
                filelen += lenb;
                buff_out(rxb, 0, 1);        /* Check length */
                if (lenb == 0)              /* If null, end of file */
                {
                    if (!fhandle || ferror(fhandle))
                        printf("ERROR writing file\n");
                    fclose(fhandle);
                    fhandle = 0;
                    newappstate(APP_IDLE);
                }
                else                        /* If not null, get block */
                {
                    buff_out(rxb, apptemp, (WORD)lenb);
                    if (fhandle)
                        fwrite(apptemp, 1, lenb, fhandle);
                }
            }
        }
    }
    else if (appstate == APP_FILE_SENDER)   /* If I'm sending a file.. */
    {                                       /* While room for another block.. */
        while (fhandle && buff_freelen(txb)>=BLOCKLEN+2)
        {                                   /* Get block from disk */
            lenb = (BYTE)fread(apptemp, 1, BLOCKLEN, fhandle);
            filelen += lenb;
            buff_in(txb, &lenb, 1);         /* Send length byte */
            buff_in(txb, apptemp, lenb);    /* ..and data */
            if (lenb < BLOCKLEN)            /* If end of file.. */
            {                               /* ..send null length */
                buff_in(txb, (BYTE *)"\0", 1);
                fclose(fhandle);
                fhandle = 0;
                newappstate(APP_IDLE);
            }
        }
    }
    return(connsig);
}

/* Do a directory of files, putting 1 name per line into O/P buffer
** Return number of files */
int do_dir(CBUFF *txb)
{
    int done=0, count=0;

#ifndef WIN32
    struct ffblk ffb;

    done = findfirst("*.*", &ffb, 0);
    while (!done)
    {
        count++;
        buff_instr(txb, ffb.ff_name);
        buff_instr(txb, "\r\n");
        done = findnext(&ffb);
    }
#else
    long h;
    struct _finddata_t fd;

    h = _findfirst("*.*", &fd);
    while (h!=-1L && !done)
    {
        count++;
        buff_instr(txb, fd.name);
        buff_instr(txb, "\r\n");
        done = _findnext(h, &fd);
    }
    _findclose(h);
#endif
    return(count);
}

/* Do a connection state transition, refresh timer, do diagnostic printout */
void newconnstate(int state)
{
    if (state!=connstate)
    {
        if (statedebug)
            printf("connstate %s\n", connstates[state]);
        if (state != STATE_CONNECTED)
            newappstate(APP_IDLE);          /* If not connected, stop app. */
    }
    connstate = state;
    errcount = 0;
    timeout(&errtimer, 0);                  /* Refresh timeout timer */
}

/* Do an application state transition, do diagnostic printout */
void newappstate(int state)
{
    if (statedebug && state!=appstate)
        printf("appstate %s\n", appstates[state]);
    appstate = state;
    errcount = 0;
    timeout(&errtimer, 0);                  /* Refresh timeout timer */
}

/* Display a signal name */
void disp_sig(int sig)
{
    printf("%s ", signames[sig]);
}

/* Make a SCRATCHP packet given command, flags and string data */
int make_scratchpds(GENFRAME *nfp, BYTE *dest, char *cmd, BYTE flags, char *str)
{
    return(make_scratchp(nfp, dest, cmd, flags, str, strlen(str)+1));
}

/* Make a SCRATCHP packet given command, flags and data */
int make_scratchp(GENFRAME *nfp, BYTE *dest, char *cmd, BYTE flags,
                  void *data, int dlen)
{
    SCRATCHPKT *sp;
    ETHERHDR *ehp;
    int cmdlen=0;

    sp = (SCRATCHPKT *)getframe_datap(&genframe);
    sp->h.ver = SCRATCHPVER;                /* Fill in the blanks.. */
    sp->h.flags = flags;
    sp->h.seq = txbuff.trial;               /* Direct seq/ack mapping.. */
    sp->h.ack = rxbuff.in;                  /* ..to my circ buffer pointers! */
    if (cmd)
    {
        strcpy((char *)sp->data, cmd);      /* Copy command string */
        cmdlen = strlen(cmd) + 1;
    }
    sp->h.dlen = cmdlen + dlen;             /* Add command to data length */
    if (dlen && data)                       /* Copy data */
        memcpy(&sp->data[cmdlen], data, dlen);
    if (nfp->g.dtype & DTYPE_ETHER)
    {
        ehp = (ETHERHDR *)nfp->buff;
        ehp->ptype = PCOL_SCRATCHP;         /* Fill in more blanks */
        memcpy(ehp->dest, dest, MACLEN);
    }
    diaghdrs[diagidx] = sp->h;              /* Copy hdr into diagnostic log */
    diaghdrs[diagidx].ver = DIAG_TX;
    diagidx = (diagidx + 1) % NDIAGS;
    return(sp->h.dlen+sizeof(SCRATCHPHDR)); /* Return length incl header */
}

/* Transmit a SCRATCHP packet. given length incl. SCRATCHP header */
int put_scratchp(GENFRAME *nfp, WORD txlen)
{
    int len=0;

    if (txlen >= sizeof(SCRATCHPHDR))       /* Check for min length */
    {
        if (pktdebug)
        {
            printf ("Tx ");
            disp_scratchp(nfp);
            printf("   ");
        }
        swap_scratchp(nfp);                 /* Byte-swap SCRATCHP header */
        if (is_ether(nfp, txlen+sizeof(ETHERHDR)))
            txlen += sizeof(ETHERHDR);
        txcount++;
        len = put_net(nfp, txlen);          /* Transmit packet */
    }
    return(len);
}

/* Check for SCRATCHP, given frame pointer & length */
int is_scratchp(GENFRAME *nfp, int len)
{
    WORD pcol;
                                            /* SLIP has no protocol field.. */
    pcol = getframe_pcol(nfp);              /* ..so assume 0 value is correct */
    return((pcol==0 || pcol==PCOL_SCRATCHP) && len>=sizeof(SCRATCHPHDR));
}

/* Byte-swap an SCRATCHP packet, return header length */
int swap_scratchp(GENFRAME *nfp)
{
    SCRATCHPKT *sp;

    sp = getframe_datap(nfp);
    sp->h.dlen = swapw(sp->h.dlen);
    sp->h.seq = swapl(sp->h.seq);
    sp->h.ack = swapl(sp->h.ack);
    return(sizeof(SCRATCHPHDR));
}

/* Display SCRATCHP packet */
void disp_scratchp(GENFRAME *nfp)
{
    printf("SCRATCHP ");
    disp_scratchphdr(getframe_datap(nfp));
    printf("\n");
}

/* Display SCRATCHP header */
void disp_scratchphdr(SCRATCHPHDR *sph)
{
    printf("F %02X S %04X A %04X Dlen %4Xh ",
           sph->flags, (WORD)sph->seq, (WORD)sph->ack, sph->dlen);
}

/* Dump out the diagnostic headers */
void dump_diags(void)
{
    SCRATCHPHDR *sph;
    int i, lastype;
    LWORD rxack=0, unacked, percent, txack=0;

    for (i=lastype=0; i<NDIAGS; i++)
    {
        sph = &diaghdrs[diagidx];
        diagidx = (diagidx + 1) % NDIAGS;
        if (sph->ver == DIAG_TX)            /* Tx packet? */
        {
            if (lastype == DIAG_TX)
                printf("\n");
            printf("Tx: ");
            disp_scratchphdr(sph);
            txack = sph->ack;
            unacked = rxack ? sph->seq + sph->dlen - rxack : 0;
            percent = (unacked * 100L) / txbuff.len;
            printf("%2ld%%", percent);
        }
        else if (sph->ver == DIAG_RX)       /* Rx packet? */
        {
            if (lastype != DIAG_TX)
                printf("                                     ");
            printf(" Rx: ");
            disp_scratchphdr(sph);
            rxack = sph->ack;
            unacked = txack ? sph->seq + sph->dlen - txack : 0;
            percent = (unacked * 100L) / rxbuff.len;
            printf("%2ld%%\n", percent);
        }
        lastype = sph->ver ? sph->ver : lastype;
    }
    if (lastype == DIAG_TX)
        printf("\n");
}

/* Read the config file into global storage */
int read_cfg(char *fname)
{
    int ok;

    if (fname)
    {
        strncpy(cfgfile, fname, MAXPATH);
        cfgfile[MAXPATH] = 0;
        strlwr(cfgfile);
        if (!strchr(cfgfile, '.'))
            strcat(cfgfile, ".cfg");
    }
    printf("Reading '%s'\n", cfgfile);
    ok = read_cfgstr(cfgfile, "net", netcfg, sizeof(netcfg)-1);
    if (ok)
    {
        printf("\nNet:    %s\n", netcfg);
        ok += read_cfgstr(cfgfile, "id", locid, IDLEN-1);
        printf("Ident:  %s\n", locid);
    }
    return(ok);
}

/* A rewrite of 'gets' to provide a max text length, and allow user to escape
** Returns length of string obtained */
int mygets(char *buff, int maxlen)
{
    char c=0;
    int n=0;

    do 
    {
        c = getch();
        if (c=='\b' && n>0)
        {
            putch('\b');
            putch(' ');
            putch('\b');
            n--;
        }
        else if (c == 0x1b)
        {
            n = 0;
            break;
        }
        else if (c>=' ' && n<maxlen)
        {
            putch(c);
            buff[n++] = c;
        }
        buff[n] = 0;
    } while (c != '\r');
    printf("\n");
    return(n);
}

/* Ctrl-break handler: set flag and return */
void break_handler(int sig)
{
    breakflag = sig;
}

/* EOF */

