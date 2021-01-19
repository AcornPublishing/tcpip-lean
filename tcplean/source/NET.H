/* Network interface definitions for 'TCP/IP Lean' */

#define PCOL_SCRATCHP   0xfeeb      /* Protocol ID for SCRATCHP */
#define MAXSLIP         1006        /* Max SLIP frame size */

#define MAXNETS         4           /* Max net interfaces, must be power of 2 */

/* Driver type bits returned by 'open_net'
** If l.s.byte is zero, use direct hardware interface.
** If non-zero, is an interrupt number (i.e. handle) for packet driver access */
#define DTYPE_ETHER     0x8000      /* Ethernet frame */
#define DTYPE_SNAP      0x4000      /* 802.3 SNAP header option */
#define DTYPE_SLIP      0x2000      /* SLIP frame */
#define DTYPE_PKTD      0x0010      /* PC packet driver interface */
#define DTYPE_NE        0x0020      /* Direct NE2000-compatible driver */
#define DTYPE_3C        0x0040      /* Direct 3COM 3C509 driver */
#define DTYPE_SER       0x0100      /* PC serial driver (direct or Win32) */
#define NETNUM_MASK     (MAXNETS-1) /* Mask for network number */

int put_frame(GENFRAME *gfp, int len);
int get_frame(GENFRAME *gfp);
WORD open_net(char *cfgstr);
void close_net(WORD dtype);
BYTE *ether_addr(WORD dtype);
WORD get_net(GENFRAME *gfp);
WORD put_net(GENFRAME *gfp, WORD len);
void poll_net(WORD dtype);
WORD receive_upcall(WORD dtype, void *buff, WORD len);
WORD transmit_upcall(WORD dtype, void *buff, WORD maxlen);
void receive_ether(WORD dtype);
void transmit_ether(WORD dtype);
WORD open_etherne(char *str, WORD dtype);
WORD open_ether3c(char *str, WORD dtype);
WORD open_slip(char *cfgstr, WORD dtype);
void close_slip(WORD dtype);
int receive_slip(WORD dtype);
WORD put_slip(BYTE *pack, WORD len, WORD dtype);

/* EOF */
