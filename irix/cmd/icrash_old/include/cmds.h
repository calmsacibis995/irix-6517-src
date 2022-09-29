#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/cmds.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

/*
 * Base definitions for TRUE, FALSE, and MAYBE.
 */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef MAYBE
#define MAYBE 2
#endif

/* Function declarations that must be in cmds.c ... This includes _cmd(),
 * _parse(), _help(), and _usage().  The 'int' and 'void' functions
 * are grouped together.  Note that for each command you add to the cmdset[]
 * declaration below, you must also add the an external declaration in this
 * list.
 */
int addtype_cmd(), addtype_parse(), avlnode_cmd(), avlnode_parse(); 
int base_cmd(), base_parse(); 
int cmdlist_cmd(), cmdlist_parse(), ctrace_cmd(), ctrace_parse();
int config_cmd(), config_parse();
int curkthread_cmd(), curkthread_parse(), debug_cmd(); 
int debug_parse(), defkthread_cmd(), defkthread_parse();
int die_cmd(), die_parse();
int dirmem_cmd(), dirmem_parse(), dis_cmd(), dis_parse();
int dump_cmd(), dump_parse(), eframe_cmd(), eframe_parse();
int etrace_cmd(), etrace_parse(), file_cmd(), file_parse();
int findsym_cmd(), findsym_parse(), from_cmd(), from_parse();
int fstype_cmd(), fstype_parse(), func_cmd(), func_parse();
int help_cmd(), help_parse(), history_cmd(), history_parse();
int hubreg_cmd(), hubreg_parse(), hwpath_cmd(), hwpath_parse();
int icrashdef_cmd(), icrashdef_parse();
int inode_cmd(), inode_parse(), inpcb_cmd(), inpcb_parse();
#ifdef ANON_ITHREADS
int ithread_cmd(), ithread_parse();
#endif
int kthread_cmd(), kthread_parse(), ktrace_cmd(), ktrace_parse();
int lastproc_cmd(), lastproc_parse(), mbstat_cmd(), mbstat_parse(); 
int mbuf_cmd(), mbuf_parse(),memory_cmd(), memory_parse();
int mlinfo_cmd(), mlinfo_parse(), mrlock_cmd(), mrlock_parse();
int mntinfo_cmd(), mntinfo_parse();
int namelist_cmd(), namelist_parse();
int nodepda_cmd(), nodepda_parse();
int outfile_cmd(), outfile_parse(), pager_cmd(), pager_parse();
int pda_cmd(), pda_parse(), printd_cmd(), printd_parse();
int pde_cmd(), pde_parse(), pfdat_cmd(), pfdat_parse();
int pfdathash_cmd(), pfdathash_parse(), pfind_cmd(), pfind_parse();
int pid_cmd(), pid_parse(), pregion_cmd(), pregion_parse(); 
int printx_cmd(), printx_parse();
int print_cmd(), print_parse(), proc_cmd(), proc_parse();
int ptov_cmd(), ptov_parse(), printo_cmd(), printo_parse();
int queue_cmd(), queue_parse(), quit_cmd(), quit_parse();
int region_cmd(), region_parse(), report_cmd(), report_parse();
int rnode_cmd(), rnode_parse(), runq_cmd(), runq_parse();
int sbe_cmd(), sbe_parse(), search_cmd(), search_parse();
int sema_cmd(), sema_parse(), set_cmd(), set_parse();
int shell_cmd(), shell_parse();
int sizeof_cmd(), sizeof_parse(), slpproc_cmd(), slpproc_parse();
int lsnode_cmd(), lsnode_parse(), socket_cmd(), socket_parse();
int socket_cmd(), socket_parse(), stack_cmd(), stack_parse();
int stat_cmd(), stat_parse(), strace_cmd(), strace_parse();
int sthread_cmd(), sthread_parse();
int stream_cmd(), stream_parse(), string_cmd(), string_parse();
int strstat_cmd(), strstat_parse(), struct_cmd(), struct_parse();
int swap_cmd(), swap_parse(), symbol_cmd(), symbol_parse();
int tcpcb_cmd(), tcpcb_parse(), tlb_cmd(), tlb_parse();
int tlb_cmd(), tlb_parse(), trace_cmd(), trace_parse();
int type_cmd(), type_parse(), uipcvn_cmd(), uipcvn_parse();
int unpcb_cmd(), unpcb_parse(), unset_cmd(), unset_parse();
int user_cmd(), user_parse(), vertex_cmd(), vertex_parse();
int vfs_cmd(), vfs_parse(), vnode_cmd(), vnode_parse();
int vproc_cmd(), vproc_parse();
int vtop_cmd(), vtop_parse(), walk_cmd(), walk_parse();
int whatis_cmd(), whatis_parse();
int zone_cmd(), zone_parse(), hinv_cmd(), hinv_parse();
void addtype_help(), addtype_usage(), avlnode_help(), avlnode_usage();
void base_help(), base_usage();
void cmdlist_help(), cmdlist_usage(), ctrace_help(); 
void ctrace_usage(), config_help(), config_usage();
void curkthread_help(), curkthread_usage();
void debug_help(), debug_usage(), defkthread_help(), defkthread_usage();
void die_help(), die_usage();
void dirmem_help(), dirmem_usage(), dis_help(), dis_usage();
void dump_help(), dump_usage(), eframe_help(), eframe_usage();
void etrace_help(), etrace_usage(), file_help(), file_usage();
void findsym_help(), findsym_usage(), from_help(), from_usage();
void fstype_help(), fstype_usage(), func_help(), func_usage();
void help_help(), help_usage(), history_help(), history_usage();
void hubreg_help(), hubreg_usage(), hwpath_help(), hwpath_usage();
void icrashdef_help(), icrashdef_usage();
void inode_help(), inode_usage(), inpcb_help(), inpcb_usage();
#ifdef ANON_ITHREADS
void ithread_help(), ithread_usage();
#endif
void kthread_help(), kthread_usage(), ktrace_help(), ktrace_usage();
void lastproc_help(), lastproc_usage();
void mbstat_help(), mbstat_usage(), mbuf_help(), mbuf_usage();
void mlinfo_help(), mlinfo_usage(), mrlock_help(), mrlock_usage();
void mntinfo_help(), mntinfo_usage();
void namelist_help(), namelist_usage();
void memory_help(), memory_usage(), nodepda_help(), nodepda_usage();
void outfile_help(), outfile_usage(), pager_help(), pager_usage();
void pda_help(), pda_usage(), printd_help(), printd_usage();
void pde_help(), pde_usage(), pfdat_help(), pfdat_usage();
void pfdathash_help(), pfdathash_usage(), pfind_help(), pfind_usage();
void pid_help(), pid_usage();
void pregion_help(), pregion_usage(), printx_help(), printx_usage();
void print_help(), print_usage(), proc_help(), proc_usage();
void ptov_help(), ptov_usage(), printo_help(), printo_usage();
void queue_help(), queue_usage(), quit_help(), quit_usage();
void region_help(), region_usage(), report_help(), report_usage();
void rnode_help(), rnode_usage(), runq_help(), runq_usage();
void sbe_help(), sbe_usage(), search_help(), search_usage();
void sema_help(), sema_usage(), set_help(), set_usage();
void shell_help(), shell_usage();
void sizeof_help(), sizeof_usage(), slpproc_help(), slpproc_usage();
void lsnode_help(), lsnode_usage(), socket_help(), socket_usage();
void socket_help(), socket_usage(), stack_help(), stack_usage();
void stat_help(), stat_usage(), strace_help(), strace_usage();
void sthread_help(), sthread_usage();
void stream_help(), stream_usage(), string_help(), string_usage();
void strstat_help(), strstat_usage(), struct_help(), struct_usage();
void swap_help(), swap_usage(), symbol_help(), symbol_usage();
void tcpcb_help(), tcpcb_usage(), tlb_help(), tlb_usage();
void tlb_help(), tlb_usage(), trace_help(), trace_usage();
void type_help(), type_usage(), uipcvn_help(), uipcvn_usage();
void unpcb_help(), unpcb_usage(), unset_help(), unset_usage();
void user_help(), user_usage(), vertex_help(), vertex_usage();
void vfs_help(), vfs_usage(), vnode_help(), vnode_usage();
void vproc_help(), vproc_usage();
void vtop_help(), vtop_usage(), walk_help(), walk_usage();
void whatis_help(), whatis_usage();
void zone_help(), zone_usage(), hinv_help(), hinv_usage();
#ifdef ICRASH_DEBUG
int mempool_cmd(), mempool_parse(), bucket_cmd(), bucket_parse();
int page_cmd(), page_parse(), block_cmd(), block_parse();
void bucket_help(), bucket_usage(), mempool_help(), mempool_usage();
void page_help(), page_usage(), block_help(), block_usage();
#endif /* ICRASH_DEBUG */


/*
 * This structure represents the set of possible commands, including
 * have to be processed internally in the command, but they would have
 * the flags available with each command.  Any additional flags will
 * to do that anyway.
 */
struct _commands {
	char *cmd;                     /* the command typed in initially      */
	char *alias;                   /* the alias this command points to    */
	int   (*cmdfunc)(command_t);   /* the base function                   */
	int   (*cmdparse)(command_t);  /* the string parse function (args!)   */
	void  (*cmdhelp)(command_t);   /* the help function                   */
	void  (*cmdusage)(command_t);  /* the usage string function           */
};
