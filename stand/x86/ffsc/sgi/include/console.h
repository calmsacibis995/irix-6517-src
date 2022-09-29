/*
 * console.h
 *	Prototypes and declarations for console (terminal/modem) control
 *	functions.
 */
#ifndef _CONSOLE_H_
#define _CONSOLE_H_


struct buf;
struct elsc;
struct log;
struct pagebuf;
struct user;


/* FD array indices for consCreate & console_t */
#define CCFD_SYS	0	/* System device FD */
#define CCFD_USER	1	/* User device FD */
#define CCFD_C2RR	2	/* Console->Router Request pipe FD */
#define CCFD_C2RA	3	/* Console->Router Acknowledge pipe FD */
#define CCFD_R2C	4	/* Router->Console pipe FD */
#define CCFD_REMOTE	5	/* FD of remote system */
#define CCFD_PROGRESS	6	/* FD for progress indicator */
#define CCFD_MAX	7	/* # of FDs */	


/* Console information */
#define CONS_NAME_LEN   11		/* Max length of console_t name */

#define CONC_BS		0		/* Backspace character */
#define CONC_ESC	1		/* Escape character */
#define CONC_END	2		/* Command end character */
#define CONC_KILL	3		/* Cancel current line */
#define CONC_EXIT	4		/* Exit current mode */
#define CONC_NULL	5		/* \0 is always last! */
#define CONC_MAXCTL	(CONC_NULL+1)	/* Max # of ctrl chars */

#define COND_PB		0		/* Piggyback response delimiter */
#define COND_HDR	1		/* Header delimiter */
#define COND_ELSC	2		/* Start Of ELSC Cmd char */
#define COND_RESPSEP	3		/* Response separator */
#define COND_OBP	4		/* Out of Band message Prefix char */
#define COND_NULL	5		/* Always end with null char! */
#define COND_MAXDELIM	(COND_NULL+1)	/* Max # of delimiter chars */

#define CONP_BACK	0		/* Page backward */
#define CONP_FWD	1		/* Page forward */
#define CONP_QUIT	2		/* Quit paged output */
#define CONP_MAXPAGER	3		/* Max # of pager chars */

typedef struct console {
	char  Name[CONS_NAME_LEN + 1];	/* Descriptive identifier */
	int   Type;			/* Type of console */
	int   BaseMode;			/* Initial/default input mode */
	int   PrevMode;			/* Previous mode */
	int   Mode;			/* Current input mode */
	int   SysState;			/* System buf state machine state */
	int   SysOffset;		/* Current offset in SysInBuf */
	int   UserState;		/* User buf state machine state */
	int   UserOffset;		/* Current offset in UserInBuf */
	int   CEcho;			/* Command echo mode */
	int   EMsg;			/* ELSC unsolicited msg mode */
	int   RMsg;			/* Response msg mode */
	int   NVRAMID;			/* NVRAM ID for persistent info */
	int   Flags;			/* Control flags */
	int   WakeUp;			/* EMSG wake up in usecs (<0 = dflt) */
	int   ProgressCount;		/* Counter for progress indicator */
	int   LinesPerPage;		/* # of lines in a page of output */
	int   FDs[CCFD_MAX];		/* Array of relevant FDs */
	struct buf  *SysInBuf;		/* Buffer for input from system */
	struct buf  *SysOutBuf;		/* Buffer for output to system */
	struct buf  *UserInBuf;		/* Buffer for input from user */
	struct buf  *UserOutBuf;	/* Buffer for output to user */
	struct buf  *MsgBuf;		/* Buffer for unsolicited messages */
	struct elsc *ELSC;		/* Attached ELSC in ELSC mode */
	struct log  *Log;		/* Associated user output log */
	struct user *User;		/* Associated user */
	struct pagebuf *PageBuf;	/* Paged output buffer */
	char  Ctrl[CONC_MAXCTL];	/* Control characters */
	char  Delim[COND_MAXDELIM];	/* Delimiter characters */
	char  Pager[CONP_MAXPAGER];	/* Pager characters */
} console_t;

#define CONT_UNKNOWN	0		/* Don't know what the user is */
#define CONT_TERMINAL	1		/* Terminal (TTY1) */
#define CONT_ALTERNATE	2		/* Remote access console (e.g. COM5) */
#define CONT_SYSTEM	3		/* User is The System (e.g. IRIX) */
#define CONT_REMOTE	4		/* Another FFSC */

