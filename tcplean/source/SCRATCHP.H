/* SCRATCHP definitions for 'Creating TCP/IP from Scratch' */

/* Ensure network structures aren't padded (DJGPP and VC) */
#pragma pack(1)

/* Connection states */
#define STATE_IDLE          0
#define STATE_IDENT         1
#define STATE_OPEN          2
#define STATE_CONNECTED     3
#define STATE_CLOSE         4
#define STATENAMES          "IDLE", "IDENT", "OPEN", "CONNECTED", "CLOSE"

/* Application states */
#define APP_IDLE            0
#define APP_ECHO_CLIENT     1
#define APP_ECHO_SERVER     2
#define APP_FILE_SENDER     3
#define APP_FILE_RECEIVER   4
#define APPNAMES            "IDLE", "ECHO_CLIENT", "ECHO_SERVER", \
    "FILE_SENDER", "FILE_RECEIVER"

/* Connection signals */
#define SIG_NULL            0
#define SIG_CMD             1
#define SIG_RESP            2
#define SIG_START           3
#define SIG_CONN            4
#define SIG_STOP            5
#define SIG_ERR             6
#define SIG_TIMEOUT         7
#define SIG_FAIL            8
#define SIG_ECHO            9

#define USER_SIGS           10  /* Marker for start of user signals */
#define SIG_USER_OPEN       10
#define SIG_USER_CLOSE      11
#define SIG_USER_IDENT      12
#define SIG_USER_DIR        13
#define SIG_USER_GET        14
#define SIG_USER_PUT        15
#define SIG_USER_ECHO       16

#define SIGNAMES "", "CMD", "RESP", "NET OPEN", "CONN", "NET CLOSE", \
    "ERR", "TIMEOUT", "FAIL", "ECHO", "USER OPEN", "USER CLOSE", \
    "USER IDENT", "USER DIR", "USER GET", "USER PUT", "USER ECHO"

/* Flag values */
#define FLAG_CMD        0x01    /* Data area contains command */
#define FLAG_RESP       0x02    /* Data area contains response */
#define FLAG_START      0x04    /* Request to start connection */
#define FLAG_CONN       0x08    /* Connected; sequenced transfer in use */
#define FLAG_STOP       0x10    /* Stop connection */
#define FLAG_ERR        0x20    /* Error; abandon connection */

/* Command strings */
#define CMD_ECHO        "echo"  /* Echo request */
#define CMD_IDENT       "ident" /* Identification request */
#define CMD_DIR         "dir"   /* Directory of files */
#define CMD_GET         "get"   /* Get file from remote */
#define CMD_PUT         "put"   /* Put file onto remote */
#define CMDLEN          40      /* Max length of command string */

#define IDLEN           20      /* Max length of ID string */

/* SCRATCHP packet header */
typedef struct {
    BYTE ver;                   /* Protocol version number */
    BYTE flags;                 /* Flag bits */
    LWORD seq;                  /* Sequence value */
    LWORD ack;                  /* Acknowledgement value */
    WORD dlen;                  /* Length of following data */
} SCRATCHPHDR;

/* SCRATCHP packet */
#define SCRATCHPDLEN 994
typedef struct {
    SCRATCHPHDR h;              /* Header */
    BYTE data[SCRATCHPDLEN];    /* Data (or null-terminated cmd/resp string) */
} SCRATCHPKT;

#define WINDOWSIZE 3000         /* Data window size */

/* EOF */
