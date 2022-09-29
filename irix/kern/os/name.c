/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.43 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/invent.h>
#include <sys/pda.h>
#include <sys/syssgi.h>
#include <sys/systeminfo.h>
#include <sys/systm.h>
#include <sys/utsname.h>
#include <sys/capability.h>
#include <string.h>
#include <sys/sat.h>

#include <ksys/vhost.h>

char	hostname[_SYS_NMLN];
short	hostnamelen;
char	domainname[_SYS_NMLN];
short	domainnamelen;
extern int 	use_old_serialnum;	/* mtune variable so people
					 * can migrate their licensed
					 * software slowly.  Shouldn't last
					 * past 5.1 or 5.2...
					 */

static	char	hw_provider[] = "sgi";
static	char	hw_architecture[] = "mips";

struct	utsname	utsname = {
#if _MIPS_SIM == _ABI64
	"IRIX64",
#else
	"IRIX",
#endif
	"IRIS",
	"",
	"",
	"Unknown"
};

struct unamea {
	struct utsname *cbuf;
};

static mutex_t	si_processor_lck;
struct getsetnamea {
	char	*name;
	sysarg_t len;
};

struct systeminfoa {
	sysarg_t command;
	char *buf;
	sysarg_t count;
};

int sethostname(struct getsetnamea *);

/*
 * Initialize uname info.
 * Machine-dependent code.
 */

void
inituname()
{
	strncpy(utsname.release, uname_release, _SYS_NMLN-1);
	strncpy(utsname.version, uname_version, _SYS_NMLN-1);

	if (cputype != 0) {
		strcpy(utsname.machine, "IP");
		numtos(cputype, &utsname.machine[2]);
	}
	mutex_init(&si_processor_lck, MUTEX_DEFAULT, "si_processor");
}

int
uname(struct unamea *uap, rval_t *rvp)
{
	register struct utsname *buf = uap->cbuf;

	if (copyout(utsname.sysname, buf->sysname, strlen(utsname.sysname)+1))
		return EFAULT;
	if (copyout(utsname.nodename, buf->nodename,strlen(utsname.nodename)+1))
		return EFAULT;
	if (copyout(utsname.release, buf->release, strlen(utsname.release)+1))
		return EFAULT;
	if (copyout(utsname.version, buf->version, strlen(utsname.version)+1))
		return EFAULT;
	if (copyout(utsname.machine, buf->machine, strlen(utsname.machine)+1))
		return EFAULT;

	rvp->r_val1 = 1;
	return 0;
}

/*
 * Service routine for systeminfo().
 */
static int
strout(char *str, int strcnt, char *buf, int count, long *sizep)
{
	register int getcnt;

	getcnt = (strcnt >= count)? count : strcnt+1;

	if (copyout(str, buf, getcnt))
		return EFAULT;

	if (strcnt >= count && subyte(buf+count-1, 0) < 0)
		return EFAULT;

	if (sizep)
		*sizep = strcnt + 1;
	return 0;
}

static long setnodename(char *buf, size_t len);


int get_host_id(char *ident_string, int *ident_number, char *serial_str) {
	register __uint32_t id_number;
	int error;
		/*
		 * SYSTEM ID [from IRIX 4.0]
		 * determined in a 'machine dependant' way.
		 *
		 * We need to change this to use an ID prom.
	         */
	        if (error = getsysid(ident_string))
			return error;

		if (serial_str)
			strncpy(serial_str, ident_string, MAXSYSIDSIZE);
#ifndef SN0
#ifdef EVEREST
		if (use_old_serialnum == 0) {
			int i;
			id_number = 0;
			for (i = 0;
			     i < MAXSYSIDSIZE && ident_string[i] != '\0'; i++) {
				id_number <<= 4;
				id_number |= (ident_string[i] & 0xf);
			}
		} else {
#endif
		id_number = *(__uint32_t *)ident_string;
		id_number += ident_string[4] << 8;
		id_number += ident_string[6] << 12;
		id_number += ident_string[8] << 16;
		id_number += ident_string[10] << 20;
#ifdef EVEREST
		}
#endif

		numtos(id_number, ident_string);
#else /*ndef SN0 */

		{
			int i;
			char 	mybuf[32];
			char	*cp, *rp;

			if (!private.p_sn00) {
				/* for SN0 we convert the string to an int */
				for (i = 0;
				     i < MAXSYSIDSIZE && ident_string[i] != '\0';
				     i++) {
					id_number <<= 4;
					id_number |= (ident_string[i] & 0xf);
				}
			}
			else {
				/* for SN00 the int is stored in the string */
				id_number = *(__uint32_t*)ident_string;
			}

			for (cp = mybuf; id_number; id_number /=10)
				*cp++ = "0123456789F"[id_number % 10];
			
			for (--cp, rp = ident_string; cp >= mybuf; )
				*rp++ = *cp--;
			*rp = '\0';
		}
	     
#endif 
		if (ident_number)
			*ident_number = id_number;

		return error;
}