#define CONM_UNKNOWN	0		/* Don't know user input mode */
#define CONM_CONSOLE	1		/* Console mode */
#define CONM_FFSC	2		/* FFSC mode */
#define CONM_ELSC	3		/* ELSC mode */
#define CONM_COPYUSER	4		/* Copy-user-to-remote mode */
#define CONM_COPYSYS	5		/* Copy-sys-to-remote mode */
#define CONM_FLASH	6		/* Firmware flash mode */
#define CONM_MODEM	7		/* Modem mode */
#define CONM_IDLE	8		/* Console is idle (unassigned) */
#define CONM_WATCHSYS	9		/* Monitor IO6 for commands */
#define CONM_RAT	10		/* Remote Access Tool mode */
#define CONM_PAGER	11		/* Pager mode */
#define CONM_MAX_MODE	11		/* Max user input mode # */

#define CONUM_UNKNOWN	0		/* Unknown user state machine state */
#define CONUM_NORMAL	1		/* Normal conditions in user buf */
#define CONUM_GOTESC	2		/* Last char was FFSC Escape */
#define CONUM_CMD	3		/* Currently reading a command */

#define CONSM_UNKNOWN	0		/* Unknown sys state machine state */
#define CONSM_NORMAL	1		/* Normal conditions */
#define CONSM_ELSCCMD	2		/* Gathering command from ELSC */
#define CONSM_GOTEND1	3		/* Got first END char from ELSC */
#define CONSM_OOBCODE	5		/* Gathering OOB opcode/status */
#define CONSM_OOBLEN	6		/* Gathering OOB length */
#define CONSM_OOBDATA	7		/* Gathering OOB data + checksum */

#define CONCE_OFF	0		/* Do not echo FFSC commands */
#define CONCE_ON	1		/* Echo FFSC commands */
#define CONCE_MAX	2		/* # of CECHO modes */

#define CONEM_OFF	0		/* Do not show ELSC unsol. msgs */
#define CONEM_TERSE	1		/* Show ELSC unsol. msgs. tersely */
#define CONEM_ON	2		/* Show ELSC unsolicited messages */
#define CONEM_MAX	3		/* # of EMSG modes */

#define CONRM_OFF	0		/* Do not show responses */
#define CONRM_ERROR	1		/* Only show error responses */
#define CONRM_ON	2		/* Show all ELSC/FFSC responses */
#define CONRM_VERBOSE	3		/* Include piggybacked FFSC cmds */
#define CONRM_MAX	4		/* # of RMSG modes */

#define CONF_ECHO	0x00000001	/* Manually echo input characters */
#define CONF_SHOWPROMPT 0x00000002	/* Print the appropriate prompt */
#define CONF_HOLDSYSIN  0x00000004	/* System input buffer is held */
#define CONF_HOLDUSERIN 0x00000008	/* User input buffer is held */
#define CONF_REDIRECTED 0x00000010	/* This isn't the intended mode */
#define CONF_PROGRESS	0x00000020	/* Show progress indicator */
#define CONF_FLASHMSG	0x00000040	/* Display reassuring flash msg */
#define CONF_IGNOREEXIT 0x00000080	/* Ignore EXIT chars */
#define CONF_EVALWAKEUP	0x00000100	/* Re-evaluate the wake-up time */
#define CONF_REBOOTMSG	0x00000200	/* Print "reboot complete" msg */
#define CONF_STOLENMSG	0x00000400	/* Print "console stolen" msg */
#define CONF_OOBMSG	0x00000800	/* Gathering OOB msg in SysInBuf */
#define CONF_NOLOG	0x00001000	/* Don't log user output */
#define CONF_NOPAGE	0x00002000	/* Don't page output */
#define CONF_READYMSG	0x00004000	/* Print "READY" msg */
#define CONF_WELCOMEMSG 0x00008000	/* Print "entering <mode>" msg */
#define CONF_OOBOKSYS	0x00010000	/* Enable OOB msgs on CCFD_SYS */
#define CONF_OOBOKUSER  0x00020000	/* Enable OOB msgs on CCFD_USER */
#define CONF_NOFFSCSYS	0x00040000	/* Disable FFSC cmds on CCFD_SYS */
#define CONF_NOFFSCUSER 0x00080000	/* Disable FFSC cmds on CCFD_USER */
#define CONF_UNSTEALMSG 0x00100000	/* Print "console restored" msg */
#define CONF_SYSCDIRECT  0x00200000	/* Print system COM target msg */


