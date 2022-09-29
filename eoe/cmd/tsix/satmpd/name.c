#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include "debug.h"
#include "name.h"

static int  gid_init (void);
static int  lbl_init (void);
static void lbl_destroy (void);

int
name_initialize (void)
{
	if (lbl_init () == -1 || gid_init () == -1)
		return (-1);
	if (atexit (lbl_destroy) == 0)
		return (0);
	lbl_destroy ();
	return (-1);
}

struct idmap
{
	id_t id;
	char name[64];
};

static const char *
map_id (id_t id, struct idmap *map)
{
	int i;

	for (i = 0; map[i].id != (id_t) 0xffff; i++)
		if (id == map[i].id)
			return (map[i].name);
	return (NULL);
}

static id_t
map_name (const char *name, struct idmap *map)
{
	int i;

	for (i = 0; map[i].id != (id_t) 0xffff; i++)
		if (strcmp (name, map[i].name) == 0)
			return (map[i].id);
	return ((id_t) -1);
}

static struct idmap uidmap[] =
{
	{0, "root"},
	{0xffff, ""},
};

int
name_to_uid (const char *name, uid_t *pwuid)
{
	unsigned long ul;
	char *vp;

	/* check for numeric ids */
	ul = strtoul (name, &vp, 10);
	if ((ul == 0 && vp == name) || *vp != '\0')
	{
		struct passwd pwd, *result;
		char buf[512];
		uid_t tmpid;

		tmpid = map_name (name, uidmap);
		if (tmpid != (uid_t) -1)
		{
			*pwuid = tmpid;
			return (0);
		}
		if (getpwnam_r (name, &pwd, buf, sizeof (buf), &result) != 0 ||
		    result == NULL)
			return (-1);
		*pwuid = result->pw_uid;
		return (0);
	}
	else
	{
		*pwuid = (uid_t) ul;
		return (0);
	}
}

int
uid_to_name (uid_t uid, char *pwname)
{
	struct passwd pwd, *result;
	char buf[512];
	const char *mapname;

	mapname = map_id (uid, uidmap);
	if (mapname != NULL)
	{
		strcpy (pwname, mapname);
		return (0);
	}
	if (getpwuid_r (uid, &pwd, buf, sizeof (buf), &result) != 0 ||
	    result == NULL)
		return (-1);
	strcpy (pwname, result->pw_name);
	return (0);
}

static struct idmap gidmap[257];

static int
gid_init_common (gid_t gid, struct idmap *map)
{
	struct group *grp;

	if (gid == 0xffff)
	{
		map->id = gid;
		map->name[0] = '\0';
		return (0);
	}

	grp = getgrgid (gid);
	if (grp == NULL)
		return (-1);
	map->id = gid;
	strcpy (map->name, grp->gr_name);
	return (0);
}

static int
gid_init (void)
{
	int gidmapsize, gmax, i;
	gid_t groups[256];

	gidmapsize = (int) sysconf (_SC_NGROUPS_MAX);
	if (gidmapsize == -1 || gidmapsize > 255)
	{
		if (debug_on (DEBUG_STARTUP))
			debug_print ("gid map size invalid: %d\n", gidmapsize);
		return (-1);
	}

	gmax = getgroups (gidmapsize, &groups[1]);
	if (gmax == -1)
	{
		if (debug_on (DEBUG_STARTUP))
			debug_print ("getgroups failed: %\n",
				     strerror (errno));
		return (-1);
	}

	groups[0] = getgid ();
	for (i = 0; i < gmax + 1; i++)
		if (gid_init_common (groups[i], &gidmap[i]) == -1)
			return (-1);
	return (gid_init_common ((gid_t) 0xffff, &gidmap[i]));
}

