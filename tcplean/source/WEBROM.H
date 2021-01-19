/* Definitions for Web ROM filesystem */

/* The filesystem is in 1 or more ROMs. At the start of the first ROM is a
** directory of 1 or more filename blocks, each of which have pointers to
** data blocks. The end of the directory is marked by a dummy length of FFFFh
**
** All the files include the appropriate HTTP headers.
*/

#define ROM_FNAMELEN    12  /* Maximum filename size */

typedef struct          /* Filename block structure */
{
    WORD len;               /* Length of file in bytes */
    WORD start;             /* Start address of file data in ROM */
    WORD check;             /* TCP checksum of file */
    BYTE flags;             /* Embedded Gateway Interface (EGI) flags */
    char name[ROM_FNAMELEN];/* Lower-case filename with extension */
} ROM_FNAME;

/* Embedded Gateway Interface (EGI) flag values */
#define EGI_ATVARS      0x01    /* '@' variable substitution scheme */
#define EGI_HASHVARS    0x02    /* '#' and '|' boolean variables */

/* EOF */
