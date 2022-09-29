#include <stdio.h>
#include <shadow.h>

main(int argc, char **argv)
{
	struct spwd *sp;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		sp = getspnam(*argv);
		if (! sp) {
			fprintf(stderr, "getspnam failed\n");
		} else {
			printf("name = %s, passwd = %s, last-change = %d, min = %d\n",
			    sp->sp_namp, sp->sp_pwdp, sp->sp_lstchg, sp->sp_min);
			printf("max = %d, warn = %d, inactive = %d, expire = %d\n",
			    sp->sp_min, sp->sp_warn, sp->sp_inact, sp->sp_expire);
			printf("flags = 0x%x\n", sp->sp_flag);
		}
	}
}
