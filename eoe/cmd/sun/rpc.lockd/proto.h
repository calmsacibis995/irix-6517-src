
/* alloc.c */
void xfree ( char **a );
void release_res ( remote_result *resp );
void release_reclock ( reclock *a );
void release_nlm_lockargs ( nlm_lockargs *a );
void release_nlm_unlockargs ( nlm_unlockargs *a );
void release_nlm_testargs ( nlm_testargs *a );
void release_nlm_cancargs ( nlm_cancargs *a );
void release_nlm_testres ( nlm_testres *a );
void release_nlm_res ( nlm_res *a );
void release_klm_lockargs ( klm_lockargs *a );
void release_klm_unlockargs ( klm_unlockargs *a );
void release_klm_testargs ( klm_testargs *a );
void release_me ( struct lm_vnode *me );
void *xmalloc ( unsigned len );
struct lm_vnode *get_me ( void );
remote_result *get_res ( void );
reclock *get_reclock ( void );
klm_lockargs *get_klm_lockargs ( void );
klm_unlockargs *get_klm_unlockargs ( void );
klm_testargs *get_klm_testargs ( void );
nlm_lockargs *get_nlm_lockargs ( void );
nlm_unlockargs *get_nlm_unlockargs ( void );
nlm_cancargs *get_nlm_cancargs ( void );
nlm_testargs *get_nlm_testargs ( void );
nlm_testres *get_nlm_testres ( void );
nlm_res *get_nlm_res ( void );
char *copy_str ( char *str );
void release_klm_req ( int proc , void *klm_req );
void release_nlm_req ( int proc , void *nlm_req );
void release_nlm_rep ( int proc , void *nlm_rep );
struct process_locks *get_proc_list ( void );

/* cnlm.c */
int proc_nlm_test_res ( remote_result *reply );
int proc_nlm_lock_res ( remote_result *reply );
int proc_nlm_cancel_res ( remote_result *reply );
int proc_nlm_unlock_res ( remote_result *reply );
int proc_nlm_granted_res ( remote_result *reply );
int nlm_call ( int proc , struct reclock *a , int retransmit );

/* freeall.c */
void *proc_nlm_freeall ( struct svc_req *Rqstp , SVCXPRT *Transp );

/* hash.c */
void print_monlocks ( void );
struct lm_vnode *find_me ( char *svr );
void insert_me ( struct lm_vnode *mp );
void add_req_to_me ( struct reclock *req , int status );
void upgrade_req_in_me ( struct reclock *req );
void remove_req_in_me ( struct reclock *req );
void add_req_to_list ( struct reclock *req , struct reclocklist *list );
void rm_req ( struct reclock *req );
struct reclock *del_req_from_list ( struct data_lock *l , struct reclocklist *list );
bool_t del_req ( struct reclock *req , struct reclocklist *list );

/* klm.c */
int proc_klm_test ( reclock *a );
int proc_klm_lock ( reclock *a );
int proc_klm_cancel ( reclock *a );
int proc_klm_unlock ( reclock *a );
int klm_reply ( int proc , remote_result *reply );
int klm_lockargstoreclock ( struct klm_lockargs *from , struct reclock *to );
int klm_unlockargstoreclock ( struct klm_unlockargs *from , struct reclock *to );
int klm_testargstoreclock ( struct klm_testargs *from , struct reclock *to );

/* libr.c */
int init ( void );
int map_kernel_klm ( reclock *a );
int map_klm_nlm ( reclock *a );
int pr_oh ( netobj *a );
int pr_fh ( netobj *a );
int pr_lock ( reclock *a );
int pr_all ( void );
char *lck2name ( int stat );
char *klm_stat2name ( int stat );
char *nlm_stat2name ( int stat );
int kill_process ( reclock *a );

/* lock.c */
void merge_lock ( struct reclock *req , struct reclocklist *list );
void lm_unlock_region ( struct reclock *req , struct reclocklist *list );
void rem_process_lock ( struct data_lock *a );
bool_t obj_cmp ( struct netobj *a , struct netobj *b );
int obj_alloc ( netobj *a , char *b , u_int n );
int obj_copy ( netobj *a , netobj *b );
bool_t same_fh ( reclock *a , reclock *b );
bool_t same_bound ( struct data_lock *a , struct data_lock *b );
bool_t same_type ( struct data_lock *a , struct data_lock *b );
bool_t same_lock ( reclock *a , struct data_lock *b );
bool_t simi_lock ( reclock *a , reclock *b );
int add_mon ( reclock *a , int i );

