/*
 * read dev/kmem
 */

#ident "$Id: kmemread.c,v 1.17 1999/04/28 07:02:32 tes Exp $"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <nlist.h>
#include <ctype.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>

#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"
#include "./kmemread.h"
#include "pmda_version.h"

static int	fd = PM_ERR_GENERIC;
static int	kmemOK = 0;

/*
 * Warning: order here has to match values of #defines in kmemread.h
 */
struct NLIST kernsymb[] = {
    {"kernel_magic"},
    {"end"},
    {"ifnet"},
    {"dkipprobed"},
    {"dkiotime"},
    {"dkscinfo"},
    {"usraid_softc"},
    {"usraid_iotime"},
    {"usraid_info"},
    {"vn_vnumber"},
    {"vn_epoch"},
    {"vn_nfree"},
    {"str_page_cur"},
    {"str_min_pages" },
    {"str_page_max"},
    {0}
};

#define LBUF_MAX 80

void
kmeminit(void)
{
    struct stat	statbuf;
    long	k_magic;
    int		sts;
    int		i;
    int		done_nlist = 0;
    int		nsymb = sizeof(kernsymb)/sizeof(kernsymb[0]);
    FILE	*f;
    char	*magic_start = "<<pcp kernmap:";
    char	*magic_end = ">>\n";
    char	magic[LBUF_MAX]; 

#if !BEFORE_IRIX6_5
    struct group	*grp;
    gid_t	old_gid;
#endif 

    sprintf(magic, "%s%s%s", magic_start, buildversion, magic_end);


    if ((f = fopen("/tmp/pcp.kernaddr", "r")) != (FILE *)0) {
	char	lbuf[LBUF_MAX];
	__psunsigned_t	x;
	char	*p;
	char	*q;
	i = -1;
	while (fgets(lbuf, sizeof(lbuf), f) != NULL) {
	    if (i == -1) {
		if (strcmp(lbuf, magic) != 0)
		    break;
	    }
	    else if (i == nsymb)
		break;
	    else {
		if ((sscanf(lbuf, "%lx", &x)) != 1)
		    break;
		p = lbuf;
		while (isxdigit(*p))
		    p++;
		p++;
		q = p;
		while (*q && *q != '\n')
		    q++;
		*q = '\0';
		if (strcmp(p, kernsymb[i].n_name) != 0)
		    break;
		kernsymb[i].n_value = x;
	    }
	    i++;
	}
	fclose(f);
	if (i == nsymb - 1)
	    goto check;
	/* the file is bad, silently remove it and FALLTHROUGH */
	unlink("/tmp/pcp.kernaddr");
    }

hardway:
    /*
     * /tmp/pcp.kernaddr either missing, or not valid
     */
    done_nlist = 1;
    fd = 0;
    if (stat("/unix", &statbuf) < 0) {
	__pmNotifyErr(LOG_ERR, "kmeminit: cannot stat \"/unix\": %s", pmErrStr(-errno));
	fd = -errno;
    }
    else if ((sts = NLIST("/unix", kernsymb)) < 0) {
	__pmNotifyErr(LOG_ERR, "kmeminit: nlist failed for \"/unix\": %s", pmErrStr(-errno));
	fd = -errno;
    }
    else if (sts != 0) {
	for (i = 0; i < KS_DKIPPROBED; i++) {
	    if (kernsymb[i].n_value == 0) {
		__pmNotifyErr(LOG_WARNING, "kmeminit: symbol %s not found", kernsymb[i].n_name);
	    }
	}
    }
    if (fd < 0)
	/* nothing we can do! */
	return;

check:

#if !BEFORE_IRIX6_5

    grp = getgrnam("sys");
    if (grp == NULL) {
	__pmNotifyErr(LOG_WARNING, "kmeminit: cannot find group \"sys\": %s", pmErrStr(-errno));
	old_gid = -1;
    }
    else {
	old_gid = getgid();
	if (setgid(grp->gr_gid) < 0)
	    __pmNotifyErr(LOG_WARNING, "kmeminit: cannot change to group \"sys\": %s", pmErrStr(-errno));
    }

#endif

    if ((fd = open("/dev/kmem", O_RDONLY)) < 0) {
	__pmNotifyErr(LOG_ERR, "kmeminit: cannot open \"/dev/kmem\": %s", pmErrStr(-errno));
	fd = -errno;
    }
    else {
	if ((int)LSEEK(fd, (__psint_t)(kernsymb[KS_KERNEL_MAGIC].n_value & ~ KMEM_ADDR_MASK), SEEK_SET) < 0) {
	    __pmNotifyErr(LOG_ERR, "kmeminit: lseek to \"&kernel_magic\" (%p): %s",
		kernsymb[KS_KERNEL_MAGIC].n_value, pmErrStr(-errno));
	    close(fd);
	    fd = -errno;
	}
	else if (read(fd, &k_magic, sizeof(k_magic)) != sizeof(k_magic)) {
	    __pmNotifyErr(LOG_ERR, "kmeminit: read \"kernel_magic\": %s", pmErrStr(-errno));
	    close(fd);
	    fd = -errno;
	}
	else if (k_magic != kernsymb[KS_END].n_value) {
	    /* k_magic is supposed to contain the &end */
	    __pmNotifyErr(LOG_ERR, "kmeminit: \"/unix\" is not the namelist for the running kernel");
	    close(fd);
	    fd = PM_ERR_GENERIC;
	}

	if (fd < 0) {
	    if (!done_nlist) {
		__pmNotifyErr(LOG_ERR, "kmeminit: \"/tmp/pcp.kernaddr\" maybe bogus ... remove it and try again");
		unlink("/tmp/pcp.kernaddr");
		goto hardway;
	    }
	}

	if (done_nlist && fd >= 0) {
	    /*
	     * map appears worth saving
	     */
	    if ((f = fopen("/tmp/pcp.kernaddr", "w")) != (FILE *)0) {
		fprintf(f, magic);
		for (i = 0; i < nsymb-1; i++)
		    fprintf(f, "%lx %s\n", kernsymb[i].n_value, kernsymb[i].n_name);
		fclose(f);
	    }
	}
    }

#if !BEFORE_IRIX6_5
    if (old_gid != -1)
	setgid(old_gid);
#endif

    kmemOK = 1;
}