int
name_to_gid (const char *name, gid_t *grgid)
{
	unsigned long ul;
	char *vp;

	/* check for numeric ids */
	ul = strtoul (name, &vp, 10);
	if ((ul == 0 && vp == name) || *vp != '\0')
	{
		struct group grp, *result;
		char buf[512];
		gid_t tmpid;

		tmpid = map_name (name, gidmap);
		if (tmpid != (gid_t) -1)
		{
			*grgid = tmpid;
			return (0);
		}
		if (getgrnam_r (name, &grp, buf, sizeof (buf), &result) != 0 ||
		    result == NULL)
			return (-1);
		*grgid = result->gr_gid;
		return (0);
	}
	else
	{
		*grgid = (gid_t) ul;
		return (0);
	}
}

int
gid_to_name (gid_t gid, char *grname)
{
	struct group grp, *result;
	char buf[512];
	const char *mapname;

	mapname = map_id (gid, gidmap);
	if (mapname != NULL)
	{
		strcpy (grname, mapname);
		return (0);
	}
	if (getgrgid_r (gid, &grp, buf, sizeof (buf), &result) != 0 ||
	    result == NULL)
		return (-1);
	strcpy (grname, result->gr_name);
	return (0);
}

static struct lbl_match
{
	mac_b_label *lbl;
	char *name;
} lbl_list[2];
static const int MSEN = 0;
static const int MINT = 1;

static void
lbl_destroy (void)
{
	msen_free ((msen_t) lbl_list[MSEN].lbl);
	msen_free ((msen_t) lbl_list[MSEN].name);

	mint_free ((mint_t) lbl_list[MINT].lbl);
	mint_free ((mint_t) lbl_list[MINT].name);
}

static int
lbl_init (void)
{
	mac_t mac;

	mac = mac_get_proc ();
	if (mac == NULL)
		return (-1);

	lbl_list[MSEN].lbl = msen_from_mac (mac);
	lbl_list[MSEN].name = msen_to_text (lbl_list[0].lbl, (size_t *) NULL);

	lbl_list[MINT].lbl = mint_from_mac (mac);
	lbl_list[MINT].name = mint_to_text (lbl_list[1].lbl, (size_t *) NULL);

	mac_free (mac);

	if (lbl_list[MSEN].lbl == NULL || lbl_list[MSEN].name == NULL ||
	    lbl_list[MINT].lbl == NULL || lbl_list[MINT].name == NULL)
	{
		lbl_destroy ();
		return (-1);
	}
	return (0);
}

static mac_b_label *
name_to_lbl (const char *name, struct lbl_match *lmp,
	     mac_b_label *(*dup) (mac_b_label *),
	     mac_b_label *(*text) (const char *))
{
	if (strcmp (name, lmp->name) == 0)
		return ((*dup) (lmp->lbl));
	return ((*text) (name));
}

msen_t
name_to_msen (const char *name)
{
	return (name_to_lbl (name, &lbl_list[MSEN], msen_dup, msen_from_text));
}

mint_t
name_to_mint (const char *name)
{
	return (name_to_lbl (name, &lbl_list[MINT], mint_dup, mint_from_text));
}

static char *
lbl_to_name (mac_b_label *lbl, size_t *size, struct lbl_match *lmp,
	     int (*equal) (mac_b_label *, mac_b_label *),
	     char *(*text) (mac_b_label *lbl, size_t *size))
{
	if (lmp->lbl->b_type == lbl->b_type && (*equal) (lmp->lbl, lbl) > 0)
	{
		size_t len = strlen (lmp->name);
		char *name = malloc (len + 1);

		if (name != NULL)
			strcpy (name, lmp->name);
		if (size != NULL)
			*size = len;
		return (name);
	}
	return ((*text) (lbl, size));
}

char *
msen_to_name (msen_t msen, size_t *size)
{
	return (lbl_to_name (msen, size, &lbl_list[MSEN],
			     msen_equal, msen_to_text));
}

char *
mint_to_name (mint_t mint, size_t *size)
{
	return (lbl_to_name (mint, size, &lbl_list[MINT],
			     mint_equal, mint_to_text));
}
