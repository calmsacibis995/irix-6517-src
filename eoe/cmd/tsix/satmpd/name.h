#ifndef NAME_HEADER
#define NAME_HEADER

#ident "$Revision: 1.1 $"

#include <sys/mac.h>

int name_initialize (void);
int name_to_uid (const char *, uid_t *);
int uid_to_name (uid_t, char *);
int name_to_gid (const char *, gid_t *);
int gid_to_name (gid_t, char *);

msen_t name_to_msen (const char *);
mint_t name_to_mint (const char *);
char  *msen_to_name (msen_t, size_t *);
char  *mint_to_name (mint_t, size_t *);

#endif /* NAME_HEADER */
