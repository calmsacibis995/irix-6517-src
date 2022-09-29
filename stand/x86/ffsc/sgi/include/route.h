/*
 * route.h
 *	Prototypes and declarations for the router task
 */
#ifndef _ROUTE_H_
#define _ROUTE_H_

struct dest;
struct msg;
struct user;


/* Global variables */
extern int   routeCheckBaseIO;		/* !=0: check if IO6 needs watching */ 
extern int   routeRebootFFSC;		/* !=0: reboot the FFSC */
extern int   routeRebuildResponses;	/* !=0: rebuild Responses array */
extern int   routeReEvalFDs;		/* !=0: re-evaluate select'ed FDs */
extern struct msg *Responses[MAX_RACKS + MAX_BAYS];	/* All responses */
extern int   outConns;       /* A number of connect tasks still
					   running */
extern SEM_ID outConnsSem;   /* A lock for proper updating of outConns */
extern SEM_ID outConnsDoneSem;  /* A sempaphore which is released when there
				are no more outstanding connect processes */
/* Function prototypes */
int  router(void);
void routeDoAdminCommand(char *);
void routeProcessELSCCommand(const char *, struct user *, struct dest *);


#ifndef PRODUCTION
STATUS routerResponses(int);
STATUS routerStat(void);
#endif /* !PRODUCTION */

#endif  /* _ROUTE_H_ */
