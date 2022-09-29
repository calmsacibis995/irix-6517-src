

#pragma weak	getutxline = _getutxline
#pragma weak	getutxent = _getutxent
#pragma weak	getutxid = _getutxid
#pragma weak	setutxent = _setutxent
#pragma weak	pututxline = _pututxline
#pragma weak	endutxent = _endutxent

#include <synonyms.h>
#include <sys/types.h>
#include <wait.h>
#include <dirent.h>
#include <utmpx.h>

/*
 * Use this file to provide 'stubs' to compile ABI programs against.
 * It is sometimes easier to add a few NULL routines here rather than
 * grag in the 'real' file since the real file might define a bunch
 * of stuff that we don't want - rather than ifdef it all to hell, putting
 * stubs is easier..
 */

float
_sqrt_s(float v)
{
	return(v);
}

double
_sqrt_d(double v)
{
	return(v);
}

/* ARGSUSED */
int
_test_and_set(int *p, int v)
{
	return 0;
}

/* ARGSUSED */
int
_flush_cache(char *addr, int nbytes, int cache)
{
	return 0;
}

void
endutxent(void)
{
}

struct utmpx *
getutxent(void)
{
	return NULL;
}

/* ARGSUSED */
struct utmpx *
getutxid(const struct utmpx *g)
{
	return NULL;
}
	
/* ARGSUSED */
struct utmpx *
getutxline(const struct utmpx *g)
{
	return NULL;
}

/* ARGSUSED */
struct utmpx *
pututxline(const struct utmpx *g)
{
	return NULL;
}

void
setutxent(void)
{
}

void
vfork(void)
{}
