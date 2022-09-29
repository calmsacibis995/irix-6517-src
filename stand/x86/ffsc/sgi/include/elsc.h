/*
 * elsc.h
 *	Prototypes and declarations for ELSC communications and control
 *	functions.
 */
#ifndef _ELSC_H_
#define _ELSC_H_

#include "ffsc.h"

struct console;
struct log;
struct msg;

/* Password information */
#define ELSC_PASSWORD_LEN	4	/* Length of the ELSC password */
#define ELSC_DFLT_PASSWORD	"none"	/* Default ELSC password */

/* Display string information */
#define ELSC_DSP_STRING_LEN	8	/* # of chars in actual string */
#define ELSC_DSP_CODE_INDEX	8	/* Index of priority code */
#define ELSC_DSP_LENGTH		10	/* Total string length */

/* Display string codes */
#define EDC_NONE	0		/* No info */
#define EDC_INFO	'1'		/* Information message */
#define EDC_WARNING	'2'		/* Warning condition */
#define EDC_FATAL	'3'		/* Severe/fatal error */
#define EDC_HUB		'4'		/* Message originated from HUB */

/* A description of a single ELSC */
typedef struct elsc {
	int	    Flags;		/* Flags (see below) */
	int	    Owner;		/* Current user of this ELSC */
	int	    FD;			/* FD for serial port */
	bayid_t	    Bay;		/* Bay ID */
	modulenum_t ModuleNum;		/* Module number */
	rackid_t    EMsgRack;		/* Rack whose COM1 gets ELSC msgs */
	rackid_t    EMsgAltRack;	/* Rack whose COM5 gets ELSC msgs */
	char	    DispData[ELSC_DSP_LENGTH];		/* Display info */
	char	    Password[ELSC_PASSWORD_LEN+1];	/* ELSC password */
	char	    Name;		/* Familiar name (U/L) */
	struct log  *Log;		/* Message log */
	struct msg  *InMsg;		/* Incoming messages from ELSC */
	struct msg  *OutMsg;		/* Outgoing messages to ELSC */
	struct msg  *OutAck;		/* Responses from ELSC */
} elsc_t;

#define EF_OFFLINE	0x00000001	/* Didn't respond to last request */

#define EO_NONE		0		/* Unowned! */
#define EO_ROUTER	1		/* Owned by the router */
#define EO_REMOTE	2		/* Owned by a remote FFSC */
#define EO_CONSOLE	3		/* Owned by a console */


/* ELSC info to save in NVRAM */
typedef struct elsc_save {
	rackid_t	EMsgRack;	/* Rack whose COM1 gets ELSC msgs */
	rackid_t	EMsgAltRack;	/* Rack whose COM5 gets ELSC msgs */
	char		Password[ELSC_PASSWORD_LEN];	/* ELSC password */
	unsigned char	reserved[20-ELSC_PASSWORD_LEN];	/* reserved */
	uint32_t	Flags;		/* Flags */
} elsc_save_t;

#define ELSC_DFLT_RACK		RACKID_UNASSIGNED	/* Default rack */
#define ELSC_DFLT_ALTRACK	RACKID_UNASSIGNED	/* Default altrack */

#define ESF_ALTRACK	0x00000001	/* EMsgAltRack is valid */
#define ESF_PASSWORD	0x00000002	/* Password is valid */


/* Global variables */
extern elsc_t ELSC[MAX_BAYS];		/* Descriptors for all local ELSCs */


/* ELSC control characters */
#define ELSC_CMD_BEGIN		"\r\024"	/* Ctrl-T, unfortunatelly that is
						   not sufficient.  We must make 
						   sure that there wasn't another
						   Ctrl-T in front of us.  If there
						   was, a CR will get rit of it,
						   and if there wasn't, this isn't
						   gonna hurt anything.
						   
						   */

#define ELSC_CMD_BEGIN_CHAR	'\024'
#define ELSC_CMD_END		"\r"	/* CR */

#define ELSC_MSG_BEGIN		"\024"	/* Ctrl-T */
#define ELSC_MSG_BEGIN_CHAR	'\024'
#define ELSC_MSG_END		"\r\n"	/* CR+LF */
#define ELSC_MSG_END_CHAR1	'\r'
#define ELSC_MSG_END_CHAR2	'\n'

#define ELSC_RESP_OK		"ok"
#define ELSC_RESP_OK_LEN	2
#define ELSC_RESP_ERR		"err"
#define ELSC_RESP_ERR_LEN	3


/* Function prototypes */
void   elscCheckResponse(elsc_t *, char *);
STATUS elscDoAdminCommand(bayid_t, const char *, char *);
STATUS elscInit(void);
STATUS elscPrintMsg(elsc_t *, char *, char *);
STATUS elscProcessCommand(elsc_t *, char *);
STATUS elscProcessInput(elsc_t *);
STATUS elscQueryModuleNum(bayid_t, modulenum_t *);
STATUS elscSendCommand(bayid_t, const char *);
STATUS elscSendPassword(bayid_t, int);
STATUS elscSetPassword(bayid_t, const char *, int);
STATUS elscSetup(elsc_t *);
void   elscUpdateDisplay(elsc_t *, char *);
STATUS elscUpdateModuleNums(void);
STATUS elscUpdateSaveInfo(void);

#ifndef PRODUCTION
STATUS elscList(int);
STATUS elscShow(elsc_t *, int, char *);
#endif  /* !PRODUCTION */

#endif  /* _ELSC_H_ */
