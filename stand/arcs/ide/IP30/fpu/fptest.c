#include "uif.h"

bool_t
fptest()
{
	extern int fpmem();
	extern int fregs();
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
		int	(*fp_func)(void);
		char	*fp_name;
	};
	static struct fp_table fp_diag[] = {
		{fpmem,		"general registers"},
		{fregs,		"control registers"}, 
		{faddsubs,	"single precision addition/subtraction"},
		{faddsubd,	"double precision addition/subtraction"},
		{fmulsubs,	"single precision multiplication/subtraction"},
		{fmulsubd,	"double precision multiplication/subtraction"},
		{fmuldivs,	"single precision multiplication/division"},
		{fmuldivd,	"double precision multiplication/division"},
		{fdivzero,	"dividing by zero"},
		{finexact,	"inexact result"},
		{finvalid,	"invalid result"},
		{foverflow,	"overflow result"},
		{funderflow,	"underflow result"},
		{fpcmput,	"infinite series"},
		{0,		0},
	};
	struct fp_table *fp;
	

	msg_printf(VRB|PRCPU, "Floating point unit test\n");

	for (fp = fp_diag; fp->fp_name; fp++) {
		if ((*fp->fp_func)()) {
			msg_printf(DBG|PRCPU, "FPU %s test FAILED\n",
				fp->fp_name);
			++errcount;
		} else
			msg_printf(DBG|PRCPU, "FPU %s test PASSED\n",
				fp->fp_name);
	}

	if (errcount == 0)
		okydoky();
	else
		sum_error("floating point unit");

	return errcount;
}
