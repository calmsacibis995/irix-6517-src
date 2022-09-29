/*
 * Copyright(c) 1993 Silicon Graphics Inc.
 */

#include <sys/sysmacros.h>

extern int scsi(void);
extern int gread(daddr_t, char *, int);
extern daddr_t exarea(daddr_t);
extern int gioctl(int, char *);
extern void scsi_readcapacity(int *);
extern void init_ex(void);
extern int sequential_func(void);
extern void adderr(int, daddr_t, char *);
