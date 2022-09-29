

#ifndef _FP_H_
#define _FP_H_

#include <sys/dvh.h>

/* The supported file systems in the floppy */ 
#define MAC_HFS                 1
#define DOS_FAT                 2
#define MAX_TYPE                3

/* The following defined error codes and constant
   were brought over from DIT MAC library        */
#define MAX_MAC_VOL_NAME    	27
#define MAX_DOS_VOL_NAME	   11
#define MAX_VOL_NAME          27
#define MAX_STR              255

#define TRUE                   1
#define FALSE                  0


/* The data struct of floppy device information */
typedef struct fpinfo_t {
    char   volumelabel[MAX_VOL_NAME+1];
    int    filesystype;	/* File system type */
    int    mediatype;	/* media type */	
    char   dev[50];
    int    devfd;
    unsigned int n_sectors;
    struct volume_header vh;    /* current vh block */
} FPINFO;


extern void   logmsg(char *, ...);
extern int    chkmounts(char *);
extern void   fpMessage(char *, ...);
extern void   fpError(char *, ...);
extern void   fpSysError(char *, ...);
extern void   fpWarn(char *, ...);
extern int    ioctl(int, int, ...);
extern void * safemalloc(unsigned int);
extern void * safecalloc(unsigned int);
extern void   fpblockwrite(int, unsigned char *, int);
extern void   fpblockread(int, unsigned char *, int);
extern int    rfpblockwrite(int, unsigned char *, int);
extern int    rfpblockread(int, unsigned char *, int);
extern struct DsDevice * newDsDevice (FPINFO *);
extern void   freeDsDevice (void ** );
#endif  /* _FP_H_ */
