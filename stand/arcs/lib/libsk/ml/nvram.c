#if !IP32 && !IP30_RPROM	/* whole file */
#ident "lib/libsk/ml/nvram.c:  $Revision: 1.123 $"

/*
 * nvram.c -- non-volatile RAM and environment functions
 */

#include <arcs/errno.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <net/in.h>
#include <stringlist.h>
#include <ctype.h>
#if IP20
#include <sys/IP20nvram.h>
#endif
#if IP22 || IP26 || IP28
#include <sys/IP22nvram.h>
#endif
#if IP30
#include <sys/RACER/IP30nvram.h>
#endif
#if EVEREST
#include <sys/EVEREST/nvram.h>
#endif
#if SN0
#include <sys/SN/nvram.h>
#endif
#include <libsc.h>
#include <libsk.h>

extern struct string_list environ_str;
extern char netaddr_default[];
extern char **environ;
extern char *check_serial_baud_rate(char*);
void initialize_envstrs(void);
extern void change_probeallscsi(void) ;

unsigned char *etoh (char *);
char *htoe (unsigned char *);

#if VERBOSE
extern int Verbose;
#define VPRINTF(arg)	if (Verbose) printf arg 
#else
#define VPRINTF(arg)
#endif

/*
 * environment initialization table
 */

