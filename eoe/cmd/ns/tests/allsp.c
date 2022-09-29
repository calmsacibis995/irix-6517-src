#include <stdio.h>
#include <shadow.h>

main(int argc, char **argv)
{
	struct spwd *sp, *s, rets;
	char buf[4086];

	while (sp = getspent()) {
		printf("name = %s, passwd = %s, last-change = %d, min = %d\n",
		    sp->sp_namp, sp->sp_pwdp, sp->sp_lstchg, sp->sp_min);
		printf("max = %d, warn = %d, inactive = %d, expire = %d\n",
		    sp->sp_min, sp->sp_warn, sp->sp_inact, sp->sp_expire);
		printf("flags = 0x%x\n", sp->sp_flag);
		if (! getspnam_r(sp->sp_namp, &rets, buf, sizeof(buf))) {
			fprintf(stderr, "failed getspnam: %s\n", sp->sp_namp);
		}
	}
}
