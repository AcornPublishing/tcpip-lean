/* Ethernet driver definitions for 'TCP/IP Lean' */

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

#define MACLEN      6           /* Ethernet (MAC) address length */
#define CRCLEN      4           /* Ethernet hardware CRC length */
#define BCASTADDR 0xff,0xff,0xff,0xff,0xff,0xff /* Broadcast address */

/* Ehernet hardware Rx frame length includes the trailing CRC */
#define MAXFRAMEC   1518        /* Maximum frame size (incl CRC) */
#define MINFRAMEC   64          /* Minimum frame size (incl CRC) */

/* Higher-level drivers exclude the CRC from the frame length */
#define MAXFRAME    1514        /* Maximum frame size (excl CRC) */
#define MINFRAME    60          /* Minimum frame size (excl CRC) */

/* Ethernet (DIX) header */
typedef struct {
    BYTE dest[MACLEN];          /* Destination MAC address */
    BYTE srce[MACLEN];          /* Source MAC address */
    WORD ptype;                 /* Protocol type or length */
} ETHERHDR;

/* Ethernet (DIX) frame; data size is frame size minus header & CRC */
#define ETHERMTU (MAXFRAME-sizeof(ETHERHDR))
typedef struct {
    ETHERHDR h;                 /* Header */
    BYTE data[ETHERMTU];        /* Data */
    LWORD crc;                  /* CRC */
} ETHERFRAME;

/* 802.3 SNAP header */
typedef struct {
    WORD lsap;                  /* Link Service Access Point */
    BYTE ctrl;                  /* Control byte */
    BYTE oui[3];                /* Organisational Unit Identifier */
    WORD ptype;                 /* Protocol type */
} SNAPHDR;

/* 802.3 SNAP frame */
#define SNAPMTU (ETHERMTU-sizeof(SNAPHDR))
typedef struct {
    ETHERHDR e;                 /* Ethernet header (pcol is length) */
    SNAPHDR  s;                 /* 802.3 SNAP header */
    BYTE data[SNAPMTU];         /* Data */
    LWORD crc;                  /* CRC */
} SNAPFRAME;

/* NE2000-compatible driver prototypes */
int init_etherne(WORD dtype, WORD baseaddr);
void close_etherne(WORD dtype);
BYTE *etherne_addr(WORD dtype);
void poll_etherne(WORD dtype);
WORD get_etherne(WORD dtype, void *pkt);
WORD put_etherne(WORD dtype, void *pkt, WORD len);

/* 3C509 driver prototypes */
int init_ether3c(WORD dtype, WORD baseaddr);
void close_ether3c(WORD dtype);
BYTE *ether3c_addr(WORD dtype);
void poll_ether3c(WORD dtype);
WORD get_ether3c(void *pkt);
WORD put_ether3c(void *pkt, WORD len);

/* EOF */
