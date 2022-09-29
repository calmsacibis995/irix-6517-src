#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/extern.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

/* Function prototypes and external variable declarations 
 * from the various icrash modules
 */

/* cmds/cmd_avlnode.c
 */
int next_avlnode(kaddr_t, k_ptr_t, int, FILE *, int *);
int walk_avltree(kaddr_t, int, FILE *);
int avlnode_cmd(command_t);
void avlnode_usage(command_t);
void avlnode_help(command_t);
int avlnode_parse(command_t);

/* cmds/cmd_base.c -- TODO 
 */

/* cmds/cmd_block.c -- included in extern/alloc.h
 */

/* cmds/cmd_bucket.c -- included in extern/alloc.h
 */

/* cmds/cmd_ctrace.c
 */
void ctrace_banner(FILE *, int);
int ctrace_cmd(command_t);
int ctrace_parse(command_t);
void ctrace_help(command_t);
void ctrace_usage(command_t);

/* cmds/cmd_curkthread.c
 */
int curkthread_cmd(command_t);
void curkthread_usage(command_t);
void curkthread_help(command_t);
int curkthread_parse(command_t);

/* cmds/cmd_dblock.c -- TODO
 */

/* cmds/cmd_debug.c
 */
void debug_banner(FILE *, int);
void list_debug_classes(FILE *);
void print_debug_value(k_uint_t, FILE *);
int debug_cmd(command_t);
void debug_usage(command_t);
void debug_help(command_t);
int debug_parse(command_t);

/* cmds/cmd_die.c -- included in extern/dwarflib.h
 */

/* cmds/cmd_dis.c
 */
int dis_cmd(command_t);
void dis_usage(command_t);
void dis_help(command_t);
int dis_parse(command_t);

/* cmds/dump.c
 */
int dump_cmd(command_t);
void dump_usage(command_t);
void dump_help(command_t);
int dump_parse(command_t);

/* cmds/cmd_eframe.c
 */
int eframe_cmd(command_t);
void eframe_usage(command_t);
void eframe_help(command_t);
int eframe_parse(command_t);

/* cmds/cmd_etrace.c
 */
int etrace_cmd(command_t);
void etrace_usage(command_t);
void etrace_help(command_t);
int etrace_parse(command_t);

/* cmds/cmd_file.c
 */
int file_cmd(command_t);
void file_usage(command_t);
void file_help(command_t);
int file_parse(command_t);

/* cmds/cmd_findpde.c -- TODO
 */

/* cmds/cmd_findsym.c
 */
void findsym_banner(FILE *ofp, int flags);
int findsym_cmd(command_t);
void findsym_usage(command_t);
void findsym_help(command_t);
int findsym_parse(command_t);

/* cmds/cmd_from.c
 */
int from_cmd(command_t);
void from_usage(command_t);
void from_help(command_t);
int from_parse(command_t);

/* cmds/cmd_fru.c
 */
int fru_cmd(command_t);
void fru_usage(command_t);
void fru_help(command_t);
int fru_parse(command_t);

/* cmds/cmd_fstype.c
 */
int fstype_cmd(command_t);
void fstype_usage(command_t);
void fstype_help(command_t);
int fstype_parse(command_t);

/* cmds/cmd_func.c
 */
void func_banner(FILE *, int);
int func_cmd(command_t);
void func_usage(command_t);
void func_help(command_t);
int func_parse(command_t);

/* cmds/cmd_help.c
 */
int help_list(command_t);
void help_all_list(command_t);
int help_found(char *);
int help_cmd(command_t);
void help_usage(command_t);
void help_help(command_t);
int help_parse(command_t);

/* cmds/cmd_history.c
 */
int history_cmd(command_t);
void history_usage(command_t);
void history_help(command_t);
int history_parse(command_t);

/* cmds/cmd_inode.c
 */
int inode_cmd(command_t);
void inode_usage(command_t);
void inode_help(command_t);
int inode_parse(command_t);

/* cmds/cmd_icrashdef.c
 */
int icrashdef_cmd(command_t);
void icrashdef_usage(command_t);
void icrashdef_help(command_t);
int icrashdef_parse(command_t);

/* cmds/cmd_inpcb.c
 */
int inpcb_cmd(command_t);
void inpcb_usage(command_t);
void inpcb_help(command_t);
int inpcb_parse(command_t);