struct nvram_entry env_table[] = {
#ifdef NVOFF_FWPATH			/* not all systems support this */
    {"FWSearchPath", "", NVOFF_FWPATH, NVLEN_FWPATH},
#endif /* NVOFF_FWPATH */
    {"OSLoadOptions", "", NVOFF_OSOPTS, NVLEN_OSOPTS},
#if EVEREST || SN0
    {"AutoLoad", "No",  NVOFF_AUTOLOAD, NVLEN_AUTOLOAD},
#if defined(SN0)
    {"dbgtty",	 DBGTTY_PATH_DEFAULT, NVOFF_DBGTTY, NVLEN_DBGTTY},
#else /* !SN0 */
    {"dbgtty",	 "multi(0)serial(0)", NVOFF_DBGTTY, NVLEN_DBGTTY},
#endif /* SN0 */
    {"root",	 "dks0d1s0", NVOFF_ROOT, NVLEN_ROOT},
#ifdef NVOFF_SWAP
    {"swap",	 "", NVOFF_SWAP, NVLEN_SWAP},
#endif
    {"fastmem",  "", NVOFF_FASTMEM, NVLEN_FASTMEM },
    {"nonstop",  "0", NVOFF_NONSTOP, NVLEN_NONSTOP },
    {"rbaud", "19200", NVOFF_RBAUD, NVLEN_RBAUD },
    {"SystemPartition", "dksc(0,1,8)", NVOFF_SYSPART, NVLEN_SYSPART},
    {"OSLoadPartition", "dksc(0,1,0)", NVOFF_OSPART, NVLEN_OSPART},
    {"OSLoader", "sash", NVOFF_OSLOADER, NVLEN_OSLOADER},
    {"OSLoadFilename", "unix", NVOFF_OSFILE, NVLEN_OSFILE},
    {"AltOSPath", "", NVOFF_ALTOSPATH, NVLEN_ALTOSPATH},
#else /* !(EVEREST || SN0) */
    {"SystemPartition", "", NVOFF_SYSPART, NVLEN_SYSPART},
    {"OSLoadPartition", "", NVOFF_OSPART, NVLEN_OSPART},
    {"OSLoader", "", NVOFF_OSLOADER, NVLEN_OSLOADER},
    {"OSLoadFilename", "", NVOFF_OSFILE, NVLEN_OSFILE},
    {"AutoLoad", "Yes", NVOFF_AUTOLOAD, NVLEN_AUTOLOAD},
    {"rbaud", "", NVOFF_RBAUD, NVLEN_RBAUD},
#endif /* (EVEREST || SN0) */
#ifdef NVOFF_TZ
    {"TimeZone", "PST8PDT", NVOFF_TZ, NVLEN_TZ},	/* PST */
#endif
#ifdef NVOFF_BOOTMASTER
    {"bootmaster", "", NVOFF_BOOTMASTER, NVLEN_BOOTMASTER },
#endif
#ifdef NVOFF_FASTFAN
    {"fastfan", "", NVOFF_FASTFAN, NVLEN_FASTFAN },
#endif
#if EVEREST || SN0
    {"console", "d", NVOFF_CONSOLE, NVLEN_CONSOLE},
#ifdef NVOFF_CONSOLE_PATH
    {"ConsolePath", CONSOLE_PATH_DEFAULT, NVOFF_CONSOLE_PATH, NVLEN_CONSOLE_PATH},
#endif
#ifdef NVOFF_SAV_CONSOLE_PATH
    {"oldConsolePath", SAV_CONSOLE_PATH_DFLT, NVOFF_SAV_CONSOLE_PATH, NVLEN_SAV_CONSOLE_PATH},
#endif

#ifdef NVOFF_G_CONSOLE_IN
    {"gConsoleIn", G_CONSOLE_IN_DEFAULT, NVOFF_G_CONSOLE_IN, NVLEN_G_CONSOLE_IN},
#endif

#ifdef NVOFF_G_CONSOLE_OUT
    {"gConsoleOut", G_CONSOLE_OUT_DEFAULT, NVOFF_G_CONSOLE_OUT, NVLEN_G_CONSOLE_OUT},
#endif

#else /* !(EVEREST || SN0) */
    {"console", "g", NVOFF_CONSOLE, NVLEN_CONSOLE},
#endif /* (EVEREST || SN0) */
    {"diagmode", "", NVOFF_DIAGMODE, NVLEN_DIAGMODE},
    {"diskless", "0", NVOFF_DISKLESS, NVLEN_DISKLESS},
    {"nogfxkbd", "", NVOFF_NOKBD, NVLEN_NOKBD},
    {"keybd", "", NVOFF_KEYBD, NVLEN_KEYBD},
#ifdef NVOFF_LANG
    {"lang", "", NVOFF_LANG, NVLEN_LANG},
#endif
#ifdef NVOFF_DLIF
    {"dlif", "", NVOFF_DLIF, NVLEN_DLIF},
#endif
#ifdef NVOFF_EF0MODE
    {"ef0mode", "", NVOFF_EF0MODE, NVLEN_EF0MODE},
#endif
#ifdef NVOFF_SCSIRT
    {"scsiretries", "", NVOFF_SCSIRT, NVLEN_SCSIRT},
#endif
#ifndef SN0
#ifdef NVOFF_SCSIHOSTID
    {"scsihostid", "0", NVOFF_SCSIHOSTID, NVLEN_SCSIHOSTID},
#endif
#else /* SN0 */
#ifdef NVOFF_SCSIHOSTID
    {"scsihostid", "00", NVOFF_SCSIHOSTID, NVLEN_SCSIHOSTID},
#endif
#endif /* SN0 */
#ifdef SN0
#ifdef NVOFF_PROBEALLSCSI
    {"ProbeAllScsi", PROBEALLSCSI_DEFAULT, NVOFF_PROBEALLSCSI, 
		     NVLEN_PROBEALLSCSI},
#endif
#ifdef NVOFF_PROBEWHICHSCSI
    {"ProbeWhichScsi", "", NVOFF_PROBEWHICHSCSI, NVLEN_PROBEWHICHSCSI},
#endif
#ifdef NVOFF_DBGNAME
    {"dbgname", "", NVOFF_DBGNAME, NVLEN_DBGNAME},
#endif
#ifdef NVOFF_RESTORE_ENV
    {"RestorePartEnv", "y", NVOFF_RESTORE_ENV, NVLEN_RESTORE_ENV},
#endif
#if defined(NVOFF_MAX_BURST) && defined(MIXED_SPEED_ENV)
    {"MaxBurst", "006", NVOFF_MAX_BURST, NVLEN_MAX_BURST},
#endif
#endif /* SN0 */
    {"dbaud", "9600", NVOFF_LBAUD, NVLEN_LBAUD},
    {"pagecolor", "", NVOFF_PGCOLOR, NVLEN_PGCOLOR},
#ifdef NVOFF_VOLUME
    {"volume", "80", NVOFF_VOLUME, NVLEN_VOLUME},
#endif
    {"sgilogo", "y", NVOFF_SGILOGO, NVLEN_SGILOGO},
#ifdef NVOFF_AUTOPOWER
    {"autopower", "y", NVOFF_AUTOPOWER, NVLEN_AUTOPOWER},
#endif
#ifdef NVOFF_MONITOR
    {"monitor", "", NVOFF_MONITOR, NVLEN_MONITOR},
#endif
#ifdef NVOFF_REBOUND
    {"rebound", REBOUND_DEFAULT, NVOFF_REBOUND, NVLEN_REBOUND},
#endif
#ifdef NVOFF_NOGUI
    {"nogui", "", NVOFF_NOGUI, NVLEN_NOGUI},
#endif
    {"netaddr", netaddr_default, NVOFF_NETADDR, NVLEN_NETADDR},
#if IP22
    {"eaddr", "ff:ff:ff:ff:ff:ff", NVOFF_ENET, NVLEN_ENET},
#endif
#if IP20 || IP26 || IP28 || IP30
    {"eaddr", "", NVOFF_ENET, NVLEN_ENET},
#endif
    {"passwd_key", "", NVOFF_PASSWD_KEY, NVLEN_PASSWD_KEY},
#ifdef NVOFF_NETPASSWD_KEY
    {"netpasswd_key", "", NVOFF_NETPASSWD_KEY, NVLEN_NETPASSWD_KEY},
#endif
#if EVEREST
    {"piggyback_reads",	"", NVOFF_PIGGYBACK, NVLEN_PIGGYBACK},
    {"splockmeteron", "", NVOFF_SPLOCK, NVLEN_SPLOCK},
#endif
#if IP26 || IP28 || IP30
    {"boottune", "1", NVOFF_BOOTTUNE, NVLEN_BOOTTUNE},
#endif
#ifdef NVOFF_UNCACHEDPROM
    {"uncachedprom","",NVOFF_UNCACHEDPROM,NVLEN_UNCACHEDPROM},
#endif
    {0, 0, 0, 0}
};

