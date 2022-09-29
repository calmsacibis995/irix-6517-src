/*
 * Copyright 1991, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */
/*
 * queuetests.h -- definitions for queuetests.c and menus.c.
 */

#ifndef	QUEUETESTS_H__
#define	QUEUETESTS_H__

/******************************************************************/


/* Individual Test Functions. */
extern int 
    openr_mac(void), openw_mac(void), access_mac(void), 
    chmod_mac(void), exece_mac(void), bsdgetpgrp_mac(void), 
    bsdsetpgrp_mac(void), kill_mac(void), setpgid_mac(void),
    msgget_mac(void), chown_dac(void), 
    msgctl_mac(void), bsdgetpgrp_dac(void), bsdsetpgrp_dac(void), 
    setpgid_dac(void), kill_dac(void), 
    rdname_dac(void), 
    setreuid_proc(void), setregid_proc(void), 
    setpgrp_proc(void), setsid_proc(void), setgroups_sup(void), 
    bsdsetpgrp_proc(void), bsdsetgroups_sup(void), getlabel_lbl(void),
    setlabel_lbl(void), getplabel_lbl(void), setplabel_lbl(void),
    getlabel_mac(void), setlabel_mac(void), getlabel_dac(void), 
    setplabel_proc(void), mac_dom_mls(void),
    mac_equ_mls(void), socket_lbl(void), socket_ioctl_lbl(void),
    transport_mac(void), agent_mac(void), 
    blockproc_mac(void),
    blockproc_dac(void),
    brk_obr(void), creat_obr(void), open_obr(void),
    mkdir_obr(void), mknod_obr(void), pipe_obr(void),
    symlink_mac(void), symlink_obr(void), msgget_obr(void), fork_obr(void),
    sproc_obr(void), semget_obr(void), shmget_obr(void),
    socket_obr(void), socketpair_obr(void);

/* 
 * Array of ptrs to the test functions. 
 */

struct test_item {
	int	(*func)();	/* Function to call which implements the test */
	char	*name;		/* Name of the test */
	short	queued;		/* Whether the test has already been queued */
	short	flags;		/* Which things this test needs configured */
};

struct test_item tests[] = {
	access_mac, "access_mac", 0, MAC,
	agent_mac, "agent_mac", 0, MAC,
	blockproc_dac, "blockproc_dac", 0, 0,
	blockproc_mac, "blockproc_mac", 0, MAC,
	brk_obr, "brk_obr", 0, 0,
	bsdgetpgrp_dac, "bsdgetpgrp_dac", 0, 0,
	bsdgetpgrp_mac, "bsdgetpgrp_mac", 0, MAC,
	bsdsetgroups_sup, "bsdsetgroups_sup", 0, 0,
	bsdsetpgrp_dac, "bsdsetpgrp_dac", 0, 0,
	bsdsetpgrp_mac, "bsdsetpgrp_mac", 0, MAC,
	bsdsetpgrp_proc, "bsdsetpgrp_proc", 0, 0,
	chmod_mac, "chmod_mac", 0, MAC,
	chown_dac, "chown_dac", 0, 0,
	creat_obr, "creat_obr", 0, 0,
	exece_mac, "exece_mac", 0, MAC,
	fork_obr, "fork_obr", 0, 0,
	getlabel_dac, "getlabel_dac", 0, MAC,
	getlabel_lbl, "getlabel_lbl", 0, MAC,
	getlabel_mac, "getlabel_mac", 0, MAC,
	getplabel_lbl, "getplabel_lbl", 0, MAC,
	kill_dac, "kill_dac", 0, 0,
	kill_mac, "kill_mac", 0, MAC,
	mac_dom_mls, "mac_dom_mls", 0, MAC,
	mac_equ_mls, "mac_equ_mls", 0, MAC,
	mkdir_obr, "mkdir_obr", 0, 0,
	mknod_obr, "mknod_obr", 0, 0,
	msgctl_mac, "msgctl_mac", 0, MAC,
	msgget_mac, "msgget_mac", 0, MAC,
	msgget_obr, "msgget_obr", 0, 0,
	open_obr, "open_obr", 0, 0,
	openr_mac, "openr_mac", 0, MAC,
	openw_mac, "openw_mac", 0, MAC,
	pipe_obr, "pipe_obr", 0, 0,
	rdname_dac, "rdname_dac", 0, 0,
	semget_obr, "semget_obr", 0, 0,
	setgroups_sup, "setgroups_sup", 0, 0,
	setlabel_lbl, "setlabel_lbl", 0, MAC,
	setlabel_mac, "setlabel_mac", 0, MAC,
	setpgid_dac, "setpgid_dac", 0, 0,
	setpgid_mac, "setpgid_mac", 0, 0,
	setpgrp_proc, "setpgrp_proc", 0, 0,
	setplabel_lbl, "setplabel_lbl", 0, MAC,
	setplabel_proc, "setplabel_proc", 0, MAC,
	setregid_proc, "setregid_proc", 0, 0,
	setreuid_proc, "setreuid_proc", 0, 0,
	setsid_proc, "setsid_proc", 0, 0,
	shmget_obr, "shmget_obr", 0, 0,
	socket_ioctl_lbl, "socket_ioctl_lbl", 0, MAC | CIPSO,
	socket_lbl, "socket_lbl", 0, MAC | CIPSO,
	socket_obr, "socket_obr", 0, 0,
	socketpair_obr, "socketpair_obr", 0, 0,
	sproc_obr, "sproc_obr", 0, 0,
	symlink_mac, "symlink_mac", 0, 0,
	symlink_obr, "symlink_obr", 0, 0,
	transport_mac, "transport_mac", 0, MAC,
	0, 0, 0, 0
};

