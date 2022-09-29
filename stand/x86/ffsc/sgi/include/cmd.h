/*
 * cmd.h
 *	Local FFSC command processing prototypes and declarations
 */

#ifndef _CMD_H_
#define _CMD_H_

struct dest;
struct tokeninfo;
struct user;


/* Command information */
typedef struct cmdinfo {
	char	*Name;
	int	(*Function)(char **, struct user *, struct dest *);
	char	*Help;
} cmdinfo_t;

extern cmdinfo_t cmdList[];


/* Public function prototypes */
int  cmdProcessFFSCCommand(const char *, struct user *, struct dest *);
void cmdChangeCtrlChar(char **, struct user *, const char *);
void cmdChangeSimpleMode(char *, struct tokeninfo *, const char *);
int  cmdParseCharSpecification(char **);


/* Return codes from cmdProcessFFSCCommand */
#define PFC_DONE	0	/* No further processing required */
#define PFC_FORWARD	1	/* Forward command to remote racks */
#define PFC_NOTFFSC	2	/* Not an FFSC command */
#define PFC_WAITRESP	3	/* Forwarding done, wait for responses */

#endif  /* _CMD_H_ */
