/* UDP definitions for 'TCP/IP Lean' */

/* UDP port numbers */
#define ECHOPORT    7       /* Echo */
#define DAYPORT     13      /* Daytime */
#define TIMEPORT    37      /* Time */
#define TFTPORT     69      /* Triial File Transfer */
#define SNMPORT     161     /* Simple Network Management */
#define MINEPORT    1024    /* Start of ephemeral (temporary) port numbers */
#define MAXEPORT    5000    /* Max ephemeral port number */

/* TFTP packets */
#define MAXTFTP     512
typedef struct              /* TFTP read/write request */
{
    IPHDR   i;              /* IP header */
    UDPHDR  u;              /* UDP header */
    WORD op;                /* Opcode */
    char data[MAXTFTP+2];   /* Filename & mode strings */
} TFTP_REQ;

typedef struct              /* TFTP data/ack/error */
{
    IPHDR   i;              /* IP header */
    UDPHDR  u;              /* UDP header */
    WORD op;                /* Opcode */
    WORD block;             /* Block or error number */
    BYTE data[MAXTFTP];     /* Data */
} TFTP_DATA;
#define TFTP_READ   1       /* Opcodes: read request */
#define TFTP_WRITE  2       /*          write request */
#define TFTP_ERR    5       /*          error */

/* Prototypes */
int is_udp(IPKT *ip, int len);
int make_udp(GENFRAME *gfp, NODE *srcep, NODE *destp, WORD dlen);
WORD check_udp(UDPKT *udp, LWORD sip, LWORD dip, int ulen);
void swap_udp(UDPKT *udp);
int udp_maxdata(GENFRAME *gfp);
void getudp_srce(GENFRAME *gfp, NODE *np);
void getudp_dest(GENFRAME *gfp, NODE *np);
int getudp_locdest(GENFRAME *gfp, NODE *np);
void disp_udp(UDPKT *udp, int dlen, int tx);
int make_tftp_req(GENFRAME *gfp, NODE *sp, NODE *dp, WORD op, char *fname,
    char *mode);

/* EOF */

