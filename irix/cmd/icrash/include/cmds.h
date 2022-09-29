#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/include/RCS/cmds.h,v 1.23 1999/05/25 19:21:38 tjm Exp $"

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
int addtype_cmd(), addtype_parse(); 
void addtype_help(), addtype_usage(); 

int anon_cmd(), anon_parse();
void anon_help(), anon_usage();

int anontree_cmd(), anontree_parse();
void anontree_help(), anontree_usage();

int avlnode_cmd(), avlnode_parse(); 
void avlnode_help(), avlnode_usage(); 

int base_cmd(), base_parse(); 
void base_help(), base_usage(); 

int cmdlist_cmd(), cmdlist_parse(); 
void cmdlist_help(), cmdlist_usage(); 

int config_cmd(), config_parse();
void config_help(), config_usage();

int ctrace_cmd(), ctrace_parse();
void ctrace_help(), ctrace_usage();

int curkthread_cmd(), curkthread_parse(); 
void curkthread_help(), curkthread_usage();

int debug_cmd(), debug_parse(); 
void debug_help(), debug_usage();

int defkthread_cmd(), defkthread_parse();
void defkthread_help(), defkthread_usage();

int die_cmd(), die_parse(); 
void die_help(), die_usage();

int dirmem_cmd(), dirmem_parse(); 
void dirmem_help(), dirmem_usage();

int dis_cmd(), dis_parse();
void dis_help(), dis_usage();

int dump_cmd(), dump_parse(); 
void dump_help(), dump_usage(); 

int eframe_cmd(), eframe_parse();
void eframe_help(), eframe_usage();

int etrace_cmd(), etrace_parse(); 
void etrace_help(), etrace_usage(); 

int file_cmd(), file_parse();
void file_help(), file_usage();

int findsym_cmd(), findsym_parse(); 
void findsym_help(), findsym_usage(); 

int from_cmd(), from_parse();
void from_help(), from_usage();

int fru_cmd(), fru_parse();
void fru_help(), fru_usage();

int fstype_cmd(), fstype_parse(); 
void fstype_help(), fstype_usage();

int func_cmd(), func_parse();
void func_help(), func_usage();

int gfxinfo_cmd(), gfxinfo_parse();
void gfxinfo_help(), gfxinfo_usage();

void help_help(), help_usage();
int help_cmd(), help_parse(); 

int hinv_cmd(), hinv_parse();
void hinv_help(), hinv_usage();

int history_cmd(), history_parse();
void history_help(), history_usage();

int hubreg_cmd(), hubreg_parse(); 
void hubreg_help(), hubreg_usage(); 

int hwpath_cmd(), hwpath_parse(); 
void hwpath_help(), hwpath_usage(); 

int icrashdef_cmd(), icrashdef_parse();
void icrashdef_help(), icrashdef_usage();

int inode_cmd(), inode_parse(); 
void inode_help(), inode_usage();

int inpcb_cmd(), inpcb_parse();
void inpcb_help(), inpcb_usage();

int kthread_cmd(), kthread_parse();
void kthread_help(), kthread_usage();

int ktrace_cmd(), ktrace_parse();
void ktrace_help(), ktrace_usage();

int lastproc_cmd(), lastproc_parse(); 
void lastproc_help(), lastproc_usage();

int lsnode_cmd(), lsnode_parse();
void lsnode_help(), lsnode_usage(); 

int mbstat_cmd(), mbstat_parse(); 
void mbstat_help(), mbstat_usage();

int mbuf_cmd(), mbuf_parse();
void mbuf_help(), mbuf_usage();

int memory_cmd(), memory_parse();
void memory_help(), memory_usage(); 

int mlinfo_cmd(), mlinfo_parse(); 
void mlinfo_help(), mlinfo_usage();

int mntinfo_cmd(), mntinfo_parse();
void mntinfo_usage(), mntinfo_help();

int mrlock_cmd(), mrlock_parse();
void mrlock_help(), mrlock_usage();

int namelist_cmd(), namelist_parse();
void namelist_help(), namelist_usage();

int nodepda_cmd(), nodepda_parse();
void nodepda_help(), nodepda_usage();

int outfile_cmd(), outfile_parse(); 
void outfile_help(), outfile_usage(); 

int pager_cmd(), pager_parse();
void pager_help(), pager_usage();

int pda_cmd(), pda_parse(); 
void pda_help(), pda_usage(); 

int printd_cmd(), printd_parse();
void printd_help(), printd_usage();

int pde_cmd(), pde_parse(); 
void pde_help(), pde_usage(); 

int pfdat_cmd(), pfdat_parse();
void pfdat_help(), pfdat_usage();

int pfdathash_cmd(), pfdathash_parse(); 
void pfdathash_help(), pfdathash_usage(); 

