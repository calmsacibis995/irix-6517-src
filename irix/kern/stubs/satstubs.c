
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */

struct ipc_perm *
sat_svipc_save()
{
	DOPANIC("sat_svipc_save stub"); /* NOTREACHED */
}
void	sat_init() {}
void	sat_lookup() { DOPANIC("sat_lookup stub"); }
void	sat_access() { DOPANIC("sat_access stub"); }
void	sat_access2() { DOPANIC("sat_access2 stub"); }
void	sat_acct() { DOPANIC("sat_acct stub"); }
void	sat_bsdipc_addr() { DOPANIC("sat_bsdipc_addr stub"); }
void	sat_bsdipc_create() { DOPANIC("sat_bsdipc_create stub"); }
void	sat_bsdipc_create_pair(){ DOPANIC("sat_bsdipc_create_pair stub"); }
void	sat_bsdipc_if_config() { DOPANIC("sat_bsdipc_if_config stub"); }
void	sat_bsdipc_missing() { DOPANIC("sat_bsdipc_missing stub"); }
void	sat_bsdipc_range() { DOPANIC("sat_bsdipc_range stub"); }
void	sat_bsdipc_resvport() { DOPANIC("sat_bsdipc_resvport stub"); }
void	sat_bsdipc_shutdown() { DOPANIC("sat_bsdipc_shutdown stub"); }
void	sat_bsdipc_snoop() { DOPANIC("sat_bsdipc_snoop stub"); }
void	sat_chmod() { DOPANIC("sat_chmod stub"); }
void	sat_chown() { DOPANIC("sat_chown stub"); }
void	sat_chrwdir() { DOPANIC("sat_chrwdir stub"); }
void	sat_clock() { DOPANIC("sat_clock stub"); }
void	sat_close() { DOPANIC("sat_close stub"); }
void	sat_ctl() { DOPANIC("sat_ctl stub"); }
void	sat_domainname_set() { DOPANIC("sat_domainname_set stub"); }
void	sat_dup() { DOPANIC("sat_dup stub"); }
void	sat_exec() { DOPANIC("sat_exec stub"); }
void	sat_exit() { DOPANIC("sat_exit stub"); }
void	sat_fchdir() { DOPANIC("sat_fchdir stub"); }
void	sat_fchown() { DOPANIC("sat_fchown stub"); }
void	sat_fd_read() { DOPANIC("sat_fd_read stub"); }
void	sat_pfd_read2() { DOPANIC("sat_pfd_read2 stub"); }
void	sat_fd_rdwr() { DOPANIC("sat_fd_rdwr stub"); }
void	sat_fork() { DOPANIC("sat_fork stub"); }
void	sat_hostid_set() { DOPANIC("sat_hostid_set stub"); }
void	sat_hostname_set() { DOPANIC("sat_hostname_set stub"); }
void	sat_open() { DOPANIC("sat_open stub"); }
void	sat_pipe() { DOPANIC("sat_pipe stub"); }
void	sat_proc_access() { DOPANIC("sat_proc_access stub"); }
void	sat_proc_acct() { DOPANIC("sat_proc_acct stub"); }
void	sat_read() { DOPANIC("sat_read stub"); }
void	sat_session_acct() { DOPANIC("sat_session_acct stub"); }
void	sat_setcap() { DOPANIC("sat_setcap stub"); }
void	sat_setgroups() { DOPANIC("sat_setgroups stub"); }
void	sat_setlabel() { DOPANIC("sat_setlabel stub"); }
void	sat_setpcap() { DOPANIC("sat_setpcap stub"); }
void	sat_setplabel() { DOPANIC("sat_setplabel stub"); }
void	sat_setregid() { DOPANIC("sat_setregid stub"); }
void	sat_setreuid() { DOPANIC("sat_setreuid stub"); }
void	sat_svipc_access() { DOPANIC("sat_svipc_access stub"); }
void	sat_svipc_create() { DOPANIC("sat_svipc_create stub"); }
void	sat_svipc_ctl() { DOPANIC("sat_svipc_ctl stub"); }
void	sat_svipc_remove() { DOPANIC("sat_svipc_remove stub"); }
void	sat_tty_setlabel() { DOPANIC("sat_tty_setlabel stub"); }
void	sat_utime() { DOPANIC("sat_utime stub"); }
void	sat_write() { DOPANIC("sat_write stub"); }
void	sat_svr4net_addr() { DOPANIC("sat_svr4net_addr stub"); }
void	sat_svr4net_create() { DOPANIC("sat_svr4net_create stub"); }
void	sat_svr4net_shutdown() { DOPANIC("sat_svr4net_shutdown stub"); }
void	sat_init_syscall() { DOPANIC("sat_init_syscall stub"); }
void	sat_set_subsysnum() { DOPANIC("sat_set_subsysnum stub"); }
void	sat_set_suflag() { DOPANIC("sat_set_suflag stub"); }
void	sat_set_comm() { DOPANIC("sat_set_comm stub"); }
void	sat_set_openfd() { DOPANIC("sat_set_openfd stub"); }
void	sat_check_flags() { DOPANIC("sat_check_flags stub"); }
void	sat_proc_init() { DOPANIC("sat_proc_init stub"); }
void	sat_proc_exit() { DOPANIC("sat_proc_exit stub"); }
void	sat_ptrace() { DOPANIC("sat_ptrace stub"); }
void	sat_update_rwdir() { DOPANIC("sat_update_rwdir"); }
void	sat_fchmod() { DOPANIC("sat_fchmod stub"); }
void	sat_set_cap() { DOPANIC("sat_set_cap stub"); }

void	sat_pn_book() { DOPANIC("sat_pn_book"); }
void	sat_pn_save() { DOPANIC("sat_pn_save"); }
void	sat_pn_start() { DOPANIC("sat_pn_start"); }
void	sat_pn_append() { DOPANIC("sat_pn_append"); }
void	sat_pn_finalize() { DOPANIC("sat_pn_finalize"); }
void	sat_save_attr() { DOPANIC("sat_save_attr"); }
void	sat_setacl() { DOPANIC("sat_setacl"); }
void	sat_sys_note() { DOPANIC("sat_sys_note"); }
