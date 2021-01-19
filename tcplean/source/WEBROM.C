/* Simple utility to prepare a ROM image of Web files. Copyright (c) Iosoft Ltd 2000
**
** This software is only licensed for distribution with the book 'TCP/IP Lean', 
** and may only be used for personal experimentation by the purchaser 
** of that book, on condition that this copyright notice is retained. 
** For commercial licensing, contact license@iosoft.co.uk
**
** This is experimental software; use it entirely at your own risk */

/*
** Files to be processed must all be in one directory
** The first entry in the ROM file directory will be INDEX.HTM
** All files are stored in ROM with an appropriate HTTP header
*/

/*
** v0.01 JPB 28/3/00
** v0.02 JPB 29/3/00 Revised file scheme with directory at start of ROM
** v0.03 JPB 30/3/00 Check for 'index.htm', put it first in filesystem
** v0.04 JPB 31/3/00 Removed data checksums and blocks
** v0.05 JPB 31/3/00 Padded files to 32-byte boundaries
** v0.06 JPB 31/3/00 Added HTML headers to all files
** v0.07 JPB 2/4/00  Removed headers again!
** v0.08 JPB 21/4/00 Added HTTP headers and TCP checksums
** v0.09 JPB 9/5/00  Changed 'SSI' to 'EGI'
**                   Added EGI flags field to ROM directory
** v0.10 JPB 12/5/00 Added EGI_HASHVARS flag
** v0.11 JPB 17/5/00 Adapted for VC6
*/

#define VERSION "0.11"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "netutil.h"
#include "webrom.h"

#define INDEXFILE   "index.htm"
#define MAXFILES    500
#define MAXFILELEN  32000

char filenames[MAXFILES][ROM_FNAMELEN+1];
char *filehdrs[MAXFILES];

char srcepath[MAXPATH+8], srcedir[MAXPATH+8], srcefile[MAXPATH+8];
char destfile[MAXPATH+8];
int netdebug;

ROM_FNAME romfname;
char filedata[MAXFILELEN+100];

/* HTTP and HTML text */
#define HTTP_OK     "HTTP/1.0 200 OK\r\n"
#define HTTP_HTM    "Content-type: text/html\r\n"
#define HTTP_TXT    "Content-type: text/plain\r\n"
#define HTTP_GIF    "Content-type: image/gif\r\n"
#define HTTP_XBM    "Content-type: image/x-xbitmap\r\n"
#define HTTP_BLANK  "\r\n"

/* Prototypes */
unsigned readfile(char *buff, char *fname, char *hdr);
long filesize(FILE *stream);

