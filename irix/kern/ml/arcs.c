/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
**************************************************************************/

/*
 * ARCS Specific Functions
 */

#ident "$Revision: 1.49 $"

#include "sys/types.h"
#include "sys/systm.h"
#include "sys/kopt.h"
#include "sys/arcs/spb.h"
#include "sys/mips_addrspace.h"
#include "sys/arcs/tvectors.h"
#include "sys/arcs/pvector.h"
#if IP30 || IP32
#include "sys/cpu.h"
#if IP30
#include "sys/pda.h"
#endif
#endif
#include "sys/cmn_err.h"
#include <string.h>

#if EVEREST || SN
#if EVEREST
#include "sys/EVEREST/evaddrmacros.h"
#if MULTIKERNEL
#include "sys/EVEREST/everest.h"
#endif
#endif	/* EVEREST */
void he_arcs_unsupported(void);
void he_arcs_restart(void);
void he_arcs_reboot(void);
#endif /* EVEREST  || SN */

#if IP22 || IP26 || IP28
#include <sys/cpu.h>
#endif

#if IP27 || IP33
# include <sys/pda.h>
#endif

#if IP22 || IP26 || IP28
void silence_bell(void);
void eisa_refresh_off(void);
void gioreset(void);
#endif

#if IP30
void silence_bell(void);
#endif

#if IP22 || IP26 || IP28 || IP30 || IP32	/* softpower systems */
extern int power_off_flag;	/* from uadmin or powerintr */
void set_autopoweron(void);
void cpu_powerdown(void);
void prom_reinit(void);
#endif

#ifdef IP32
extern void early_gbe_stop(void);    /* From IP32init.c; shuts down GBE DMA */
#endif

static int arcs_to_dsk(char *arcs, char *dsk);
extern char eaddr[];

/*
 * Some systems load the ARCS prom into memory.  This variable indicates
 * that calls through the ARCS transfer vectors are no longer permissible.
 */
int _prom_cleared = 0;

#if defined(_K64PROM32) || defined(_KN32PROM32)
#define ADDR_TO_FUNCTION(_x)	(void *)(__psint_t)(_x)
extern LONG call_prom_cached(__int32_t,__int32_t,__int32_t,__int32_t,void *);

#if defined(_KN32PROM32)
#define KPHYSTO32K0(_x)	(int)(((__psint_t)(_x) & 0x1fffffff) | (0x80000000))
#define K032TOKPHYS(_x) (((_x) & 0x1fffffff) | _ARCS_K0BASE)
#endif

#endif /*_K64PROM32 || _KN32PROM32 */

#ifdef SABLE
/* For SABLE boots, sometimes we have a prom, sometimes we do not.  Return
 * failure if SPB is not initialized.
 */
#define SPBCHECK(val) {if (SPB->Signature != SPBMAGIC) return val;}
#define SPBCHECK_PANIC(str) {if (SPB->Signature != SPBMAGIC) panic(str);}
#else
#define SPBCHECK(val)
#define SPBCHECK_PANIC(str)
#endif

LONG
arcs_write(ULONG fd, void *buf, ULONG n, ULONG *cnt)
{
	LONG rc;

#if defined(EVEREST) && defined(MULTIKERNEL) && !defined(EVMK_UNSAFE_CONSOLE)
	/* Only the golden cell can use the console, so lie to caller */

	if (evmk_cellid != evmk_golden_cellid)
		return(*cnt);
#endif
#if EVEREST || SN || IP30
	if (_prom_cleared) {
#if EVEREST || defined(SABLE)
		extern int he_arcs_write(char *, ULONG, ULONG *);

		return he_arcs_write((char *)buf, n, cnt);
#else
		extern int ioc3_console_write(char *, ULONG, ULONG *);
		int err;

		err = ioc3_console_write((char *)buf, n, cnt);
# if IP27
		/* On IP27, if there is an ELSC and we are panicing,
		 * copy the message to the ELSC uart as well.
		 */
		if (in_panic_mode() && private.p_elsc_portinfo) {
		    extern void du_down_write(void*, char*, int);
		    du_down_write(private.p_elsc_portinfo, buf, n);
		}
# endif

		return(err);
#endif
	}
#endif /* EVEREST || SN || IP30 */

	SPBCHECK(1);

#if IP32
	{
	extern int mh16550_console_write(char *buf, ULONG n, ULONG *cnt);

	rc = mh16550_console_write((char *)buf, n, cnt);
	}
#elif defined(_K64PROM32) || defined(_KN32PROM32)
	rc = call_prom_cached((__int32_t)fd,
			        (__int32_t)KPHYSTO32K0(buf),
				(__int32_t)n,
			        (__int32_t)KPHYSTO32K0(cnt),
				ADDR_TO_FUNCTION(__TV->Write));
#else
	rc = __TV->Write(fd,buf,n,cnt);
#endif /* _K64PROM32 || _KN32PROM32 */

	return rc;
}

