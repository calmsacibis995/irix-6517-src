/*
 * user.h
 *	Prototypes and declarations for user-related functions
 */
#ifndef _USER_H_
#define _USER_H_

#include "dest.h"

struct console;
struct msg;


/* User contexts */
#define USER_NAME_LEN	15
typedef struct user {
	char	Name[USER_NAME_LEN+1];	/* Familiar name of user */
	int	Type;			/* Type of user */
	int	Flags;			/* Control flags */
	int	NVRAMID;		/* NVRAM ID for persistent info */
	int	Options;		/* OPTIONS flags */
	int	Authority;		/* Authority level */
	dest_t	DfltDest;		/* Default destination */
	struct msg	*InReq;		/* Incoming requests */
	struct msg	*InAck;		/* Responses to incoming requests */
	struct msg	*OutReq;	/* Outgoing requests */
	struct msg	*OutAck;	/* Responses from outgoing requests */
	struct console	*Console;	/* Related console */

	/* Input mode */
	/* Ptr to "pushed" context */
} user_t;

/* user types */
#define UT_UNKNOWN	0		/* Unknown user type */
#define UT_SYSTEM	1		/* User is The System (e.g. IRIX) */
#define UT_CONSOLE	2		/* User is an ordinary console */
#define UT_DISPLAY	3		/* User is the display/switches */
#define UT_REMOTE	4		/* User is the remote console */
#define UT_ADMIN	5		/* Administrative commands context */
#define UT_FFSC		6		/* User is a remote FFSC */
#define UT_LOCAL	7		/* (Pseudo-)User is the local FFSC */

/* user flags */
#define UF_ACTIVE	0x00000001	/* Accept incoming requests */
#define UF_AVAILABLE	0x00000002	/* User is available for work */

/* OPTIONS flags */
#define UO_DELAYPROMPT	0x00000001	/* Show FFSC prompt after *2nd* char */
#define UO_AUTOFFSCMODE	0x00000002	/* Enter FFSC mode on empty cmd */
#define UO_ELSCMODEECHO 0x00000004	/* Tell ELSC to echo in ELSC mode */
#define UO_PRINTBLANK	0x00000008	/* Print blank ELSC messages */
#define UO_NORESPONSE	0x00000010	/* No Response expected for message */

					/* "For internal use only" */
#define UO_ROBMODE	0x80000000	/* Rob Mode destinations */

/* authority levels */
#define AUTH_NONE	0		/* No authority level */
#define AUTH_BASIC	100		/* Non-destructive commands only */
#define AUTH_SUPERVISOR	500		/* System commands */
#define AUTH_SERVICE	1000		/* Configuration commands */


/* Persistent user information */
typedef struct userinfo {
	uint32_t Flags;			/* Control flags */
	int32_t  NVRAMID;		/* NVRAM ID of this entry */
	int32_t	 Options;		/* Option flags */
	int32_t	 Authority;		/* Authority level */
	int32_t	 reserved[4];		/* reserved for future enhancements */
	char	 DfltDest[DEST_STRLEN];	/* Default destination */
} userinfo_t;

#define UIF_VALID	0x00000001	/* userinfo_t is valid */
#define UIF_DESTVALID	0x00000002	/* default destination valid */

extern const userinfo_t userDefaultUserInfo;


/* Password information */
#define MAX_PASSWORD_LEN 16	/* Max length of a password string */
typedef struct passinfo {
	char	Supervisor[MAX_PASSWORD_LEN];
	char	Service[MAX_PASSWORD_LEN];
	char	reserved[MAX_PASSWORD_LEN * 3];
} passinfo_t;

extern passinfo_t userPasswords;


/* Function prototypes */
user_t *userAcquireRemote(user_t *);
STATUS userChangeInputMode(user_t *, int, int);
void   userExtractUserInfo(const user_t *, userinfo_t *);
STATUS userInit(void);
void   userInsertUserInfo(user_t *, const userinfo_t *);
void   userReleaseRemote(user_t *);
STATUS userSendCommand(user_t *, const char *, char *);
STATUS userUpdatePasswords(void);
STATUS userUpdateSaveInfo(user_t *);

#ifndef PRODUCTION
STATUS userList(int);
STATUS userShow(user_t *, int, char *);
#endif  /* !PRODUCTION */

#endif  /* _USER_H_ */
