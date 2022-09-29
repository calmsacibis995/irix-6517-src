#ifndef CRON_PERMIT_H
#define CRON_PERMIT_H

#ident "$Revision: 1.1 $"

extern int per_errno;

char *getuser(uid_t);
int   allowed(const char *, const char *, const char *);

#endif	/* CRON_PERMIT_H */
