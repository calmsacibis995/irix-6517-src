#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <abi_mutex.h>
#include <malloc.h>
#include <unistd.h>
#include <ns_api.h>
#include <ns_daemon.h>

typedef struct mem {
	void		*src;
	size_t		len;
	struct mem	*next;
} mem_t;

typedef struct arena {
	void		*arena;
	mem_t		*parts;
	struct arena	*next;
} arena_t;
arena_t *arenas = 0;
abilock_t alock = {0};

static void *
grow(size_t len, void *arena)
{
	arena_t *ap;
	mem_t *mp;
	void *vp;
	int fd;
	void *guess;

	fd = nsd_open("/dev/zero", O_RDWR, 0);
	if (fd < 0) {
		return (void *)-1;
	}

	for (ap = arenas; ap && (ap->arena != arena); ap = ap->next);

	guess = (ap && ap->parts) ? (char *)ap->parts->src + ap->parts->len : 0;

	vp = nsd_mmap(guess, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (vp == MAP_FAILED) {
		return (void *)-1;
	}

	if (guess && (vp == guess)) {
		ap->parts->len += len;
		return vp;
	}

	mp = nsd_calloc(1, sizeof(*mp));
	if (! mp) {
		munmap(vp, len);
		return (void *)-1;
	}
	mp->src = vp;
	mp->len = len;

	if (ap) {
		mp->next = ap->parts;
		ap->parts = mp;
	} else {
		ap = nsd_calloc(1, sizeof(*ap));
		if (! ap) {
			munmap(vp, len);
			free(mp);
			return (void *)-1;
		}
		ap->arena = (arena) ? arena : vp;
		ap->parts = mp;
		ap->next = arenas;
		arenas = ap;
	}

	return vp;
}

void
m_release(void *arena)
{
	arena_t *ap, **end;
	mem_t *mp;

	spin_lock(&alock);
	
	for (end = &arenas, ap = arenas; ap && (ap->arena != arena);
	    end = &ap->next, ap = ap->next);
	if (ap) {
		for (mp = ap->parts; mp; mp = ap->parts) {
			ap->parts = mp->next;
			munmap(mp->src, mp->len);
			free(mp);
		}
		*end = ap->next;
		free(ap);
	}

	release_lock(&alock);
}

void *
m_arena(void)
{
	void *vp, *r;

	spin_lock(&alock);

	vp = grow(1024, 0);
	if (! vp) {
		return (void *)-1;
	}

	r = acreate(vp, 1024, 0, 0, grow);
	if (! r) {
		m_release(vp);
		release_lock(&alock);
		return (void *)vp;
	}

	release_lock(&alock);
	return r;
}

char *
m_strdup(char *s, size_t len, void *a)
{
	char *r;

	if (! len) {
		len = strlen(s);
	}
	if (a) {
		spin_lock(&alock);
		r = amalloc(len + 1, a);
		release_lock(&alock);
	} else {
		r = nsd_malloc(len + 1);
	}
	if (r) {
		memcpy(r, s, len);
		r[len] = 0;
	}

	return r;
}

void *
m_malloc(size_t len, void *a)
{
	void *vp;

	if (a) {
		spin_lock(&alock);
		vp = amalloc(len, a);
		release_lock(&alock);
		return vp;
	} else {
		return nsd_malloc(len);
	}
}

void *
m_calloc(int c, size_t len, void *a)
{
	void *vp;

	if (a) {
		spin_lock(&alock);
		vp = acalloc(c, len, a);
		release_lock(&alock);
		return vp;
	} else {
		return nsd_calloc(c, len);
	}
}

void *
m_realloc(void *m, size_t len, void *a)
{
	void *vp;

	if (a) {
		spin_lock(&alock);
		vp = arealloc(m, len, a);
		release_lock(&alock);
		return vp;
	} else {
		return nsd_realloc(m, len);
	}
}

void
m_free(void *m, void *a)
{
	if (a) {
		spin_lock(&alock);
		afree(m, a);
		release_lock(&alock);
	} else {
		free(m);
	}
}
