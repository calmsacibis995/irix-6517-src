/*
 * buf.h
 *	Declarations & prototypes for the FFSC I/O buffer functions
 */

#ifndef _BUF_H_
#define _BUF_H_

struct log;


#define BUF_EOLSEQ_LEN 3	/* Max # of chars in an EOL sequence */

/* Type declarations */
typedef struct buf {
	int	FD;		/* I/O descriptor */
	char	*Buf;		/* Start of buffer */
	char	*Curr;		/* Current location in buffer */
	int	Size;		/* Size of buffer */
	int	Len;		/* Bytes currently in use */
	int	Avail;		/* Bytes available */
	int	Flags;		/* Control flags */
	int	HoldCnt;	/* Number of HOLD's pending */
	int	EOLNdx;		/* Current index into EOL sequence */
	char	EOLSeq[BUF_EOLSEQ_LEN + 1];	/* The EOL Sequence */
	fd_set	*FDSet;		/* Ptr to fd_set for our FD */
	int	*NumFDs;	/* Ptr to # of FDs in FDSet */
	struct log *Log;	/* Supplemental log */
} buf_t;

#define BUF_INPUT	0x00000001	/* Input buffer */
#define BUF_OUTPUT	0x00000002	/* Output buffer */
#define BUF_MESSAGE	0x00000004	/* Buffer hold msgs, not "real" I/O */
#define BUF_HOLD	0x00000008	/* Do not access device */
#define BUF_FULL	0x00000010	/* Input buffer is full */
#define BUF_OVERFLOW	0x00000020	/* Output buffer has overflowed */
#define BUF_MONEOL	0x00000040	/* Monitor for EOL */
#define BUF_EOL		0x00000080	/* Currently at EOL */

#define BUF_MSG_END	"\r\n"		/* Official buf_t Message Delimiter */
#define BUF_MSG_END_LEN	2		/* Length of BUF_MSG_END string */


/* Function prototypes */
STATUS  bufAppendAddMsgs(buf_t *, buf_t *, int, buf_t *);
STATUS	bufAppendBuf(buf_t *, buf_t *);
STATUS	bufAppendBufToEOL(buf_t *, buf_t *);
STATUS  bufAppendMsgs(buf_t *, buf_t *);
void	bufAttachFD(buf_t *, int);
void	bufAttachLog(buf_t *, struct log *);
void	bufClear(buf_t *);
void	bufDetachFD(buf_t *);
void	bufDetachLog(buf_t *);
STATUS	bufFlush(buf_t *);
STATUS	bufHold(buf_t *);
buf_t	*bufInit(int, int, fd_set *, int *);
void	bufLeftAlign(buf_t *, char *);
STATUS	bufRead(buf_t *);
STATUS	bufRelease(buf_t *);
int	bufWrite(buf_t *, char *, int);

#ifndef PRODUCTION
STATUS  bufShow(buf_t *, int, char *);
#endif  /* !PRODUCTION */

#endif  /* _BUF_H_ */