/* cmds/cmd_ithread.c
 */
int ithread_cmd(command_t);
void ithread_usage(command_t);
void ithread_help(command_t);
int ithread_parse(command_t);

/* cmds/cmd_kthread.c
 */
int kthread_cmd(command_t);
void kthread_usage(command_t);
void kthread_help(command_t);
int kthread_parse(command_t);

/* cmds/cmd_ktrace.c
 */
int ktrace_cmd(command_t);
void ktrace_usage(command_t);
void ktrace_help(command_t);
int ktrace_parse(command_t);

/* cmds/cmd_mblock.c -- TODO
 */

/* cmds/cmd_mbstat.c
 */
int mbstat_cmd(command_t);
void mbstat_usage(command_t);
void mbstat_help(command_t);
int mbstat_parse(command_t);

/* cmds/cmd_mbuf.c
 */
int mbuf_cmd(command_t);
void mbuf_usage(command_t);
void mbuf_help(command_t);
int mbuf_parse(command_t);

/* cmds/cmd_mntinfo.c
 */
int mntinfo_cmd(command_t);
void mntinfo_usage(command_t);
void mntinfo_help(command_t);
int mntinfo_parse(command_t);

/* cmds/cmd_mempool.c -- included in extern/alloc.h
 */

/* cmds/cmd_nodepda.c
 */
int nodepda_cmd(command_t);
void nodepda_usage(command_t);
void nodepda_help(command_t);
int nodepda_parse(command_t);

/* cmds/cmd_outfile.c
 */
int outfile_cmd(command_t);
void outfile_usage(command_t);
void outfile_help(command_t);
int outfile_parse(command_t);

/* cmds/cmd_page.c -- included in extern/alloc.h
 */

/* cmds/cmd_pager.c
 */
int pager_cmd(command_t);
void pager_usage(command_t);
void pager_help(command_t);
int pager_parse(command_t);

/* cmds/cmd_pd.c
 */
int printd_cmd(command_t);
void printd_usage(command_t);
void printd_help(command_t);
int printd_parse(command_t);

/* cmds/cmd_pda.c
 */
int pda_cmd(command_t);
void pda_usage(command_t);
void pda_help(command_t);
int pda_parse(command_t);

/* cmds/cmd_pde.c
 */
int pde_cmd(command_t);
void pde_usage(command_t);
void pde_help(command_t);
int pde_parse(command_t);

/* cmds/cmd_pfdat.c
 */
int pfdat_cmd(command_t);
void pfdat_usage(command_t);
void pfdat_help(command_t);
int pfdat_parse(command_t);

/* cmds/cmd_pfdathash.c
 */
int tpfdathash_cmd(command_t);
void pfdathash_usage(command_t);
void pfdathash_help(command_t);
int pfdathash_parse(command_t);

/* cmds/cmd_pfind.c
 */
int pfind_cmd(command_t);
void pfind_usage(command_t);
void pfind_help(command_t);
int pfind_parse(command_t);

/* cmds/cmd_pid.c
 */
int pid_cmd(command_t);
void pid_usage(command_t);
void pid_help(command_t);
int pid_parse(command_t);

/* cmds/cmd_pmap.c
 */
int pmap_cmd(command_t);
void pmap_usage(command_t);
void pmap_help(command_t);
int pmap_parse(command_t);

/* cmds/cmd_po.c
 */
int printo_cmd(command_t);
void printo_usage(command_t);
void printo_help(command_t);
int printo_parse(command_t);

/* cmds/cmd_pregion.c
 */
int pregion_cmd(command_t);
void pregion_usage(command_t);
void pregion_help(command_t);
int pregion_parse(command_t);

/* cmds/cmd_print.c
 */
int print_cmd(command_t);
void print_usage(command_t);
void print_help(command_t);
int print_parse(command_t);

/* cmds/cmd_proc.c
 */
int proc_cmd(command_t);
void print_perror(FILE *, int, int);
void proc_usage(command_t);
void proc_help(command_t);
int proc_parse(command_t);

/* cmds/cmd_ptov.c
 */
int ptov_cmd(command_t);
void ptov_usage(command_t);
void ptov_help(command_t);
int ptov_parse(command_t);

/* cmds/cmd_px.c
 */
