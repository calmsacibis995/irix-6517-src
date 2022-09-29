#ifdef __STDC__
	#pragma weak cap_envp	= _cap_envp
	#pragma weak cap_envl	= _cap_envl
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/capability.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#ident "$Revision: 1.2 $"

static int
cap_env_common (int flags, cap_value_t wanted)
{
	const int cap_enabled = (int) sysconf(_SC_CAP);
	const int root = (geteuid () == 0);
	int error = 0;
	cap_t cap;

	/* check arguments */
	if (wanted == CAP_ALL_OFF || (wanted & CAP_ALL_ON) != wanted)
	{
		setoserror (EINVAL);
		return (-1);
	}

	/* strict superuser environment */
	if (cap_enabled == CAP_SYS_DISABLED)
	{
		if (root == 0)
		{
			setoserror (EPERM);
			error = -1;
		}
		return (error);
	}

	/* should never fail */
	cap = cap_get_proc ();
	if (cap == NULL)
		return (-1);
	wanted |= cap->cap_permitted;

	/*
	 * If our desired and permitted capability sets are different,
	 * but we can get the desired capabilities either because we
	 * have CAP_SETPCAP capability or we are superuser in a
	 * superuser-enabled environment, then get them.
	 */
	if (wanted != cap->cap_permitted)
	{
		if (cap->cap_effective & CAP_SETPCAP)
			cap->cap_permitted = wanted;
		else if (cap->cap_permitted & CAP_SETPCAP)
		{
			cap->cap_effective |= CAP_SETPCAP;
			error = cap_set_proc (cap);
			cap->cap_effective &= ~CAP_SETPCAP;
			cap->cap_permitted = wanted;
		}
		else if (cap_enabled == CAP_SYS_SUPERUSER && root)
		{
			cap->cap_permitted = wanted;
		}
		else
		{
			setoserror (EPERM);
			error = -1;
		}
			
		/*
		 * If no errors occurred, we have sufficient
		 * privilege to set our capability set
		 */
		if (error == 0)
			error = cap_set_proc (cap);
	}

	/* handle flags */
	if (error == 0 && (flags & CAP_ENV_SETUID))
		error = setuid (getuid ());

	if (error == 0 && (flags & CAP_ENV_RECALC))
		error = cap_set_proc_flags (CAP_FLAG_PURE_RECALC);

	/* clean up and return */
	cap_free (cap);
	return (error);
}

int
cap_envl (int flags, ...)
{
	cap_value_t wanted = CAP_ALL_OFF, got;
	va_list ap;

	va_start (ap, flags);
	while (got = (cap_value_t) va_arg (ap, cap_value_t))
		wanted |= got;
	va_end (ap);

	return (cap_env_common (flags, wanted));
}

int
cap_envp (int flags, size_t count, const cap_value_t *caps)
{
	cap_value_t wanted;

	/* check arguments */
	if (count == 0 || caps == NULL)
	{
		setoserror (EINVAL);
		return (-1);
	}

	/* create desired capability list */
	wanted = CAP_ALL_OFF;
	while (count--)
		wanted |= caps[count];

	return (cap_env_common (flags, wanted));
}
