/* $Revision: 1.2 $ */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <locale.h>
#include <pfmt.h>

main()
{
	uid_t	euid = 	geteuid();
	struct	passwd	*p;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel("UX:whoami");

	if (!(p = getpwuid(euid))) {
		pfmt(stderr, MM_ERROR,
			":8:no login associated with uid %u.\n", euid);
                exit(1);
	}
	printf("%s\n", p->pw_name);
	exit (0);
}
