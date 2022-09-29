/* unixperf - an x11perf-style Unix performance benchmark */

/* test_naming.c tests name resolution and getXbyY functions */

#include "unixperf.h"
#include "test_naming.h"

#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

unsigned int
DoGetpwnam(void)
{
	int ii;
	struct passwd *pwent;

	for (ii = 0; ii < 40; ii++) {
		pwent = getpwnam("root");
		if (pwent == NULL)
			DOSYSERROR("failed to find root using getpwnam()");
	}

	return ii;
}

unsigned int
DoGetpwuid(void)
{
	int ii;
	struct passwd *pwent;

	for (ii = 0; ii < 40; ii++) {
		pwent = getpwuid(0);
		if (pwent == NULL)
			DOSYSERROR("failed to find uid 0 using getpwuid()");
	}

	return ii;
}

unsigned int
DoGetpwentAll(void)
{
	struct passwd *pwent;

	setpwent();
	while ((pwent = getpwent()) != NULL)
		;
	endpwent();

	return 1;
}

unsigned int
DoGethostbyname(void)
{
	int ii;
	struct hostent *hent;

	for (ii = 0; ii < 40; ii++) {
		hent = gethostbyname("localhost");
		if (hent == NULL)
			DOSYSERROR("failed to resolve localhost");
	}

	return ii;
}
