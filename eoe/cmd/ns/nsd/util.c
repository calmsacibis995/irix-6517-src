#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <sys/procfs.h>
#include <alloca.h>
#include <paths.h>
#include <errno.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include <fcntl.h>

extern long nsd_timeout;
extern int nsd_silent;
extern int nsd_level;
extern nsd_file_t *__nsd_mounts;
extern nsd_libraries_t *nsd_liblist;

typedef struct nsd_bsize {
	void			*ptr;
	size_t			size;
	struct nsd_bsize	*next;
} nsd_bsize_t;
static nsd_bsize_t *bmaplist;

extern size_t __pagesize;

#define ROUNDUP(a, b) ((a > 0) ? (((a) + (b) - 1) / (b)) * (b) : (b))

/*
** This routine is called for large allocations.  Instead of using malloc
** which would enlarge our heap, we mmap /dev/zero.  We assume that size
** is a multiple of the page size.
*/
void *
nsd_bmalloc(size_t size)
{
	void *result;
	int fd;
	nsd_bsize_t *mem;

	mem = (nsd_bsize_t *)nsd_calloc(1, sizeof(nsd_bsize_t));
	if (! mem) {
		nsd_logprintf(NSD_LOG_RESOURCE, "nsd_bmalloc: failed calloc\n");
		return (void *)0;
	}

	fd = nsd_open("/dev/zero", O_RDWR, 0);
	if (fd < 0) {
		free(mem);
		return (void *)0;
	}
	result = nsd_mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (result == MAP_FAILED) {
		close(fd);
		free(mem);
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_bmalloc: mmap failed: %s\n", strerror(errno));
		return (void *)0;
	}
	close(fd);

	mem->ptr = result;
	mem->size = size;
	mem->next = bmaplist;
	bmaplist = mem;

	return result;
}

void
nsd_bfree(void *ptr)
{
	nsd_bsize_t *mem, **last;

	for (last = &bmaplist, mem = bmaplist; mem && (mem->ptr != ptr);
	    last = &mem->next, mem = mem->next);
	if (mem) {
		munmap(ptr, mem->size);
		*last = mem->next;
		free(mem);
	}
}

