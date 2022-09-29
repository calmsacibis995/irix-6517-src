/***********************************************************************\
*	File:		ip27log.c					*
*									*
*	NOTE: Flash PROM cannot be changed while running out of it.	*
*									*
*	This module provides easy-to-use environment variable and	*
*	logging functionality.  It is an additional layer on top	*
*	of promlog and fprom.						*
*									*
*	NOTE: Please keep in sync with kernel version in		*
*			irix/kern/ml/SN/SN0/ip27log.c			*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/SN/agent.h>

#ifdef _STANDALONE
#include <stdio.h>
#include <stdarg.h>
#include <libkl.h>
#include <hub.h>
#else
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#endif

#include <sys/SN/arch.h>
#include <sys/SN/fprom.h>
#include <sys/SN/SN0/ip27log.h>

#ifndef _PROM_EMULATOR
/*
 * We must not have multiple CPUs trying to write to an FPROM
 * simultaneously (in standalone, it's only locked out against
 * the other CPU on the same Hub, which is better than nothing).
 */

#if _STANDALONE

#define LOCK_DEFS
#define LOCK		hub_lock_timeout(HUB_LOCK_FPROM, 2000000)
#define UNLOCK		hub_unlock(HUB_LOCK_FPROM)

#else /* _STANDALONE */

#define LOCK_DEFS	int s
#define LOCK		if (!in_panic_mode()) \
			    s = mutex_spinlock(&nodepda->fprom_lock)
#define UNLOCK		if (!in_panic_mode()) \
			    mutex_spinunlock(&nodepda->fprom_lock, s)

#endif /* _STANDALONE */

/*
 * IP27prom-specific Prom Log routines
 */

