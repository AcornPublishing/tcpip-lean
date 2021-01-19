/* Win32 serial & other functions for 'Creating TCP/IP from Scratch' */
/*
** v0.01 JPB 24/11/99
** v0.02 JPB 21/3/00 Added LiteLink initialisation, and 'txstr' function
** v0.03 JPB 23/3/00 Added 'selectuart' for multiple channels
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <conio.h>

#include "serpc.h"
#include "win32def.h"

HANDLE commh=INVALID_HANDLE_VALUE;
int commchan;
HANDLE commhandles[MAXCOMMS];

/* Permissible IRDA baud rates */
long irbauds[] = {115200L, 57600L, 38400L, 19200L, 9600L, 4800L, 2400L, 0};

HANDLE opencomm(INT port, long baud);
void closecomm(HANDLE h);
int writecomm(HANDLE h, char *buff, int len);
int readcomm(HANDLE h, char *buff, int maxlen);

/* Initialise serial port, return 0 if failed */
unsigned setuart(int com, long baud, int irq)
{
    unsigned ret=0;

    if (commchan>=0 && commchan<MAXCOMMS &&
        (commh=opencomm(com, baud)) != INVALID_HANDLE_VALUE)
    {
        commhandles[commchan] = commh;
        ret = 1;
    }
    return(ret);
}

/* Uninitialise serial port */
void clearuart(void)
{
    closecomm(commh);
}

/* Select a serial channel for initialisation or read/write  */
void selectuart(int n)
{
    commh = commhandles[commchan = n];
}

/* Receive a character, NOCHAR if none */
unsigned rxchar(void)
{
    char c;

    return(readcomm(commh, &c, 1) ? c&0xff : NOCHAR);
}

/* Transmit a character */
void txchar(unsigned char c)
{
    writecomm(commh, &c, 1);
}

/* Send a string out of the serial port */
void txstr(char *s)
{
    while (*s)
        txchar(*s++);
}

/* Open comm port (1 to 9) given baud rate. Disable XON/XOFF handshaking 
** Return port handle, or invalid handle if error */
HANDLE opencomm(INT port, long baud)
{
    COMMTIMEOUTS touts;
    DCB dcb;
    char temps[]="COMx";
    HANDLE h;

    memset(&touts, 0, sizeof(touts));
    touts.ReadIntervalTimeout = MAXDWORD;
    temps[3] = (char)('0' + port);
    h = CreateFile(temps, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 
                 0, NULL);
    if (h!=INVALID_HANDLE_VALUE && SetupComm(h, 2048, 2048) &&
        SetCommTimeouts(h, &touts) && GetCommState(h, &dcb))
    {
        dcb.BaudRate = baud;
        dcb.Parity = NOPARITY;
        dcb.fOutX = dcb.fInX = 0;
        SetCommState(h, &dcb);
    }
    else
        h = INVALID_HANDLE_VALUE;
    return(h);
}


/* Close comm port */
void closecomm(HANDLE h)
{
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
}

/* Write to comm port */
int writecomm(HANDLE h, char *buff, int len)
{
    ULONG count=0;

    WriteFile(h, buff, len, &count, NULL);
    return((int)count);
}

/* Read from comm port */
int readcomm(HANDLE h, char *buff, int maxlen)
{
    ULONG count=0;

    ReadFile(h, buff, maxlen, &count, NULL);
    return((int)count);
}

/* Return millisecond time */
DWORD mstime(void)
{
    return(GetTickCount());
}

/* Set IRDA baud rate using 'LiteLink' protocol; return 0 if invalid rate */
int litelink(long baud)
{
    int n=0, ok=0;

    while (irbauds[n] && !(ok = baud==irbauds[n]))
        n++;                                    /* Find baud rate table index */
    if (ok)
    {
        EscapeCommFunction(commh, SETDTR);      /* Allow device to warm up */
        EscapeCommFunction(commh, SETRTS);      /* ..with dtr & rts high */
        delay(50);
        EscapeCommFunction(commh, CLRRTS);      /* Pulse RTS low to reset */
        delay(2);
        EscapeCommFunction(commh, SETRTS);
        while (n--)                             /* Loop using table index.. */
        {
            delay(2);
            EscapeCommFunction(commh, CLRDTR);  /* ..pulse DTR low */
            delay(2);
            EscapeCommFunction(commh, SETDTR);  /* ..then high */
        }
        delay(2);
    }
    return(ok);
}

/* EOF */

