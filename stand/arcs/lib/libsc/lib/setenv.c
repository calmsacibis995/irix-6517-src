#ident	"lib/libsc/lib/setenv.c:  $Revision: 1.10 $"

/* 
 * setenv.c - programming interface to set environment variables
 *	This version of setenv doesn't touch the nvram.  The one
 *	in libsk calls cpu_set_nvram.
 */

#include <stringlist.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsc_internal.h>

extern struct string_list environ_str;
extern int Debug, Verbose, _udpcksum;

#define READONLY 1
#define VARIABLE 2

static struct special {
    char *name;
    int *value;
    int flags;
} special_tab[] = {
    {"DEBUG", &Debug, VARIABLE},
    {"VERBOSE", &Verbose, VARIABLE},
    {"_udpcksum", &_udpcksum, VARIABLE},
    {"gfx", 0, READONLY},
    {"cpufreq", 0, READONLY},
    {"eaddr", 0, READONLY},
};

static int special_len = sizeof(special_tab)/sizeof(special_tab[0]);
int setenv_override;

/*
 * setenv - sets variable name to value.  If value is 0 or the 
 * NULL string, then setenv unsets the variable name and clears 
 * it from the environment.  Returns -1 if the variable is 
 * readonly, or -2 if setting the nvram failed.  If override is not
 * 0, then the READONLY flag is ignored (to allow the system firmware
 * to initialize READONLY variables).
 */
int
_setenv (char *name, char *value, int override)
{
    int unset = (value == 0) || (*value == '\0');
    int i;

    setenv_override = override;
    for (i = 0; i < special_len; ++i) {
	if (!strcasecmp (name, special_tab[i].name)) {
	    if (!override && (special_tab[i].flags & READONLY))
		return EACCES;
	    if (special_tab[i].flags & VARIABLE)
		if (unset)
		    *special_tab[i].value = 0;
		else
		    atob(value, special_tab[i].value);
	    break;
	}
    }

    if (unset)
	delete_str(name, &environ_str);
    else
	replace_str(name, value, &environ_str);

    return ESUCCESS;
}