int printx_cmd(command_t);
void printx_usage(command_t);
void printx_help(command_t);
int printx_parse(command_t);

/* cmds/cmd_queue.c
 */
void queue_banner(FILE *, int);
void print_queue(kaddr_t, k_ptr_t, char *, int, FILE *);
/* TODO
int print_stty_ld();
int print_termio();
*/
int do_queues(kaddr_t, int, FILE *);
int queue_cmd(command_t);
void queue_usage(command_t);
void queue_help(command_t);
int queue_parse(command_t);

/* cmds/cmd_quit.c
 */
int quit_cmd(command_t);
void quit_usage(command_t);
void quit_help(command_t);
int quit_parse(command_t);

/* cmds/cmd_region.c
 */
int region_cmd(command_t);
void region_usage(command_t);
void region_help(command_t);
int region_parse(command_t);

/* cmds/cmd_report.c
 */
int report_cmd(command_t);
void report_usage(command_t);
void report_help(command_t);
int report_parse(command_t);

/* cmds/cmd_rnode.c
 */
int rnode_cmd(command_t);
void rnode_usage(command_t);
void rnode_help(command_t);
int rnode_parse(command_t);

/* cmds/cmd_runq.c
 */
int runq_cmd(command_t);
void runq_usage(command_t);
void runq_help(command_t);
int runq_parse(command_t);

/* cmds/cmd_sbe.c
 */
/*
int mc3_sbe_fru(evcfginfo_t *, int, FILE *);
*/
int sbe_cmd(command_t);
void sbe_usage(command_t);
void sbe_help(command_t);
int sbe_parse(command_t);

/* cmds/cmd_search.c
 */
int search_memory(unsigned char *, unsigned char *, int, kaddr_t, 
	k_uint_t, int, FILE *);
int pattern_match(unsigned char *, unsigned char *, unsigned char *, int);
int search_cmd(command_t);
void search_usage(command_t);
void search_help(command_t);
int search_parse(command_t);

/* cmds/cmd_sema.c
 */
int sema_cmd(command_t);
void sema_usage(command_t);
void sema_help(command_t);
int sema_parse(command_t);

/* cmds/cmd_set.c
 */
int set_cmd(command_t);
void set_usage(command_t);
void set_help(command_t);
int set_parse(command_t);

/* cmds/cmd_shell.c
 */
int shell_cmd(command_t);
void shell_usage(command_t);
void shell_help(command_t);
int shell_parse(command_t);

/* cmds/cmd_sizeof.c
 */
int sizeof_cmd(command_t);
void sizeof_usage(command_t);
void sizeof_help(command_t);
int sizeof_parse(command_t);

/* cmds/cmd_slpproc.c
 */
void slpproc_banner(FILE *, int);
int print_slpproc(kaddr_t, k_ptr_t, int, FILE *);
int slpproc_cmd(command_t);
void slpproc_usage(command_t);
void slpproc_help(command_t);
int slpproc_parse(command_t);

/* cmds/cmd_snode.c
 */
int snode_cmd(command_t);
void snode_usage(command_t);
void snode_help(command_t);
int snode_parse(command_t);

/* cmds/cmd_socket.c
 */
int socket_cmd(command_t);
void socket_usage(command_t);
void socket_help(command_t);
int socket_parse(command_t);

/* cmds/cmd_stack.c
 */
int stack_cmd(command_t);
void stack_usage(command_t);
void stack_help(command_t);
int stack_parse(command_t);

/* cmds/cmd_stat.c
 */
int stat_cmd(command_t);
void stat_usage(command_t);
void stat_help(command_t);
int stat_parse(command_t);

/* cmds/cmd_sthread.c
 */
int sthread_cmd(command_t);
void sthread_usage(command_t);
void sthread_help(command_t);
int sthread_parse(command_t);

/* cmds/cmd_strace.c
 */
int strace_cmd(command_t);
void strace_usage(command_t);
void strace_help(command_t);
int strace_parse(command_t);

/* cmds/cmd_stream.c
 */
void stream_banner(FILE *, int);
/*
int print_stream(kaddr_t, k_ptr_t, stream_rec_t *, int, FILE *);
*/
int list_streams(int, FILE *);
int stream_cmd(command_t);
void stream_usage(command_t);
void stream_help(command_t);
int stream_parse(command_t);