int ip27log_setup(promlog_t *l, nasid_t nasid, int flags)
{
    int			r;
    int			manu_code, dev_code;
    fprom_t		f;

    if (flags & IP27LOG_MY_FPROM_T)
	f = l->f;
    else {
	f.base		= (void *) (nasid < 0 || nasid == get_nasid() ?
				    LBOOT_BASE : NODE_RBOOT_BASE(nasid));
	f.dev		= FPROM_DEV_HUB;
	f.afn		= 0;
	f.aparm[0]	= (rtc_time_t) 0;
    }

    fprom_reset(&f);

    if ((flags & (IP27LOG_FOR_WRITING | IP27LOG_DO_INIT | IP27LOG_DO_CLEAR)) &&
	(r = fprom_probe(&f, &manu_code, &dev_code)) < 0) {
	if (flags & IP27LOG_VERBOSE)
	    printf("Flash PROM not writable: %s\n", fprom_errmsg(r));
	return r;
    }

#ifdef _STANDALONE
    if ((r = promlog_init(l, &f, 14, hub_cpu_get())) < 0) {
#else
    /* Use private.p_slice instead of cputoslice since
     * we may not always be guaranteed shared memory.
     * Moreover it is much more efficient.
     */
    if ((r = promlog_init(l, &f, 14, private.p_slice)) < 0) {
#endif
	if (r != PROMLOG_ERROR_MAGIC) {
	    if (flags & IP27LOG_VERBOSE)
		printf("Cannot access PROM log: %s\n", promlog_errmsg(r));
	    return r;
	} else if ((flags & IP27LOG_DO_INIT) == 0) {
	    if (flags & IP27LOG_VERBOSE)
		printf("PROM log appears uninitialized "
		       "(see initlog command)\n");
	    return r;
	}
    }

    if ((flags & (IP27LOG_DO_INIT | IP27LOG_DO_CLEAR)) && 
		(r = promlog_clear(l, (flags & IP27LOG_DO_INIT))) < 0) {
	if (flags & IP27LOG_VERBOSE)
	    printf("Cannot clear PROM log: %s\n", promlog_errmsg(r));
	return r;
    }

    return 0;
}

int ip27log_setenv(nasid_t nasid, char *key, char *value, int flags)
{
    promlog_t		l;
    int			r;
    LOCK_DEFS;

    LOCK;

    if ((r = ip27log_setup(&l, nasid, flags | IP27LOG_FOR_WRITING)) < 0)
	goto done;

    /*
     * Undocumented feature to allow making log entries.
     */

    if (strcmp(key, "Fatal") == 0 ||
	strcmp(key, "Error") == 0 ||
	strcmp(key, "Info") == 0)
	r = promlog_put_log(&l, key, value);
    else
	r = promlog_put_var(&l, key, value);

    if (r < 0) {
	if (flags & IP27LOG_VERBOSE)
	    printf("Could not setenv %s to \"%s\": %s\n",
		   key, value, promlog_errmsg(r));
	goto done;
    }

    r = 0;

done:

    UNLOCK;

    return r;
}

int ip27log_getenv(nasid_t nasid,
		   char *key, char *value, char *defl, int flags)
{
    promlog_t		l;
    int			r;

    if ((r = ip27log_setup(&l, nasid, flags)) < 0)
	goto fail;

    if ((r = promlog_lookup(&l, key, value, 0)) < 0) {
	if (defl == 0 && (flags & IP27LOG_VERBOSE))
	    printf("Could not getenv %s: %s\n",
		   key, promlog_errmsg(r));
	goto fail;
    }

    return 0;

 fail:

    if (value) {
	if (defl) {
	    strcpy(value, defl);
	    r = 0;
	} else
	    value[0] = 0;
    }

    return r;
}

int ip27log_unsetenv(nasid_t nasid, char *key, int flags)
{
    promlog_t		l;
    int			r;

    if ((r = ip27log_setup(&l, nasid, flags | IP27LOG_FOR_WRITING)) < 0)
	return r;

    if ((r = promlog_first(&l, PROMLOG_TYPE_VAR)) < 0 ||
	(r = promlog_find(&l, key, PROMLOG_TYPE_VAR)) < 0) {
	if (flags & IP27LOG_VERBOSE)
	    printf("Could not unsetenv %s: variable not set\n", key);
	return r;
    }

    if ((r = promlog_delete(&l)) < 0) {
	if (flags & IP27LOG_VERBOSE)
	    printf("Could not delete %s: %s\n",
		   key, promlog_errmsg(r));
	return r;
    }

    return 0;
}

/*
 * ip27log_printf
 *
 *   Writes an error log entry.  May not be used in DEX mode.
 *   Should be used sparingly.
 */

#ifdef _STANDALONE
int
ip27log_iprintf(nasid_t nasid, int severity_flag, const char *fmt, va_list ap)
#else
int
ip27log_iprintf(nasid_t nasid, int severity_flag, char *fmt, va_list ap)
#endif
{
    promlog_t		l;
    int			r, severity;
    char	       key[32], buf[PROMLOG_VALUE_MAX * 2], *t;
    LOCK_DEFS;

    vsprintf(buf, fmt, ap);
    buf[PROMLOG_VALUE_MAX - 2] = 0;	/* Truncate if too long */

    for (t = buf; *t; t++)
	;

    if (t > buf && t[-1] != '\n') {
	t[0] = '\n';
	t[1] = 0;
    }

    key[0] = 0;

    if (severity_flag & IP27LOG_FLAG_DUP)
        strcpy(key, IP27LOG_DUP_DLM);

    severity = (severity_flag & IP27LOG_SEVERITY_MASK) >> IP27LOG_SEVERITY_SHFT;

    switch (severity) {
    case IP27LOG_INFO:
    default:
        strcpy(key + strlen(key), IP27LOG_INFO_KEY);
	break;
    case IP27LOG_ERROR:
        strcpy(key + strlen(key), IP27LOG_ERROR_KEY);
	break;
    case IP27LOG_FATAL:
        strcpy(key + strlen(key), IP27LOG_FATAL_KEY);
	break;
    }

    LOCK;

    if ((r = ip27log_setup(&l, nasid, IP27LOG_FOR_WRITING)) < 0 ||
	(r = promlog_put_log(&l, key, buf)) < 0)
	printf("ip27log: write failed: %s\n", promlog_errmsg(r));

    UNLOCK;

    return r;
}

/*VARARGS1*/
#ifdef _STANDALONE
void
ip27log_printf(int severity, const char *fmt, ...)
#else
void
ip27log_printf(int severity, char *fmt, ...)
#endif
{
	va_list ap;

	nasid_t nasid = get_nasid();
	va_start(ap, fmt);
	ip27log_iprintf(nasid, severity, fmt, ap);
	va_end(ap);
}

/*VARARGS1*/
#ifdef _STANDALONE
void
ip27log_printf_nasid(nasid_t nasid, int severity, const char *fmt, ...)
#else
void
ip27log_printf_nasid(nasid_t nasid, int severity, char *fmt, ...)
#endif
{
	va_list ap;

	va_start(ap, fmt);
	ip27log_iprintf(nasid, severity, fmt, ap);
	va_end(ap);
}

#ifndef _STANDALONE
static int ip27log_panic_first = 1;

void
ip27log_panic_printf(int severity, char *fmt, va_list ap)
{
	char		hostident[MAXSYSIDSIZE];

	/*
	 * Check if we are already in promlogging mode
	 * if so we are trying to do a nested printf
	 * in which we case we should not do anything
	 */

	if (in_promlog_mode())
		return;

	/*
	 * Remember that this is the first time we are
	 * doing the prom logging during a particular
	 * printf. This prevents looping in case of
	 * nested printfs due to a prom error lower down.
	 */

	enter_promlog_mode();

	if (ip27log_panic_first)
		getsysid(hostident);

	if (ip27log_panic_first)
		ip27log_printf_nasid(COMPACT_TO_NASID_NODEID(cnodeid()),
			severity, "Serial #: %s\n", hostident);

	ip27log_iprintf(COMPACT_TO_NASID_NODEID(cnodeid()), severity, 
		fmt, ap);

	ip27log_panic_first = 0;

	/*
	 * We are done with promlogging the current string.
         * It is safe to exit the promlog mode.
         */

	exit_promlog_mode();
}
#endif

#else /* _PROM_EMULATOR */
int     ip27log_setup(promlog_t *l, nasid_t nasid, int flags)
{
	return 0;
}

int     ip27log_setenv(nasid_t nasid,
                       char *key, char *value, int flags)
{
	return 0;
}

int     ip27log_getenv(nasid_t nasid,
                       char *key, char *value, char *defl, int flags)
{
	if (!strcmp(key, IP27LOG_MODULE_KEY)) {
		if (value)
			sprintf(value, "1");
		return 0;
	}
	else if (!strcmp(key, IP27LOG_PARTITION)) {
		if (value)
			sprintf(value, "0");
		return 0;
	}
	else if (!strcmp(key, DISABLE_MEM_MASK)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, DISABLE_MEM_SIZE)) {
		if (value)
			sprintf(value, "00000000");
		return -1;
	}
	else if (!strcmp(key, DISABLE_MEM_REASON)) {
		if (value)
			sprintf(value, "00000000");
		return -1;
	}
	else if (!strcmp(key, SWAP_BANK)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, FORCE_CONSOLE)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, DISABLE_CPU_A)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, DISABLE_CPU_B)) {
		if (value)
			*value = 0;
		return -1;
	}