LONG
arcs_load(CHAR *path, ULONG topaddr, ULONG *execaddr, ULONG *lowaddr)
{
	LONG rc;

	if (_prom_cleared)
		cmn_err(CE_PANIC, "Call through ARCS prom after prom cleared.");

	SPBCHECK(1);

#if defined(_K64PROM32) || defined(_KN32PROM32)
	rc = call_prom_cached((__int32_t)KPHYSTO32K0(path),
				(__int32_t)topaddr,
				(__int32_t)KPHYSTO32K0(execaddr),
				(__int32_t)KPHYSTO32K0(lowaddr),
				ADDR_TO_FUNCTION(__TV->Load));
#else
	rc = __TV->Load(path,topaddr,execaddr,lowaddr);
#endif /* _K64PROM32 || _KN32PROM32 */

	return rc;
}

void
prom_autoboot(void)
{
#ifdef IP32
	void (*restart_addr)(int);
	extern void update_flash_version(void);
#endif
#ifdef R4600SC
	extern unsigned int two_set_pcaches;
	extern unsigned int boot_sidcache_size;

	if (two_set_pcaches && boot_sidcache_size)
		_r4600sc_disable_scache();
#endif
    
#if IP22 || IP26 || IP28
	silence_bell();			/* make sure on/off bell is off */

#if IP22 || IP26			/* IP28 gioresets in all proms */
	gioreset();
#endif
#endif
#if IP30
	silence_bell();			/* make sure on/off bell is off */
#endif	/* IP30 */
		

	SPBCHECK_PANIC("sable reboot!");

#if IP22 || IP26 || IP28 || IP30 || IP32
	/* Check if we should power off (power off leads to panic that
	 * automatically reboots), call prom_reinit() to handle this case.
	 */
	if (power_off_flag) {
		prom_reinit();
		/*NOTREACHED*/
	}
#endif

#if EVEREST || SN
	he_arcs_reboot();
#elif IP32
	flash_write_env();
	ecc_disable();
	/* Make sure we shut down GBE DMA before going to PROM */
	early_gbe_stop();
	/* jump back to prom at restart_addr */
	update_flash_version();
	if (flash_version(FLASHPROM_MAJOR) >= 2)
		restart_addr = (void (*)())0xbfc00100;
	else
		restart_addr = (void (*)())0xbfc00000;	/* older proms */

	(*restart_addr)(FW_REBOOT);
#elif defined(_K64PROM32) || defined(_KN32PROM32)
	(void)call_prom_cached(0,0,0,0,ADDR_TO_FUNCTION(__TV->Reboot));
#else
	(__TV->Reboot)();
#endif /* _K64PROM32 || _KN32PROM32 */
}

void
prom_reinit(void)
{
#if IP22
	int fh;
	char *arg;
#ifdef R4600SC
	extern unsigned int two_set_pcaches;
	extern unsigned int boot_sidcache_size;

	if (two_set_pcaches && boot_sidcache_size)
		_r4600sc_disable_scache();
#endif

	if (fh = is_fullhouse())
		eisa_refresh_off();
	silence_bell();		/* make sure on/off bell is off */
	set_autopoweron();	/* do set up to power back on automaticly */
	gioreset();		/* resets FDDI if present and not yet reset */
	if(power_off_flag) {
		if (fh || (!(arg = kopt_find("prompoweroff")) ||
					(*arg != 'y' &&  *arg != 'Y')))
		    cpu_powerdown();
		    /* NOTREACHED */
		else
#if defined(_K64PROM32) || defined(_KN32PROM32)
	(void)call_prom_cached((__int32_t)0,
			        (__int32_t)0,
				(__int32_t)0,
			        (__int32_t)0,
				ADDR_TO_FUNCTION(__TV->PowerDown));
#else
		    __TV->PowerDown();	/* calling prom power off plays the */
					/* ... tune on the way down */
#endif /* _K64PROM32 || _KN32PROM32 */
	}
#endif

#if IP26 || IP28
	eisa_refresh_off();
	silence_bell();			/* make sure on/off bell is off */
	set_autopoweron();		/* do set up to power switch maint */
#ifdef IP26
	gioreset();
#endif

	if (power_off_flag)
		cpu_powerdown();
		/*NOTREACHED*/
#endif
#ifdef IP30
	if (cpuid() == master_procid) {
		silence_bell();
		/* do set up to power back on automatically */
		set_autopoweron();
		if (power_off_flag)
			cpu_soft_powerdown();
		/*NOTREACHED*/
	}
#endif

#if IP32
	void (*restart_addr)(int);
	extern void update_flash_version(void);

	flash_write_env();
	set_autopoweron();
	if (power_off_flag)
		cpu_powerdown();
	ecc_disable();			/* turn off ecc-checking for prom */
	/* Make sure we shut down GBE DMA before going to PROM */
	early_gbe_stop();
	/* jump back to prom at restart_addr */
	update_flash_version();
	if (flash_version(FLASHPROM_MAJOR) >= 2)
		restart_addr = (void (*)())0xbfc00100;
	else
		restart_addr = (void (*)())0xbfc00000;

	(*restart_addr)(FW_RESTART);
#endif	/* IP32 */

	SPBCHECK_PANIC("sable restart!");

#if EVEREST || SN
	he_arcs_restart();
#elif (defined(_K64PROM32) || defined(_KN32PROM32)) && !defined(IP32)
	(void)call_prom_cached(0,0,0,0,ADDR_TO_FUNCTION(__TV->Restart));
#elif !defined(IP32)
	(__TV->Restart());
#endif
}