/******************************************************************/


/* 
 * multiqueue table
 *
 *  This table is used by queue_tests().  It lists suites of tests
 *  that can be run.  When a suite is specified, queue_tests() calls
 *  itself with the name of each test listed under the suite.
 */             
struct multiqueue_item {
    char  *name;
    char **tests;
};

/* These arrays contain the tests that their corresponding suite
 *  is supposed to run */
char *all_tests[] = { "mac", "dac", "libmls", "lbl", "obr", "sup", "soc", 0 };
char *mac_tests[] = { "pathname_mac", "ipc_mac", 0 };
char *dac_tests[] = { "ipc_dac", "proc", "pathname_dac",  0 };
char *libmls_tests[] = { "mac_dom_mls", "mac_equ_mls", 0 };
char *lbl_tests[] = { "pn_lbl", "proc_lbl",  0 };
char *pathname_mac_tests[] = {
	"pndr_mac", "pndw_mac", "pnar_mac", "pnaw_mac", "pnex_mac",  0 };
char *ipc_mac_tests[] = {
	"pidr_mac", "pidw_mac", "piar_mac", "piaw_mac", "vipc_mac",  0 };
char *ipc_dac_tests[] = {
	"pidr_dac", "pidw_dac", "piar_dac", "piaw_dac",  0 };
char *pndr_mac_tests[] = { "openr_mac", "symlink_mac",  0 };
char *pndw_mac_tests[] = { "openw_mac",  0 };
char *pnar_mac_tests[] = { "access_mac", "getlabel_mac",  0 };
char *pnaw_mac_tests[] = { "chmod_mac", "setlabel_mac",  0 };
char *pnex_mac_tests[] = { "exece_mac",  0 };
char *piar_mac_tests[] = { "bsdgetpgrp_mac",  0 };
char *piaw_mac_tests[] = {
	"bsdsetpgrp_mac", "kill_mac", "setpgid_mac", "blockproc_mac",  0 };
char *vipc_mac_tests[] = { "msgget_mac", "msgctl_mac",  0 };
char *piar_dac_tests[] = { "bsdgetpgrp_dac", "rdname_dac",  0 };
char *piaw_dac_tests[] = {
	"bsdsetpgrp_dac", "kill_dac", "setpgid_dac", "blockproc_dac",  0 };
char *open_tests[] = { "openr_mac", "openw_mac",  0 };
char *access_tests[] = { "access_mac",  0 };
char *chmod_tests[] = { "chmod_mac",  0 };
char *chown_tests[] = { "chown_dac",  0 };
char *exece_tests[] = { "exece_mac",  0 };
char *bsdgetpgrp_tests[] = { "bsdgetpgrp_mac", "bsdgetpgrp_dac",  0 };
char *bsdsetpgrp_tests[] = {
	"bsdsetpgrp_mac", "bsdsetpgrp_dac", "bsdsetpgrp_proc",  0 };
