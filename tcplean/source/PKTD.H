/* PC/TCP packet driver definitions for 'TCP/IP Lean' */

/* Packet classes */
#define PKTD_ETHER          1       /* DIX Ethernet */
#define PKTD_SLIP           6       /* SLIP */
#define PKTD_IETHER         11      /* 802.3 SNAP */

#define ANYTYPE             0xffff

/* Packet driver function call numbers. From Appendix B. */
#define DRIVER_INFO         1
#define ACCESS_TYPE         2
#define RELEASE_TYPE        3
#define SEND_PKT            4
#define TERMINATE           5
#define GET_ADDRESS         6
#define RESET_INTERFACE     7
#define GET_PARAMETERS      10
#define AS_SEND_PKT         11
#define SET_RCV_MODE        20
#define GET_RCV_MODE        21
#define SET_MULTICAST_LIST  22
#define GET_MULTICAST_LIST  23
#define GET_STATISTICS      24
#define SET_ADDRESS         25

WORD open_pktd(WORD dype);
void close_pktd(WORD dtype);
int get_pktd(WORD dtype, void *pkt, int maxlen);
int put_pktd(WORD dtype, void *pkt, int len);
BYTE *etherpktd_addr(WORD dtype);

/* EOF */
