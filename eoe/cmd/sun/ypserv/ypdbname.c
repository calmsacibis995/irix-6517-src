/*
 * NIS utility routine to map client map names to names
 * that are short enough to work with 14 character file names.
 */

/*
 * Translation table for mapping dbnames.
 *
 * The mapped name must be short enough to remain unique
 * after the addition of the suffixes ".t.dir" and ".t.pag".
 * The 'makedbm' program adds ".t" and the dbm package
 * adds the ".dir" and ".pag".  With 14 character names,
 * 10 characters is the limit.
 *
 * Let's not try to be cute for now, just an easy table
 * lookup will do the trick.
 */
static struct trans {
	char *realname;
	char *nickname;
} yp_name_map[] = {

	/*    Real                 Nickname	*/
	"passwd.byname",	"passwd.byname.m",
	"passwd.byuid",		"passwd.byuid.m",
	"group.byname",		"group.byname.m",
	"group.bygid",		"group.bygid.m",
	"hosts.byname",		"hosts.byname.m",
	"hosts.byaddr",		"hosts.byaddr.m",
	"protocols.byname",	"protocols.byname.m",
	"protocols.bynumber",	"protocols.bynumber.m",
	"services.byname",	"services.byname.m",
	"networks.byname",	"networks.byname.m",
	"networks.byaddr",	"networks.byaddr.m",
	"mail.aliases",		"mail.aliases.m",
	"netgroup",		"netgroup.m",
	"netgroup.byuser",	"netgroup.byuser.m",
	"netgroup.byhost",	"netgroup.byhost.m",
	"ethers.byaddr",	"ethers.byaddr.m",
	"ethers.byname",	"ethers.byname.m",
	"rpc.bynumber",		"rpc.bynumber.m",

	0,			0,	/* end of list */
};

/*
 * NIS database name translation
 *
 * The map names as defined by SUN are too long to work as file
 * names on systems that have traditional 14 character file names.
 * Hide this limitation from NIS clients by simply mapping the map
 * names to shorter names. If no match, just return the original name.
 */
void
yp_make_dbname(register char *name,	/* input name to be translated */
	       char *newname,		/* buffer for returned name */
	       int len)			/* length of 'newname' buffer */
{
	register struct trans *tp;

	for (tp = yp_name_map; tp->realname; tp++) {
		if (strcmp(tp->realname, name) == 0) {
			strncpy(newname, tp->nickname, len);
			return;
		}
	}
	strncpy(newname, name, len);
}

/*
 * NIS database reverse name translation: convert our short names
 * into the official map names. If no match, just return the 
 * original argument.
 */

char *
yp_real_mapname(name)
	register char *name;	/* map name to be translated */
{
	register struct trans *tp;

	for (tp = yp_name_map; tp->realname; tp++) {
		if (strcmp(tp->nickname, name) == 0) {
			return tp->realname;
		}
	}
	return name;
}