int
kmemread(__psunsigned_t addr, void *buf, int nbytes)
{
    if (!kmemOK)
	return -EFAULT;
    else if (ALIGNED_KMEM_ADDR(addr) == 0) {
	__pmNotifyErr(LOG_WARNING, "kmemread unaligned address %p", addr);
	return -EFAULT;
    }

    return unaligned_kmemread(addr, buf, nbytes);
}

int
unaligned_kmemread(__psunsigned_t addr, void *buf, int nbytes)
{
    int		e = fd;

    if (!kmemOK)
	return -EFAULT;
    else if (VALID_KMEM_ADDR(addr) == 0) {
	__pmNotifyErr(LOG_WARNING, "kmemread bad address %p", addr);
	return -EFAULT;
    }

    if (e >= 0) {
	if ((int)LSEEK(fd, (__psint_t)(addr & ~ KMEM_ADDR_MASK), SEEK_SET) < 0) {
	    __pmNotifyErr(LOG_WARNING, "kmemread: lseek failed for addr %p: %s",
			 addr, pmErrStr(-errno));
	    e = -errno;
	}
	else if ((e = (int)read(fd, buf, nbytes)) < 0) {
	    __pmNotifyErr(LOG_WARNING, "kmemread: read failed for addr %p: %s",
			 addr, pmErrStr(-errno));
	    e = -errno;
	    /* note: return nbytes like a real read */
	}
    }

    return e;
}
