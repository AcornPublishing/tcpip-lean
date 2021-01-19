/* TCP definitions for 'Creating TCP/IP from scratch' */

/* Well-known TCP port numbers */
#define ECHOPORT    7       /* Echo */
#define DAYPORT     13      /* Daytime */
#define CHARPORT    19      /* Character generator */
#define TELPORT     23      /* Telnet remote login */
#define HTTPORT     80      /* HTTP */
#define MINEPORT    1024    /* Start of ephemeral (temporary) port numbers */
#define MAXEPORT    5000    /* Max ephemeral port number */

/* Dummy values for the segment data length */
#define DLEN_NODATA -1      /* Segment received, but no data */

/* TCP states */
#define TSTATE_STRINGS \
    "closed",       "SYN rcvd",     "established", \
    "close wait",   "last ACK",     "reset rcvd",   \
    "active open",  "ARP sent",     "ARP rcvd",     "SYN sent", \
    "active close", "FIN wait 1",   "FIN wait 2",   "closing",\
    "timed wait"
typedef enum {
    /* Passive open & close */
    TCP_CLOSED,             /* Closed */
    TCP_SYNR,               /* SYN recvd: send SYN ACK */
    TCP_EST,                /* Established: connection OK */
    TCP_CLWAIT,             /* Close wait: FIN recvd, send data then FIN ACK */
    TCP_LASTACK,            /* Last ACK: waiting for ACK of my FIN */
    TCP_RSTR,               /* Reset received */
    /* Active open and close */
    TCP_AOPEN,              /* Active open: send ARP */
    TCP_ARPS,               /* ARP sent, awaiting reply */
    TCP_ARPR,               /* ARP reply received */
    TCP_SYNS,               /* SYN sent: awaiting SYN ACK */
                            /* Established: same as for passive open */
    TCP_ACLOSE,             /* Active close: send FIN */
    TCP_FINWT1,             /* FIN wait 1: FIN sent, waiting for ACK */
    TCP_FINWT2,             /* FIN wait 2: receiving data, waiting for FIN */
    TCP_CLING,              /* Closing: awaiting final ACK */
    TCP_TWAIT               /* Timed wait */
} TCP_STATE;

/* Simplified connection states for applications */
typedef enum {
    TCP_IDLE,               /* Idle */
    TCP_OPEN,               /* Opening connection */
    TCP_CONN,               /* Connected */
    TCP_DATA,               /* Connected, data received */
    TCP_NODATA,             /* Connected, no data received */
    TCP_CLOSE               /* Closing connection */
} CONN_STATE;

/* Storage structure for a single TCP socket
** The positions of the first 3 items are fixed to simplify initialisation */
typedef struct {
    int         index;          /* Index number - must be first */
    CBUFF       rxb,            /* Receive & transmit circular buffers */
                txb;            /* (must be 2nd and 3rd) */
    NODE        loc,            /* Local and remote nodes */
                rem;
    TCP_STATE   state;          /* Current connection state */
    int         server;         /* Flag to identify server socket */
    LWORD       time;           /* Time at last state change (msec) */
    int         timeout,        /* Timeout value (0=no timeout) */
                retries;        /* Number of retries left */
    WORD        txmss,          /* Max seg data size for my transmit */
                rxwin,          /* Current Rx & Tx window sizes */
                txwin;
    LWORD       rxseq,          /* Seq & ack values in latest Rx segment */
                rxack;
    BYTE        txflags,        /* Latest Tx flags */
                connflags;      /* Extra Tx connection flags (push/urgent) */
    int         txdlen;         /* Latest transmit data length */ 
    void        *app;           /* Pointer to application-specific data */
} TSOCK;

/* Telnet option byte values (subset) */
#define TEL_IAC     255         /* Interpret As Command */
#define TEL_DONT    254         /* Don't do it */
#define TEL_DO      253         /* Do it */
#define TEL_WONT    252         /* Won't do it */
#define TEL_WILL    251         /* Will do it */
#define TEL_ECHO    1           /* Echo option */
#define TEL_SGA     3           /* Suppress Go-Ahead option */
#define TEL_AUTH    37          /* Authentication option */

/* Prototypes */
int open_tcp(TSOCK *ts, GENFRAME *gfp, NODE *locp, NODE *remp);
int close_tcp(TSOCK *ts);
void reset_tcp(TSOCK *ts, GENFRAME *gfp);
int is_tcp(IPKT *ip, int len);
int make_tcp(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE flags,
    LWORD seq, LWORD ack, WORD window, WORD dlen);
int make_sock_tcp(GENFRAME *gfp, TSOCK *ts, BYTE flags, WORD dlen);
int make_reset_resp(GENFRAME *gfp, int rdlen);
WORD check_tcp(TCPKT *tcp, LWORD sip, LWORD dip, int tlen);
void swap_tcp(TCPKT *tcp);
int tcp_maxdata(GENFRAME *gfp);
void gettcp_srce(GENFRAME *gfp, NODE *np);
void gettcp_dest(GENFRAME *gfp, NODE *np);
int gettcp_locdest(GENFRAME *gfp, NODE *np);
WORD gettcp_opt(TCPKT *tcp, WORD *mssp);
void disp_tcp(TCPKT *tcp, int dlen, int tx);
void new_state(TSOCK *ts, int news);
void arp_receive(TSOCK tss[], int nsocks, GENFRAME *gfp);
void tcp_receive(TSOCK tss[], int nsocks, GENFRAME *gfp, int dlen);
void tcp_poll(TSOCK tss[], int nsocks, GENFRAME *gfp);
int tsock_rx(TSOCK *ts, GENFRAME *gfp, int dlen);
int tsock_estab_rx(TSOCK *ts, GENFRAME *gfp, int dlen);
int tsock_estab_tx(TSOCK *ts, GENFRAME *gfp, int force);
int remake_tsock(TSOCK *ts, GENFRAME *gfp);

/* EOF */