#ifdef _NT_PROM
/* Table of alternate default nvram settings to use if running
 * little endian
 */
char *alt_defaults[] = {
#ifdef NVOFF_FWPATH
    0,						/* FWSearchPath */
#endif
    "scsi()disk(1)rdisk()partition(1)",		/* SystemPartition */
    "scsi()disk(1)rdisk()partition(1)",		/* OSLoadPartition */
    "osloader.exe",				/* OSLoader */
    "\\nt\\system\\ntoskrnl.exe",		/* OSLoadFilename */
    0,						/* OSLoadOptions */
    0,						/* AutoLoad */
    0,						/* TimeZone */
    0,						/* console */
    0,						/* diagmode */
    0,						/* diskless */
    0,						/* nogfxkbd */
    0,						/* keybd */
    0,						/* lang */
    0,						/* scsiretries */
    "6",					/* scsihostid */
    0,						/* dbaud */
    0,						/* rbaud */
    0,						/* pagecolor */
    0,						/* volume */
    0,						/* sgilogo */
#ifdef NVOFF_AUTOPOWER
    0,						/* autopower */
#endif
    0,						/* nogui */
    0,						/* netaddr */
    0,						/* eaddr */
    0,						/* passwd_key */
    0						/* netpasswd_key */
#ifdef DEBUG
    ,0						/* verbose */
    ,0						/* debug */
#endif
};

/* Given a pointer to an entry in the env table, return the correct
 * default value -- the alternate default entry if running little 
 * endian and the entry exists, otherwise the entry from the 
 * normal env table.
 */
#define default(entry)							\
    ((little_endian && alt_defaults[(entry)-env_table]) ? 		\
	alt_defaults[(entry)-env_table] : (entry)->nt_value)
#else
#define default(entry)		(entry)->nt_value
#endif

#ifndef NVOFF_NETPASSWD_KEY
/* some nvrams do not define this, so make it a odd value to keep down #ifs */
#define NVOFF_NETPASSWD_KEY	0xfedcba98
#endif

int little_endian;

/*
 * init_env -- initialize environment variables
 */
