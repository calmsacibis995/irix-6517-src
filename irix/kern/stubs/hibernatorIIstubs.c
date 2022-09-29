/*	Copyright (c) 1995 Softway Pty Ltd	*/
#ident	"$Revision: 1.3 $"

const int HibernatorIsInstalled = 0;

void	hib_exec_remove_proc() {}
void	hib_exit_freeproc() {}
void	hib_fork_getproc()  {}
void	hib_init() {}
void	hib_pgrp_mark_process() {}
void	hib_pgrp_unmark_process() {}
void	hib_acct_acctbuf() {}
int	hib_procfs_propen() {return 0;}
int	hib_procfs_check_ioctl() {return 0;}
int	hib_procfs_zdisp() {return 0;}
int	hib_procfs_do_ioctl() {return 0;}
int	hib_procfs_isprwrioctl() {return 0;}
int	hib_fcntl() {return 0;}