/* Convert arcs variables into data understood by the kernel.
 */
void
arcs_setuproot(void)
{
	char *ospt, rootbuf[20];

	/*  Setup root variable if OSLoadPartition is set and conversion
	 * is successfull.
	 */
	if ((ospt=kopt_find("OSLoadPartition")) && arcs_to_dsk(ospt,rootbuf))
		kopt_set("root",rootbuf);
}

#if !EVEREST && !SN

CHAR
*arcs_getenv(CHAR *name)
{
	CHAR *rc;

	SPBCHECK(0);

#if defined(_K64PROM32) || defined(_KN32PROM32)
	rc = (CHAR *)call_prom_cached((__int32_t)KPHYSTO32K0(name),0,0,0,
			ADDR_TO_FUNCTION(__TV->GetEnvironmentVariable));
#else
	rc = __TV->GetEnvironmentVariable(name);
#endif

	return rc;
}

LONG
arcs_nvram_tab(char *addr, int size)
{
	LONG rc;

	SPBCHECK(0);

#if defined(_K64PROM32) || defined(_KN32PROM32)
	rc = call_prom_cached((__int32_t)KPHYSTO32K0(addr),(__int32_t)size,
				0,0,ADDR_TO_FUNCTION(__PTV->GetNvramTab));
#else
	rc = __PTV->GetNvramTab(addr, size);
#endif

	return rc;
}

void
arcs_eaddr(unsigned char *cp)
{
	bcopy(eaddr, cp, 6);
}

void
prom_eaddr(char *buf)
{
	arcs_eaddr((unsigned char *)buf);
}

#endif	/* !EVEREST && !SN */

#if IP20
COMPONENT *
arcs_getchild(COMPONENT *cmp)
{
	return(__TV->GetChild(cmp));
}

COMPONENT *
arcs_getpeer(COMPONENT *cmp)
{
	return(__TV->GetPeer(cmp));
}

/*
 * find the first component with the matching type in the configuration tree
 * with c as its root
 */
COMPONENT *
arcs_findtype(COMPONENT *c, CONFIGTYPE type)
{
	COMPONENT *c1;

	if (c == (COMPONENT *)NULL)
		return (COMPONENT *)NULL;

	if (c->Type == type)
		return c;

	if ((c1 = arcs_findtype(arcs_getchild(c), type)) != (COMPONENT *)NULL)
		return c1;

	if ((c1 = arcs_findtype(arcs_getpeer(c), type)) != (COMPONENT *)NULL)
		return c1;

	return (COMPONENT *)NULL;
}

#endif /* IP20 */


static int
arcs_to_dsk(char *arcs, char *dsk)
{
	char *tmp, *p = arcs;
	int scsi, disk, part, len;

	scsi = part = 0;
	disk = -1;

	if (!strncmp("dksc",p,4)) {
	    if (!(tmp=strchr(p,'(')) || !*(++tmp))
		return(0);	 
	    scsi =  atoi(tmp);
	    if (!(tmp=strchr(tmp,',')) || !*(++tmp))
		return(0); 
	    disk = atoi(tmp);
	    if (!(tmp=strchr(tmp,',')) || !*(++tmp))
		return(0); 
	    part = atoi(tmp);
	    if (!(tmp=strchr(p,')')))
		return 0;
	}
	else {
	    while (*p) {
		if (!(tmp=strchr(p,'(')) || !*(++tmp))
			return(0);

		len = (unsigned long)tmp - (unsigned long)p;

		if (len < 4)
			/*skip*/;
		else if (!strncmp("scsi",p,4))
			scsi = atoi(tmp);
		else if (!strncmp("disk",p,4))
			disk = atoi(tmp);
		else if (!strncmp("part",p,4))
			part = atoi(tmp);

		if (!(p=strchr(tmp,')')))
			return(0);

		p++;		/* skip ')' */
	    }
	}
	/* disk(X) must be specified */
	if (disk == -1)
		return(0);

	sprintf(dsk,"dks%dd%ds%d",scsi,disk,part);
	return(1);
}
