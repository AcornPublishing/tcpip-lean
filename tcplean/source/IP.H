/* IP Structures for 'TCP/IP Lean' */
/*
** v0.01 JPB 1/8/92 
** v0.02 JPB 29/7/97 Added fake packet type for SLIP
** v0.03 JPB 16/12/98 Added 'MAXUDPDATA'
** v0.04 JPB 16/12/99 Updated for book
*/

/* Ensure network structures aren't padded (DJGPP and VC) */
#pragma pack(1)

#define MAXDATA 1500            /* Maximum Ethernet data size */
#define MAXPACK (MAXDATA+14)    /* Maximum packet size (incl. datalink hdr) */
#define MAXSEG (MAXDATA-40)     /* Maximum TCP data ('segment') size */
#define BCASTIP 0xffffffffL     /* Broadcast IP address */

/* Structure for one node */
typedef struct
{
    WORD  dtype;                /* Driver type */
    BYTE  mac[MACLEN];          /* MAC (Ethernet) addr */
    LWORD ip;                   /* IP addr */
    LWORD mask;                 /* Subnet mask */
    LWORD gate;                 /* Gateway addr */
    WORD  port;                 /* TCP or UDP port number */
} NODE;

#define PCOL_ARP    0x0806      /* Protocol type: ARP */
#define PCOL_IP     0x0800      /*                IP */

/* ***** ARP (Address Resolution Protocol) packet ***** */
typedef struct
{
    WORD  hrd,              /* Hardware type */
          pro;              /* Protocol type */
    BYTE  hln,              /* Len of h/ware addr (6) */
          pln;              /* Len of IP addr (4) */
    WORD  op;               /* ARP opcode */
    BYTE  smac[MACLEN];     /* Source MAC (Ethernet) addr */
    LWORD sip;              /* Source IP addr */
    BYTE  dmac[MACLEN];     /* Destination Enet addr */
    LWORD dip;              /* Destination IP addr */
} ARPKT;

#define HTYPE       0x0001  /* Hardware type: ethernet */
#define ARPPRO      0x0800  /* Protocol type: IP */
#define ARPXXX      0x0000  /* ARP opcodes: unknown opcode */
#define ARPREQ      0x0001  /*              ARP request */
#define ARPRESP     0x0002  /*              ARP response */
#define RARPREQ     0x0003  /*              RARP request */
#define RARPRESP    0x0004  /*              RARP response */

/* ***** IP (Internet Protocol) header ***** */
typedef struct
{
    BYTE  vhl,              /* Version and header len */
          service;          /* Quality of IP service */
    WORD  len,              /* Total len of IP datagram */
          ident,            /* Identification value */
          frags;            /* Flags & fragment offset */
    BYTE  ttl,              /* Time to live */
          pcol;             /* Protocol used in data area */
    WORD  check;            /* Header checksum */
    LWORD sip,              /* IP source addr */
          dip;              /* IP dest addr */
#if BIGHEAD
          LWORD option;     /* Dummy option field for debugging */
#endif
} IPHDR;
#define PICMP   1           /* Protocol type: ICMP */
#define PTCP    6           /*                TCP */
#define PUDP   17           /*                UDP */

/* ***** IP packet ('datagram') ***** */
#define MAXIP (MAXFRAME-sizeof(IPHDR))
typedef struct
{
    IPHDR  i;               /* IP header */
    BYTE   data[MAXIP];     /* Data area */
} IPKT;

/* ***** ICMP (Internet Control Message Protocol) header ***** */
typedef struct
{
    BYTE  type,             /* Message type */
          code;             /* Message code */
    WORD  check,            /* Checksum */
          ident,            /* Identifier (possibly unused) */
          seq;              /* Sequence number (possibly unused) */
} ICMPHDR;
#define ICREQ           8   /* Message type: echo request */
#define ICREP           0   /*               echo reply */
#define ICUNREACH       3   /*               destination unreachable */
#define ICQUENCH        4   /*               source quench */
#define UNREACH_NET     0   /* Destination Unreachable codes: network */
#define UNREACH_HOST    1   /*                                host */
#define UNREACH_PORT    3   /*                                port */
#define UNREACH_FRAG    4   /*     fragmentation needed, but disable flag set */