/* Console information held in NVRAM */
#define MAX_CONSINFO_CHARSTR	8

typedef struct consoleinfo {
	char	 Ctrl[MAX_CONSINFO_CHARSTR];	/* Control chars */
	char	 Delim[MAX_CONSINFO_CHARSTR];	/* Delimiter chars */
	char	 CEcho;			/* Command echoing mode */
	char	 EMsg;			/* ELSC message mode */
	char	 RMsg;			/* Response message mode */
	char	 NVRAMID;		/* NVRAM ID for persistent info */
	uint32_t PFlags;		/* Persistent flags */
	int32_t	 WakeUp;		/* Wake up interval in 0.1 sec units */
	int32_t  LinesPerPage;		/* Lines per page of paged output */
	char	 Pager[MAX_CONSINFO_CHARSTR];	/* Pager chars */
	int32_t  reserved[2];		/* reserved for future expansion */
} consoleinfo_t;

#define CIF_VALID	0x00000001	/* This entry contains valid data */
#define CIF_REBOOTMSG	0x00000002	/* Print "reboot complete" message */
#define CIF_PAGERVALID	0x00000004	/* Pager fields are valid */
#define CIF_NOPAGE	0x00000008	/* Don't page output */


/* Prompt strings */
#define STR_BS			"\b \b"
#define STR_BS_LEN		3
#define STR_CMDCANCEL		" CANCEL\r\nMMSC> "
#define STR_CMDCANCEL_LEN	15
#define STR_END			"\r\n"
#define STR_END_LEN		2
#define STR_ESCPROMPT		" MMSC> "
#define STR_ESCPROMPT_LEN	7
#define STR_KILLESCPROMPT	"\b\b\b\b\b\b\b       \b\b\b\b\b\b\b"
#define STR_KILLESCPROMPT_LEN	21
#define STR_FFSCPROMPT		"\r\nMMSC> "
#define STR_FFSCPROMPT_LEN	8


/* Buffer sizes */
#define CONSOLE_KBD_BUF_SIZE		160	/* ~2 lines */
#define CONSOLE_SCREEN_BUF_SIZE		4096	/* 2+ 80x24 screens */
#define CONSOLE_MSG_BUF_SIZE		(2048 * MAX_RACKS) /* 1 screen/rack */


/* Public function prototypes */
STATUS consInit(void);
STATUS consUpdateConsInfo(console_t *);

/* Console mode entry functions */
STATUS ceEnterMode(console_t *, int);

STATUS ceEnterConsoleMode(console_t *);
STATUS ceEnterCopySysMode(console_t *);
STATUS ceEnterCopyUserMode(console_t *);
STATUS ceEnterELSCMode(console_t *);
STATUS ceEnterFFSCMode(console_t *);
STATUS ceEnterFlashMode(console_t *);
STATUS ceEnterIdleMode(console_t *);
STATUS ceEnterModemMode(console_t *);
STATUS ceEnterPagerMode(console_t *);
STATUS ceEnterRATMode(console_t *);
STATUS ceEnterWatchSysMode(console_t *);

/* Console mode exit functions */
STATUS clLeaveCurrentMode(console_t *);

STATUS clLeaveConsoleMode(console_t *);
STATUS clLeaveCopyUserMode(console_t *);
STATUS clLeaveCopySysMode(console_t *);
STATUS clLeaveELSCMode(console_t *);
STATUS clLeaveFFSCMode(console_t *);
STATUS clLeaveFlashMode(console_t *);
STATUS clLeaveModemMode(console_t *);
STATUS clLeavePagerMode(console_t *);
STATUS clLeaveRATMode(console_t *);
STATUS clLeaveWatchSysMode(console_t *);

/* Console task utility functions */
int    cmCheckTTYFlags(console_t *);
void   cmPagerUpdate(console_t *);
void   cmPrintMsg(console_t *, char *);
void   cmPrintResponse(console_t *, char *);
void   cmSendFFSCCmd(console_t *, char *, char *);
void   cmSendFFSCCmdNoPB(console_t *, char *, char *);

#ifndef PRODUCTION
STATUS consList(int);
STATUS consShow(console_t *, int, char *);
#endif  /* !PRODUCTION */

/* Router input functions */
STATUS crProcessCmd(console_t *, char *);
STATUS crRouterIn(console_t *);

/* System port input functions */
STATUS csSysIn(console_t *);

/* User input functions */
STATUS cuUserIn(console_t *);

#endif  /* _CONSOLE_H_ */