void
init_env(void)
{
	register struct nvram_entry *et;
	register char *cp;
	extern int sk_sable;

	VPRINTF(("init_env()\r\n"));
	init_str(&environ_str);

	if (! cpu_is_nvvalid()) {
#if IP20 || IP26 || IP30 || EVEREST
		if (cpu_is_oldnvram()) {
		    /* If we have the old nvram layout, try to
		     * update the nvram in a reasonable manner from the
		     * old nvram information.
		     */
		    printf("old NVRAM layout; converting to new layout.\n");
		    cpu_update_nvram();
		} else
#endif
#if EVEREST || SN0 || SABLE	/* EVEREST and SN0 are only sable environments */
		if (!sk_sable)
#endif
		{
#if IP22
		    int nvram_init = 0;

		    if (!is_fullhouse() && cpu_restart_rtc()) {
			/*  On machines with dallas nvrams, this forces
			 * the clock to start.  If cpu_restart_rtc() returns
			 * true we have a factory fresh part, and we need
			 * to skip this and set the eaddr to the default of
			 * "ff:...", which makes it compatable with other
			 * systems.
			 */
			cpu_nvram_init();
			nvram_init = 1;
		    }
#endif
		    /* We're not upgrading, but somehow got the
		     * nvram trashed.
		     */
		    printf("\nNVRAM checksum is incorrect: reinitializing.\n");

		    for (et = env_table; et->nt_nvlen; et++) {
			if (et->nt_nvaddr == NVOFF_ENET
#if IP22
			&& (!nvram_init)
#endif
			   ) {
			    char *enet;
			    cp=cpu_get_nvram_offset(et->nt_nvaddr,et->nt_nvlen);
			    enet = htoe((unsigned char *)cp);
			    (void)new_str2(et->nt_name, enet, &environ_str);
		    	}
#if IP22
			/* XXX IP24 systems tend to get nvram trashed often 
			 * XXX currently, so take a risk and save the netaddr
			 * XXX to ease the pain.  This is needs to be removed
			 * XXX when the hardware is fixed.
			 */
 			else if (et->nt_nvaddr == NVOFF_NETADDR &&
				 !is_fullhouse() && !nvram_init) {
 			    struct in_addr in;
 			    char *inet;
 			    cp=cpu_get_nvram_offset(et->nt_nvaddr,et->nt_nvlen);
 			    bcopy (cp, &in, sizeof (in));
 			    inet = (char *)inet_ntoa(in);
			    (void)new_str2(et->nt_name, inet, &environ_str);
 			}
#endif
			else if (et->nt_nvaddr == NVOFF_PASSWD_KEY ||
				 et->nt_nvaddr == NVOFF_NETPASSWD_KEY) {
				/*  Reset passwds to no passwd since
				 * syssetenv will reject these.
				 */
				cpu_set_nvram_offset(et->nt_nvaddr,
						     et->nt_nvlen,
						     default(et));
			}
			else {
			    VPRINTF(("init_env: syssetenv(%s,%s)\n",
				et->nt_name, default(et)));

			    if (syssetenv (et->nt_name, default(et)))
				printf ("Could not set NVRAM variable %s.\n",
					et->nt_name);
			}
		    }
		}

		cpu_set_nvvalid(delete_str, &environ_str);
	} else {
		initialize_envstrs();
	}
#if IP30
	/* add persistent variables to environment */
	cpu_get_nvram_persist(new_str2, &environ_str);
#endif

	environ = environ_str.strptrs;

	/* set ConsoleIn/Out from console
	 */
	init_consenv(0);

	/*
	 * determine cpu frequency
	 */
	if (cp = cpu_get_freq_str(cpuid()))
		syssetenv("cpufreq", cp);

#ifdef SN0
        change_probeallscsi() ;
	cpu_fix_dbgtty() ;
#endif
}