/* ***** ICMP packet ('datagram') ***** */
#define MAXICMP (MAXIP-sizeof(ICMPHDR))
typedef struct icmp
{
    IPHDR    i;             /* IP header */
    ICMPHDR  c;             /* ICMP header */
    BYTE     data[MAXICMP]; /* Data area */
} ICMPKT;

/* ***** UDP (User Datagram Protocol) header ***** */
typedef struct udph
{
    WORD  sport,            /* Source port */
          dport,            /* Destination port */
          len,              /* Length of datagram + this header */
          check;            /* Checksum of data, header + pseudoheader */
} UDPHDR;

#define MAXUDP (MAXIP-sizeof(UDPHDR))
/* ***** UDP packet ('datagram') ***** */
typedef struct udp
{
    IPHDR   i;              /* IP header */
    UDPHDR  u;              /* UDP header */
    BYTE    data[MAXUDP];   /* Data area */
} UDPKT;

/* ***** Pseudo-header for UDP or TCP checksum calculation ***** */
/* The integers must be in hi-lo byte order for checksum */
typedef struct              /* Pseudo-header... */
{
    LWORD srce,             /* Source IP address */
          dest;             /* Destination IP address */
    BYTE  z,                /* Zero */
          pcol;             /* Protocol byte */
    WORD  len;              /* UDP length field */
} PHDR;

/* ***** TCP (Transmission Control Protocol) header ***** */
typedef struct tcph
{
    WORD  sport,            /* Source port */
          dport;            /* Destination port */
    LWORD seq,              /* Sequence number */
          ack;              /* Ack number */
    BYTE  hlen,             /* TCP header len (num of bytes << 2) */
          flags;            /* Option flags */
    WORD  window,           /* Flow control credit (num of bytes) */
          check,            /* Checksum */
          urgent;           /* Urgent data pointer */
} TCPHDR;
#define TFIN        0x01    /* Option flags: no more data */
#define TSYN        0x02    /*           sync sequence nums */
#define TRST        0x04    /*           reset connection */
#define TPUSH       0x08    /*           push buffered data */
#define TACK        0x10    /*           acknowledgement */
#define TURGE       0x20    /*           urgent */

/* ***** TCP packet ('segment') ***** */
typedef struct tcp
{
    IPHDR   i;              /* IP header */
    TCPHDR  t;              /* TCP header */
    BYTE    data[MAXDATA];  /* Data area (oversized) */
} TCPKT;

/* Prototypes */
int is_arp(GENFRAME *gfp, int len);
int make_arp(GENFRAME *gfp, NODE *srcep, NODE *destp, WORD code);
void swap_arp(GENFRAME *gfp);
int is_ip(GENFRAME *gfp, int len);
int make_ip(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE pcol, WORD dlen);
void swap_ip(GENFRAME *gfp);
int ip_maxdata(GENFRAME *gfp);
NODE *findloc_ip(LWORD locip);
void getip_srce(GENFRAME *gfp, NODE *np);
void getip_dest(GENFRAME *gfp, NODE *np);
int getip_locdest(GENFRAME *gfp, NODE *np);
LWORD gate_ip(NODE *remp, NODE *locp);
int on_subnet(LWORD remip, NODE *locp);
int is_icmp(IPKT *ip, int len);
int make_icmp(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE type, BYTE code,
    WORD dlen);
int icmp_unreach(GENFRAME *gfp, NODE *srcep, NODE *destp, BYTE code);
void swap_icmp(ICMPKT *icmp);
int icmp_maxdata(GENFRAME *gfp);

/* EOF */