/* cmds/cmd_string.c
 */
int string_cmd(command_t);
void string_usage(command_t);
void string_help(command_t);
int string_parse(command_t);

/* cmds/cmd_strstat.c
 */
int print_strdzone(int, FILE *);
int strstat_cmd(command_t);
void strstat_usage(command_t);
void strstat_help(command_t);
int strstat_parse(command_t);

/* cmds/cmd_struct.c
 */
void struct_banner(FILE *, int);
void print_struct(symdef_t *, int, FILE *);
int struct_cmd(command_t);
void struct_usage(command_t);
void struct_help(command_t);
int struct_parse(command_t);

/* cmds/cmd_swap.c
 */
int swap_cmd(command_t);
void swap_usage(command_t);
void swap_help(command_t);
int swap_parse(command_t);

/* cmds/cmd_symbol.c
 */
void symbol_banner(FILE *, int);
int print_symbol(struct syment *, char *, int, FILE *);
int symbol_cmd(command_t);
void symbol_usage(command_t);
void symbol_help(command_t);
int symbol_parse(command_t);

/* cmds/cmd_tcpcb.c
 */
int tcpcb_cmd(command_t);
void tcpcb_usage(command_t);
void tcpcb_help(command_t);
int tcpcb_parse(command_t);

/* cmds/cmd_tlb.c
 */
void tlb_banner(FILE *, int);
void print_tlb(int, k_ptr_t, FILE *);
int tlb_cmd(command_t);
void tlb_usage(command_t);
void tlb_help(command_t);
int tlb_parse(command_t);

/* cmds/cmd_trace.c
 */
int trace_cmd(command_t);
void trace_usage(command_t);
void trace_help(command_t);
int trace_parse(command_t);

/* cmd/cmd_type.c
 */
int type_cmd(command_t);
void type_usage(command_t);
void type_help(command_t);
int type_parse(command_t);

/* cmds/cmd_uipcvn.c
 */
int uipc_vnsocket(kaddr_t, int, int *, FILE *);
int uipcvn_cmd(command_t);
void uipcvn_usage(command_t);
void uipcvn_help(command_t);
int uipcvn_parse(command_t);

/* cmds/cmd_unpcb.c
 */
int unpcb_cmd(command_t);
void unpcb_usage(command_t);
void unpcb_help(command_t);
int unpcb_parse(command_t);

/* cmds/cmd_unset.c
 */
int unset_cmd(command_t);
void unset_usage(command_t);
void unset_help(command_t);
int unset_parse(command_t);

/* cmds/cmd_uthread.c
 */
int uthread_cmd(command_t);
void uthread_usage(command_t);
void uthread_help(command_t);
int uthread_parse(command_t);

/* cmds/cmd_utrace.c
 */
int utrace_cmd(command_t);
void utrace_usage(command_t);
void utrace_help(command_t);
int utrace_parse(command_t);

/* cmds/cmd_vfs.c
 */
int vfs_cmd(command_t);
void vfs_usage(command_t);
void vfs_help(command_t);
int vfs_parse(command_t);

/* cmds/cmd_vnode.c
 */
int vnode_cmd(command_t);
void vnode_usage(command_t);
void vnode_help(command_t);
int vnode_parse(command_t);

/* cmds/cmd_vproc.c
 */
int vproc_cmd(command_t);
void vproc_usage(command_t);
void vproc_help(command_t);
int vproc_parse(command_t);

/* cmds/cmd_vsocket.c
 */
int vsocket_cmd(command_t);
void vsocket_usage(command_t);
void vsocket_help(command_t);
int vsocket_parse(command_t);

/* cmds/cmd_vtop.c
 */
void print_addr(kaddr_t, int, kaddr_t, kaddr_t, kaddr_t, FILE *);
int vtop_cmd(command_t);
void vtop_usage(command_t);
void vtop_help(command_t);
int vtop_parse(command_t);

/* cmds/cmd_walk.c
 */
int walk_cmd(command_t);
void walk_usage(command_t);
void walk_help(command_t);
int walk_parse(command_t);

/* cmds/cmd_whatis.c
 */
int whatis_cmd(command_t);
void whatis_usage(command_t);
void whatis_help(command_t);
int whatis_parse(command_t);

/* cmds/cmd_sthread.c
 */