int
systeminfo(register struct systeminfoa *uap, rval_t *rvp)
{
	char	*name;
	/* Make sure this has word alignment */
	union {
		__int32_t	lident[MAXSYSIDSIZE / sizeof(__int32_t)];
		char		cident[MAXSYSIDSIZE];
#define hostident	hostid.cident
	} hostid;
	char	*ptr = hostident;
	int	error, i;
	register __uint32_t sum;
	long rval1;

	switch (uap->command) {

	case SI_SYSNAME:
		name = utsname.sysname;
		break;

	case SI_HOSTNAME:
		name = utsname.nodename;
		break;

	case SI_RELEASE:
		name = utsname.release;
		break;

	case SI_VERSION:
		name = utsname.version;
		break;

	case SI_MACHINE:
		name = utsname.machine;
		break;

	case SI_ARCHITECTURE:
		name = hw_architecture;
		break;

	case SI_HW_SERIAL:
	case _MIPS_SI_SERIAL:
	        if (error = get_host_id(hostident,NULL,NULL))
			return error;

		name = hostident;
		break;

	case SI_HW_PROVIDER:
		name = hw_provider;
		break;

	case SI_INITTAB_NAME:
		/*
		 * Temporary kludge to return /etc/inittab untill
		 * we figure out how to pass alternate initfiles
		 * to /etc/init
		 */
		name = "/etc/inittab";
		break;

	case SI_SET_HOSTNAME: {
		/*
		 * Map to sethostname.
		 */
		struct getsetnamea args;
		args.name = uap->buf;
		args.len  = sizeof(utsname.sysname) - 1;
		if (sethostname(&args) == 0)
			rvp->r_val1 = args.len;
		else
			rvp->r_val1 = 0;
		return 0;
	}

	case _MIPS_SI_VENDOR:
	case _MIPS_SI_OS_PROVIDER:
		name = "Silicon Graphics, Inc.";
		break;
	case _MIPS_SI_OS_NAME:
	        name = utsname.sysname;
		break;
	case _MIPS_SI_HW_NAME:
	        name = utsname.machine;
		break;
	case _MIPS_SI_NUM_PROCESSORS: {
		sprintf(ptr, "%d", numcpus);
		name = ptr;
		break;
	}
	case _MIPS_SI_HOSTID: {
		extern __int32_t hostid;

		/* for 64 bit systems, this makes sures that the hostid isn't */
		/* sign extended. */
		unsigned long l = (unsigned long) (unsigned int) hostid;

		sprintf(ptr, "%lx", l);
		name = ptr;
		break;
	}
	case _MIPS_SI_OSREL_MAJ: {
		char *rel_ptr = utsname.release;

		name = ptr;
		while (*rel_ptr && *rel_ptr != '.') {
			*ptr++ = *rel_ptr++;
		}
		*ptr = '\0';
		break;
	}
	case _MIPS_SI_OSREL_MIN: {
		char *rel_ptr = utsname.release;

		while (*rel_ptr && *rel_ptr != '.') {
			rel_ptr++;
		}
		if (! *rel_ptr) {
			name = "0";
			break;
		}
		rel_ptr++;
		name = ptr;
		while (*rel_ptr) {
			*ptr++ = *rel_ptr++;
		}
		*ptr = '\0';
		break;
	}
	case _MIPS_SI_OSREL_PATCH:
		name = "0";
		break;
	case _MIPS_SI_PROCESSORS: {
		inventory_t *curr = 0;	/* current item */
		invplace_t iplace = INVPLACE_NONE;
		int n = 0;		/* number of processors */
		int major, minor, ptrlen;
		char *cp, *m_ptr = 0, *m_base;
		static char *si_processor_save;
		extern char *cpu_rev_find(int, int*, int*);

		mutex_lock(&si_processor_lck, PZERO);
		if (si_processor_save) {
			name = si_processor_save;
			mutex_unlock(&si_processor_lck);
			break;
		}
		m_base = m_ptr = kern_malloc(maxcpus * 16);
		*m_ptr = '\0';
		while ((curr = get_next_inventory(&iplace)) != NULL) {
			if (curr->inv_class == INV_PROCESSOR &&
			    curr->inv_type == INV_CPUCHIP) {
				cp = cpu_rev_find(curr->inv_state,
						&major, &minor);
				if (n++ == 0) { /* 1st chip */
					sprintf(ptr, "%s %d.%d",
						cp, major, minor);
					ptrlen = strlen(ptr);
				} else { /* 2-N chips */
					sprintf(m_ptr, "%s %d.%d, ", 
						cp, major, minor);
					m_ptr += strlen(m_ptr);
				}
			}
		}
		if (numcpus > 1 && n == 1) {
			/* multiple same cpus */
			for (n = numcpus; n > 0; n--, m_ptr += ptrlen + 2) {
				sprintf(m_ptr, "%s, ", ptr);
			}
			m_ptr -= 2;
			*m_ptr = '\0';
		} else { /* only 1 cpu or multiple different CPUs */
			strcpy(m_ptr, ptr);
			m_ptr += ptrlen;
		}
		si_processor_save = kern_malloc(m_ptr - m_base + 1);
		strcpy(si_processor_save, m_base);
		kern_free(m_base);
		name = si_processor_save;
		mutex_unlock(&si_processor_lck);
		break;
	}

	case _MIPS_SI_AVAIL_PROCESSORS: {
		sum = 0;
		for (i=0; i < maxcpus; i++) {
			if ((pdaindr[i].CpuId != -1) &&
				(pdaindr[i].pda->p_flags & PDAF_ENABLED))
				sum++;
		}
		sprintf(ptr, "%d", sum);
		name = ptr;
		break;
	}

	default:
		return EINVAL;
	}

	error = strout(name, strlen(name), uap->buf, uap->count, &rval1);
	rvp->r_val1 = rval1;
	return(error);
}

