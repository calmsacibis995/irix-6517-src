/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.17 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <mutex.h>
#include <strings.h>
#include <sgidefs.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/ckpt_sys.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/pmo.h>
#include <ckpt.h>
#include <ckpt_internal.h>
#include "librestart.h"

#ifdef DEBUG
char cpr_file[64];
int cpr_line;
pid_t debug_pid;
#endif

int *errfd_p;
pid_t *errpid_p;
int *__errnoaddr = &errno;

extern int munmap_local(void *, size_t);

#define	FOR_EACH_PROPERTY(i, p)	\
	for (i = 0, p = tp->share_prop_tab; \
		i < cp->cr_handle_count; i++, p++)


int
ckpt_read_share_property_low(ckpt_ta_t *tp,
			     ckpt_magic_t magic,
			     int *misc)
{
	cr_t *cp = tp->crp;
	ckpt_prop_t *prop;
	ckpt_phandle_t *phand;
	int i, rc;

	if (tp == NULL) {
		ckpt_perror("No target argument structure", 0);
		return (-1);
	}
	if (cp->cr_index == NULL) {
		ckpt_perror("shared index was not yet read. Abort...", 0);
		return (-1);
	}
	/*
	 * find the right property handle
	 */
	FOR_EACH_PROPERTY(i, phand) {
		if (phand->prop_magic == magic)
			break;
	}
	if (i >= cp->cr_handle_count) {
		ckpt_perror("Don't know how to process property", 0);
		return (-1);
	}
	/*
	 * go thru all property with the given magic
	 */
	for (i = 0, prop = cp->cr_index; i < cp->cr_pci.pci_prop_count;
		i++, prop++) {
		int n;

		if (magic != prop->prop_magic || prop->prop_quantity == 0)
			continue;

		/*
		 * Point to the correct property in the statefile
		 */
		if (lseek(tp->ssfd, (off_t)prop->prop_offset, SEEK_SET) < 0) {
			ckpt_perror("Cannot seek indexfile", ERRNO);
			return (-1);
		}
		for (n = 0; n < prop->prop_quantity; n++) {
			if (rc = (*phand->prop_restart)(cp, tp, magic, misc))
				return (rc);
		}
	}
	return (0);
}

int
ckpt_init_proc_property(ckpt_ta_t *tp)
{
	size_t size;
	/*
	 * The targ args struct pointed to by tp already has index
	 * header loaded.  
	 */
	lseek(tp->ifd, sizeof(ckpt_ppi_t), SEEK_SET);

	size = sizeof(ckpt_prop_t) * tp->ixheader.ppi_prop_count;
	if ((tp->index = (ckpt_prop_t *)ckpt_malloc((long)size)) == NULL) {
		ckpt_perror("Failed to allocate memory", ERRNO);
		return (-1);
	}
	if (read(tp->ifd, tp->index, size) < 0) {
		ckpt_perror("read error", ERRNO);
		return (-1);
	}
	return (0);
}

void
ckpt_free_proc_property(ckpt_ta_t *tp)
{
	ckpt_free(tp->index);
}

int
ckpt_read_proc_property_low(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	cr_t *cp = tp->crp;
	ckpt_phandle_t *prop_tab = tp->proc_prop_tab;
	ckpt_prop_t *prop;
	ckpt_phandle_t *phand;
	int i, rc;

	/*
	 * find the right property handle
	 */
	for (i = 0, phand = prop_tab;  i < cp->cr_procprop_count; i++, phand++) {
		if (phand->prop_restart == NULL)
			continue;

		if (phand->prop_magic == magic)
			break;
	}
	if (i == cp->cr_procprop_count) {
		ckpt_perror("Don't know how to handle property", 0);
		return (-1);
	}
	/*
	 * go thru all property with the given magic
	 */
	for (i = 0, prop = tp->index; i < tp->ixheader.ppi_prop_count;
		i++, prop++) {
		int n;

		if (magic != prop->prop_magic || prop->prop_quantity == 0)
			continue;

		/*
		 * Point to the correct property in the statefile
		 */
		if (lseek(tp->sfd, (off_t)prop->prop_offset, SEEK_SET) < 0) {
			ckpt_perror("Cannot seek statefile", ERRNO);
			return (-1);
		}
		for (n = 0; n < prop->prop_quantity; n++) {
			if (rc = (*phand->prop_restart)(tp, magic))
				return (rc);
		}
	}
	return (0);
}