void main(int argc, char *argv[])
{
    char *p, *fname;
    FILE *out;
    int i, nfiles=1, err=0;
    unsigned len;
    long filelen, romoff;
    WORD endw=0xffff;
    ROM_FNAME *rfp;

    printf("WEBROM v" VERSION "\n");          /* Sign on */
    printf("File sizes include HTTP headers\n");
    rfp = &romfname;
    if (argc < 3)
    {
        if (argc < 2)
            printf("No destination file specified\n");
        else
            printf("No source filepath specified\n");
        printf("e.g. WEBROM test.rom c:\\temp\\romdocs\n");
        exit(1);
    }
    strcpy(destfile, argv[1]);
    if ((p=strrchr(destfile, '.'))==0 || !isalpha(*(p+1)))
        strcat(destfile, ".ROM");
    strlwr(destfile);
    if (argv[2][0]!='\\' && argv[2][1]!=':' && argv[2][0]!='.')
        strcpy(srcepath, ".\\");
    strcat(srcepath, argv[2]);
    if (srcepath[strlen(srcepath)-1] != '\\')
        strcat(srcepath, "\\");
    strlwr(srcepath);
    strcat(strcpy(srcedir, srcepath), "*.*");

    /* First pass: get files in source directory, check for index.htm */
    if ((fname = find_first(srcedir)) != 0) do
    {
        if (strlen(fname) > ROM_FNAMELEN)
            printf("ERROR: long filename '%s' not included\n", fname);
        else if (!stricmp(fname, INDEXFILE))
        {
            strcpy(filenames[0], fname);
            strlwr(filenames[0]);
            filehdrs[0] = HTTP_OK HTTP_HTM HTTP_BLANK;
        }
        else if (strlen(fname) > 2)
        {
            strcpy(filenames[nfiles], fname);
            strlwr(filenames[nfiles]);
            filehdrs[nfiles] =
                strstr(fname, ".htm") ? HTTP_OK HTTP_HTM HTTP_BLANK :
                strstr(fname, ".egi") ? HTTP_OK HTTP_HTM HTTP_BLANK :
                strstr(fname, ".txt") ? HTTP_OK HTTP_TXT HTTP_BLANK :
                strstr(fname, ".gif") ? HTTP_OK HTTP_GIF HTTP_BLANK :
                strstr(fname, ".xbm") ? HTTP_OK HTTP_XBM HTTP_BLANK :
                HTTP_OK HTTP_BLANK;
            nfiles++;
        }
    } while ((fname = find_next()) != 0 && nfiles<MAXFILES);
    if (!filenames[0][0])
        printf("ERROR: default file '%s' not found\n", INDEXFILE);
    else if (nfiles > MAXFILES)
        printf("ERROR: only %u files allowed\n", MAXFILES);
    else if ((out=fopen(destfile, "wb"))==0)
        printf("ERROR: can't open destination file\n");
    else
    {
        /* Second pass: create ROM directory */
        printf("Creating %s using %u files from %s\n",
            destfile, nfiles, srcepath);
        romoff = nfiles * sizeof(ROM_FNAME) + 2;
        for (i=0; i<nfiles && !err; i++)
        {
            printf("%12s ", fname=filenames[i]);
            if ((len = readfile(filedata, fname, filehdrs[i])) == 0)
            {
                printf("ERROR reading file\n");
                err++;
            }
            else
            {
                printf("%5u bytes\n", len);
                rfp->start = (WORD)romoff;
                rfp->len = len;
                rfp->check = csum(filedata, (WORD)len);
                rfp->flags = strstr(fname, ".egi") ? EGI_ATVARS+EGI_HASHVARS:0;
                memset(rfp->name, 0, ROM_FNAMELEN);
                strncpy(rfp->name, fname, strlen(fname));
                fwrite(rfp, 1, sizeof(ROM_FNAME), out);
                romoff += rfp->len;
            }
        }
        fwrite(&endw, 1, 2, out);

        /* Third pass: write out file data */
        for (i=0; i<nfiles && !err; i++)
        {
            if ((len = readfile(filedata, filenames[i], filehdrs[i])) == 0)
            {
                printf("ERROR reading '%s'\n", filenames[i]);
                err++;
            }
            else
                fwrite(filedata, 1, len, out);
        }
        if (!err)
        {
            filelen = ftell(out);
            if (filelen == romoff)
                printf("\nTotal ROM    %5lu bytes\n", filelen);
            else
                printf("ERROR: ROM size %lu, file size %lu\n", romoff, filelen);
            if (ferror(out))
                printf("ERROR writing output file\n");
        }
        fclose(out);
    }
}

/* Read the HTTP header and file into buffer, return total len, 0 if error */
unsigned readfile(char *buff, char *fname, char *hdr)
{
    unsigned len;
    FILE *in;

    strcpy(buff, hdr);
    len = strlen(hdr);
    strcat(strcpy(srcefile, srcepath), fname);
    in = fopen(srcefile, "rb");
    len += fread(&buff[len], 1, MAXFILELEN, in);
    if (ferror(in))
        len = 0;
    fclose(in);
    return(len);
}

/* Return size of opened file in bytes */
long filesize(FILE *stream)
{
   long curpos, length;

   curpos = ftell(stream);
   fseek(stream, 0L, SEEK_END);
   length = ftell(stream);
   fseek(stream, curpos, SEEK_SET);
   return(length);
}

/* EOF */
