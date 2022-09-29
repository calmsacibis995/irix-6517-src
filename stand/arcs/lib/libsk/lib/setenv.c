#if !IP32 /* whole file */
#ident	"lib/libsk/lib/setenv.c:  $Revision: 1.10 $"

/* 
 * setenv.c - programming interface to set environment variables
 *	This version of setenv calls cpu_set_nvram to modify the nvram.
 *	The one in libsc doesn't touch the nvram.
 */

#include "stringlist.h"
#include "arcs/errno.h"
#include <libsc.h>
#include <libsk.h>

extern struct string_list environ_str;
extern int Debug, Verbose, _udpcksum;

#define SENV_READONLY 1
#define VARIABLE 2
#define SENV_LOCKNVRAM 4

static struct special {
    char *name;
    int *value;
    int flags;
} special_tab[] = {
    {"DEBUG", &Debug, VARIABLE},
    {"VERBOSE", &Verbose, VARIABLE},
    {"_udpcksum", &_udpcksum, VARIABLE},
    {"gfx", 0, SENV_READONLY},
    {"cpufreq", 0, SENV_READONLY},
    {"eaddr", 0, SENV_READONLY},
    {"NVRam=Locked", 0, SENV_LOCKNVRAM},
};

static int special_len = sizeof(special_tab)/sizeof(special_tab[0]);
int setenv_override;

/*
 * setenv - sets variable name to value.  If value is 0 or the 
 * NULL string, then setenv unsets the variable name and clears 
 * it from the environment.  Returns -1 if the variable is 
 * readonly, or -2 if setting the nvram failed.  If override is a
 * 1, then the SENV_READONLY flag is ignored (to allow the system firmware
 * to initialize SENV_READONLY variables).
 * If override is a 2, then the variable is added to "persistent" storage (ie.,
 * added to NVRAM if available).
 *
 * A special command "setenv -f NVRam=Locked *" will set the NVRAM
 * lock bit.  This is a duplicate of the mfg. cfuse function.  Note
 * there is no corresponding "unlock" command.
 */
int
_setenv (char *name, char *value, int override)
{
    int unset = (value == 0) || (*value == '\0');
    int i;
    int error;

    setenv_override = override;
    for (i = 0; i < special_len; ++i) {
	if (!strcasecmp (name, special_tab[i].name)) {
	    if (override != 1 && (special_tab[i].flags & SENV_READONLY))
		return EACCES;
	    if (special_tab[i].flags & VARIABLE)
		if (unset)
		    *special_tab[i].value = 0;
		else
		    atob(value, special_tab[i].value);
	    if (override == 1 && special_tab[i].flags & SENV_LOCKNVRAM
	    && value && *value == '*') {
		cpu_nv_lock(1);		/* we only provide the "locking" */
		return ESUCCESS;
	    }
	    break;
	}
    }


    /* Try setting the RAM no matter what
     * _EXCEPT_ if trying to change a "locked" variable!
     * This would be a very easy way to change the serial number of
     * a machine. (Turns out that the only other error is an NVRAM
     * write error.)
     */

    error =  cpu_set_nvram (name, unset ? "" : value);
    if (error == EACCES) 
	return error;

    /* the argument to the setenv command is invalid, don't replace */
    /*  the string in memory, return 0 so there is not more action */
    if (error == EINVAL)
	return 0;

    if (unset)
	delete_str(name, &environ_str);
    else
	replace_str(name, value, &environ_str);

    return error;
}
#endif /* IP32 */
