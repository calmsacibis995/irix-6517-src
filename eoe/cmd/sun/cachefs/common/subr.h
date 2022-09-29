/* -----------------------------------------------------------------
 *
 *			subr.h
 *
 * Function prototypes for subr.c
 */

/* #pragma ident "@(#)subr.h   1.3     93/02/02 SMI" */
/*
 *  (c) 1992  Sun Microsystems, Inc
 */
#include <ftw.h>

#define gettext( fmt )		(fmt)

#define CACHEID_PREFIX		"cachefs:"
#define CACHEID_PREFIX_LEN	strlen(CACHEID_PREFIX)

struct user_values {
	int uv_maxblocks;
	int uv_maxfiles;
	int uv_hiblocks;
	int uv_lowblocks;
	int uv_hifiles;
	int uv_lowfiles;
};

int cachefs_dir_lock(char *cachedirp, int shared);
int cachefs_dir_unlock(int fd);
int cachefs_label_file_get(char *filep, struct cache_label *clabelp);
int cachefs_label_file_put(char *filep, struct cache_label *clabelp);
int cachefs_inuse(char *cachedirp);
void pr_err(char *fmt, ...);
void log_err(char *fmt, ...);
int delete_file(const char *namep, const struct stat *statp, int flg,
	struct FTW *ftwp);
int convert_cl2uv(const struct cache_label *clp, struct user_values *uvp,
	const char *dirp);
int convert_uv2cl(const struct user_values *uvp, struct cache_label *clp,
	const char *dirp);
int create_cache(char *dirp, struct user_values *uvp);
int check_user_values_for_sanity(const struct user_values *uvp);
void user_values_defaults(struct user_values *uvp);
int delete_all_cache(char *dirp);
int dir_is_empty(char *path);
int delete_cache(char *dirp, char *namep);
int read_file_header(const char *path, char *fileheader);
int get_mount_point(char *cachedirp, char *specp, char **pathpp, int validate);
char *get_cacheid(char *, char *);
char *make_filename(char *);
