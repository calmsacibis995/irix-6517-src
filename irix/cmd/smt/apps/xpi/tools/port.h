/* @(#)port.h	1.1, AMD */
/* File port.h */

#ifdef MSDOS

#define READ   "r"
#define WRITE  "w"
#define WR_RD  "w+"
#define RDBIN  "rb"
#define WRBIN  "wb"
#define RDWRBIN  "w+b"

#endif

#ifdef unix

#define READ   "r"
#define WRITE  "w"
#define WR_RD  "w+"
#define RDBIN  "r"
#define WRBIN  "w"
#define RDWRBIN  "w+"

#endif
