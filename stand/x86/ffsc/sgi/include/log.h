/*
 * log.h
 *	Declarations & prototypes for FFSC logging functions
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <semlib.h>		/* For SEM_ID */


#define LOG_NAME_LEN	15

typedef struct log {
	char	Name[LOG_NAME_LEN + 1];	/* Familiar name */
	int	Flags;		/* Control flags */
	int	Type;		/* Type of data being logged */
	int	Num;		/* Log number */

	char	*Buf;		/* Text buffer */
	int	MaxSize;	/* Max size of buffer */
	int	BufSize;	/* Number of bytes in use in buffer */
	int	BufAvail;	/* Number of bytes remaining in buffer */
	char	*BufStart;	/* Ptr to start of buffer */
	char	*BufNext;	/* Ptr to next available byte in buffer */

	char	**Lines;	/* Pointers to individual lines in buffer */
	int	MaxLines;	/* Max number of line pointers */
	int	NumLines;	/* Number of line pointers in use */
	int	StartLine;	/* Index of first line's pointer */
	int	NextLine;	/* Index of next line's pointer */

	SEM_ID	Lock;		/* Hold semaphore */
} log_t;

/* Flags */
#define LOGF_DISABLE	0x00000001	/* Disable logging */

/* Data types */
#define LOGT_ELSC	1	/* ELSC output */
#define LOGT_CONSOLE	2	/* Console output */
#define LOGT_DEBUG	3	/* Debugging output */
#define LOGT_DISPLAY	4	/* Display cmds/responses */


/* Assorted constants */
#define LOG_TRUNCATE_TRAILER		"...\r\n"
#define LOG_TRUNCATE_TRAILER_LEN	5

#define MAX_LOGBUFSIZE 80*200*7  /* The Maximum combined size of all the log
				    buffers */


/* Global logs */
#define LOG_TERMINAL	0
#define LOG_ELSC_UPPER	1
#define LOG_ELSC_LOWER	2
#define LOG_BASEIO	3
#define LOG_ALTCONS	4
#define LOG_DEBUG	5
#define LOG_DISPLAY	6

#define NUM_LOGS	7
#define MAX_LOGS	8

extern log_t *logs[NUM_LOGS];
extern log_t *ELSCLogs[MAX_BAYS];

#define logTerminal	(logs[LOG_TERMINAL])
#define logELSCUpper	(logs[LOG_ELSC_UPPER])
#define logELSCLower	(logs[LOG_ELSC_LOWER])
#define logBaseIO	(logs[LOG_BASEIO])
#define logAltCons	(logs[LOG_ALTCONS])
#define logDebug	(logs[LOG_DEBUG])
#define logDisplay	(logs[LOG_DISPLAY])


/* Persistent log info (saved in NVRAM) */
typedef struct loginfo {
	int32_t	Flags;		/* Control flags */
	int32_t	MaxLines;	/* Max number of line pointers */
	int32_t	AvgLength;	/* Average length of a line */
	int32_t reserved;	/* reserved for future enhancements */
} loginfo_t;

#define LIF_VALID	0x00000001	/* This entry is valid */
#define LIF_DISABLE	0x00000002	/* Logging disabled */

extern loginfo_t ffscLogInfo[MAX_LOGS];	/* Saved info about logs */


/* Function prototypes */
void	logClear(log_t *);
log_t	*logCreate(char *, int, int, loginfo_t *);
STATUS	logHold(log_t *);
STATUS	logInitSGI(void);
int	logRead(log_t *, char *, int);
int	logReadLines(log_t *, char *, int, int, int);
void	logRelease(log_t *);
STATUS	logUpdateSaveInfo(void);
void	logWrite(log_t *, char *, int);
void	logWriteLine(log_t *, char *);

#ifndef PRODUCTION
STATUS	ldump(log_t *, int, int);
STATUS  ldumpb(log_t *);
STATUS  lfill(log_t *, int);
STATUS  llines(log_t *);
STATUS  llist(int);
STATUS  lshow(log_t *, int, char *);
#endif  /* !PRODUCTION */

#endif  /* _LOG_H_ */
