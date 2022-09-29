#ident "$Revision: 1.3 $"

#include "versions.h"
#include <sys/param.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include "sim.h"
#include "addr.h"
#include "command.h"
#include "type.h"
#include "io.h"
#include "output.h"

#if VERS < V_62
typedef	uint xfs_dahash_t;
#endif

static int hash_f(int argc, char **argv);
static void hash_help(void);

static const cmdinfo_t hash_cmd =
	{ "hash", NULL, hash_f, 1, 1, 0, "string",
	  "calculate hash value", hash_help };

static void
hash_help(void)
{
	dbprintf(
"\n"
" 'hash' prints out the calculated hash value for a string using the\n"
"directory/attribute code hash function.\n"
"\n"
" Usage:  \"hash <string>\"\n"
"\n"
);

}

/*
 * stolen from xfs_da_btree.c
 *
 * Implement a simple hash on a character string.
 * Rotate the hash value by 7 bits, then XOR each character in.
 */
xfs_dahash_t
xfs_da_hashname(
	char		*name,
	int		namelen)
{
	xfs_dahash_t	hash;

	hash = 0;
	for (; namelen > 0; namelen--)
		hash = *name++ ^ ((hash << 7) | (hash >> (32-7)));
	return hash;
}

/* ARGSUSED */
static int
hash_f(
	int		argc,
	char		**argv)
{
	xfs_dahash_t	hashval;

	hashval = xfs_da_hashname(argv[0], (int)strlen(argv[0]));
	dbprintf("0x%x\n", hashval);
	return 0;
}

void
hash_init(void)
{
	add_command(&hash_cmd);
}