void *
nsd_brealloc(void *ptr, size_t size)
{
	void *new, *guess;
	nsd_bsize_t *mem, **last;
	int fd;

	/*
	** Look for previous mapping.
	*/
	for (last = &bmaplist, mem = bmaplist; mem && (mem->ptr != ptr);
	    last = &mem->next, mem = mem->next);
	
	/*
	** Just return if we all ready at least size.
	*/
	if (mem && (mem->size >= size)) {
		return mem->ptr;
	}

	/*
	** Attempt to resize the current map.
	*/
	if (mem) {
		fd = nsd_open("/dev/zero", O_RDWR, 0);
		if (fd) {
			guess = (char *)mem->ptr + mem->size;

			new = nsd_mmap(guess, size - mem->size,
			    PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
			close(fd);
			if (new != MAP_FAILED) {
				if (new == guess) {
					mem->size = size;
					return ptr;
				}
				munmap(new, size - mem->size);
			}
		}
	}

	/*
	** New map, or above resize failed.
	*/
	new = nsd_bmalloc(size);
	if (! new) {
		if (mem) {
			munmap(ptr, mem->size);
			*last = mem->next;
			free(mem);
		}
		return 0;
	}

	/*
	** Copy in data, and free old segment.
	*/
	if (mem) {
		memcpy(new, mem->ptr, mem->size);
		nsd_bfree(ptr);
	}

	return new;
}

int
nsd_set_result(nsd_file_t *rq, int status, char *data, int len,
    nsd_free_proc *freedata)
{
	char *olddata;
	nsd_free_proc *olddfree;
	size_t needed;

	if (! rq || ! data || len < 0) {
		return NSD_ERROR;
	}

	/*
	** Set the data.
	*/
	olddata = rq->f_data;
	olddfree = rq->f_free;
	if (! data) {
		rq->f_data = (char *)0;
		rq->f_free = (nsd_free_proc *)0;
		rq->f_used = rq->f_size = 0;
	} else if (freedata == DYNAMIC) {
		rq->f_data = data;
		rq->f_free = (nsd_free_proc *)free;
		rq->f_size = len;
		rq->f_used = (data[len - 1] == (char)0) ? len - 1 : len;
	} else if (freedata == VOLATILE) {
		/*
		** We always malloc an extra couple of bytes assuming that
		** the higher level code may come back right away and add
		** a newline.  We also always null terminate the string.
		*/
		while (data[len - 1] == 0) {
			len--;
		}
		needed = len + 2;
		if (needed > __pagesize) {
			needed = ROUNDUP(needed, __pagesize);
			rq->f_data = (char *)nsd_bmalloc(needed);
			if (! rq->f_data) {
				rq->f_data = olddata;
				return NSD_ERROR;
			}
			rq->f_free = nsd_bfree;
		} else {
			rq->f_data = (char *)nsd_malloc(needed);
			if (! rq->f_data) {
				rq->f_data = olddata;
				return NSD_ERROR;
			}
			rq->f_free = free;
		}
		memcpy(rq->f_data, data, len);
		rq->f_data[len] = (char)0;
		rq->f_size = needed;
		rq->f_used = len;
	} else {
		rq->f_data = data;
		rq->f_free = freedata;
		rq->f_size = len;
		rq->f_used = (data[len - 1] == (char)0) ? len - 1 : len;
	}

	/*
	** Set the status.
	*/
	rq->f_status = status;

	/*
	** Free any old information.
	*/
	if (olddata && olddfree) {
		(*olddfree)(olddata);
	}

	return NSD_OK;
}

/*
** This routine will reallocate the memory in the data section, and
** append the supplied data to the buffer.
*/
int
nsd_append_result(nsd_file_t *rq, int status, char *data, int len)
{
	char *new;
	size_t needed;

	if (! rq || ! data) {
		return NSD_ERROR;
	}

	while (data[len - 1] == (char)0) {
		len--;
	}
	if (rq->f_data && rq->f_size) {
		needed = rq->f_used + len;
		if (rq->f_size > needed) {
			memcpy(rq->f_data + rq->f_used, data, len);
			rq->f_data[needed] = (char)0;
			rq->f_used += len;
		} else {
			if (needed > __pagesize) {
				needed = ROUNDUP(needed + 1, __pagesize);
				new = (char *)nsd_bmalloc(needed);
				if (! new) {
					return NSD_ERROR;
				}
				memcpy(new, rq->f_data, rq->f_used);
				if (rq->f_free) {
					(*rq->f_free)(rq->f_data);
				}
				memcpy(new + rq->f_used, data, len);
				rq->f_data = new;
				rq->f_free = nsd_bfree;
				rq->f_size = needed;
				rq->f_used += len;
				new[rq->f_used] = (char)0;
			} else {
				new = (char *)nsd_malloc(needed + 1);
				if (! new) {
					return NSD_ERROR;
				}
				memcpy(new, rq->f_data, rq->f_used);
				memcpy(new + rq->f_used, data, len);
				new[needed] = (char)0;
				if (rq->f_free) {
					(*rq->f_free)(rq->f_data);
				}
				rq->f_data = new;
				rq->f_free = free;
				rq->f_size = needed + 1;
				rq->f_used = needed;
			}
		}
	} else {
		needed = len + 1;
		if (needed > __pagesize) {
			needed = ROUNDUP(needed, __pagesize);
			new = (char *)nsd_bmalloc(needed);
			if (! new) {
				return NSD_ERROR;
			}
			rq->f_free = nsd_bfree;
			rq->f_size = needed;
		} else {
			new = (char *)nsd_malloc(needed + 1);
			if (! new) {
				return NSD_ERROR;
			}
			if (rq->f_free) {
				(*rq->f_free)(rq->f_data);
			}
			rq->f_free = free;
			rq->f_size = needed + 1;
		}

		memcpy(new, data, len);
		new[len] = (char)0;
		rq->f_used = len;
		rq->f_data = new;
	}

	rq->f_status = status;

	return NSD_OK;
}

/*
** This routine is similar to append_result, but it sticks a newline
** between the entries.
*/
int
nsd_append_element(nsd_file_t *rq, int status, char *data, int len)
{
	char *new;
	size_t needed;

	if (! rq || ! data) {
		return NSD_ERROR;
	}

	while (data[len - 1] == (char)0) {
		len--;
	}

	needed = rq->f_used + len + 2;
	if (needed > __pagesize) {
		needed = ROUNDUP(needed, __pagesize);
		new = (char *)nsd_bmalloc(needed);
		if (! new) {
			return NSD_ERROR;
		}
		if (rq->f_data && rq->f_used) {
			memcpy(new, rq->f_data, rq->f_used);
			if (new[rq->f_used - 1] != '\n') {
				new[rq->f_used++] = '\n';
			}
		}
		if (rq->f_free && rq->f_data) {
			(*rq->f_free)(rq->f_data);
		}
		rq->f_free = nsd_bfree;
	} else {
		new = (char *)nsd_malloc(needed);
		if (! new) {
			return NSD_ERROR;
		}
		if (rq->f_data && rq->f_used) {
			memcpy(new, rq->f_data, rq->f_used);
			if (new[rq->f_used - 1] != '\n') {
				new[rq->f_used++] = '\n';
			}
		}
		if (rq->f_free && rq->f_data) {
			(*rq->f_free)(rq->f_data);
		}
		rq->f_free = free;
	}
	rq->f_size = needed;

	memcpy(new + rq->f_used, data, len);
	rq->f_used += len;
	new[rq->f_used] = (char)0;

	rq->f_data = new;
	rq->f_status = status;

	return NSD_OK;
}

/*
** This routine just prints a formated message to either the log or
** the screen.
*/
/*VARARGS2*/
void
nsd_logprintf(int level, char *format, ...)
{
	struct timeval t;
	va_list ap;
	int priority ;

	if (nsd_level >= level) {
		if (nsd_silent) {
			switch (nsd_level) {
			  case NSD_LOG_CRIT:
				priority=LOG_CRIT;
				break;
			  case NSD_LOG_RESOURCE:
				priority=LOG_ERR;
				break;
			  case NSD_LOG_OPER:
				priority=LOG_NOTICE;
				break;
			  case NSD_LOG_MIN:
			  case NSD_LOG_LOW:
			  case NSD_LOG_HIGH:
			  case NSD_LOG_MAX:
			  default:
				priority=LOG_DEBUG;
			}
			va_start(ap, format);
			vsyslog(priority, format, ap);
			va_end(ap);
		} else {
			(void) gettimeofday(&t, NULL);
			(void) fprintf(stderr, "%19.19s: ", ctime(&t.tv_sec));
			va_start(ap, format);
			(void) vfprintf(stderr, format, ap);
			va_end(ap);
			fflush(stderr);
		}
	}
}

/*
** This routine attempts to free up any memory or cached space.
*/
void
nsd_shake(int priority)
{
	nsd_libraries_t *ll;
	time_t now, incr;
	int i;

	incr = (time_t)(nsd_timeout / 10000);
	for (time(&now), i = 0; i < priority; now += incr, i++);
#if 0
	nsd_file_timeout((nsd_file_t **)0, now);
#endif

	if (priority > 2) {
		nsd_portmap_shake();
		nsd_map_close_all();
	} else {
		nsd_map_shake(priority, now);
	}

	for (ll = nsd_liblist; ll; ll = ll->l_next) {
		if (ll->l_funcs[SHAKE]) {
			(*ll->l_funcs[SHAKE])(priority, now);
		}
	}
}

/*
** This is an inline wrapper around malloc to attempt a shake if the
** allocation fails.
*/
void *
nsd_malloc(size_t size)
{
	void *result;

	result = malloc(size);
	if (! result) {
		nsd_shake(9);
		result = malloc(size);
	}

	return result;
}

/*
** This is an inline wrapper around calloc to attempt a shake if the
** allocation fails.
*/
void *
nsd_calloc(size_t count, size_t size)
{
	void *result;

	result = calloc(count, size);
	if (! result) {
		nsd_shake(9);
		result = calloc(count, size);
	}

	return result;
}

/*
** This is an inline wrapper around realloc to attempt a shake if the
** allocation fails.
*/
void *
nsd_realloc(void *ptr, size_t size)
{
	void *result;

	result = realloc(ptr, size);
	if (! result) {
		nsd_shake(9);
		result = realloc(ptr, size);
	}

	return result;
}

/*
** This duplicates a string using the above malloc routines with shake
** extensions.
*/
char *
nsd_strdup(char *s) {
	int len;
	char *result;

	len = strlen(s);
	result = nsd_malloc(len + 1);
	if (result) {
		memcpy(result, s, len);
		result[len] = (char)0;
	}

	return result;
}

/*
** This is a wrapper around mmap which will call shake and retry if the
** map fails.
*/
void *
nsd_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *result;

	result = mmap(addr, len, prot, flags, fd, off);
	if ((result == MAP_FAILED) &&
	    ((errno == EAGAIN) || (errno == ENOMEM))) {
		nsd_shake(9);
		result = mmap(addr, len, prot, flags, fd, off);
	}

	return result;
}

