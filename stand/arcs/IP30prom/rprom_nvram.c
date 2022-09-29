#ident	"IP30prom/rprom_nvram.c:  $Revision: 1.2 $"

/*
 * rprom_nvram.c -- rprom nvram support, subset of libsk/ml/nvram.c
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <arcs/errno.h>
#include <net/in.h>
#include <stringlist.h>
#include <ctype.h>
#include <libsc.h>
#include <libsk.h>

#include <sys/RACER/IP30nvram.h>
#include <sys/RACER/sflash.h>

/* debugging prinfs */
#ifdef VERBOSE
#define DPRINTF(x)	printf x

/* should be converted to flash pds logging */
#define PRINTF(x)	printf x
#else

#define DPRINTF(x)
#define PRINTF(x)

#endif

/* forward fcts */
char *htoe (unsigned char *);

/*
 * memory copies of nvram env variables that we care about
 */
char	nvram_bootmaster[NVLEN_BOOTMASTER];
ushort	nvram_rpromflgs;
char	nvram_netaddr[NVLEN_NETADDR];
char	nvram_ef0mode[NVLEN_EF0MODE];
char	nvram_eaddr[32];

/*
 * init_env -- initialize environment variables
 */
void
init_env(void)
{
    struct nvram_entry *et;
    char *cp;

    DPRINTF(("init_env()\n"));

    if ( !cpu_is_nvvalid()) {
	extern void flash_nv_cntr_bad(void);
	PRINTF(("\nNVRAM checksum is incorrect:\n"));
	/*
	 * alert flash code to not rely on dallas nvram counters
	 * and proceed with the RPROM recovery
	 */
	flash_nv_cntr_bad();
    }

    nvram_rpromflgs = flash_get_nv_rpromflg();
    Verbose = RPROM_FLG_VRB(nvram_rpromflgs); 
    Debug = RPROM_FLG_DBG(nvram_rpromflgs);
    DPRINTF(("FLG %x Verbose %d Debug %d\n",
	     nvram_rpromflgs,
    	     Verbose, Debug));

    cp = cpu_get_nvram_offset(NVOFF_BOOTMASTER, NVLEN_BOOTMASTER);
    bcopy(cp, nvram_bootmaster, NVLEN_BOOTMASTER);

    cp = cpu_get_nvram_offset(NVOFF_NETADDR, NVLEN_NETADDR);
    strcpy(nvram_netaddr, cp);

    /*
     * we get the eaddr from the nic
     */
    cp = cpu_get_nvram_offset(NVOFF_ENET, NVLEN_ENET);
    strcpy(nvram_eaddr, htoe((unsigned char *)cp));
}

char *
htoe (unsigned char *hex)
{
	static char buf[32];
	int nvram_idx;
	unsigned char *cp;
	char *p = buf;

	for ( nvram_idx = 0, cp = hex; nvram_idx < 6; nvram_idx++, cp++) {
		if ( nvram_idx != 5 ) {
			sprintf(p,"%02x:", *cp);
			p+=3;
		}
		else {
			sprintf(p,"%02x", *cp);
			p+=2;
		}
	}
	*p = '\0';
	return buf;
}

/*
 * we should allow only 3 nvram var to be set in rprom, and that
 * is the nflashed_rprom, nflashed_fprom, nflashed_pds variables.
 */
int
setenv_nvram(char *s1, char *s2)
{
    DPRINTF(("setenv_nvram(%s,%s) from 0x%x\n", s1, s2, __return_address));
    return(ESUCCESS);

}

int
cpu_set_nvram(char *match, char *newstring)
{
    DPRINTF(("cpu_set_nvram(%s,%s) from 0x%x\n",
	    match, newstring, __return_address));
    return(ESUCCESS);
}

/*
 * eaddr and netaddr are the current critical stuff
 */
char *
getenv (const char *name)
{
    char *rv = 0;

    DPRINTF(("getenv(%s) from 0x%x\n", name, __return_address));
    if (strcasecmp(name, "bootmaster") == 0) {
	rv = nvram_bootmaster;
    }
    else if (strcasecmp(name, "eaddr") == 0) {
	rv = nvram_eaddr;
    }
    else if (strcasecmp(name, "ef0mode") == 0) {
	rv = nvram_ef0mode;
    }
    else if (strcasecmp(name, "netaddr") == 0) {
	rv = nvram_netaddr;
    }

    DPRINTF(("  %s=%s\n", name, rv?rv:""));
    return(rv);
}

int
syssetenv (char *name, char *value)
{
    DPRINTF(("syssetenv(%s,value) from 0x%x\n", name,value, __return_address));
    return(0);
}

int
setenv (char *name, char *value)
{
    DPRINTF(("setenv(%s,%s) from 0x%x\n", name,value, __return_address));
    return(ESUCCESS);
}

int
_setenv (char *name, char *value, int override)
{
    DPRINTF(("_setenv(%s,%s,%d) from 0x%x\n",
    	    name, value, override, __return_address));
    return ESUCCESS;
}


/* env string related */
/* bootp.c ref */

struct string_list environ_str;
LONG 
SetEnvironmentVariable(CHAR *name, CHAR *value)
{
    DPRINTF(("SetEnvironmentVariable(%s,%s) from 0x%x\n",
    	     name,value, __return_address));
    return(ESUCCESS);
}

void
replace_str(char *name, char *value, struct string_list *slp)
{
    DPRINTF(("replace_str(%s,%s,0x%x) from 0x%x\n",
    	    name, value, slp, __return_address));
}

