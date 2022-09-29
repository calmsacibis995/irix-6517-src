#ifndef CRON_ELM_H
#define CRON_ELM_H

#ident "$Revision: 1.1 $"

void  el_init(int, time_t, time_t, int);
void *el_first(void);
void  el_add(void *, time_t, int);
void  el_remove(int, int);
void  el_delete(void);
int   el_empty(void);

#endif	/* CRON_ELM_H */