int xthread_cmd(command_t);
void xthread_usage(command_t);
void xthread_help(command_t);
int xthread_parse(command_t);

/* cmds/cmd_zone.c
 */
int zone_cmd(command_t);
void zone_usage(command_t);
void zone_help(command_t);
int zone_parse(command_t);

/* cmds/cmd_anon.c
 */
int  anon_cmd(command_t);
void anon_usage(command_t);
void anon_help(command_t);
int  anon_parse(command_t);

/* cmds/cmd_anontree.c
 */
int  anontree_cmd(command_t);
void anontree_usage(command_t);
void anontree_help(command_t);
int  anontree_parse(command_t);

/* cmds/cmds.c -- included in include/cmds.h
 */
extern struct _commands commands;
extern char ofile[];
extern char pfile[];
extern unsigned have_args;
extern unsigned	mask[];
extern struct _commands cmdset[];

int parse_flags(command_t *, int, int);
int parse_cmd(command_t *, int, FILE *);
void get_cmd(command_t *, char *);
char *cat_cmd_args(command_t);
int close_ofp(command_t);
int checkrun_cmd(command_t, FILE *);
char *next_command(char **);
int process_commands(FILE *);
void sig_handler();

/* get_ra.s
 */
caddr_t *get_ra();

/* init.c
 */ 
extern klib_t *K;				/* pointer to libklib control struct         */
extern symtab_t *stp;           /* symbol table pointer                      */
extern int symbol_table;        /* mdebug/dwarf                              */
extern int tree_type;           /* Base source tree for unix kernel          */
extern struct mbufconst mbufconst; /* Sizes of mbuf constants                */

/* XXX - These may need to be deleted (after being moved to libklib)
 */
extern mem_validity_check;      /* Is it ok to check for valid physmem       */
extern int patchnum;            /* Current patch level (if any)              */
extern int nfstype;             /* number of vfs's in vfssw[]                */

extern kaddr_t S_error_dumpbuf; /* HW error message buffer                   */
extern kaddr_t S_ithreadlist;   /* start of ithread list                     */
extern kaddr_t S_lbolt;         /* current clock tick                        */
extern kaddr_t S_mbuf_page_cur; /* Number of cluster pages in use            */
extern kaddr_t S_mlinfolist;    /* loadable module info                      */
extern kaddr_t S_pdaindr;       /* pointer to table of pda_s structs         */
extern kaddr_t S_Nodepdaindr;   /* pointer to table of nodepda_s structs     */
extern kaddr_t S_sthreadlist;   /* start of sthread list                     */
extern kaddr_t S_strst;         /* streams statistics stuff                  */
extern kaddr_t S_strdzone;      /* Streams zone memory                       */
extern kaddr_t S_time;          /* current time                              */
extern kaddr_t S_xthreadlist;   /* start of xthread list                     */
extern kaddr_t _dumpproc;       /* kernel address of panicing proc           */
extern kaddr_t _activefiles;    /* linked list of open files (DEBUG only)    */
extern kaddr_t _hbuf;           /* buf hash table                            */
extern kaddr_t _hwgraph;        /* hwgraph (hw inventory, etc.)              */
extern kaddr_t _kmem_zones;     /* kmem zone list                            */
extern kaddr_t _kptbl;          /* kernel page table                         */
extern kaddr_t _pidactive;      /* pid (process) active list                 */
extern kaddr_t _pidtab;         /* pid_entry table                           */
extern kaddr_t _phash;          /* pfdat hash table                          */
extern kaddr_t _phashlast;      /* last pfdat hash bucket                    */
extern kaddr_t _pfdat;          /* pfdat table                               */
extern kaddr_t _proc;           /* proc table                                */
extern kaddr_t _putbuf;         /* putbuf                                    */
extern kaddr_t _uipc_vnlist;    /* table of active sockets                   */
extern k_ptr_t dproc;           /* dumpproc                                  */
extern k_ptr_t dumpregs;        /* saved registers after core dump           */
extern k_ptr_t hwgraph;         /* the hwgraph                               */
extern k_ptr_t kptblp;          /* kernel page table                         */
extern k_ptr_t strstp;          /* Streams statas struct                     */
extern k_ptr_t tlbdumptbl;      /* tlb dump table                            */
extern k_ptr_t uipc_vnlist;     /* array of free socket vnode lists          */
extern k_ptr_t utsp;            /* utsname struct                            */
extern k_ptr_t _vfssw;          /* vfs switch table                          */