/*
	else if (!strcmp(key, DISABLE_NODE_BRD)) {
		if (value)
			*value = 0;
		return -1;
	}
*/
	else if (!strcmp(key, CPU_LOG_SN)) {
		if (value)
			strcpy(value, defl);
		return 0;
	}
	else if (!strcmp(key, MEM_LOG_SN)) {
		if (value)
			strcpy(value, defl);
		return 0;
	}
	else if (!strcmp(key, EARLY_INIT_CNT_A)) {
		if (value)
			strcpy(value, defl);
		return 0;
	}
	else if (!strcmp(key, EARLY_INIT_CNT_B)) {
		if (value)
			strcpy(value, defl);
		return 0;
	}
	else if (!strcmp(key, IP27LOG_NASID)) {
		if (value)
			sprintf(value, "%d", nasid);
		return 0;
	}
	else if (!strcmp(key, INV_CPU_MASK)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, INV_MEM_MASK)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, INV_MEM_SZ)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, PREMIUM_BANKS)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, IP27LOG_DOMAIN)) {
		if (value)
			sprintf(value, "1");
		return 0;
	}
	else if (!strcmp(key, IP27LOG_CLUSTER)) {
		if (value)
			sprintf(value, "1");
		return 0;
	}
	else if (!strcmp(key, IP27LOG_CELL)) {
		if (value)
			sprintf(value, "1");
		return 0;
	}
	else if (!strcmp(key, BACK_PLANE)) {
		if (value)
			sprintf(value, "180");
		return 0;
	}
	else if (!strcmp(key, IP27LOG_DISABLE_IO)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, IP27LOG_OVNIC)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, IP27LOG_NASIDOFFSET)) {
		if (value)
			*value = 0;
		return -1;
	}
	else if (!strcmp(key, IP27LOG_GMASTER)) {
		if (value)
			*value = 0;
		return -1;
	}
	else
		return 0;
}

int     ip27log_unsetenv(nasid_t nasid,
                         char *key, int flags)
{
	return 0;
}

#ifdef _STANDALONE
void    ip27log_printf(int severity, const char *fmt, ...)
{
	;
}
#else
/* The kernel doesn't like this const */
void    ip27log_printf(int severity, char *fmt, ...)
{
	;
}
void    ip27log_panic_printf(int severity, char *fmt, va_list ap)
{
	;
}
#endif

#ifdef _STANDALONE
void    ip27log_printf_nasid(nasid_t nasid, int severity, const char *fmt, ...)
{
	;
}
#else
/* The kernel doesn't like this const */
void    ip27log_printf_nasid(nasid_t nasid, int severity, char *fmt, ...)
{
	;
}
#endif
#endif /* _PROM_EMULATOR */
