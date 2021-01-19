/* Embedded Gateway Interface definitions for 'Creating TCP/IP from scratch' */

/* HTTP and HTML text */
#define HTTP_OK     "HTTP/1.0 200 OK\r\n"
#define HTTP_NOFILE "HTTP/1.0 404 Not found\r\n"
#define HTTP_HTM    "Content-type: text/html\r\n"
#define HTTP_TXT    "Content-type: text/plain\r\n"
#define HTTP_GIF    "Content-type: image/gif\r\n"
#define HTTP_XBM    "Content-type: image/x-xbitmap\r\n"
#define HTTP_BLANK  "\r\n"
#define HTTP_MAXLEN 80

#define EGI_STARTS  "<!--#"
#define EGI_STARTLEN (sizeof(EGI_STARTS)-1)
#define EGI_ENDS    "-->"
#define EGI_EXT     ".egi"

#define MAX_EGINAME 31
#define MAX_EGIVAL  127

#define EGI_BUFFLEN 1024

/* Application-specific storage */
#define VARSPACE 1000
typedef struct {
    FILE *in;                       /* File I/P pointer */
    long count;                     /* State variable/counter */
    void (*egi)(TSOCK *ts, char *s);/* Pointer to EGI handler routine */
    int vlen;                       /* Total used length of variable space */
    char vars[VARSPACE];            /* Space for storing local variables */
} APPDATA;

/* Structure to store an EGI handler function */
typedef struct
{
    void (*func)(TSOCK *ts, char *str);
    char name[MAX_EGINAME];
} EGI_FUNC;

/* Prototypes */
int buff_inprintf(CBUFF *bp, char *str, ...);
int put_connvar(TSOCK *ts, char *name, char *val);
char *get_connvar(TSOCK *ts, char *name);
char *find_connvar(TSOCK *ts, char *name);
int put_connvarlen(TSOCK *ts, char *name, int namlen, char *val, int valen);
void url_connvars(TSOCK *ts, char *str);
int url_decode(char *str, int len);
FILE *url_fopen(char *str);
void disp_connvars(TSOCK *ts);

/* EOF */
