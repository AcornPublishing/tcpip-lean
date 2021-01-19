/* PC serial definitions for 'TCP/IP Lean'   */

unsigned setuart(int com, long baud, int irq);
void selectuart(int n);
unsigned initser(unsigned addr, long baud, int irq);
int litelink(long baud);
void clearuart(void);
void rxclear(void);
unsigned rxchar(void);
void txstr(char *s);
void txchar(unsigned char c);

typedef struct
{
    int rts:1;
    int dtr:1;
    int cts:1;
    int dsr:1;
    int dcd:1;
} SERSTATUS;

SERSTATUS getserstatus(void);
void setserstatus(SERSTATUS hs);

#define NOCHAR  0xffff          /* Ident for no character */

#define UART1   0x3f8           /* Serial port base addr: COM1=3F8, COM2=2F8 */
#define UART2   0x2f8
#define SERVEC1 12              /* Interrupt vector: COM1=12, COM2=11 */
#define SERVEC2 11

#define PIC1    0x20            /* Prog Interrupt Controller 1 base addr */
#define PIC2    0xA0            /* Prog Interrupt Controller 2 base addr */

#define RBUFSIZ 1024            /* Size (in chars) of receive buffer */
#define TBUFSIZ 1024            /* Size (in chars) of transmit buffer */

#define MAXCOMMS 4              /* Max number of opened comm channels */

/* EOF */