void symbols_init();
void init(FILE *);
void init_global_vars(FILE *);

/* libsym/dwarflib.c
 */

/* libsym/elflib.c
 */

/* libsym/symbol.c
 */
struct syment *get_sym(char *, int);
void free_syment(struct syment *);
kaddr_t get_sym_addr(char *);
kaddr_t get_sym_ptr(char *);

/* libsym/symlib.c
 */
extern int line_numbers_valid;
extern int sym_cnt, func_cnt, addr_cnt;
extern int line_numbers_valid;
extern char namebuf[];
extern symaddr_t *slist;
extern symaddr_t *slist2;
extern symaddr_t *hash_addrs[];
int STRUCT(char *);
int FIELD(char *, char *);
int IS_FIELD(char *, char *);
int FIELD_SZ(char *, char *);
int base_value(k_ptr_t, char *, char *, k_uint_t *);
int get_funcsize(kaddr_t);
char *get_funcname(kaddr_t);
kaddr_t get_funcaddr(kaddr_t);
char *get_srcfile(kaddr_t);
int get_lineno(kaddr_t);
btnode_t *t_make_node(char *);
int t_insert_node(btnode_t *, btnode_t *);
btnode_t *t_find_node(btnode_t *, char *, int *);

/* libutil/alloc.c
 */
void init_mempool();
#ifdef ICRASH_DEBUG
k_ptr_t _alloc_block(int, int, caddr_t *);
#else
k_ptr_t alloc_block(int, int);
#endif
k_ptr_t dup_block(caddr_t *, int);
int free_block(caddr_t *);
void free_temp_blocks();

/* libutil/eval.c
 */

/*
 * libutil/getstrings.c
 */
void get_uname(char *, dump_hdr_t *, int);
void get_panicstr(char *, dump_hdr_t *, int);
void log_putbuf(dump_hdr_t *, int, FILE *);
void expand_header(dump_hdr_t *, FILE *);

/* libutil/proc.c
 */
int proc_files(kaddr_t, int, int, FILE *);

/* libutil/report.c
 */

/* libutil/socket.c
 */
void get_tcpaddr(unsigned, short, char *);

/* libutil/stream.c
 */

/* libutil/struct.c
 */
int struct_index(char *);
int struct_size(int);
void list_structs(FILE *);
int walk_structs(char *, char *, int, kaddr_t, int, FILE *);

void avlnode_banner(FILE *, int);
int print_avlnode(kaddr_t, k_ptr_t, int, FILE *);

void bhv_desc_banner(FILE *, int);
int print_bhv_desc(kaddr_t, k_ptr_t, int, FILE *);

void buf_banner(FILE *, int);
int print_buf(kaddr_t, k_ptr_t, int, FILE *);

void eframe_s_banner(FILE *, int);

k_ptr_t get_vfile(kaddr_t, k_ptr_t, int);
void vfile_banner(FILE *, int);
int print_vfile(kaddr_t, k_ptr_t, int, FILE *);

void graph_vertex_s_banner(FILE *, int);
int print_graph_vertex_s(kaddr_t, k_ptr_t, int, FILE *);

void eframe_s_banner(FILE *, int);
int print_eframe_s(kaddr_t, k_ptr_t, int, FILE *);

k_ptr_t get_inode(kaddr_t, k_ptr_t, int);
void inode_banner(FILE *, int);
int print_inode(kaddr_t, k_ptr_t, int, FILE *);

void inpcb_banner(FILE *, int);
int print_inpcb(kaddr_t, k_ptr_t, int, FILE *);

void inventory_s_banner(FILE *, int);
int print_inventory_s(kaddr_t, k_ptr_t, int, FILE *);

void ithread_s_banner(FILE *, int);
int print_ithread_s(kaddr_t, k_ptr_t, int, FILE *);
int list_active_ithreads(int, FILE *);

void kthread_banner(FILE *, int);
int print_kthread(kaddr_t, k_ptr_t, int, FILE *);
int print_kthread_full(k_ptr_t, int, FILE *);

void mbuf_banner(FILE *, int);
int print_mbuf(kaddr_t, k_ptr_t, int, FILE *);

