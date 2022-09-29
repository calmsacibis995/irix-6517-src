#ifndef LREP_HEADER
#define LREP_HEADER

#ident "$Revision: 1.1 $"

#include <sys/t6attrs.h>

struct ids {
	uid_t user;
	gid_t primary_group;
	t6groups_t groups;
};

int  configure_lrep_mappings (const char *);
void deconfigure_lrep_mappings (void);

void *lrep_from_text (const char *, u_short);
char *lrep_to_text (void *, u_short);
void *lrep_from_nrep (char *, u_short, const char *);
char *lrep_to_nrep (void *, u_short, const char *, size_t *);
void *lrep_copy (void *, u_short, size_t *);
void  destroy_lrep (void *, u_short);

#endif /* LREP_HEADER */
