#include "uif.h"

bool_t
fptest()
{
	extern int fpmem();
	extern int fpregs();
	extern int faddsubd();
	extern int faddsubs();
	extern int fmulsubd();
	extern int fmulsubs();
	extern int fmuldivd();
	extern int fmuldivs();
	extern int fdivzero();
	extern int finexact();
	extern int finvalid();
	extern int foverflow();
	extern int funderflow();
	extern int fpcmput();

	int errcount = 0;
	struct fp_table {
		int	(*fp_func)();
		char	*fp_name;
	};
	static struct fp_table fp_diag[] = {
		{fpmem,		"general registers"},
		{fpregs,	"control registers"},
		{faddsubd,	"double precision addition/subtraction"},
		{faddsubs,	"single precision addition/subtraction"},
		{fmulsubd,	"double precision multiplication/subtraction"},
		{fmulsubs,	"single precision multiplication/subtraction"},
		{fmuldivd,	"double precision multiplication/division"},
		{fmuldivs,	"single precision multiplication/division"},
		{fdivzero,	"dividing by zero"},
		{finexact,	"inexact result"},
		{finvalid,	"invalid result"},
		{foverflow,	"overflow result"},
		{funderflow,	"underflow result"},
		{fpcmput,	"infinite series"},
		{0,		0},
	};
	struct fp_table *fp;
	
	run_cached();

	msg_printf(VRB, "Floating point unit test\n");

	for (fp = fp_diag; fp->fp_name; fp++) {
		if ((*fp->fp_func)()) {
			msg_printf(DBG, "FPU %s test FAILED\n", fp->fp_name);
			++errcount;
		} else
			msg_printf(DBG, "FPU %s test PASSED\n", fp->fp_name);
		busy(1);
	}

	if (errcount == 0)
		okydoky();
	else
		sum_error("floating point unit");

	run_uncached();

	busy(0);
	return errcount;
}