void ml_info_banner(FILE *, int);
k_ptr_t get_ml_info(kaddr_t, int, k_ptr_t);
int print_ml_info(kaddr_t, k_ptr_t, int, FILE *);

int print_ml_sym(kaddr_t, k_ptr_t, k_ptr_t, FILE *);

void mntinfo_banner(FILE *, int);
k_ptr_t get_mntinfo(kaddr_t, k_ptr_t, int);
int print_mntinfo(kaddr_t, k_ptr_t, int, FILE *);

void mrlock_s_banner(FILE *, int);
int print_mrlock_s(kaddr_t, k_ptr_t, int, FILE *);

void nodepda_s_banner(FILE *, int);
int print_nodepda_s(kaddr_t, k_ptr_t, int, FILE *);

void pda_s_banner(FILE *, int);
int print_pda_s(kaddr_t, k_ptr_t, int, FILE *);

k_ptr_t get_pde(kaddr_t, k_ptr_t);
void pde_banner(FILE *, int) ;
int print_pde(kaddr_t, k_ptr_t, FILE *);

void pfdat_banner(FILE *, int);
int print_pfdat(kaddr_t, k_ptr_t, int, FILE *);

void pid_entry_banner(FILE *, int);
int print_pid_entry(kaddr_t, k_ptr_t, int, FILE *);

void pmap_banner(FILE *, int);
int print_pmap(kaddr_t, k_ptr_t, int, FILE *);

void pregion_banner(FILE *, int);
int print_pregion(kaddr_t, k_ptr_t, int, FILE *);
void list_mapped_pages(k_ptr_t, int, FILE *);
int list_proc_pregions(kaddr_t, int, int, FILE *);

k_ptr_t get_proc(kaddr_t, int, int);
void proc_banner(FILE *, int);
int print_proc(kaddr_t, k_ptr_t, int, FILE *);
int list_active_procs(int, FILE *);

void region_banner(FILE *, int);
int print_region(kaddr_t, k_ptr_t, int, FILE *);
int list_region_pfdats(k_ptr_t, int, FILE *);

k_ptr_t get_rnode(kaddr_t, k_ptr_t, int);
void rnode_banner(FILE *, int);
int print_rnode(kaddr_t, k_ptr_t, int, FILE *);

void sema_s_banner(FILE *, int);
int print_sema_s(kaddr_t, k_ptr_t, int, FILE *);

k_ptr_t get_lsnode(kaddr_t, k_ptr_t, int);
void lsnode_banner(FILE *, int);
int print_lsnode(kaddr_t, k_ptr_t, int, FILE *);

k_ptr_t get_socket(kaddr_t, k_ptr_t, int);
void socket_banner(FILE *, int);
int print_socket(kaddr_t, k_ptr_t, int, FILE *);
int list_sockets(int, FILE *);

void sthread_s_banner(FILE *, int);
int print_sthread_s(kaddr_t, k_ptr_t, int, FILE *);
int list_active_sthreads(int, FILE *);

k_ptr_t get_swapinfo(kaddr_t, k_ptr_t, int);
void swapinfo_banner(FILE *, int);
int print_swapinfo(kaddr_t, k_ptr_t, int, FILE *);

void tcpcb_banner(FILE *, int);
int print_tcpcb(kaddr_t, k_ptr_t, int, FILE *);

void unpcb_banner(FILE *, int);
int print_unpcb(kaddr_t, k_ptr_t, int, FILE *);

void uthread_s_banner(FILE *, int);
int print_uthread_s(kaddr_t, k_ptr_t, int, FILE *);

k_ptr_t get_vfs(kaddr_t, k_ptr_t, int);
void vfs_banner(FILE *, int);
int print_vfs(kaddr_t, k_ptr_t, int, FILE *);

void vfssw_banner(FILE *, int);
int print_vfssw(int, int, FILE *);

k_ptr_t get_vnode(kaddr_t, k_ptr_t, int);
void vnode_banner(FILE *, int);
int print_vnode(kaddr_t, k_ptr_t, int, FILE *);

k_ptr_t get_anon(kaddr_t, k_ptr_t, int);
void anon_banner(FILE *, int);
int print_anon(kaddr_t, k_ptr_t, int, FILE *);

void vproc_banner(FILE *, int);
int print_vproc(kaddr_t, k_ptr_t, int, FILE *);