char *setpgid_tests[] = { "setpgid_dac", "setpgid_mac",  0 };
char *kill_tests[] = { "kill_dac", "kill_mac",  0 };
char *rdname_tests[] = { "rdname_dac",  0 };
char *msgget_tests[] = { "msgget_mac",  0 };
char *msgctl_tests[] = { "msgctl_mac",  0 };
char *proc_tests[] = { "uid", "gid", "sid", "plbl",   0 };
char *uid_tests[] = { "setreuid_proc",  0 };
char *gid_tests[] = { "setregid_proc", "setgroups_sup", "bsdsetgroups_sup", 0 };
char *sid_tests[] = { "setsid_proc", "setpgrp_proc", "bsdsetpgrp_proc",  0 };
char *setreuid_tests[] = { "setreuid_proc",  0 };
char *setregid_tests[] = { "setregid_proc",  0 };
char *setpgrp_tests[] = { "setpgrp_proc",  0 };
char *setsid_tests[] = { "setsid_proc",  0 };
char *setgroups_tests[] = { "setgroups_sup",  0 };
char *bsdsetgroups_tests[] = { "bsdsetgroups_sup",  0 };
char *pn_lbl_tests[] = { "getlabel_lbl", "setlabel_lbl",  0 };
char *proc_lbl_tests[] = { "setplabel_lbl", "getplabel_lbl",  0 };
char *getlabel_tests[] = { "getlabel_lbl", "getlabel_mac", "getlabel_dac", 0 };
char *setlabel_tests[] = { "setlabel_lbl", "setlabel_mac", 0 };
char *getplabel_tests[] = { "getplabel_lbl",  0 };
char *setplabel_tests[] = { "setplabel_lbl", "setplabel_proc",  0 };
char *symlink_tests[] = { "symlink_mac", "symlink_obr",  0 };
char *label_tests[] = { "pn_lbl", "proc_lbl",  0 };
char *pnar_dac_tests[] = { "getlabel_dac",  0 };
char *pathname_dac_tests[] = { "pnar_dac", 0 };
char *plbl_tests[] = { "setplabel_proc",   0 };
char *soc_lbl_tests[] = { "socket_lbl", "socket_ioctl_lbl",  0 };
char *soc_mac_tests[] = { "transport_mac", "agent_mac",  0 };
char *soc_tests[] = { "soc_lbl", "soc_mac",  0 };
char *blockproc_tests[] = { "blockproc_mac", "blockproc_dac",  0 };
char *obr_tests[] = {
	"creat_obr", "open_obr", "mkdir_obr", "symlink_obr", "mknod_obr",
	"pipe_obr",  "msgget_obr", "semget_obr", "shmget_obr", "socket_obr",
	"socketpair_obr",  "brk_obr", "fork_obr", "sproc_obr",  0 };
char *sup_tests[] = { "bsdsetgroups_sup", "setgroups_sup", 0 };

struct multiqueue_item multiqueue[] = {
	"access", access_tests,
	"all", all_tests,
	"blockproc", blockproc_tests,
	"bsdgetpgrp", bsdgetpgrp_tests,
	"bsdsetgroups", bsdsetgroups_tests,
	"bsdsetpgrp", bsdsetpgrp_tests,
	"chmod", chmod_tests,
	"chown", chown_tests,
	"dac", dac_tests,
	"exece", exece_tests,
	"getlabel", getlabel_tests,
	"getplabel", getplabel_tests,
	"gid", gid_tests,
	"ipc_dac", ipc_dac_tests,
	"ipc_mac", ipc_mac_tests,
	"kill", kill_tests,
	"label", label_tests,
	"lbl", lbl_tests,
	"libmls", libmls_tests,
	"mac", mac_tests,
	"msgctl", msgctl_tests,
	"msgget", msgget_tests,
	"obr", obr_tests,
	"open", open_tests,
	"pathname_dac", pathname_dac_tests,
	"pathname_mac", pathname_mac_tests,
	"piar_dac", piar_dac_tests,
	"piaw_dac", piaw_dac_tests,
	"piaw_mac", piaw_mac_tests,
	"plbl", plbl_tests,
	"pn_lbl", pn_lbl_tests,
	"pnar_dac", pnar_dac_tests,
	"pnar_mac", pnar_mac_tests,
	"pnaw_mac", pnaw_mac_tests,
	"pndr_mac", pndr_mac_tests,
	"pndw_mac", pndw_mac_tests,
	"pnex_mac", pnex_mac_tests,
	"proc", proc_tests,
	"proc_lbl", proc_lbl_tests,
	"rdname", rdname_tests,
	"setgroups", setgroups_tests,
	"setlabel", setlabel_tests,
	"setpgid", setpgid_tests,
	"setpgrp", setpgrp_tests,
	"setplabel", setplabel_tests,
	"setregid", setregid_tests,
	"setreuid", setreuid_tests,
	"setsid", setsid_tests,
	"sid", sid_tests,
	"soc", soc_tests,
	"soc_lbl", soc_lbl_tests,
	"soc_mac", soc_mac_tests,
	"sup", sup_tests,
	"symlink", symlink_tests,
	"uid", uid_tests,
	"vipc_mac", vipc_mac_tests,
	0, 0
};

#endif /* ifndef queuetest.h */