/*
 * Set utsname.nodename.  Return length of the new nodename.
 */
static long
setnodename(register char *buf, register size_t len)
{
	char	*hp, *np;

	bcopy(buf, hostname, len);
	hostname[len] = '\0';
	hostnamelen = len;

	/*
	 * Copy the hostname into the utsname structure sans the domainname.
	 */
	hp = hostname;
	np = utsname.nodename;
	while (*hp && *hp !=  '.')
		*np++ = *hp++;
	*np++ = '\0';
	return (np - utsname.nodename);
}

/*
 * These system calls were moved here from bsd/sockets/hostname.c
 * so that all name getting/setting happens in one place. They have
 * been altered to use the above routines.
 */

int
gethostname(struct getsetnamea *uap)
{
	char	vhostname[_SYS_NMLN];
	size_t	vhostnamelen = _SYS_NMLN;
	size_t	len;

	if ((len = uap->len) > SYS_NMLN - 1)
		len = SYS_NMLN - 1;
	VHOST_GETHOSTNAME(vhostname, &vhostnamelen);
	vhostname[vhostnamelen] = '\0';
	return strout(vhostname, vhostnamelen, uap->name, len, NULL);
}

int
sethostname(struct getsetnamea *uap)
{
	char	vhostname[_SYS_NMLN];
	size_t	vhostnamelen = (size_t)uap->len;
	int	error;

	if (!_CAP_ABLE(CAP_SYSINFO_MGT))
		error = EPERM;
	else if (vhostnamelen > SYS_NMLN - 1)
		error = EINVAL;
	else if (copyin(uap->name, vhostname, vhostnamelen) != 0)
		error = EFAULT;
	else {
		error = 0;
		vhostname[vhostnamelen] = '\0';
		VHOST_SETHOSTNAME(vhostname, &vhostnamelen);
		uap->len = vhostnamelen;
	}
	_SAT_HOSTNAME_SET(vhostname, error);
	return error;
}

int
getdomainname(struct getsetnamea *uap)
{
	char	vdomainname[_SYS_NMLN];
	size_t	vdomainnamelen = _SYS_NMLN;
	size_t len;

	if ((len = uap->len) > SYS_NMLN - 1)
		len = SYS_NMLN - 1;
	VHOST_GETDOMAINNAME(vdomainname, &vdomainnamelen);
	vdomainname[vdomainnamelen] = '\0';
	return strout(vdomainname, vdomainnamelen, uap->name, len, NULL);
}

int
setdomainname(struct getsetnamea *uap)
{
	char	vdomainname[_SYS_NMLN];
	size_t	vdomainnamelen = (size_t)uap->len;

	if (!_CAP_ABLE(CAP_SYSINFO_MGT))
		return EPERM;
	if (vdomainnamelen > SYS_NMLN - 1) {
		_SAT_DOMAINNAME_SET(NULL, EINVAL);
		return EINVAL;
	}
	if (copyin(uap->name, vdomainname, vdomainnamelen)) {
		_SAT_DOMAINNAME_SET(NULL, EFAULT);
		return EFAULT;
	}
	VHOST_SETDOMAINNAME(vdomainname, vdomainnamelen);
	_SAT_DOMAINNAME_SET(vdomainname, 0);
	return 0;
}


/*
 * Physical host operations:
 */
/* ARGSUSED */
void
phost_gethostname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	ASSERT(*len >= (size_t) hostnamelen);
	bcopy(hostname, name, hostnamelen);
	*len = hostnamelen;
}

/* ARGSUSED */
void
phost_sethostname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	/* Set hostname and return length of nodename */
	ASSERT(*len < SYS_NMLN);
	*len = setnodename(name, *len);
}

/* ARGSUSED */
void
phost_getdomainname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		*len)
{
	ASSERT(*len >= (size_t) hostnamelen);
	bcopy(domainname, name, domainnamelen);
	*len = domainnamelen;
}

/* ARGSUSED */
void
phost_setdomainname(
	bhv_desc_t	*bdp,
	char		*name,
	size_t		len)
{
	ASSERT(len < SYS_NMLN);
	bcopy(name, domainname, len);
	domainnamelen = len;
	domainname[len] = 0;
}
