#include <sys/cpu.h>
#include <sys/sbd.h>
#include "uif.h"
lpackd()
{
	int i;
	int failed;
	unsigned int oldSR = GetSR();

	clear_nofault();

	msg_printf(VRB,"Start lpackd\n");
	SetSR(oldSR | SR_CU1);
	for(i=0;i<75;i++) {
		failed=lpackd_();
		if (failed == 1) {
			SetSR(oldSR);
			return(1);
		}
		/* if(Chkpe() == 1) return(1); */
	}
/*	msg_printf(VRB,"\nStart swapped cache lpackd\n");
	for(i=0;i<50;i++) {
		swap_cache();
		failed=lpackd_();
		if (failed == 1 || Chkpe() == 1) {
			unswap_cache();
			return(1);
		}
	}
	unswap_cache();
*/
        printf("\n");
	SetSR(oldSR);
	return(0);
}

#ifdef NOTDEF
lpackd_swap()
{
	int i;
	int failed;
	clear_nofault();
	findfp();
	msg_printf(VRB,"\nStart swapped cache lpackd\n");
	for(i=0;i<50;i++) {
		swap_cache();
		failed=lpackd_();
		if (failed == 1 || Chkpe() == 1) {
			unswap_cache();
			return(1);
		}
	}
	unswap_cache();
        printf("\n");
	return(0);
}
#endif

strprnt_(char **a) { printf("%s", a); }
err_strprnt_(char **a) { msg_printf(ERR,"%s", a); }
newline_() { printf("\n"); }
dblprnt_(double *d)
{
	int exp;
	char buf[64];
	exp = _dtoa(buf,20,*d,1) + 1 ;
	buf[0]='.';
	printf(" %sE%d", buf,exp);
}    
