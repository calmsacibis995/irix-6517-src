/* ftpLib.h - arpa File Transfer Protocol library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,11sep92,jmm  added external definition for ftpErrorSupress (for spr #1257)
01h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01e,19oct90,shl  changed ftpCommand() to use variable length argument list.
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,07aug90,shl  added INCftpLibh to #endif.
01b,20mar87,dnw  prepended FTP_ to reply codes.
01a,07nov86,dnw  written
*/

#ifndef __INCftpLibh
#define __INCftpLibh

#ifdef __cplusplus
extern "C" {
#endif

/* For FTP specification see RFC-765 */

/* Reply codes. */

#define FTP_PRELIM	1	/* positive preliminary */
#define FTP_COMPLETE	2	/* positive completion */
#define FTP_CONTINUE	3	/* positive intermediate */
#define FTP_TRANSIENT	4	/* transient negative completion */
#define FTP_ERROR	5	/* permanent negative completion */

#define FTP_NOACTION    550     /* requested action not taken */

/* Type codes */

#define	TYPE_A		1	/* ASCII */
#define	TYPE_E		2	/* EBCDIC */
#define	TYPE_I		3	/* image */
#define	TYPE_L		4	/* local byte size */

/* Form codes */

#define	FORM_N		1	/* non-print */
#define	FORM_T		2	/* telnet format effectors */
#define	FORM_C		3	/* carriage control (ASA) */

/* Structure codes */

#define	STRU_F		1	/* file (no record structure) */
#define	STRU_R		2	/* record structure */
#define	STRU_P		3	/* page structure */

/* Mode types */

#define	MODE_S		1	/* stream */
#define	MODE_B		2	/* block */
#define	MODE_C		3	/* compressed */

/* Record Tokens */

#define	REC_ESC		'\377'	/* Record-mode Escape */
#define	REC_EOR		'\001'	/* Record-mode End-of-Record */
#define REC_EOF		'\002'	/* Record-mode End-of-File */

/* Block Header */

#define	BLK_EOR		0x80	/* Block is End-of-Record */
#define	BLK_EOF		0x40	/* Block is End-of-File */
#define BLK_ERRORS	0x20	/* Block is suspected of containing errors */
#define	BLK_RESTART	0x10	/* Block is Restart Marker */

#define	BLK_BYTECOUNT	2	/* Bytes in this block */

/* externals */

extern BOOL ftpErrorSupress;	/* TRUE = don't print error messages */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	ftpLogin (int ctrlSock, char *user, char *passwd,
			  char *account);
extern STATUS 	ftpXfer (char *host, char *user, char *passwd, char *acct,
			 char *cmd, char *dirname, char *filename,
			 int *pCtrlSock, int *pDataSock);
extern int 	ftpCommand (int ctrlSock, char *fmt, int arg1, int arg2,
			    int arg3, int arg4, int arg5, int arg6);
extern int 	ftpDataConnGet (int dataSock);
extern int 	ftpDataConnInit (int ctrlSock);
extern int 	ftpHookup (char *host);
extern int 	ftpReplyGet (int ctrlSock, BOOL expecteof);

#else	/* __STDC__ */

extern STATUS 	ftpLogin ();
extern STATUS 	ftpXfer ();
extern int 	ftpCommand ();
extern int 	ftpDataConnGet ();
extern int 	ftpDataConnInit ();
extern int 	ftpHookup ();
extern int 	ftpReplyGet ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCftpLibh */
