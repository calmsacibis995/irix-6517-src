#ident "lib/libsk/ml/nvcompat.c:  $Revision: 1.23 $"

/*
 * nvcompat.c -- nvram compatitility functions
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <stringlist.h>
#include <libsc.h>
#include <ctype.h>
#include <net/in.h>
#include <libsk.h>

#if DEBUG
extern int Debug;
#endif

#if defined(IP20)
/* Defines from the old IP12nvram.h file.  These specify the old layout
 * of the nvram needed for IP12 -> IP20 upgrade.
 */
#define OLD_NVLEN_MAX		128
#define OLD_NVOFF_BOOTMODE	0
#define OLD_NVLEN_BOOTMODE	1
#define OLD_NVOFF_NETADDR	2
#define OLD_NVLEN_NETADDR	16
#define OLD_NVOFF_LBAUD		(OLD_NVOFF_NETADDR + OLD_NVLEN_NETADDR)
#define OLD_NVLEN_LBAUD		5
#define OLD_NVOFF_RBAUD		(OLD_NVOFF_LBAUD + OLD_NVLEN_LBAUD)
#define OLD_NVLEN_RBAUD		5
#define OLD_NVOFF_CONSOLE	(OLD_NVOFF_RBAUD + OLD_NVLEN_RBAUD)
#define OLD_NVLEN_CONSOLE	1
#define OLD_NVOFF_SCCOLOR	(OLD_NVOFF_CONSOLE + OLD_NVLEN_CONSOLE)
#define OLD_NVLEN_SCCOLOR	6
#define OLD_NVOFF_PGCOLOR	(OLD_NVOFF_SCCOLOR + OLD_NVLEN_SCCOLOR)
#define OLD_NVLEN_PGCOLOR	6
#define OLD_NVOFF_LGCOLOR	(OLD_NVOFF_PGCOLOR + OLD_NVLEN_PGCOLOR)
#define OLD_NVLEN_LGCOLOR	6
#define OLD_NVOFF_PAD0		(OLD_NVOFF_LGCOLOR + OLD_NVLEN_LGCOLOR)
#define OLD_NVLEN_PAD0		2
#define OLD_NVOFF_CHECKSUM	(OLD_NVOFF_PAD0 + OLD_NVLEN_PAD0)
#define	OLD_NVLEN_CHECKSUM	1
#define OLD_NVOFF_DISKLESS	(OLD_NVOFF_CHECKSUM + OLD_NVLEN_CHECKSUM)
#define	OLD_NVLEN_DISKLESS	1
#define OLD_NVOFF_NOKBD		(OLD_NVOFF_DISKLESS + OLD_NVLEN_DISKLESS)
#define OLD_NVLEN_NOKBD		1
#define OLD_NVOFF_BOOTFILE	(OLD_NVOFF_NOKBD + OLD_NVLEN_NOKBD)
#define OLD_NVLEN_BOOTFILE	50
#define	OLD_NVOFF_PASSWD_KEY	(OLD_NVOFF_BOOTFILE+OLD_NVLEN_BOOTFILE)
#define	OLD_NVLEN_PASSWD_KEY  	17
#define OLD_NVOFF_VOLUME	(OLD_NVOFF_PASSWD_KEY+OLD_NVLEN_PASSWD_KEY)
#define OLD_NVLEN_VOLUME	3
#define	OLD_NVLEN_ENET		6
#define OLD_NVOFF_ENET		(OLD_NVLEN_MAX-OLD_NVLEN_ENET)

/*
 * oldnvchecksum -- calculate new checksum for non-volatile RAM
 */
static char
oldnvchecksum(void)
{
    register int nv_reg;
    unsigned short nv_val;
    /*
     * Seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
     * (all-ones) checksum.
     */
    register signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = 0; nv_reg < OLD_NVLEN_MAX / 2; nv_reg++)
    {
	cpu_get_nvram_buf(nv_reg << 1, 2, (char *)&nv_val);
	if (nv_reg == (OLD_NVOFF_CHECKSUM / 2))
	   if (OLD_NVOFF_CHECKSUM & 0x01)
	      checksum ^= nv_val >> 8;
	   else
	      checksum ^= nv_val & 0xff;
	else
	   checksum ^= (nv_val >> 8) ^ (nv_val & 0xff);
	/* following is a tricky way to rotate */
	checksum = (checksum << 1) | (checksum < 0);
    }

    return (char)checksum;
}

int
cpu_is_oldnvram(void)
{
    /* try twice */
    if (oldnvchecksum() != 
	    *cpu_get_nvram_offset(OLD_NVOFF_CHECKSUM,OLD_NVLEN_CHECKSUM) &&
	oldnvchecksum() != 
	    *cpu_get_nvram_offset(OLD_NVOFF_CHECKSUM,OLD_NVLEN_CHECKSUM) )
	return 0;

    return 1;
}

/* These are the old nvram entries we wish to copy to the new
 * nvram locations.
 */
