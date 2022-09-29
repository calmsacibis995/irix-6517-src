#ifdef __STDC__
	#pragma weak cap_acquire = _cap_acquire
	#pragma weak cap_surrender = _cap_surrender
	#pragma weak cap_set_proc_flags = _cap_set_proc_flags
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/capability.h>
#include <unistd.h>
#include <errno.h>

#ident "$Revision: 1.4 $"

/*
 * Make each permitted capability in caps[] effective.
 * Return the old capability set, or NULL on error
 */
/* move out of function scope so we get a global symbol for use with data cording */
static int cap_enabled = -1;

cap_t
cap_acquire (int ncap, const cap_value_t *caps)
{
    int i, changed = 0;
    cap_t ocap, cap;

    /* error checking */
    if (caps == (cap_value_t *) NULL)
    {
	setoserror (EINVAL);
	return ((cap_t) NULL);
    }

    /* check if capabilities are enabled */
    if (cap_enabled == -1)
	    cap_enabled = (sysconf (_SC_CAP) > 0);
    if (!cap_enabled)
    {
	setoserror (ENOSYS);
	return ((cap_t) NULL);
    }

    /* get the current capability set */
    ocap = cap_get_proc ();
    if (ocap == (cap_t) NULL)
	return (ocap);

    /* duplicate it */
    cap = cap_dup (ocap);
    if (cap == (cap_t) NULL)
    {
	(void) cap_free (ocap);
	return (cap);
    }

    /* acquire the new capabilities in the duplicate */
    for (i = 0; i < ncap; i++)
    {
	cap_flag_value_t flag_value;
	int r;

	r = cap_get_flag (cap, caps[i], CAP_PERMITTED, &flag_value);
	if (r == 0 && flag_value == CAP_SET)
	{
	    if (cap_set_flag (cap, CAP_EFFECTIVE, 1, (cap_value_t *) caps + i, CAP_SET) == -1)
	    {
		(void) cap_free (ocap);
		(void) cap_free (cap);
		return ((cap_t) NULL);
	    }
	    changed++;
	}
    }

    /* set them */
    if (changed && cap_set_proc (cap) == -1)
    {
	(void) cap_free (ocap);
	(void) cap_free (cap);
	return ((cap_t) NULL);
    }
    (void) cap_free (cap);

    /* return the old capability set */
    return (ocap);
}

void
cap_surrender (cap_t cap)
{
    if (cap != (cap_t) NULL)
    {
	(void) cap_set_proc (cap);
	(void) cap_free (cap);
    }
}

int
cap_set_proc_flags (cap_value_t flags)
{
	if ((flags & CAP_FLAGS) != flags) {
		setoserror (EINVAL);
		return -1;
	}
	if (syssgi(SGI_PROC_ATTR_SET, SGI_CAP_PROCESS_FLAGS,
	    &flags, sizeof(flags)) < 0)
		return -1;
	return 0;
}