void
initialize_envstrs(void)
{
	register struct nvram_entry *et;
	register char *cp;

	for (et = env_table; et->nt_nvlen; et++) {
		if (et->nt_nvlen  &&
		    *(cp=cpu_get_nvram_offset(et->nt_nvaddr, et->nt_nvlen))) {
			/* for backwards compatibility, check that
			 * newly added nvram variables have reasonable
			 * values.
			 */
			if (et->nt_nvaddr == NVOFF_NOKBD &&
				*cp != '1' && *cp != '0' ) {
				cpu_set_nvram_offset(et->nt_nvaddr,
						et->nt_nvlen,
						default(et));
				continue;
			}
			if ((et->nt_nvaddr == NVOFF_PASSWD_KEY ||
			     et->nt_nvaddr == NVOFF_NETPASSWD_KEY)) {
				if (illegal_passwd (cp))
					cpu_set_nvram_offset(
						et->nt_nvaddr,
						et->nt_nvlen,
						default(et));
				/* dont put passwords in env! */
				continue;
			}
			if (et->nt_nvaddr == NVOFF_VOLUME &&
				(*cp < '0' || *cp > '9') ) {
				cpu_set_nvram_offset(et->nt_nvaddr,
						et->nt_nvlen,
						default(et));
				continue;
			}

			/* convert from internal representation to
			 * ascii
			 */
			if (et->nt_nvaddr == NVOFF_AUTOLOAD) {
				(void)new_str2(et->nt_name,
				    *cp == 'Y' ? "Yes" : "No",
				    &environ_str);
				continue;
			}
			if (et->nt_nvaddr == NVOFF_ENET) {
				char *enet = htoe((unsigned char *)cp);
				(void)new_str2(et->nt_name, enet,
				    &environ_str);
				continue;
			}
#if (NVLEN_NETADDR < 20)
			if (et->nt_nvaddr == NVOFF_NETADDR) {
				struct in_addr in;
				char *inet;
				bcopy (cp, &in, sizeof (in));
				inet = (char *)inet_ntoa(in);
				(void)new_str2(et->nt_name, inet,
				    &environ_str);
				continue;
			}
#endif
#if (NVLEN_RBAUD == 1)
			if (et->nt_nvaddr == NVOFF_RBAUD) {
				char buf[8];
				if (*cp) {
					sprintf(buf,"%d",
						(uint)(*cp)*1200);
					(void)new_str2(et->nt_name, buf,
						 &environ_str);
					continue;
				}
			}
#endif
#if IP22 || IP26 || IP28 || IP30
			/*  If the password jumper is pulled out, set the
			 * console to g (graphics), and try to warn the
			 * user.
			 */
			if (et->nt_nvaddr == NVOFF_CONSOLE && jumper_off()) {
				new_str2("ConsoleWarning",
					 "PASSWD JUMPER MISSING--forced to g.",
					 &environ_str);
				new_str2(et->nt_name, "g", &environ_str);
				continue;
			}
#endif

			/* default */
			if (new_str2(et->nt_name, cp, &environ_str))
				break; /* horrible failure message ? */
		}
	}
#if IP22
	/* add persistent variables to environment */
	cpu_get_nvram_persist(new_str2, &environ_str);
#endif
}

/*
 * Set nv ram variable s1 to s2
 *
 */
int
setenv_nvram(char *s1, char *s2)
{
	register struct nvram_entry *et;
	extern int setenv_override;

	/*
	 * search env_table to see if this is a non-volatile parameter,
	 * if so, update nvram
	 */
	for (et = env_table; et->nt_nvlen; et++) {
		if (!strcasecmp(et->nt_name, s1)) {
			if (et->nt_nvlen && et->nt_nvaddr != NVOFF_PASSWD_KEY &&
			    et->nt_nvaddr != NVOFF_NETPASSWD_KEY) {

			    /* some of the nvram variables need special
			     * processing to convert from ascii to storage
			     * representation
			     */
			    if (et->nt_nvaddr == NVOFF_AUTOLOAD) {
				/* this just keys on 'Y' or 'N' as the first
				 * character
				 */
				if (islower(*s2))
				    *s2 = _toupper(*s2);
				return cpu_set_nvram_offset(et->nt_nvaddr,et->nt_nvlen,s2);
			    } else if (et->nt_nvaddr == NVOFF_ENET) {
				unsigned char *cp = etoh(s2);

				if (cp == NULL)
				    return EINVAL;
#if IP20 || IP22 || IP26 || IP28 || IP30
				if (nvram_is_protected())
					return EACCES;
#endif

				if (cpu_set_nvram_offset(NVOFF_ENET, NVLEN_ENET, (char *)cp))
				    return EIO;

#if EVEREST || SN0
				{
					char *_cp;

					_cp = cpu_get_nvram_offset(NVOFF_ENET,
								   NVLEN_ENET);
					if (bcmp((void *)cp, (void *)_cp,
						 NVLEN_ENET) != 0)
						return EACCES;
				}
#endif
				return ESUCCESS;
				
#if (NVLEN_NETADDR < 20)
			    } else if (et->nt_nvaddr == NVOFF_NETADDR) {
				struct in_addr addr;
				addr =  inet_addr(s2);
				return cpu_set_nvram_offset( NVOFF_NETADDR,
					NVLEN_NETADDR, (char*)&addr.s_addr);
#endif
#if (NVLEN_RBAUD == 1)
			    } else if (et->nt_nvaddr == NVOFF_RBAUD) {
				char *s3 = check_serial_baud_rate(s2);
				int val= atoi(s3);
				char cval;
				if (strlen(s2) == 0)
					return cpu_set_nvram_offset(et->nt_nvaddr,et->nt_nvlen,s2);
				else if (strcmp(s2, s3)) {
					printf("Invalid baud rate\n");
					new_str2(et->nt_name, s3, &environ_str);
					return EINVAL;
				}
				if (val) val /= 1200;
				cval = (char)val;
				return cpu_set_nvram_offset(NVOFF_RBAUD,
					NVLEN_RBAUD, &cval);
#endif

			    } else if (et->nt_nvaddr == NVOFF_LBAUD) {
				char *s3 = check_serial_baud_rate(s2);
				if (strcmp(s2, s3)) {
					printf("Invalid baud rate, setting dbaud to %s\n",
					  s3);
					new_str2(et->nt_name, s3, &environ_str);
				}
				return cpu_set_nvram_offset(NVOFF_LBAUD,
					NVLEN_LBAUD, s3);

#if defined(NVOFF_AUTOPOWER)
			    } else if (et->nt_nvaddr == NVOFF_AUTOPOWER) {
				int rc = cpu_set_nvram_offset(NVOFF_AUTOPOWER,
						NVLEN_AUTOPOWER, s2);
				/* Re-init power switch setting imediately.
				 * We cannot do it in the "init" path, since
				 * this will break "wakeupat".
				 */
#if IP22 || IP26 || IP28
				ip22_power_switch_on();
#elif IP30
				ip30_power_switch_on();
#endif
				return(rc);
#endif
#ifdef NVOFF_PDSVAR
			    } else if (et->nt_nvaddr == NVOFF_PDSVAR) {
				cpu_set_nvram_persist(s1, s2);
				return 0;
#endif
#ifdef SN0
			    } else if (et->nt_nvaddr == NVOFF_CONSOLE_PATH) {
			        /* If ConsolePath is changed save that value */
				cpu_set_sav_cons_path(s2) ;
				return cpu_set_nvram_offset(et->nt_nvaddr,et->nt_nvlen,s2);
#endif
			    } else
				return cpu_set_nvram_offset(et->nt_nvaddr,et->nt_nvlen,s2);
			}
			break;
		}
	}

#if IP22 || IP30
	/* if we got down here, the var's not in fixed NVRAM */
	if (et->nt_nvlen == 0 && setenv_override == 2) 
		cpu_set_nvram_persist(s1, s2);
#endif
	return 0;
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
#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0'):(('A'<=(c))&&((c)<='F'))? \
			((c)-'A'+10)  : ((c)-'a'+10))


