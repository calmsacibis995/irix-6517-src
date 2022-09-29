#ident	"plp.h:  $Revision: 1.1 $"

#define	FIRST_CHAR	0x20
#define	LAST_CHAR	0x5f
#define	NCHARS		(LAST_CHAR - FIRST_CHAR + 1 + 2) /* 2 for lf/cr */
#define	RWCHARS		(LAST_CHAR - FIRST_CHAR + 1)

#define	LFIRST_CHAR		0x0
#define	LLAST_CHAR		0xff
#define	LNCHARS			(LLAST_CHAR - LFIRST_CHAR + 1 + 1) /*1 for lf*/
#define	LRWCHARS			(LLAST_CHAR - LFIRST_CHAR+1)

#define MS1			1000		/* 1 ms */
#define	PRINTER_DELAY		(5 * 1000)	/* 5 sec */
#define	PRINTER_RESET_DELAY	8		/* 8 ms */
