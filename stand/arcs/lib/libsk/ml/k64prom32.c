/*  Routines to convert data from 32 bit underlying prom to 64 bit standalone
 * program usage.  Done at run-time to ensure common libsc code.
 */
#ident "$Revision: 1.5 $"

#if _K64PROM32

#define __USE_SPB32	1

#include <sys/types.h>
#include <sys/immu.h>
#include <arcs/spb.h>
#include <stringlist.h>
#include <libsc.h>
#include <libsk.h>

extern struct string_list environ_str;
extern struct string_list argv_str;
void initargv(long *, char ***);
void initenv(char ***);

__uint32_t k64p32_callback_0(__uint32_t);

char **
k64p32_copystrings(__int32_t *wp, struct string_list *slp)
{
        init_str(slp);
        for (; wp && *wp; wp++) {
                new_str1((char *)((__psint_t)*wp), slp);
	}
        return(slp->strptrs);
}

/*
 * initenv - copy environment someplace safe so we can
 *      play with it.
 */
void
k64p32_initenv(__int32_t **envp)
{
	if (_isK64PROM32()) {
		environ = k64p32_copystrings(*envp, &environ_str);
		*((char ***)envp) = environ;
		return;
	}
	initenv((char ***)envp);
}


/*
 * initargv - copy argv variables someplace safe
 */
void
k64p32_initargv(long *ac, __int32_t **av)
{
	if (_isK64PROM32()) {
		init_str(&argv_str);
		while ((*ac)--) {
			if (**av)
				new_str1((char *)((__psint_t)**av), &argv_str);
			++(*av);
		}

		*ac = argv_str.strcnt;
		*((char ***)av) = argv_str.strptrs;
		return;
	}
	initargv(ac,(char ***)av);
}
#endif
