#ifndef __SMTD_SVC_H
#define __SMTD_SVC_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */


extern enum fddi_pc_type getpctype(SMT_MAC *, int, int);
extern void reset_rids();
extern SMT_PATH *getpathbyrid(int);
extern SMT_PHY *getphybyrid(int);
extern SMT_MAC *getmacbyrid(int);
extern SMT_MAC *getmacbyname(char *, int);
extern void smtd_timeout();
extern void smtd_indicate(SMT_MAC *);
extern void smtd_resp_map(int, int, char *, int);
extern int unq_fs(SMT_FS_INFO *);
extern int enq_fs(SMT_FS_INFO *);
extern void fsq_clearlog();
extern void fsq_log(u_char, u_char, int);
extern void fsq_stat(SMT_FSSTAT *);
extern void fsq_del(SMT_FSSTAT *);
#endif /* __SMTD_SVC_H */