void vsocket_banner(FILE *, int);
int print_vsocket(kaddr_t, k_ptr_t, int, FILE *);

void xfs_inode_banner(FILE *, int);
int print_xfs_inode(kaddr_t, k_ptr_t, int, FILE *);

void zone_banner(FILE *, int);
int print_zone(kaddr_t, k_ptr_t, int, FILE *);

void xthread_s_banner(FILE *, int);
int print_xthread_s(kaddr_t, k_ptr_t, int, FILE *);
int list_active_xthreads(int, FILE *);

/*
 * cmd/cmd_pfdat.c
 */
extern int next_valid_pfn(int pfn_first,int pfn_last);
extern int valid_pfn(int pfn);
	
/* libutil/timer.c
 */

/* libutil/trace.c -- included in extern/trace.h
 */

/* libutil/util.c
 */
int get_value(char *, int *, int, kaddr_t *);
void print_bit_value(k_ptr_t, int, int, int, int, FILE *);
void print_char(k_ptr_t, int, FILE *);
void print_uchar(k_ptr_t, int, FILE *);
void print_int2(k_ptr_t, int, FILE *);
void print_uint2(k_ptr_t, int, FILE *);
void print_int4(k_ptr_t, int, FILE *);
void print_uint4(k_ptr_t, int, FILE *);
void print_float4(k_ptr_t, int, FILE *);
void print_int8(k_ptr_t, int, FILE *);
void print_uint8(k_ptr_t, int, FILE *);
void print_float8(k_ptr_t, int, FILE *);
void print_base(k_ptr_t, int, int, int, FILE *);
int clear_dump();
void fatal(char *);
kaddr_t do_math(char *);
int dump_memory(kaddr_t, k_uint_t, int, FILE *);
kaddr_t dump_string(kaddr_t, int, FILE *);
void print_string(char *, FILE *);
void print_putbuf(FILE *);
void print_utsname(FILE *);
void print_curtime(FILE *);
char *helpformat(char *);
void addinfo(__uint32_t *, __uint32_t *, int);
int list_pid_entries(int, FILE *);
void node_memory_banner(FILE *, int);
int print_node_memory(FILE *, int, int);
int print_dump_page_range(FILE *, kaddr_t, int, int);
int print_memory_bank_info(FILE *, int, banks_t *, int);
int list_dump_memory(FILE *);
int list_system_memory(FILE *, int);

/* machdep.c
 */
void set_machine_dependant();

/* main.c
 */
extern FILE *errorfp;			 /* Global error outfile              */
extern char namelist[];          /* name list (normally /unix)        */
extern char corefile[];          /* core file (live = /dev/mem)       */
extern char indexname[];         /* name of index file                */
extern char outfile[];           /* name of global output file        */
extern char icrashdef[];         /* name of file containing icrashdef */
extern char fromfile[];          /* name of global command file       */
extern char execute_cmd[];       /* name of command to execute        */
extern char *program;            /* program name (argv[0])            */
extern int prog;                 /* program type (icrash or fru)      */
extern int bounds_flag;          /* bounds flag option                */
extern int pager_flag;           /* turns on use of page with output  */
extern int write_flag;           /* write option flag                 */
extern int full_flag;            /* full option flag                  */
extern int force_flag;           /* force FRU analysis flag           */
extern int all_flag;             /* all option flag (error dumpbuf)   */
extern int err_flag;             /* error flag                        */
extern int report_flag;          /* Generate report output            */
extern int report_level;         /* Determines level of report output */
extern int availmon;             /* availability report flag          */
extern char availmonfile[];      /* name of availmon output file      */
extern int cleardump;            /* flag to clear dump                */
extern int bounds;               /* bounds value with -n option       */
extern int version;              /* flag to print version number      */
extern int active;               /* examining an active system        */
extern int ignoreos;             /* ignore OS level in utsname struct */
extern int writeflag;            /* flag to determine if writing out  */
extern int icrashdef_flag;       /* flag to read in icrashdef info    */
extern int fromflag;             /* flag to specify from commands     */
extern int execute_flag;         /* next arg is a command to execute  */
extern int suppress_flag;        /* suppress opening text             */

/* External functions
 */
extern char *strpbrk();
extern char *getenv();
extern unsigned long long strtoull();
