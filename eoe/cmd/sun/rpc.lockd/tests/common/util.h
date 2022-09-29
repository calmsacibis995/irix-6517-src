char *locktype_to_str(int);
char *lockcmd_to_str(int);
void print_svcreq(struct svc_req *, char *);
void report_lock_error(char *, char *, char *, int);
int valid_addresses(caddr_t, int);
int isfile(char *);
void get_host_info(char *, char **, char **);
char *absolute_path(char *);

#if _SUNOS
#define mntent      mnttab
#define mnt_dir     mnt_mountp
#define mnt_fsname  mnt_special
#define setmntent	fopen
#define endmntent	fclose
#endif /* _SUNOS */

struct mntent *find_mount_point(char *);