/*
** This is a wrapper around open which will call shake and retry on failure.
*/
int
nsd_open(const char *file, int flags, mode_t mode)
{
	int result;

	result = open(file, flags, mode);
	if (result < 0 && (errno == EMFILE)) {
		nsd_shake(9);
		result = open(file, flags, mode);
	}

	return result;
}

/*
** This is a wrapper around mdbm_open which will call shake and retry on
** failure.
*/
MDBM *
nsd_mdbm_open(char *file, int flags, mode_t mode, int psize)
{
	MDBM *result;

	result = mdbm_open(file, flags, mode, psize);
	if (! result) {
		/* 
		** if the file is not a valid mdbm file, recreate it.
		*/
		if (errno == ESTALE || errno == EBADF || errno == EBADFD) {
			nsd_logprintf(NSD_LOG_MIN, 
				      "Recreating invalide cache file %s", 
				      file);
			result = mdbm_open(file, flags|O_CREAT|O_TRUNC, mode, 
					   psize);
		} else {
			nsd_logprintf(NSD_LOG_MIN,
			    "nsd_mdbm_open(%s) failed with errno %d: %s\n",
			    file, errno, strerror(errno));
			nsd_shake(9);
			result = mdbm_open(file, flags, mode, psize);
		}
	}

	return result;
}