int pfind_cmd(), pfind_parse();
void pfind_help(), pfind_usage();

int pid_cmd(), pid_parse(); 
void pid_help(), pid_usage();

int pregion_cmd(), pregion_parse(); 
void pregion_help(), pregion_usage();

int printx_cmd(), printx_parse();
void printx_help(), printx_usage();

int print_cmd(), print_parse(); 
void print_help(), print_usage(); 

int proc_cmd(), proc_parse();
void proc_help(), proc_usage();

int ptov_cmd(), ptov_parse();
void ptov_help(), ptov_usage();

int printo_cmd(), printo_parse();
void printo_help(), printo_usage();

int queue_cmd(), queue_parse(); 
void queue_help(), queue_usage(); 

int quit_cmd(), quit_parse();
void quit_help(), quit_usage();

int region_cmd(), region_parse(); 
void region_help(), region_usage();

int report_cmd(), report_parse();
void report_help(), report_usage();

int rnode_cmd(), rnode_parse();
void rnode_help(), rnode_usage();

int runq_cmd(), runq_parse();
void runq_help(), runq_usage();

int sbe_cmd(), sbe_parse(); 
void sbe_help(), sbe_usage(); 

int search_cmd(), search_parse();
void search_help(), search_usage();

int sema_cmd(), sema_parse(); 
void sema_help(), sema_usage(); 

int set_cmd(), set_parse();
void set_help(), set_usage();

int shell_cmd(), shell_parse();
void shell_help(), shell_usage();

int sizeof_cmd(), sizeof_parse(); 
void sizeof_help(), sizeof_usage(); 

int slpproc_cmd(), slpproc_parse();
void slpproc_help(), slpproc_usage();

int socket_cmd(), socket_parse();
void socket_help(), socket_usage();

int stack_cmd(), stack_parse();
void stack_help(), stack_usage();

int stat_cmd(), stat_parse(); 
void stat_help(), stat_usage();

int strace_cmd(), strace_parse();
void strace_help(), strace_usage();

int sthread_cmd(), sthread_parse();
void sthread_help(), sthread_usage();

int stream_cmd(), stream_parse(); 
void stream_help(), stream_usage(); 

int string_cmd(), string_parse();
void string_help(), string_usage();

int strstat_cmd(), strstat_parse(); 
void strstat_help(), strstat_usage(); 

int struct_cmd(), struct_parse();
void struct_help(), struct_usage();

int swap_cmd(), swap_parse();
void swap_help(), swap_usage(); 

int symbol_cmd(), symbol_parse();
void symbol_help(), symbol_usage();

int tcpcb_cmd(), tcpcb_parse();
void tcpcb_help(), tcpcb_usage(); 

int tlb_cmd(), tlb_parse();
void tlb_help(), tlb_usage(); 

int trace_cmd(), trace_parse();
void trace_help(), trace_usage();

int type_cmd(), type_parse();
void type_help(), type_usage(); 

int uipcvn_cmd(), uipcvn_parse();
void uipcvn_help(), uipcvn_usage();

int unpcb_cmd(), unpcb_parse();
void unpcb_help(), unpcb_usage(); 

int unset_cmd(), unset_parse();
void unset_help(), unset_usage();

int user_cmd(), user_parse();
void user_help(), user_usage(); 

int uthread_cmd(), uthread_parse();
void uthread_help(), uthread_usage(); 

int utrace_cmd(), utrace_parse();
void utrace_help(), utrace_usage();

int vertex_cmd(), vertex_parse();
void vertex_help(), vertex_usage(); 

int vfs_cmd(), vfs_parse();
void vfs_help(), vfs_usage(); 

int vnode_cmd(), vnode_parse();
void vnode_help(), vnode_usage();

int vproc_cmd(), vproc_parse();
void vproc_help(), vproc_usage();

int vsocket_cmd(), vsocket_parse();
void vsocket_help(), vsocket_usage();

int vtop_cmd(), vtop_parse();
void vtop_help(), vtop_usage(); 

int walk_cmd(), walk_parse();
void walk_help(), walk_usage();

int whatis_cmd(), whatis_parse();
void whatis_help(), whatis_usage(); 

int xthread_cmd(), xthread_parse();
void xthread_help(), xthread_usage();

int zone_cmd(), zone_parse();
void zone_help(), zone_usage();

#ifdef ICRASH_DEBUG
int bucket_cmd(), bucket_parse();
void bucket_help(), bucket_usage(); 

int mempool_cmd(), mempool_parse();
void mempool_help(), mempool_usage();

int page_cmd(), page_parse(); 
void page_help(), page_usage(); 

int block_cmd(), block_parse();
void block_help(), block_usage();
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
