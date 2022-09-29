#ifndef CRON_FUNCS_H
#define CRON_FUNCS_H

#ident "$Revision: 1.1 $"

int    days_btwn(int, int, int, int, int, int);
int    leap(int);
int    days_in_mon(int, int);
void  *xmalloc(size_t);
int    set_cloexec(int);

#endif	/* CRON_FUNCS_H */
