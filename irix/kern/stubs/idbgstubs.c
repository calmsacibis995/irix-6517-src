#include "sys/types.h"
#include "sys/systm.h"
#include "sys/vnode.h"
#include "sys/idbg.h"
#include "sys/sema.h"

static void _idbg_setup(void){}
/* ARGSUSED */
static int  _idbg_copytab(caddr_t *x, int y, rval_t *z){return nodev();}
/* ARGSUSED */
static void _idbg_tablesize(union rval *a){}
/* ARGSUSED */
static int  _idbg_dofunc(struct idbgcomm *x, rval_t *y){return nodev();}
static int  _idbg_error(void){return (0);}
/* ARGSUSED */
static void _idbg_switch(int x, int y){}
/* ARGSUSED */
static int _idbg_addfunc(char *x, void (*y)()){return 0;}
/* ARGSUSED */
static void _idbg_delfunc(void (*y)()){}
/* ARGSUSED */
static void _qprintf(char *f, va_list ap){}
/* ARGSUSED */
static void __prvn(vnode_t *x, int y){}
/* ARGSUSED */
static void _idbg_wd93(__psint_t x){}

void _idbg_late_setup(void){}
static int  _idbg_unload(void){return (0); }
/* ARGSUSED */
static void _idbg_addfssw(struct idbgfssw *i){}
/* ARGSUSED */
static void _idbg_delfssw(struct idbgfssw *i){}
/* ARGSUSED */
static void _printflags(uint64_t f, char **s, char *n){}
/* ARGSUSED */
static char *_padstr(char *buf, int len){return 0;}
/* ARGSUSED */
static void _pmrlock(struct mrlock_s *m){}
/* ARGSUSED */
static void _prsymoff(void *a, char *b, char *c){}

int dbstab[] = {0};
char nametab[] = {0};
int symmax = 0;
int namesize = 0;

idbgfunc_t idbgdefault = {/*stubs*/(int *)1L, _idbg_setup, _idbg_copytab,
    _idbg_tablesize,
    _idbg_dofunc, _idbg_error, _idbg_switch, _idbg_addfunc, _idbg_delfunc,
    _qprintf, __prvn, _idbg_wd93, _idbg_late_setup, _idbg_unload,
    _idbg_addfssw, _idbg_delfssw, _printflags, _padstr, _pmrlock, _prsymoff};
