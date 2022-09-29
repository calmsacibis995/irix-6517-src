#ident "$Header: "

/* From: main.c
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

/* From: init.c
 */
extern struct mbufconst mbufconst; /* Sizes of mbuf constants                */
extern vtab_t *vtab;            /* Pointer to table of eval variable info    */
extern int klp_hndl;            /* handne of the current klib_s struct */
extern int nfstype;             /* number of vfs's in vfssw[]                */
extern int mbpages;             /* Number of mbuf pages in use               */
extern kaddr_t S_mbpages;       /* Number of mbuf pages in use               */
extern kaddr_t S_strdzone;      /* Streams zone memory - XXX OLD 			 */
extern kaddr_t _hbuf;           /* buf hash table                            */
extern k_ptr_t strstp;          /* Streams statas struct                     */
extern k_ptr_t tlbdumptbl;      /* tlb dump table                            */
extern k_ptr_t _vfssw;          /* vfs switch table                          */

void init_global_vars(FILE *);
int init(FILE *);

/* External functions
 */
extern char *strpbrk();
extern char *getenv();
extern unsigned long long strtoull();

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

/* cmds/cmd_queue.c
 */
void queue_banner(FILE *, int);
void print_queue(kaddr_t, k_ptr_t, char *, int, FILE *);

/* cmds/cmd_stream.c
 */
void stream_banner(FILE *, int);

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

k_ptr_t get_mntinfo(kaddr_t, k_ptr_t, int);
void mntinfo_banner(FILE *, int);
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

/* libutil/util.c
 */
int clear_dump();
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