/* main.c */
char *prtime ( void );
void start_timer ( int nflag );

/* msg.c */
msg_entry *retransmitted ( struct reclock *a , int proc );
msg_entry *search_msg ( remote_result *resp );
msg_entry *queue ( struct reclock *a , int proc );
int dequeue ( msg_entry *msgp );
void dequeue_lock ( struct reclock *a );
int add_reply ( msg_entry *msgp , remote_result *resp );
void msgqtimer ( void );

/* ncvt.c */
int reclocktonlm_lockargs ( struct reclock *from , nlm_lockargs *to );
int reclocktonlm_cancargs ( struct reclock *from , nlm_cancargs *to );
int reclocktonlm_unlockargs ( struct reclock *from , nlm_unlockargs *to );
int reclocktonlm_testargs ( struct reclock *from , nlm_testargs *to );
int nlm_lockargstoreclock ( nlm_lockargs *from , struct reclock *to );
int nlm_unlockargstoreclock ( nlm_unlockargs *from , struct reclock *to );
int nlm_cancargstoreclock ( nlm_cancargs *from , struct reclock *to );
int nlm_testargstoreclock ( nlm_testargs *from , struct reclock *to );
int nlm_testrestoremote_result ( nlm_testres *from , remote_result *to );
int nlm_restoremote_result ( nlm_res *from , remote_result *to );
void nlm_versx_conversion ( int procedure , char *from );

/* priv.c */
void priv_prog ( struct svc_req *rqstp , SVCXPRT *transp );

/* share.c */
void *init_nlm_share ( void );
void *proc_nlm_share ( struct svc_req *Rqstp , SVCXPRT *Transp );
void *destroy_client_shares ( char *client );

/* sm_monitor.c */
struct stat_res *stat_mon ( char *sitename , char *svrname , int my_prog , int my_vers , int my_proc , int func , int len , char *priv );
void cancel_mon ( void );

/* snlm.c */
void clear_blk_host ( u_long ipaddr );
int proc_nlm_test ( struct reclock *a );
int proc_nlm_test_msg ( struct reclock *a );
int proc_nlm_lock ( struct reclock *a );
int proc_nlm_lock_msg ( struct reclock *a );
int proc_nlm_cancel ( struct reclock *a );
int proc_nlm_cancel_msg ( struct reclock *a );
int proc_nlm_unlock ( struct reclock *a );
int proc_nlm_unlock_msg ( struct reclock *a );
int proc_nlm_granted ( struct reclock *a );
int proc_nlm_granted_msg ( struct reclock *a );
int nlm_reply ( int proc , nlm_stats stat , struct reclock *a );
int grant_remote ( void );

/* sysid.c */
void print_sysidtable ( void );
sysid_t mksysid ( pid_t pid , u_long ipaddr , int *isnewp );
pid_t sysid2pid ( sysid_t sysid );
void relsysid ( sysid_t sysid );
int save_fd ( sysid_t sysid , int fd );
void foreach_fd ( u_long ipaddr , void (*proc )(int, sysid_t));
void init_fdtable ( void );
void fd_block ( int fd , int cnt );
void fd_unblock ( struct reclock * );
void fdgctimer ( void );
void remove_fd ( int fd );
int get_fd ( struct reclock *a , int *isnewp );
void free_fd ( int fd );
int record_lock( int, struct reclock *, struct flock * );

/* udp.c */
int hash ( unsigned char *name );
void delete_hash ( char *host );
int call_udp ( char *host , int prognum , int versnum , int procnum , xdrproc_t inproc , void *in , xdrproc_t outproc , void *out , int valid_in , int tot );
int call_tcp ( char *host , int prognum , int versnum , int procnum , xdrproc_t inproc , void *in , xdrproc_t outproc , void *out , int valid_in , int tot );
