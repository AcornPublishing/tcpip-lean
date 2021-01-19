/* Win32 definitions for 'TCP/IP Lean' */

#include <memory.h>
#include <io.h>

#ifndef BYTE
#define BYTE unsigned char
#endif
#ifndef WORD
#define WORD unsigned short
#endif

#define MAXPATH 256

#define inp(a)      _inp(a)
#define outp(a, b)  _outp(a, b)
#define inpw(a)     _inpw(a)
#define outpw(a, b) _outpw(a, b)

void msdelay(WORD millisec);
#define delay(x)    msdelay(x)

/* EOF */

