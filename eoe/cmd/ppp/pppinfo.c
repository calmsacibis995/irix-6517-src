/* fetch information about a PPP link
 */

#ident "$Revision: 1.3 $"

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/stream.h>

#include "pppinfo.h"
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined(PPP_IRIX_64)
#include "if_ppp.h"
#else
#include <sys/if_ppp.h>
#endif

static int info_unit = -1;
static int info_fd = -1;
static int info_poke_fd = -1;
char status_path[RENDPATH_LEN];
char status_poke_path[RENDPATH_LEN];

char pppinfo_err[64];


static void
close_info(char *fmt, ...)
{
	va_list args;
	int serrno = errno;


	va_start(args, fmt);
	if (fmt != 0) {
		(void)vsprintf(pppinfo_err, fmt, args);
	}
	va_end(args);

	if (info_fd >= 0) {
		(void)close(info_fd);
		info_fd = -1;
	}
	if (info_poke_fd >= 0) {
		(void)close(info_poke_fd);
		info_poke_fd = -1;
	}
	errno = serrno;
}


static int
open_info(int unit)
{
	if (info_poke_fd < 0) {
		(void)sprintf(status_poke_path, RENDDIR_STATUS_POKE_PAT, unit);
		/* open for reading and writing to prevent SIGPIPE */
		info_poke_fd = open(status_poke_path, O_RDWR, O_NONBLOCK);
		if (info_poke_fd < 0) {
			close_info("open_info(%s): %s",
				   status_poke_path, strerror(errno));
			return -1;
		}
	}

	if (info_fd < 0) {
		(void)sprintf(status_path, RENDDIR_STATUS_PAT, unit);
		info_fd = open(status_path, O_RDONLY, 0);
		if (info_fd < 0) {
			close_info("open_info(%s): %s",
				   status_path, strerror(errno));
			return -1;
		}
	}

	return 0;
}



static int
check_vers(struct ppp_status *ps)
{
	if (ps->ps_version != PS_VERSION) {
		close_info("wrong %s ps_version=%d instead of %d",
			   status_path, ps->ps_version, PS_VERSION);
		return -1;
	}
	if (ps->ps_pi.pi_version != PI_VERSION) {
		close_info("wrong %s SIOC_PPP_INFO version %d"
			   " instead of %d",
			   status_path, ps->ps_pi.pi_version, PI_VERSION);
		return -1;
	}
	return 0;
}



int					/* >=0 if ok */
pppinfo_get(int unit,			/* ask about this unit */
	    struct ppp_status *ps)	/* put answer here */
{
	struct stat st;
	time_t now;
	int i;


	/* if the unit number changes, switch directories */
	if (unit != info_unit)
		close_info(0);

	if (0 > open_info(unit))
		return -1;

	if (0 > fstat(info_fd, &st)) {
		close_info("fstat(%s): %s", status_path, strerror(errno));
		return -1;
	}

	/* if the data is stale, poke the daemon */
	now = time(0);
	if (st.st_mtime != now) {
		if (1 != write(info_poke_fd, "", 1)) {
			close_info("write(%s): %s",
				   status_poke_path, strerror(errno));
			return -1;
		}
		/* give the daemon time to respond */
		sginap(20);
	}

	if (0 > lseek(info_fd, 0, SEEK_SET)) {
		close_info("rewind(%s): %s", status_path, strerror(errno));
		return -1;
	}
	i = read(info_fd, ps, sizeof(*ps));
	if (i < 0) {
		close_info("read(%s): %s",
			   status_path, strerror(errno));
		return -1;
	}
	if (i != sizeof(*ps)) {
		if (check_vers(ps) < 0)
			return -1;

		/* read a second time in case the daemon was writing the
		 * file when we read it.
		 */
		if (0 > lseek(info_fd, 0, SEEK_SET)) {
			close_info("rewind(%s): %s",
				   status_path, strerror(errno));
			return -1;
		}
		errno = 0;
		i = read(info_fd, ps, sizeof(*ps));
		if (i < 0) {
			close_info("read(%s): %s",
				   status_path, strerror(errno));
			return -1;
		}
	}

	if (ps->ps_cur_date != now && ps->ps_cur_date != time(0)) {
		strcpy(pppinfo_err,"daemon not responding");
		return -1;
	}
	return check_vers(ps);
}



int
pppinfo_next_unit(int unit,
		  int delta,
		  struct ppp_status *ps)
{
	int i;

	for (i = 0; i < MAX_PPP_DEVS; i++) {
		unit += delta;
		if (unit < 0)
			unit = MAX_PPP_DEVS-1;
		else if (unit >= MAX_PPP_DEVS)
			unit = 0;
		if (pppinfo_get(unit, ps) >= 0)
			return unit;
		if (delta != -1 && delta != 1)
			return -1;
	}
	return -1;
}
