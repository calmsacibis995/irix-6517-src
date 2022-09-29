/* find all user names in /etc/passwd
 *	This is not as simple as running sed(1) over /etc/passwd in the
 *	presence of Yellow Pages
 */

#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

char	cmd_label[] = "UX:calnames";

main()
{
	register struct passwd *ent;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	setpwent();
	while (ent = getpwent())
		printf("%s %s\n", ent->pw_name, ent->pw_dir);
	exit(0);
}