/*
** This is a hack.  All the getXbyY() calls call ns_lookup() which turns
** around and calls us, so we end up hanging on ourselves.  By including
** this ns_lookup() it allows us to use all the getXbyY calls to read
** the local files.
*/
/* ARGSUSED */
int
_ns_lookup(ns_map_t *map, char *domain, char *table, const char *key,
    char *library, char *buf, size_t len)
{
	return NS_FATAL;
}

/*
** Same thing as the above.  All the getXent() calls will result in a
** recursive call which hangs, so this routine exists in case protocol
** libraries try to use one of the name service API routines.
*/
/* ARGSUSED */
FILE *
_ns_list(ns_map_t *map, char *domain, char *table, char *library)
{
	return (FILE *)0;
}

/*
** This routine is used by nsd_local below.  It just builds up the list
** of local addresses.
*/
static struct timeval when_local;
static int num_local = 0;
static union addrs {
	char			buf[1];
	struct ifa_msghdr	ifam;
	struct in_addr		a[1];
} *addrs;
static size_t addrs_size = 0;

static void
getlocal(void)
{
	int i;
	size_t needed;
	struct timeval now;
	int mib[6];
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct sockaddr *sa;

	/*
	** Get current addresses periodically, in case interfaces change.
	*/
	gettimeofday(&now);
	if (addrs_size > 0 && now.tv_sec < when_local.tv_sec+60) {
		return;
	}
	when_local = now;
	num_local = 0;

	/*
	** Fetch the interface list, without too many system calls since
	** we do it repeatedly.
	*/
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	for (;;) {
		if ((needed = addrs_size) != 0) {
			if (sysctl(mib, 6, addrs, &needed, 0, 0) >= 0) {
				break;
			}
			if (errno != ENOMEM && errno != EFAULT) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "sysctl failed: %s\n", strerror(errno));
				return;
			}
			free(addrs);
			needed = 0;
		}
		if (sysctl(mib, 6, 0, &needed, 0, 0) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE, "sysctl failed: %s\n",
			    strerror(errno));
			return;
		}
		addrs = (union addrs *)nsd_calloc(1, addrs_size = needed);
	}

	ifam_lim = (struct ifa_msghdr *)(addrs->buf + needed);
	for (ifam = &addrs->ifam; ifam < ifam_lim; ifam = ifam2) {
		ifam2 = (struct ifa_msghdr*)((char*)ifam + ifam->ifam_msglen);

		if (ifam->ifam_type == RTM_IFINFO) {
			ifm = (struct if_msghdr *)ifam;
			continue;
		}
		if (ifam->ifam_type != RTM_NEWADDR) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "unable to parse sysctl result\n");
			return;
		}
		if (!(ifm->ifm_flags & IFF_UP)) {
			continue;
		}

		/*
		** Find the interface address among the other addresses.
		*/
		sa = (struct sockaddr *)(ifam+1);
		for (i = 0; i <= RTAX_IFA && sa < (struct sockaddr *)ifam2;
		    i++) {
			if ((ifam->ifam_addrs & (1 << i)) == 0) {
				continue;
			}
			if (i == RTAX_IFA) {
				break;
			}
#ifdef _HAVE_SA_LEN
			sa = (struct sockaddr *)((char*)sa
			    + ROUNDUP(sa->sa_len, sizeof(__uint64_t)));
#else
			sa = (struct sockaddr *)((char*)sa +
			    ROUNDUP(_FAKE_SA_LEN_DST(sa), sizeof(__uint64_t)));
#endif
		}
		if (i > RTAX_IFA
#ifdef _HAVE_SA_LEN
		    || sa->sa_len == 0
#endif
		    || sa >= (struct sockaddr *)ifam2
		    || sa->sa_family != AF_INET) {
			continue;
		}
		addrs->a[num_local++] = ((struct sockaddr_in *)sa)->sin_addr;
	}
}