unsigned char *
etoh (char *enet)
{
    static unsigned char dig[6], *cp;
    int i;

    for ( i = 0, cp = (unsigned char *)enet; *cp; ) {
	if ( *cp == ':' ) {
	    cp++;
	    continue;
	} else if ( !isxdigit(*cp) || !isxdigit(*(cp+1)) ) {
	    return NULL;
	} else {
	    if ( i >= 6 )
		return NULL;
	    dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
	    cp += 2;
	}
    }
    
    return i == 6 ? dig : NULL;
}


/*
 * resetenv -- reset environment variables.
 */
int
resetenv(void)
{
	register struct nvram_entry *et;

	/*
	 * re-init the env stringlist
	 */
	init_str(&environ_str);
#ifdef IP30
	{
	    void cpu_resetenv(void);

	    cpu_resetenv();
	}
#endif
	for (et = env_table; et->nt_nvlen; et++) {

		if (et->nt_nvaddr == NVOFF_PASSWD_KEY) /* don't reset passwds */
			continue;
		if (et->nt_nvaddr == NVOFF_NETPASSWD_KEY)
			continue;
		if (et->nt_nvaddr == NVOFF_ENET) {
			char *cp = cpu_get_nvram_offset(NVOFF_ENET,NVLEN_ENET);
			char *enet = htoe((unsigned char *)cp);

			(void)new_str2(et->nt_name, enet, &environ_str);
			continue;
		}
		
		if (syssetenv (et->nt_name, default(et)))
			printf ("Could not reset NVRAM variable %s.\n",
				et->nt_name);
	}

	/* re-initialize boot variables that get overridden with "" */
	_init_bootenv();

	/* insure NVRAM is valid after changes */
	cpu_set_nvvalid(delete_str, &environ_str);

	return 0;
}

/*
 * get_nvram_tab - copy the nvram table to a given address
 *
 *	this is used by the kernel to copy the nvram structure from the prom
 * Returns the # of bytes NOT copied, so the kernel, etc. can detect
 * if the table has increased in size.
 */

int
get_nvram_tab (char *addr, int size)
{
    int totlen = size < sizeof(env_table) ? size : sizeof(env_table);
    bcopy (env_table, addr, totlen);
    return sizeof(env_table) - totlen;
}
#endif /* !IP32 || !IP30_RPROM */
