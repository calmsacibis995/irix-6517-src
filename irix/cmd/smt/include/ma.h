#ifndef __MA_H
#define __MA_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.11 $
 */


extern int cached_soc;

extern int sm_multi(char*, LFDDI_ADDR *, int, int);
extern int sm_open(char *, int, int *, fd_set *);
extern void sm_close(int *, fd_set *);
extern int sm_getconf(char *, int, struct smt_conf *);
extern int sm_setconf(char *, int, struct smt_conf *);
extern int sm_stat(char *, int, struct smt_st *);
extern int sm_trace(char *, int);
extern int sm_maint(char *, int, enum pcm_ls);
extern int sm_arm(char *, int, u_long *, int);
extern int sm_lem_fail(char *, int, int);
extern void sm_d_bec(char *, void*, int, int);
extern int sm_reset(char *, int);
extern void sm_set_macaddr(char *, LFDDI_ADDR*);

#endif /* __MA_H */