/*
** This routine will check that an address is local.  This includes
** loopback, the primary address of each interface, and any aliases.
*/
int
nsd_local(struct sockaddr_in *sin)
{
	int i;

	if (! sin) {
		return FALSE;
	}

	if (sin->sin_addr.s_addr == INADDR_LOOPBACK) {
		return TRUE;
	}

	getlocal();
	for (i = 0; i < num_local; i++) {
		if (sin->sin_addr.s_addr == addrs->a[i].s_addr) {
			return TRUE;
		}
	}

	return FALSE;
}

void
nsd_dump(FILE *fp)
{
	nsd_libraries_t *ll;
	extern char * __nsd_version;
	extern char * nsd_program;
	extern struct sockaddr_in _nfs_sin;

	fprintf(fp, "nsd operating parameters:\n\n");
	fprintf(fp, "log level = %d\n", nsd_level);
	fprintf(fp, "nfs server port = %d\n", _nfs_sin.sin_port);
	fprintf(fp, "working directory = %s\n", NS_HOME_DIR);

	fprintf(fp, "\nWalking directory tree:\n\n");
	nsd_file_list(0, fp, 0);

	fprintf(fp, "\nWalking BTree:\n\n");
	nsd_blist(0, fp);

	for (ll = nsd_liblist; ll; ll = ll->l_next) {
		if (ll->l_funcs[DUMP]) {
			fprintf(fp, "\nLibrary status for: %s\n\n", ll->l_name);
			(*ll->l_funcs[DUMP])(fp);
		}
	}
}

/*
** This is a simple routine to cat multiple strings into a buffer.
*/
/*VARARGS2*/
int
nsd_cat(char *buf, int len, int count, ...)
{
	va_list ap;
	int i=0;
	char *q;

	if (! buf || len < 1) {
		return 0;
	}

	len--;
	va_start(ap, count);
	while (count--) {
		q = va_arg(ap, char *);
		for (; *q && (i < len); *buf++ = *q++, i++);
	}
	va_end(ap);
	*buf = 0;
	
	return i;
}
