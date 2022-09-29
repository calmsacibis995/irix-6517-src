#ifndef __SEH_SGM_H_
#define __SEH_SGM_H_

typedef struct system_table_s {
  int  sys_id;			/* unique id for subscribed system */
  char *hostname;		/* hostname */
  int  serial_num;		/* serial number of host */
  int  active;			/* is this record active ? */
  int local;			/* is this the machine we are running on ? */
} system_table_t;

#endif /* __SEH_SGM_H_ */
