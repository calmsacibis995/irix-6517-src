#if defined(SGI_EXTENSIONS) && !defined(SGI_COMPAT_H)
#define SGI_COMPAT_H

/* Prototyptes for functions defined in sgi_compat.c */

void sgi_pre_defaults (ENVELOPE *);
void sgi_post_defaults (ENVELOPE *);

#if defined(SGI_MAP_NAMESERVER)
int   getM_rr (char *, int, char **, int, char *, int, bool *);
void  ns_expand_mbox (char *);
bool  gen_address (char *, int, bool *);
bool  gen_name (char *,int, bool *);
bool  gen_canon_name (char *, int, bool *, int);
char *ns_map_lookup (MAP *, char *, char **, int *);
#endif 

#if defined(SGI_MAP_PA)
char *pa_map_lookup (MAP *, char *, char **, int *);
#endif

#if defined(SGI_DEFINE_VIA_PROG)
void define_via_prog (char, char *, ENVELOPE *);
void sgi_eval_pipe(char *);
EXTERN bool sgi_prog_safe;
#endif

#if defined(SGI_NIS_ALIAS_KLUDGE)
void sgi_nis_alias_kludge (ENVELOPE *);
#endif

#if defined(SGI_FREEZE_FILE)
char *getfcname (void);
int   thaw (char *);
#endif

#if defined(SGI_GETLA)
int sgi_kmem_init (void);
int sgi_getla (void);
#endif

#if defined(SGI_COUNT_PROC)
int  sgi_cntprocs (char *);
void sgi_do_cntprocs (void);
#endif

#if defined(SGI_SENDMAIL_LA_SEM)
void sgi_register_self_sem(void);
#endif

#if defined(SGI_OLD_MAILER_SETTING)
void sgi_fix_mailers_flags (void);
#endif

#if defined(SGI_MATCHGECOS_EXTENSION)
int partialstring (char *, char *);
struct passwd *finduser (char *, bool *);
#endif

#if defined(SGI_HOST_RES_ORDER) && defined(NAMED_BIND)
bool sgi_getcanonname (char *, int, bool);
#endif

#if defined(SGI_UNIXFROMLINE_SAME_AS_FROMHEADER)
char *tidyaddr (char *);
#endif

#if defined(EXTERNAL_LOAD_IF_NAMES)
void load_if_names(void);
#endif

#if defined(SGI_CHECKCOMPAT)
int sgi_checkcompat(ADDRESS *, ENVELOPE *);
#endif

#if defined(SGI_SESMGR)
void sesmgr_session(int);
void sesmgr_socket(int *);
#endif

#if defined(SGI_AUDIT)
void audit_sendmail (const char *, const char *);
#endif

#if defined(SGI_CAP)
int cap_setuid(uid_t);
int cap_seteuid(uid_t);
int cap_setreuid(uid_t, uid_t);
int cap_setgid(gid_t);
int cap_initgroups(char *, gid_t);
int cap_chroot(const char *);
int cap_bind(int s, const struct sockaddr *, int);

#if defined(SGI_AUDIT)
int cap_satvwrite(int, int, char *, ...);
#endif

#if defined(SGI_MAC)
int   cap_mac_set_proc(void *);
void *cap_mac_get_file(const char *);
int   cap_attr_get(const char *, const char *, char *, int *, int);
int   cap_attr_set(const char *, const char *, const char *, int, int);
#endif
#endif

#if defined(SGI_MAC)
#include <sys/mac.h>

extern mac_t ProcLabel;
extern int TrustQ;

/* mac label primitives */
mac_t mac_dblow_label (void);
mac_t mac_dbadmin_label (void);
mac_t mac_wildcard_label (void);
void  mac_label_free (mac_t);

/* label comparison primitives */
int mac_label_dominate (mac_t, mac_t);
int mac_label_equal (mac_t, mac_t);

/* process primitives */
mac_t mac_proc_getlabel (void);
mac_t mac_swap (mac_t);
int   mac_proc_setlabel (mac_t);
void  mac_restore (mac_t);

/* file primitives */
mac_t mac_file_getattr (const char *file, const char *);
int   mac_file_setattr (const char *file, const char *, mac_t label);

/* user clearance primitives */
int mac_user_cleared (const char *, mac_t);

/* queue access primitives */
int q_open(const char *, int, mode_t);
int q_rename(const char *, const char *);
int q_fstat(int, struct stat *);
int q_stat(const char *, struct stat *);
int q_unlink(const char *);
int q_chdir(const char *);

int q_dfopen(char *, int, int, int);
FILE *q_fopen(const char *, const char *);
FILE *q_freopen(const char *, const char *, FILE *);

void *q_opendir(const char *);

mac_t q_getmac(const char *);
int   q_setmac(const char *, mac_t);
#endif

#endif /* SGI_COMPAT_H */