int
ckpt_setroot(ckpt_ta_t *ta, pid_t mypid)
{
	int rc;

	if (IS_SPROC(&ta->pi) && IS_SID(&ta->pi)) {
		/*
		* Have the last id sharing member do the update
		*/
		rc = ckpt_sid_barrier(&ta->pi, mypid);
		if (rc < 0) {
			ckpt_perror("shared id barrier error", ERRNO);
			return (-1);
		}
		if (rc == 0)
			return (0);

	}
	(void)setreuid((uid_t)-1, 0); /* seteuid(0) */

	if (IS_SPROC(&ta->pi) && IS_SID(&ta->pi)) {
		rc = ckpt_sproc_unblock(mypid,
					sidlist->sid_count,
					sidlist->sid_pid);
		if (rc < 0) {
			ckpt_perror("ckpt_sproc_unblock", ERRNO);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_restore_identity_late(ckpt_ta_t *ta, pid_t mypid)
{
	int rc;
	prcred_t *cred = &ta->pi.cp_cred;

	if (IS_SPROC(&ta->pi) && IS_SID(&ta->pi)) {
		/*
		 * Have the last id sharing member do the update
		 */
		rc = ckpt_sid_barrier(&ta->pi, mypid);
		if (rc < 0) {
			ckpt_perror("shared id barrier error", ERRNO);
			return (-1);
		}
		if (rc == 0)
			return (0);
	}
	/*
	 * Both real and effective are currently set to root.
	 * First change effective, then real
	 */
	if (syssgi(SGI_CKPT_SYS, CKPT_SETSUID, cred->pr_suid) < 0) {
		ckpt_perror("setsuid", ERRNO);
		return (-1);
	}
	if (setreuid((uid_t)-1, cred->pr_euid) < 0) {	/* seteuid() */
		ckpt_perror("seteuid", ERRNO);
		return (-1);
	}
	if (setreuid(cred->pr_ruid, (uid_t)-1) < 0) {	/* setruid() */
		ckpt_perror("seteuid", ERRNO);
		return (-1);
	}
	if (IS_SPROC(&ta->pi) && IS_SID(&ta->pi)) {

		rc = ckpt_sproc_unblock(mypid,
					sidlist->sid_count,
					sidlist->sid_pid);
		if (rc < 0) {
			ckpt_perror("ckpt_sproc_unblock", ERRNO);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_seteuid(ckpt_ta_t *ta, uid_t *uid, int entry)
{
	uid_t previous;
	ckpt_pi_t *pip = (ta)? &ta->pi : NULL;

	if (entry && pip && IS_SPROC(pip) && IS_SID(pip)) {
		if (ckpt_mutex_acquire(&sidlist->sid_uidlock) < 0)
			return (-1);

	}
	previous = geteuid();
	if (setreuid((uid_t)-1, *uid) < 0)	/* seteuid() */
		return (-1);

	if (!entry && pip && IS_SPROC(pip) && IS_SID(pip)) {
		/*
		 * reloease lock, unblock other procs
		 */
		if (ckpt_mutex_rele(&sidlist->sid_uidlock) < 0)
			return (-1);
	}
	*uid = previous;
	return (0);
}
/*
 * Open a file.  First try as user.  If this works, or user is
 * creating, return result.  (Note, that for NFS mounted, this is more
 * powerful then trying as root, since root gets mapped to nobody).  
 * If it fails, then try as root, then check permission on underlying object.
 */
int 
ckpt_open(ckpt_ta_t *ta, char *pathname, int oflag, int mode)
{
	uid_t was = 0;
	int fd, eflag = 0;

	fd = open(pathname, oflag, mode);
	if ((fd >= 0) || (oflag & O_CREAT))
		return (fd);

	if (ckpt_seteuid(ta, &was, 1) < 0)
		return (-1);

	fd = open(pathname, oflag, mode);

	if (ckpt_seteuid(ta, &was, 0) < 0) {
		close(fd);
		return(-1);
	}
	if (fd < 0)
		return (-1);

	if((oflag&0x3) == O_RDONLY) eflag =  R_OK;
	if((oflag&0x3) == O_WRONLY) eflag =  W_OK;
	if((oflag&0x3) == O_RDWR  ) eflag =  R_OK|W_OK;
	/* we want to check if the user has requested permission to access */
	if (syssgi(SGI_CKPT_SYS, CKPT_FDACCESS, fd, eflag|EFF_ONLY_OK) < 0) {
		close(fd);
		return (-1);
	}
	return (fd);
}

void
ckpt_perror(char *str, int errno)
{
	char errstr[36];

	write(*errfd_p, "CPR Error: ", ckpt_strlen("CPR Error: "));
	write(*errfd_p, str, ckpt_strlen(str));

	if (errno) {
		ckpt_itoa(errstr, errno);
		write(*errfd_p, ": errno ", ckpt_strlen(": errno "));
		write(*errfd_p, errstr, ckpt_strlen(errstr));
	}
	write(*errfd_p, ": Pid : ", ckpt_strlen(": Pid : "));
	ckpt_itoa(errstr, *errpid_p);
	write(*errfd_p, errstr, ckpt_strlen(errstr));
	write(*errfd_p, "\n", ckpt_strlen("\n"));
}

void
ckpt_abort(int cprfd, pid_t mypid, ulong pindex)
{
	char status = -1;

	ckpt_update_proclist_states(pindex, CKPT_PL_ERR);
	(void)write(cprfd, &status, 1);
	kill(mypid, SIGKILL);
	/* no return */
}

/*
 * ckpt_malloc/ckpt_free (use it only after ckpt_relvm)
 * 
 * simple memory allocator to avoid brk area - let the kernel find a place
 * that has nothing mapped - MAP_LOCAL to avoid collisions in case of sproc
 */
void *
ckpt_malloc(long nbytes)
{
	__int64_t *allocp;
	int fd;

	nbytes += sizeof(*allocp);

	fd = open("/dev/zero", O_RDWR);
	if (fd < 0)
		return (NULL);
	allocp = (__int64_t *)mmap(0, nbytes, PROT_READ|PROT_WRITE, MAP_LOCAL|MAP_PRIVATE, fd, 0);
	if (allocp == (__int64_t *)(-1L)) {
		ckpt_perror("ckpt_malloc", errno);
	}
	close(fd);
	*allocp = (__int64_t)nbytes;
	return ((void *)++allocp);
}

char *
ckpt_alloc_mempool(long len, char *mempool, long *used)
{
	*used += len;
	if (*used > MAXDATASIZE) {
		ckpt_perror("Out of memory buffer", 0);
		return (NULL);
	}
	return (mempool+*used-len);
}

void 
ckpt_free(void *ptr)
{
	__int64_t *allocp;

	allocp = (__int64_t *)ptr;
	--allocp;
	munmap_local((void *)allocp, *allocp);
}
/*
 * Special syscall section:
 */
/*
 * Wrapper for close syscall
 */
int
close(int fd)
{
	return(__close(fd));
}
int
pm_attach(pmo_handle_t pm_handle, void* base_addr, size_t length)
{
	vrange_t vrange;

	vrange.base_addr = (caddr_t)base_addr;
	vrange.length = length;

	return ((pmo_handle_t)pmoctl(PMO_PMATTACH, pm_handle, &vrange));
}

int
pm_destroy(pmo_handle_t pm_handle)
{
        return ((pmo_handle_t)pmoctl(PMO_PMDESTROY, pm_handle, NULL));
}

int
migr_range_migrate(void* base_addr, size_t length, pmo_handle_t pmo_handle)
{
	vrange_t vrange;

	vrange.base_addr = (caddr_t)base_addr;
	vrange.length = length;

	return ((int)pmoctl(PMO_USERMIGRATE, &vrange, pmo_handle));
}

/*
 * Special munmap_local
 */
int
munmap_local(void *addr, size_t len)
{
	return((int)syssgi(SGI_CKPT_SYS, CKPT_MUNMAP_LOCAL, addr, len));
}
/*
 * Basic support routines
 */
/*
 * Private copy of strcat...no libc refs here!
 */
void
ckpt_strcat(char *s1, char *s2)
{
	while (*s1)
		s1++;
	while (*s1++ = *s2++);
}

char *
ckpt_strcpy(char *des, char *src)
{
	char *origdes;

	origdes = des;
	while (*des++ = *src++);
	return (origdes);
}

int
ckpt_strlen(char *str)
{
	int n = 0;

	while(*str++)
		n++;
	return (n);
}

void
ckpt_itoa(char *str, int value)
{
	char buf[36];
	int len;

        len = sizeof(buf);
	buf[--len] = '\0';

	do {
                buf[--len] = '0' + (char)(value % 10);
                value = value/10;
        } while (value != 0);

	str[0] = '\0';
	ckpt_strcat(str, &buf[len]);
}

#ifdef DEBUG_FDS
void
ckpt_scan_fds(ckpt_ta_t *tp)
{
	int minfd = tp->pi.cp_maxfd+1;
	int prfd;
	int ttyfd;
	ckpt_statbuf_t sbuf;
	char buf[20];
	int len;
	int fd;
	dev_t dev;
	ino_t ino;

	if (IS_SPROC(&tp->pi) && IS_SFDS(&tp->pi)) {
		if (ckpt_test_then_add(&sfdlist->sfd_sync, 1) != (sfdlist->sfd_count - 1))
			return;
		sfdlist->sfd_sync = 0;
	}
	if ((ttyfd = open("/dev/tty", O_WRONLY)) < 0)
		return;
	if ((prfd = open(tp->procpath, O_RDONLY)) < 0) {
		close(ttyfd);
		return;
	}
	/*
	 * look for fds open above minfd
	 */
	sbuf.ckpt_fd = minfd;
	while (1) {
		if (ioctl(prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			if (sbuf.ckpt_fd == prfd) {
				sbuf.ckpt_fd++;
				continue;
			}
			close(prfd);
			close(ttyfd);
			return;
		}
		if (sbuf.ckpt_fd == ttyfd) {
			sbuf.ckpt_fd++;
			continue;
		}
		write(ttyfd, tp->procpath, ckpt_strlen(tp->procpath));
		write(ttyfd, ":danglng fd: ", ckpt_strlen(":danglng fd: "));

		fd = sbuf.ckpt_fd++;
		len = 19;

		while (fd != 0) {
			buf[len--] = '0' + (fd % 10);
			fd = fd/10;
		}
		write(ttyfd, &buf[len], 20-len);
		write(ttyfd, ":(dev ino): ", ckpt_strlen(":(dev ino): "));
		dev = sbuf.ckpt_dev;
		len = 19;

		while (dev != 0) {
			buf[len--] = '0' + (char)(dev % 10);
			dev = dev/10;
		}
		write(ttyfd, &buf[len], 20-len);
		write(ttyfd, " ", 1);
		ino = sbuf.ckpt_ino;
		len = 19;

		while (ino != 0) {
			buf[len--] = (char)((unsigned long)'0' + (ino % 10));
			ino = ino/10;
		}
		write(ttyfd, &buf[len], 20-len);
		write(ttyfd, "\n", 1);
	}
}
#endif
#if defined(DEBUG) || defined (DEBUG_READV)
void
ckpt_debug(pid_t pid, char *str, long decval, ulong_t hexval)
{
	char outbuf[160];
	char buf[21];
	int len = 0;
	int sign = 0;

	if (decval < 0) {
		sign++;
		decval = -decval;
	}
	/*
	 * write out the pid
	 */
	if (pid == 0)
		pid = debug_pid;

	ckpt_strcpy(outbuf, "pid:");

	buf[20] = '\0';
	len = 19;

	while (pid != 0) {
		buf[len--] = (char)('0' + (pid % 10));
		pid = pid/10;
	}
	buf[len] = '0';

	ckpt_strcat(outbuf, &buf[len]);
	ckpt_strcat(outbuf, ":");
	/*
	 * write out the string
	 */
	ckpt_strcat(outbuf, str);
	ckpt_strcat(outbuf, ":");
	/*
	 * write out decimal value
	 */
	buf[20] = '\0';
	len = 19;
	while (decval != 0) {
		buf[len--] = '0' + (char)(decval % 10);
		decval = decval/10;
	}
	buf[len] = '0';
	if (sign)
		ckpt_strcat(outbuf, "-");
	ckpt_strcat(outbuf, &buf[len]);
	ckpt_strcat(outbuf, ":");
	/*
	 * write out hex value
	 */
	buf[20] = '\0';
	len = 19;
	if (hexval == 0)
		buf[len--] = '0';
	else while (hexval != 0) {
		buf[len--] = "0123456789abcdef"[hexval&0xf];
		hexval = hexval >> 4;
	}
	buf[len--] = 'x';
	buf[len] = '0';
	ckpt_strcat(outbuf, &buf[len]);
	ckpt_strcat(outbuf, "\n");
	write(*errfd_p, outbuf, ckpt_strlen(outbuf));
}
#endif
#ifdef DEBUG_XXX
void
ckpt_hexdump(ulong_t num)
{
	int len = 19;
	char buf[20];

	while (num != 0) {
		buf[len--] = "0123456789abcdef"[num&0xf];
		num = num >> 4;
	}
	buf[len--] = 'x';
	buf[len] = '0';
	write(*errfd_p, &buf[len], 20-len);
	write(*errfd_p, " ", 1);
}
void
chksum(ckpt_memmap_t *memmap)
{
	char procpath[20];
	int myfd;
	char *pagebuf;
	int i;
	ulong len;
	uint sum = 0;
	uint *ptr;
	int nread;

	sprintf(procpath, "/proc/%d", mypid);
	myfd = open(procpath, O_RDONLY);
	if (myfd < 0) {
		ckpt_perror(*errfd_p, "/proc open", ERRNO);
		return;
	}
	pagebuf = ckpt_malloc(memmap->cm_pagesize);
	if (lseek(prfd, (off_t)memmap->cm_vaddr, SEEK_SET) != (off_t)memmap->cm_vaddr) {
		ckpt_perror(*errfd_p, "lseek", ERRNO);
		goto out;
	}
	for (len = 0; len < memmap->cm_len; len += memmap->cm_pagesize) {
		if ((nread = read(prfd, pagebuf, memmap->cm_pagesize)) < 0) {
			ckpt_perror(*errfd_p, "read proc chksum", ERRNO);
			goto out;
		}
		if (nread != memmap->cm_pagesize)
			ckpt_perror(*errfd_p, "short /proc read", 0);

		ptr = (uint *)pagebuf;
		for (i = 0; i < memmap->cm_pagesize/sizeof(uint); i++)
			sum = sum ^ ptr[i];
	}
out:
	close(myfd);

	ckpt_perror(*errfd_p, "Vaddr: ", ERRNO);
	ckpt_hexdump((ulong_t)memmap->cm_vaddr, *errfd_p);
	ckpt_perror(*errfd_p, " Sum: ", ERRNO);
	ckpt_hexdump(sum, *errfd_p);
	ckpt_perror(*errfd_p, "\n", ERRNO);
	ckpt_free(pagebuf);
}
#endif
#ifdef DEBUG_READV
void
ckpt_dump_iov(iovec_t *iov, int iovcnt)
{
	int i;

	ckpt_debug(0, "READV:", (long)iovcnt, (ulong_t)iovcnt);
	for (i = 0; i < iovcnt; i++)
		ckpt_debug(0, "iov", (long)iov[i].iov_len, (ulong_t)iov[i].iov_base);
}
#endif
#ifdef DEBUG
char *
ckpt_hexstr(char *buf, ulong_t num, int len)
{
	buf[--len] = 0;
	while (num != 0) {
		buf[--len] = "0123456789abcdef"[num&0xf];
		num = num >> 4;
	}
	buf[--len] = 'x';
	buf[--len] = '0';
	return (&buf[len]);
}
char *
ckpt_decstr(char *buf, ulong_t num, int len)
{
	buf[--len] = 0;
	do {
		buf[--len] = "0123456789"[num%10];
		num = num / 10;
	} while (num != 0);

	return (&buf[len]);
}
void
ckpt_log(char *log, char *str, long val1, long val2, long val3)
{
	int logfd;
	char buf[20];
	char outbuf[200];

	logfd = open(log, O_CREAT|O_WRONLY|O_APPEND, 0777);
	if (logfd < 0)
		return;

	ckpt_strcpy(outbuf, str);
	ckpt_strcat(outbuf, ":");
	ckpt_strcat(outbuf, ckpt_decstr(buf, val1, sizeof(buf)));
	ckpt_strcat(outbuf, ":");
	ckpt_strcat(outbuf, ckpt_decstr(buf, val2, sizeof(buf)));
	ckpt_strcat(outbuf, ":");
	ckpt_strcat(outbuf, ckpt_hexstr(buf, val3, sizeof(buf)));
	ckpt_strcat(outbuf, "\n");

	write(logfd, outbuf, ckpt_strlen(outbuf));
	close(logfd);
}
#endif
