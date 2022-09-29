# include	"grosview.h"

# include	<sys/mman.h>
# include	<sys/sbd.h>
# include	<fcntl.h>
# include	<nlist.h>
# include	<string.h>
# include	<unistd.h>

# include	<elf.h>

extern char	*vmunix;
static char	*mem = "/dev/kmem";

# define MAXNL		100

static int	memfd = -1;

#define coreadj(x)	((long)x & 0x7fffffff)
#define coreadj64(x)	((x) & 0x7fffffffffffffffLL)

static int is64bitobj;
static int testelf64(char *);

static struct nlist64 nl64[MAXNL] = {
#ifdef notdef
	{ "avenrun", 0, 0, 0 },
	{ "_physmem_start", 0, 0, 0},		/* new in /bvd for IP20 */
#endif
	{ 0, 0, 0, 0 }
};

static struct nlist nl[MAXNL] = {
#ifdef notdef
	{ "avenrun", 0, 0, 0 },
#define PHYSMEM_START 1
	{ "_physmem_start", 0, 0, 0},		/* new in /bvd for IP20 */
#endif
	{ 0, 0, 0, 0 }
};

void
stg_open(void)
{
   int		i;
#ifdef notdef
   int		err;
#endif

	if (memfd >= 0)
		return;

	if ((memfd = open(mem, O_RDONLY)) == -1) {
		fprintf(stderr, "can't open memory special file\n");
		exit(1);
	}

	is64bitobj = testelf64(vmunix);

	if (is64bitobj) {
		if (nlist64(vmunix, nl64) == -1) {
			fprintf(stderr, "can't open namelist file\n");
			exit(1);
		}
		for (i = 0; nl64[i].n_name != 0; i++) {
			nl[i].n_value = coreadj(nl64[i].n_value);
			nl[i].n_type = nl64[i].n_type;
		}
	} else {
		if (nlist(vmunix, nl) == -1) {
			fprintf(stderr, "can't open namelist file\n");
			exit(1);
		}
	}
#ifdef notdef
	for (err = 0, i = 0; nl[i].n_name != 0; i++)
		if (nl[i].n_value == 0 && i != PHYSMEM_START) {
			fprintf(stderr, "can't get value of \"%s\"\n",
				nl[i].n_name);
			err++;
		}
	if (err)
		exit(1);
#endif
}

off64_t
stg_getval(char *s)
{
   int		i;

	stg_open();
	for (i = 0; nl[i].n_name != 0; i++)
		if (strcmp(s, nl[i].n_name) == 0)
			return(nl[i].n_value);
	if (i >= MAXNL) {
		fprintf(stderr, "out of namelist storage space\n");
		exit(1);
	}

	if (is64bitobj) {
		nl64[i+1].n_name = 0;
		nl64[i].n_name = s;
		nl64[i].n_value = 0;
		if (nlist64(vmunix, &nl64[i]) == -1) {
			fprintf(stderr, "can't re-open namelist file\n");
			exit(1);
		}
		nl[i].n_value = coreadj(nl64[i].n_value);
		nl[i].n_type = nl64[i].n_type;
	} else {
		nl[i+1].n_name = 0;
		nl[i].n_name = s;
		nl[i].n_value = 0;
		if (nlist(vmunix, &nl[i]) == -1) {
			fprintf(stderr, "can't re-open namelist file\n");
			exit(1);
		}
	}
	if (nl[i].n_value == 0) {
		fprintf(stderr, "can't get value of \"%s\"\n", s);
		exit(1);
	}
        if (is64bitobj) {
	    return(nl64[i].n_value);
	}
        else {
	    return((off64_t) nl[i].n_value);
	}
}

#ifdef notdef
void
stg_sread(char *s, void *buf, int len)
{
   long		oval;

	stg_open();
	oval = coreadj(stg_getval(s));
	if (lseek(memfd, oval, 0) == -1) {
		fprintf(stderr, "error seeking to %#x on memory\n", oval);
		exit(1);
	}
	if (read(memfd, buf, len) != len) {
		fprintf(stderr, "error reading from memory\n");
		exit(1);
	}
}
#endif

/*
 * read a kernel address stored at a symbol
 */
void
stg_sptr_read(char *s, off64_t *buf)
{
	off64_t oval64;
	long oval;
	void *ptr;

	int len;

	stg_open();
	if (is64bitobj) {
		oval64 = coreadj64(stg_getval(s));
		len = 8;
		ptr = &oval64;
	}
	else {
		oval64 = coreadj(stg_getval(s));
		len = 4;
		ptr = &oval;
	}
	if (lseek64(memfd, oval64, 0) == -1) {
		fprintf(stderr, "error seeking to %#llx on memory\n", oval64);
		exit(1);
	}
	if (read(memfd, ptr, len) != len) {
		fprintf(stderr, "error reading from memory\n");
		exit(1);
	}
	if (is64bitobj) {
		*buf = oval64;
	}
	else {
		*buf = oval;
	}
}

void
stg_vread(off64_t addr, void *buf, int len)
{
	off64_t oval;

	stg_open();
	oval = (addr & ~K0BASE);
	if (lseek64(memfd, oval, 0) == -1) {
		fprintf(stderr, "error seeking to %#llx on memory\n", addr);
		exit(1);
	}
	if (read(memfd, buf, len) != len) {
		fprintf(stderr, "error reading from memory\n");
		exit(1);
	}
}

static int
testelf64(char *img)
{
	int ufd;
	Elf32_Ehdr ehdr;


	if ((ufd = open(img, O_RDONLY, 0)) < 0) {
		fprintf(stderr, "can't open namelist file\n");
		exit(1);
	}

	/* Pull in the elf header */
	if (read(ufd, (char *)&ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
		(void) close(ufd);
		return(0);
	}

	/* make sure this is an ELF kernel */
	if( !IS_ELF(ehdr) ) {
		(void) close(ufd);
		return(0);
	}

	(void)close(ufd);

	/* See if we're 64bit or not */
	return (ehdr.e_ident[EI_CLASS] == ELFCLASS64);
}