static struct onvram_entry {
	char *nt_name;
	int nt_nvaddr;
	int nt_nvlen;
} old_table[] = {
	{ "netaddr",	OLD_NVOFF_NETADDR,	OLD_NVLEN_NETADDR },
	{ "dbaud",	OLD_NVOFF_LBAUD,	OLD_NVLEN_LBAUD },
	{ "rbaud",	OLD_NVOFF_RBAUD,	OLD_NVLEN_RBAUD },
	{ "diskless",	OLD_NVOFF_DISKLESS,	OLD_NVLEN_DISKLESS },
	{ "pagecolor",	OLD_NVOFF_PGCOLOR,	OLD_NVLEN_PGCOLOR },
	{ "nogfxkbd",	OLD_NVOFF_NOKBD,	OLD_NVLEN_NOKBD },
	{ "passwd_key",	OLD_NVOFF_PASSWD_KEY,OLD_NVLEN_PASSWD_KEY },
	{ "volume",	OLD_NVOFF_VOLUME,	OLD_NVLEN_VOLUME },
	{ "bootmode",	OLD_NVOFF_BOOTMODE,	OLD_NVLEN_BOOTMODE },
	{ "console",	OLD_NVOFF_CONSOLE,	OLD_NVLEN_CONSOLE },
	{ 0,0,0 }
};

/* These are the new nvram entries that are not updated from the
 * old nvram.
 */
static char *not_updated[] = {
    "OSLoader",
    "OSLoadFilename",
    "OSLoadOptions",
    "SystemPartition",
    "OSLoadPartition",
    "TimeZone",
    "FWSearchPath",
    "scsiretries",
    "scsihostid",
    "netpasswd_key",
    "keybd",
    "lang",
    "sgilogo",
    "nogui",
    0
};

/* see comment in nvram.c */
#if _NT_PROM
extern int little_endian;
#define default(entry)							\
    ((little_endian && alt_defaults[(entry)-env_table]) ? 		\
	alt_defaults[(entry)-env_table] : (entry)->nt_value)

extern char *alt_defaults[];
#else
#define default(entry)		(entry)->nt_value
#endif

extern struct nvram_entry env_table[];

void
cpu_update_nvram(void)
{
    struct onvram_entry *old;
    struct nvram_entry *new;
    char oldnv[128], string[64], **cp;

    /* Get a copy of the entire old nvram image
     */
    cpu_get_nvram_buf(0, OLD_NVLEN_MAX, oldnv);

    /* Clear out the old checksum
     */
    string[0] = 0;
    cpu_set_nvram_offset(OLD_NVOFF_CHECKSUM, OLD_NVLEN_CHECKSUM, string);

    cpu_nv_lock(0);		/* unlock the nvram */

    /* copy the ethernet address to its new location
     */
    bcopy (oldnv+OLD_NVOFF_ENET, string, OLD_NVLEN_ENET);
    string[OLD_NVLEN_ENET] = '\0';
    syssetenv ("eaddr", htoe((unsigned char *)string));
    cpu_nv_lock(1);		/* relock the nvram at the new location */

    /* Copy the old nvram information to the new locations.
     */
    for (old = old_table; old->nt_nvlen; old++) {

	if (!strcmp (old->nt_name, "bootmode")) {
		*string = *((char *)oldnv+old->nt_nvaddr);
		/* set AutoLoad, and diagmode based on old bootmode */
		syssetenv("AutoLoad", *string == 'c' ? "Yes" : "No");
		syssetenv("diagmode", *string == 'd' ? "v" : "");
		continue;
	}

	/* The rest of the entries are the same size and meaning
	 *
	 * Find the corresponding new entry
	 */
	for (new = env_table; new->nt_nvlen; new++)
	    if (!strcmp(new->nt_name, old->nt_name))
		break;

	if (new->nt_nvlen) {
	    bcopy (oldnv+old->nt_nvaddr, string, old->nt_nvlen);
	    string[old->nt_nvlen] = '\0';
	    syssetenv (new->nt_name, string);
	}
    }

    /* Now fixup the remainder of the nvram that had no corresponding
     * entries in the old nvram.
     */
    for (cp = not_updated; *cp; ++cp) {
	/* Find the corresponding new entry */
	for (new = env_table; new->nt_nvlen; new++)
	    if (!strcmp(new->nt_name, *cp))
		break;

	if (new->nt_nvlen)
	    syssetenv (*cp, default(new));
    }
}
#elif IP26 || IP28		/* Handle IP22 -> IP26/28 upgrade */
int
cpu_is_oldnvram(void)
{
	char nvchecksum(void);

	/*  If do not have a revision 8 (old) layout, the layout is scrambled.
	 */
	if (*cpu_get_nvram_offset(NVOFF_REVISION,NVLEN_REVISION) != 8)
		return 0;

	/*  Given a revision 8 layout, if checksum is ok, we have a IP22 ->
	 * teton/IP28 upgrade, so return true.
	 */
	return	nvchecksum() ==
		*cpu_get_nvram_offset(NVOFF_CHECKSUM,NVLEN_CHECKSUM);
}

void
cpu_update_nvram(void)
{
	void initialize_envstrs(void);

	/*  Update all variables that changed from the real rev 8 to the
	 * non-rolled rev 8, and in the rev 9.
	 */
	cpu_set_nvram_offset(NVOFF_MONITOR,NVLEN_MONITOR,"");
	cpu_set_nvram_offset(NVOFF_REBOUND,NVLEN_REBOUND,REBOUND_DEFAULT);
	cpu_set_nvram_offset(NVOFF_BOOTTUNE,NVLEN_BOOTTUNE,"1");
	cpu_set_nvvalid(NULL,NULL);
	initialize_envstrs();
}
#endif /* IP20/IP26/IP28 */
