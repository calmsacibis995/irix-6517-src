/*
 * This program does a symbolic dump of pages within a shared object data
 * segment that have a usage less than the optional threshold (default 20%).
 * (This can also be used on a standard binary)
 *
 * usage: solocal [-Omdtasuf#] [-Apath] [-p#|name] [-Spath] \
 *			library-path-name [threshold]
 *
 *		-Apath-to-archive	Cross reference with Archive info
 *		-m			Dump out proposed makefile object ordering
 *		-O			Just dump symbol/object with filter
 *		-Pname			Give priority to process usage
 *		-Spath			Use snap file for usage deltas
 *		-T#			Gather text fault info # times
 *              -a			Collect info from swap too
 *              -s			Quick symbol dump by size
 *		-u			Dump symbol table in usage order
 *              -f#			Filter variables with usage < # 
 *              -p#|name		Get data only for process pid# or name
 *`		-t			Trace - print lots of stuff to verify
 *					 that symbol lookup is working.
 *		-d			Give details on written areas
 *              library-path-name	Path name to shared object
 *              threshold		Percent of page dirty threshold 
 */

/*
 * Note this program needs to be completely rewritten.
 */

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ustat.h>
#include <ftw.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <mntent.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/procfs.h>
#include <paths.h>
#include <libw.h>
#include <locale.h>
#include <fmtmsg.h>
#include <stdarg.h>
#include <sgi_nl.h>

/*
 * System page size and object name
 */
int pagesize;
char *objectname;
int dflg, Tflg;

/*
 * Path to /proc filesystem
 */
char prpathname[BUFSIZ];

/*
 * process information structure from /proc
 */
struct prpsinfo info;

/*
 * macro to print and clear a set of flags
 */
#define FLAGSPRF(maskt, maskc, name) { \
	if (flags & (maskt)) { \
		printf(name); \
		flags &= ~(maskc); \
		if (flags) printf("/"); \
	} \
}

/* symbol sorting flags (prsortinsert) */
#define BYDSIZE		1	/* decreasing size (largest first) */
#define BYIUSAGE	2	/* increasing usage */
#define BYDUSAGE	3	/* decreasing usage */
#define BYVADDR		4	/* by increasing virtual address */

/*
 * process region map information from /proc
 */
#define MAXMAPS         256
struct prmap_sgi maps[MAXMAPS];

/*
 * Table of object symbol data
 */
static struct symbol {
	long	vaddr;
	char	*name;
	char	*weak;
	char	*archive;
	long	type;
	long	size;
	long	usage;
	struct symbol *next, *sort;
} *syms, *syme, *sorts, *files;
static long vstart, vbss, vrdata, vtext;

static struct symbol *
prlookupsym(char *name, char type, struct symbol *symb)
{
	struct symbol *sym;

	/* Find first fit */
	for (sym = symb; sym; sym = sym->next) {
		/* When making a .so ld seems to change some symbols
		 * from B to D
		 */
		if (type != sym->type &&
				(type != 'B' || sym->type != 'D') &&
				(type != 'b' || sym->type != 'd'))
			continue;
		if (sym->name && strcmp(name, sym->name) == 0)
			return sym;
		if (sym->weak && strcmp(name, sym->weak) == 0)
			return sym;
	}

	return NULL;
}

static struct symbol *
prfindsym(long vaddr)
{
	register struct symbol *sym, *symb, *prev = NULL;
	static struct symbol *save;

	/* Check last one for match (fast path!) */
	if (save) {
		if (vaddr >= save->vaddr) {
			if (save->next) {
				if (save->next->vaddr > vaddr)
					return save;
			} else {
				return save;
			}
			symb = save;
		} else {
			symb = syms;
		}
	} else {
		symb = syms;
	}

	/* Find first fit (assumes table sorted in ascending order) */
	for (sym = prev = symb; sym; sym = sym->next) {
		if (vaddr < sym->vaddr) {
			save = prev;
			return prev;
		}
		prev = sym;
	}
	save = prev;

	return prev;
}

static struct symbol *
prnewsym(char *str, long vaddr, char type, int weak)
{
	struct symbol *nsym;
	char *newname = strdup(str);

	/* Weak binding to real symbol name? (vis-versa also) */
	if ((nsym = prfindsym(vaddr)) != NULL) {
		if (nsym->type == type && nsym->vaddr == vaddr) {
			if (weak)
				nsym->weak = newname;
			else
				nsym->name = newname;
			return NULL;
		}
	}

	/* Hack, compiler is broken */
	if (!strcmp(str, "__program_header_table"))
		return NULL;
	if (!strcmp(str, "__elf_header"))
		return NULL;

	/* Create new symbol */
	nsym = malloc(sizeof (struct symbol));
	nsym->vaddr = vaddr;
	nsym->name = nsym->weak = NULL;
	nsym->type = type;
	nsym->size = nsym->usage = 0;
	nsym->archive = NULL;
	nsym->next = NULL;
	nsym->sort = NULL;

	/* Assign string to correct name slot - always make 'name' valid */
	if (weak)
		nsym->weak = newname;
	nsym->name = newname;
	return (nsym);
}

static void
prsyminsert(char *str, long vaddr, char type)
{
	struct symbol *symi, *nsym;

	/* Get symbol at that location */
	symi = prfindsym(vaddr);

	/* Create new symbol */
	if ((nsym = prnewsym(str, vaddr, type, 0)) == NULL)
		return;

	/* the new symbol might actually have to go before 'symi' */
	if (vaddr < symi->vaddr) {
		nsym->next = symi;
		syms = nsym;
		nsym->size = symi->vaddr - vaddr;
	} else {
		/* Add our new guy after it */
		nsym->next = symi->next;
		if (symi->next == NULL) {
			syme = nsym;
		} else {
			nsym->size = symi->next->vaddr - nsym->vaddr;
		}
		symi->next = nsym;
		symi->size = nsym->vaddr - symi->vaddr;
	}
}

static void
prsortinsert(struct symbol *sym, int flag)
{
	struct symbol *next, *prev = NULL;

	/* Search for insert position */
	for (next = sorts; next; next = next->sort) {
		if (flag == BYDSIZE) {
			/* Sort by (decreasing) size */
			if (next->size < sym->size)
				break;
		} else if (flag == BYIUSAGE) {
			/* Sort by increasing usage */
			if (next->usage > sym->usage)
				break;
		} else if (flag == BYDUSAGE) {
			/* Sort by decreasing usage */
			if (next->usage < sym->usage)
				break;
		} else if (flag == BYVADDR) {
			/* sort by increasing vaddr */
			if (next->vaddr > sym->vaddr)
				break;
		}
		prev = next;
	}

	/* Add new guy to list */
	if (prev == NULL) {
		sym->sort = sorts;
		sorts = sym;
	} else {
		/* insert before 'next' */
		sym->sort = next;
		prev->sort = sym;
	}
}

static void
prgetfiles(char *aname, int usage)
{
	struct symbol *symi;

	/* Search for existing entry with this name */
	for (symi = files; symi; symi = symi->next) {
		if (strcmp(symi->name, aname) == 0) {
			if (symi->usage < usage)
				symi->usage = usage;
			return;
		}
	}

	/* Add new guy to list */
	symi = prnewsym(aname, 0L, 'A', 0);
	symi->usage = usage;
	symi->next = files;
	files = symi;
}

static int
prgetsyms(char *libname)
{
	struct symbol *nsym;
	long vaddr, size;
	char str[BUFSIZ], line[BUFSIZ], type, ltype;
	int weak;
	FILE *pp;

	sprintf(str, "nm -x -Bn %s", libname);
	if ((pp = popen(str, "r")) != NULL) {
		while (fgets(line, BUFSIZ, pp) != NULL) {
			sscanf (line, "%lx %c %s", &vaddr, &type, str);
			if (vaddr == 0)
				continue;
			ltype = isupper(type) ? tolower(type) : type;
			if (!vrdata && (ltype == 'r'))
				vrdata = vaddr;
			if (!vtext && (ltype == 't'))
				vtext = vaddr;
			if (!Tflg && ((ltype == 't') || (ltype == 'r')))
				continue;
			if ((ltype == 'u') || (type == 'a'))
				continue;
			if ((ltype == 'i') || (ltype == 'f'))
				continue;
			if (type == 'A') {
				/* Hack to get GP[] table */
				if (strcmp(str, "_gp_disp") == 0) {
					prsyminsert("GOT", vaddr -
						(32*1024 - 16), 'G');
				}
				continue;
			}
			if (str[0] == '.') {
				if (!strcmp(str, ".data"))
					vstart = vaddr;
				if (!vstart && !strcmp(str, ".sdata"))
					vstart = vaddr;
				continue;
			}
			if (!vbss && ((ltype == 'b') || (ltype == 's')))
				vbss = vaddr;
			if (!vstart && ((ltype == 'd') || (ltype == 'g')))
				vstart = vaddr;
			if (syme) {
				syme->size = vaddr - syme->vaddr;
			}

			/* Create new symbol */
			weak = (line[strlen(line) - 2] == ')');
			if (!(nsym = prnewsym(str, vaddr, type, weak))) {
				continue;
			}

			/* Add to end of list */
			if (syms == NULL) {
				syms = syme = nsym;
			} else {
				syme->next = nsym;
				syme = nsym;
			}
		}
		pclose (pp);
	} else {
		return 0;
	}

	/* use size to get got table */
	sprintf(str, "size -x %s", libname);
	if ((pp = popen(str, "r")) != NULL) {
		int n;
		while (fgets(line, BUFSIZ, pp) != NULL) {
			n = sscanf (line, " %s %lx %lx", str, &size, &vaddr);
			if (n != 3)
				continue;
			if (strcmp(str, ".got"))
				continue;
			prsyminsert(str, vaddr, 'G');

		}

		pclose (pp);
		return 1;
	}
	return 0;
}

static
prgetarchive(char *archname)
{
	struct symbol *sym, *symb;
	long vaddr;
	char file[BUFSIZ], str[BUFSIZ], line[BUFSIZ], type;
	FILE *pp;

	sprintf(str, "nm -x -Bon %s", archname);
	if ((pp = popen(str, "r")) != NULL) {
		while (fgets(line, BUFSIZ, pp) != NULL) {
			if (isspace(line[0]))
				continue;
#if OLD
			/* nm output changed again */
			if (!isdigit(line[0])) {
				strcpy(file, line);
				file[strlen(file) - 2] = NULL;
				continue;
			}
#endif
			sscanf (line, "%s %x %c %s", file, &vaddr, &type, str);
			if (!Tflg && (type == 't' || type == 'T'))
				continue;
			if (!Tflg && (type == 'r' || type == 'R'))
				continue;
			if (type == 'a' || type == 'A')
				continue;
			if (type == 'u' || type == 'U')
				continue;
			if (type == 'n' || type == 'N')
				continue;
			if ((type == 'I') || (type == 'F'))
				continue;
			if (str[0] == '.')
				continue;
			if (type == 'C')
				type = 'B';
			symb = syms;
again:
			if ((sym = prlookupsym(str, type, symb)) == NULL)
				continue;
			if (sym->archive == NULL) {
				char *p = strchr(file, ':');
				char *p1;
				if (p == NULL) {
					printf("unk nm format <%s>\n", line);
					exit(1);
				}
				p++;
				if ((p1 = strchr(p, ':')) == NULL) {
					printf("unk nm format2 <%s>\n", line);
					exit(1);
				}
				*p1 = '\0';
				sym->archive = strdup(p);
			} else {
				symb = sym->next;
				goto again;
			}
		}
		pclose (pp);

		return 1;
	}

	return 0;
}

/*
 * Save text region access info
 */
static struct srtext {
	short		*abuf;
	long		vaddr;
	int		bufsize;
	int		offset;
	prpgd_sgi_t	*tpbuf;
	int		tvalid, tdyn, trdata, ttext, tsize;
} txtinfo;
#define	TRND	4				/* Round to TGRAIN...... */
#define TGRAIN	8				/* 8 bytes resolution... */
#define	TELEMN	(TGRAIN / sizeof (short))

static void
prsavetext(int fd, struct prmap_sgi *map, int flag)
{
	register pgd_t *pgd;
	register short *a;
	register int cnt;

#ifdef notyet
	/* Paranoid */
	if ((long)map->pr_vaddr != vstart)
		return;
#endif

	/* Allocate space */
	if (txtinfo.abuf == NULL) {
		/* All the maps are the same virtual size */
		txtinfo.vaddr = (long)map->pr_vaddr;
		txtinfo.bufsize = map->pr_size;
		txtinfo.offset = map->pr_off;
		txtinfo.abuf = (short *)calloc(1, map->pr_size / TELEMN);
		txtinfo.tpbuf = (prpgd_sgi_t *)malloc(sizeof (prpgd_sgi_t) +
			(map->pr_size / pagesize) * sizeof (pgd_t));

		/* Enable CLEAR flag on all pages */
		for (cnt = 0, pgd = txtinfo.tpbuf->pr_data;
		     cnt < map->pr_size / pagesize;
		     cnt++, pgd++) {
			pgd->pr_flags = flag ? PGF_CLEAR : 0;
			pgd->pr_value = 0;
		}
	}

	/* Get page table info */
	txtinfo.tpbuf->pr_vaddr = map->pr_vaddr;
	txtinfo.tpbuf->pr_pglen = map->pr_size / pagesize;
	if (ioctl(fd, PIOCPGD_SGI, txtinfo.tpbuf) < 0) {
		perror("ioctl(PIOCPGD_SGI)");
		return;
	}
	if (!flag) {
		return;
	}
	
	/* Save vfault info in accumulation array */
	for (cnt = 0, pgd = txtinfo.tpbuf->pr_data, a = txtinfo.abuf;
	     cnt < txtinfo.tpbuf->pr_pglen;
	     cnt++, pgd++) {
		if (pgd->pr_flags & PGF_FAULT) {
			++a[(cnt * pagesize + pgd->pr_value + TRND) / TGRAIN];
		}
	}
}

static void
prtextreport()
{
	register pgd_t *pgd;
	register int cnt, xrdata, xtext;

	/* Calculate thresholds */
	vrdata &= ~(pagesize - 1);
	xrdata = (vrdata - txtinfo.vaddr) / pagesize;
	vtext &= ~(pagesize - 1);
	xtext = (vtext - txtinfo.vaddr) / pagesize;
	txtinfo.tsize = txtinfo.bufsize / pagesize - xtext;

	/* Gather up valid statistics */
	for (cnt = 0, pgd = txtinfo.tpbuf->pr_data;
	     cnt < txtinfo.tpbuf->pr_pglen;
	     cnt++, pgd++) {
		if (pgd->pr_flags & PGF_VALHISTORY) {
			++txtinfo.tvalid;
			if (vrdata && vrdata < vtext) {
				if (cnt < xrdata)
					++txtinfo.tdyn;
				else if (cnt < xtext)
					++txtinfo.trdata;
				else
					++txtinfo.ttext;
			} else {
				if (cnt < xtext)
					++txtinfo.tdyn;
				else
					++txtinfo.ttext;
			}
		}
	}
}

/*
 * Save region data
 */
static struct srdata {
	int		*cbuf, *tbuf, *obuf;
	short		*dbuf;	/* a short cnt per page of how many 'dirty' versions
				 * there are */
	long		vaddr;
	int		bufsize;
	int		offset;
	prpgd_sgi_t	*tpbuf;
	int		totdirtypages;
} ldinfo;
static int incoreonly = 1;

static void
prsavedata(int fd, struct prmap_sgi *map, int scale, char *Spath)
{
	register int offset, cnt, *c, *o, *t;
	register pgd_t *pgd;
	struct symbol *sym, *prev = NULL;
	static firsttime = 1;

#if 0
	/* Paranoid */
	if ((long)map->pr_vaddr != vstart)
		return;
#endif

	/* Allocate space */
	if (ldinfo.cbuf == NULL) {
		/* All the maps are the same virtual size */
		ldinfo.vaddr = (long)map->pr_vaddr;
		ldinfo.bufsize = map->pr_size;
		ldinfo.offset = map->pr_off;
		ldinfo.cbuf = (int *)calloc(1, map->pr_size);
		ldinfo.tbuf = (int *)calloc(1, pagesize);
		ldinfo.obuf = (int *)calloc(1, map->pr_size);
		ldinfo.tpbuf = (prpgd_sgi_t *)calloc(1, sizeof (prpgd_sgi_t) +
			(map->pr_size / pagesize) * sizeof (pgd_t));
		ldinfo.dbuf = (short *)calloc(1, (map->pr_size / pagesize) * sizeof(short));
	}

	/* Get original data on first pass */
	if (firsttime) {
		register int fd2, bytes;
		register char *path = Spath ? Spath : objectname;

		/* Open object and extract original data */
		if ((fd2 = open(path, 0)) < 0) {
			perror("open");
			exit(-7);
		}
		if (!Spath && lseek(fd2, map->pr_off, 0) < 0) {
			perror("lseek");
			exit(-8);
		}
		if (vbss && vstart)
			bytes = vbss - vstart;
		else
			bytes = map->pr_size;
		if (read(fd2, ldinfo.obuf, bytes) < 0) {
			perror("read");
			abort();
		}
		close(fd2);
		firsttime = 0;
	}

	/* Get page table info */
	ldinfo.tpbuf->pr_vaddr = map->pr_vaddr;
	ldinfo.tpbuf->pr_pglen = map->pr_size / pagesize;
	if (ioctl(fd, PIOCPGD_SGI, ldinfo.tpbuf) < 0) {
		perror("ioctl(PIOCPGD_SGI)");
		return;
	}

	/* Seek into process and read the data */
	for (offset = 0, pgd = ldinfo.tpbuf->pr_data;
	     offset < map->pr_size;
	     offset += pagesize, pgd++) {
		/* Don't bother to get data from non-dirty pages */
		if (incoreonly && ((pgd->pr_flags & PGF_ISDIRTY) == 0))
			continue;

		/* XXX assumes we are only called once */
		ldinfo.totdirtypages++;

		ldinfo.dbuf[offset / pagesize]++;
		/* Get page worth of data and merge for later */
		if (lseek(fd, (off_t)map->pr_vaddr + offset, SEEK_SET) < 0)
			perror("lseek");
		if (read(fd, (char *)ldinfo.tbuf, pagesize) < 0)
			perror("read");

		/* Compare with original data (unrolled for speed) */
		if (dflg)
			printf("Changes to page %#lx\n",
						(ptrdiff_t)map->pr_vaddr + offset);
		c = ldinfo.cbuf + (offset / sizeof (int));
		o = ldinfo.obuf + (offset / sizeof (int));
		t = ldinfo.tbuf;
		for (cnt = 0; cnt < pagesize; cnt += 8 * sizeof (int)) {
			int s;

			if (bcmp(o, t, 8 * sizeof(int)) == 0) {
				o += 8; t += 8; c += 8;
				continue;
			}
			for (s = 0; s < 8; s++) {
				if (*o != *t) {
					long saddr = (ptrdiff_t)map->pr_vaddr +
						offset + cnt + (s * sizeof(int));
					sym = prfindsym(saddr);
					if (sym) {
						if (dflg) {
							int offset = saddr - sym->vaddr;
							if (offset == 0)
								printf("   %s:",
									sym->name);
							else
								printf("   %s+%d:",
									sym->name,
									offset);
						}
						if (sym != prev) {
							prev = sym;
							sym->usage++;
						}
					}
					if (dflg)
						printf("%#lx\told:%#x<%.4s> new:%#x<%.4s>\n",
						saddr, *o, (char *)o, *t, (char *)t);

					*c += scale;
				}
				o++, t++, c++;
			}
		}
	}
}

static void
prprettysym(struct symbol *sym, long raddr, int cnt)
{
	static int column = 8;
	char linebuf[80];
	int len;

	/* Advance to next line */
	if (sym == NULL) {
		printf("\t");
		column = 8;
		return;
	}

	/* NULL name? */
	if (sym->name == NULL) {
		sym->name = "?";
	}

	/* Check for page cross inside variable */
	if (sym->vaddr < raddr) {
		len = raddr - sym->vaddr;
		sprintf(linebuf, "%s+%x<%d>", sym->name, len, cnt);
	} else {
		sprintf(linebuf, "%s<%d>", sym->name, cnt);
	}

	/* Print symbol */
	len = strlen(linebuf);	
	if (column + len > 78) {
		printf("\n\t");
		column = 8;
	}
	printf("%s ", linebuf);
	column += len + 1;
}

/*
 * Symbol table report
 */
void
prsymreport(int flag, int filter)
{
	struct symbol *sym;
	char *name;

	/* Sort by option */
	for (sym = syms; sym; sym = sym->next) {
		prsortinsert(sym, flag);
	}
		
	/* Report all greater than threshold */
	if (flag == BYVADDR)
		printf("%-32s%12s%10s%8s%5s\n",
			"#Name", "Vaddr", "Size", "Use", "Type");
	else
		printf("%-32s%-25s%8s%8s%5s\n",
			"#Name", "Object File", "Size", "Use", "Type");
	for (sym = sorts; sym; sym = sym->sort) {
		if (flag == BYDSIZE && (sym->size < filter))
			break;
		if (flag == BYDUSAGE && (sym->usage < filter))
			break;
		name = sym->weak ? sym->weak : sym->name;
another:
		if (flag == BYVADDR)
			printf("%-32s%#10x%#8x%8d ",
				name, sym->vaddr,
				sym->size, sym->usage);
		else
			printf("%-32s%-25s%8d%8d ",
				name, sym->archive ? sym->archive : "N/A",
				sym->size, sym->usage);
		switch (sym->type) {
			case 'b': printf(" bss"); break;
			case 'B': printf(" Bss"); break;
			case 's': printf("sbss"); break;
			case 'S': printf("Sbss"); break;
			case 'd': printf("data"); break;
			case 'D': printf("Data"); break;
			case 'g': printf("gdta"); break;
			case 'G': printf("Gdta"); break;
			case 'r': printf("rdta"); break;
			case 'R': printf("Rdta"); break;
			case 't': printf("text"); break;
			case 'T': printf("Text"); break;
			default : printf("????");
		}
		if (name == sym->weak) {
			printf("W\n");
			name = sym->name;
			goto another;
		}
		printf("\n");
	}
}

static void
usage()
{
	fprintf(stderr, "usage: solocal [-dtHOamsuf#] [-p#|name] \\\n");
	fprintf(stderr, "\t[-Ssnap-path] [-Aarchive-path] [-T#]\\\n");
	fprintf(stderr, "\tDSO-pathname [threshold]\n");
	fprintf(stderr, "\t\t-d - give details about written areas\n"
			"\t\t-t - trace - useful to debug/verify symbol matching\n"
			"\t\t-H - print in hex rather than symbol names\n"
			"\t\t-O - dump symbols/object with filter\n"
			"\t\t-m - print proposed makefile object build order\n"
			"\t\t-a - look in swap also\n"
			"\t\t-s - just dump symbols by size\n"
			"\t\t-u - dump symbol table in usage order\n"
			"\t\t-f# - filter on usage < #percent \n");
	exit(0);
}

main(int argc, char **argv)
{
	register struct prmap_sgi *map;
	register int i, fd, pgn, bytes, dbytes, pdirty, pdmatch, btotal;
	register DIR *dirp;
        prmap_sgi_arg_t maparg;
	struct dirent *direntp;
	struct symbol *sym, *prev = NULL;
        struct stat statbuf;
	int vaddr, cutoff, threshold, offset, nmaps, *cbuf, scale, flags;
	int mflg = 0, sflg = 0, uflg = 0, Hflg = 0, Oflg = 0;
	int filter = 0, Tpasses = 0, Ttotal, pvalid;
	char *Proclist = NULL, *procname = NULL, *Aflg = NULL, *Sflg = NULL;
	long Ticks;
	short *abuf;
	dev_t ldev = 0;
	ino_t lino = 0;
	pgd_t *pgd;
	pid_t pid = 0;
	int nmappings = 0, nwriteablemappings = 0;
	int totsquanderpages = 0;
	int trsym = 0;

	/* Check for command line options */
	while ((i = getopt(argc, argv, "dtA:OP:S:T:Hasmuf:p:")) != EOF) {
		switch (i) {
			case 'd':
				dflg = 1; /* details */
				break;
			case 't':
				trsym = 1; /* trace symbols */
				break;
			case 'A':
				Aflg = optarg;
				break;
			case 'H':
				Hflg = 1;
				break;
			case 'O':
				Oflg = 1;
				filter = 1;
				break;
			case 'P':
				Proclist = optarg;
				break;
			case 'S':
				Sflg = optarg;
				break;
			case 'T':
				Tflg = 1;
				Ticks = CLK_TCK / 2;
				Tpasses = atoi(optarg);
				break;
			case 'a':
				incoreonly = 0;
				break;
			case 'f':
				filter = atoi(optarg);
				break;
			case 'm':
				mflg = 1;
				filter = 1;
				break;
			case 'p':
				pid = strtol(optarg, &procname, 10);
				if (*procname != NULL) {
					pid = 0;
					procname = optarg;
				}
				break;
			case 's':
				sflg = 1;
				break;
			case 'u':
				uflg = 1;
				break;
			case '?':
				usage();
				break;
		}
	}

	/* Look for required & optional arguments */
	if (optind != argc) {
		/* object name */
		objectname = argv[optind++];

		/* optional threshold */
		pagesize = getpagesize();
		threshold = 20;
		if (optind != argc) {
			threshold = atoi(argv[optind]);
			if ((threshold < 0) || (threshold > 100)) {
				fprintf(stderr, "threshold > 0 and < 100\n");
				exit(0);
			}
		}
	} else {
		usage();
	}

	/* Convert percent value into byte count */
	cutoff = (pagesize * threshold) / 100;

	/* Lookup object name */
        if (stat (objectname, &statbuf) != 0) {
		fprintf(stderr, "object %s not found\n", objectname);
		exit(-1);
	}
	ldev = statbuf.st_dev;
	lino = statbuf.st_ino;

	/* Get symbol names from object */
	if (!prgetsyms(objectname)) {
		fprintf(stderr, "get symbols failed on %s\n", objectname);
		exit(-2);
	}

	/* Cross-reference with optional archive */
	if (Aflg) {
		prgetarchive(Aflg);
	}

	/* Dump threshold symbols? */
	if (sflg) {
		printf("Quick dump of symbols with size >= %d bytes\n",cutoff);
		prsymreport(BYDSIZE, cutoff);
		exit(0);
	}

	/* tracing symbols? */
	if (trsym) {
		printf("Dump of symbols in increasing vaddr\n");
		prsymreport(BYVADDR, 0);
	}

	/* Open /proc directory */
	if ((dirp = opendir("/proc")) == NULL) {
		perror("opendir");
		exit(-3);
	}

	/* Text info? */
	if (Tflg) {
		if (!Oflg && !mflg) {
			fprintf(stderr, "Text sampling requires -O or -m\n");
			Tflg = Tpasses = 0;
		} else {
			fprintf(stderr, "Gathering TEXT fault info for ");
			fprintf(stderr, "%d seconds...\n", Tpasses);
			Tpasses *= 2;
		}
	}

repeat:
	/* Scan the entire directory (T times) */
	while ((direntp = readdir(dirp)) != NULL) {
		/* Create process path name */
		sprintf(prpathname, "/proc/%s", direntp->d_name);

		/* Skip . and .. and other junk in /proc */
		if (!isdigit(direntp->d_name[0]))
			continue;

		/* Open process portal */
		if ((fd = open(prpathname, 0)) < 0) {
			continue;
		}

		/* Get process info structure */
		if (ioctl(fd, PIOCPSINFO, &info) < 0) {
			perror("ioctl(PIOCPSINFO)");
			continue;
		}
		if (pid && info.pr_pid != pid) {
			(void) close(fd);
			continue;
		}
		if (!strcmp(info.pr_fname, "solocal")) {
			(void) close(fd);
			continue;
		}
		if (procname && strcmp(info.pr_fname, procname)) {
			(void) close(fd);
			continue;
		}
		if (Proclist && strcmp(info.pr_fname, Proclist))
			scale = 10;
		else
			scale = 1;

		/* Skip over system processes */
		if (info.pr_size <= 0) {
			(void) close(fd);
			continue;
		}
	
		/* Get process map structures */
		maparg.pr_vaddr = (caddr_t)maps;
		maparg.pr_size = sizeof maps;
		if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) < 0) {
			perror("ioctl(PIOCMAP_SGI)");
			continue;
		}

		/* Search regions to find object segments */
		for (map = maps, i = nmaps; i-- > 0; ++map) {
			if (map->pr_mflags & (MA_STACK|MA_BREAK|MA_SHMEM))
				continue;
			if ((map->pr_ino != lino) || (map->pr_dev != ldev))
				continue;

			/* found someone mapping lib */
			if (Tpasses == 0)
				nmappings++;
			if (map->pr_mflags & MA_EXEC) {
				prsavetext(fd, map, Tflg);
				continue;
			}
			if ((map->pr_mflags & (MA_COW|MA_WRITE)) !=
					      (MA_COW|MA_WRITE))
				continue;
			if (Tpasses == 0)
				nwriteablemappings++;

			/* Save data segment info */
			if (Tpasses == 0) {
				if (dflg)
					printf("Mapped writable by <%s>:%d\n", 
						info.pr_fname, info.pr_pid);
				prsavedata(fd, map, scale, Sflg);
			}
		}
		(void) close(fd);
	}
	if (Tflg && Tpasses--) {
		sginap(Ticks);
		rewinddir(dirp);
		goto repeat;
	}
	closedir(dirp);

	/* Anything to report? */
	if (ldinfo.bufsize == 0) {
		if (procname) {
			printf("Process %s not found\n", procname);
		} else {
			printf("Library %s not in use\n", objectname);
		}
		exit(0);
	}
	prtextreport();

	/* Set size of last symbol using end (often bigger than reality) */
	if (strcmp(syme->name, "end")) {
		syme->size = (ldinfo.vaddr + ldinfo.bufsize) - syme->vaddr;
	}

	/* Gather some basic statistics */
	for (pgn = 0, pgd = ldinfo.tpbuf->pr_data, cbuf = ldinfo.cbuf,
	     pvalid = pdirty = pdmatch = btotal = 0;
	     pgn < ldinfo.bufsize / pagesize;
	     pgn++, pgd++, cbuf += pagesize / sizeof (int)) {
		int wrthistory, flags = pgd->pr_flags;

		/* Count symbol byte differences */
		wrthistory = flags & PGF_WRTHISTORY;
		flags &= ~PGF_WRTHISTORY;
		vaddr = pgn * pagesize;
		dbytes = bytes = 0;
		for (i = 0; i < pagesize / sizeof (int);
		     i++, vaddr += sizeof (int)) {
			/* Filter */
			if (cbuf[i] < filter) {
				cbuf[i] = 0;
				continue;
			}

			/* Record use of variable */
			if (cbuf[i]) {
				sym = prfindsym(ldinfo.vaddr + vaddr);
				if (trsym) {
					printf("0x%x use %d",
						ldinfo.vaddr + vaddr, cbuf[i]);
					if (sym)
						printf(" sym %s symsize %d\n",
							sym->name, sym->size);
				}
				if (sym != NULL) {
					if (sym != prev)
						bytes += sym->size;
					/* WRONG
					if (sym->usage < cbuf[i])
						sym->usage = cbuf[i];
					 */
				}
				flags |= PGF_WRTHISTORY;
				dbytes += sizeof (int);
				prev = sym;
			}
			flags |= wrthistory;
		}
		pgd->pr_flags = flags;

		/* Record stats */
		pgd->pr_value = dbytes;
		btotal += bytes;
		if (pgd->pr_flags & PGF_VALHISTORY)
			++pvalid;
		if (pgd->pr_flags & PGF_WRTHISTORY)
			++pdirty;
		if ((dbytes > 0) && (dbytes <= cutoff))
			++pdmatch;
	}

	/* Add text usage info to symbol table */
	if (Tflg) {
		struct symbol *sym;
		for (i = 0, abuf = txtinfo.abuf;
		     i < txtinfo.bufsize / TGRAIN;
		     i++, abuf++) {
			if (*abuf) {
				vaddr = txtinfo.vaddr + i * TGRAIN;
				if ((sym = prfindsym(vaddr)) != NULL) {
					if (*abuf > sym->usage)
						sym->usage = *abuf;
				}
			}
		}
	}

	/* Print out symbols by usage? */
	if (Oflg) {
		printf("#");
		for (i = 0; i < argc; i++) {
			printf("%s ", argv[i]);
		}
		printf("\n#Symbols found -\n");
		prsymreport(BYDUSAGE, filter);
		exit(0);
	}

	/* Dump a makefile object list */
	if (mflg) {
		struct symbol *sym;
		int slen, llen;

		/* Check for archive table */
		if (!Aflg) {
			fprintf(stderr, "No archive file specified!\n");
			exit(0);
		}

		/* Merge symbols into archive listing */
		for (sym = syms; sym; sym = sym->next) {
			int factor;
			if (sym->usage > 0) {
				factor = 0;
				/* Data always has priority over text!!! */
				if (sym->type != 'r' && sym->type != 'R' &&
				    sym->type != 't' && sym->type != 'T') {
					factor = 20;
				}
				if (sym->archive) {
					prgetfiles(sym->archive, sym->usage << factor);
				}
			}
		}

		/* Sort archive symbols in reverse usage order */
		for (i = 0, sym = files; sym; sym = sym->next, ++i) {
			prsortinsert(sym, BYIUSAGE);
		}

		/* Print the listing */
		printf("Makefile object list, %d files total -\n\t", i);
		for (llen = 8, sym = sorts; sym; sym = sym->sort) {
			slen = strlen(sym->name) + 1;
			if (slen + llen > 77) {
				printf("\\\n\t");
				llen = 8;
			}
			printf("%s ", sym->name);
			llen += slen;
		}
		printf("\n\n");
	}

	/* Count text usage info for following report */
	if (Tflg) {
		for (Ttotal = 0, sym = syms; sym; sym = sym->next) {
			if (sym->usage > 0) {
				if (sym->type != 't' && sym->type != 'T') {
					continue;
				}
				Ttotal += sym->size;
			}
		}
	}
	
	/* Print pages with usage > 0 but less than threshold */
	printf("GENERAL INFORMATION REPORT\n");
	printf("Library name '%s'\n", objectname);
	printf("\tText Segment: Address=0x%x, Size=0x%x\n",
		txtinfo.vaddr, txtinfo.bufsize);
	printf("\tData Segment: Address=0x%x, Size=0x%x\n",
		ldinfo.vaddr, ldinfo.bufsize);
	printf("# of mappings of object:%d writable mappings %d total dirty pages %d\n",
		nmappings, nwriteablemappings, ldinfo.totdirtypages);
	printf("Text info - total # of pages %d, # valid %d\n",
		txtinfo.bufsize / pagesize, txtinfo.tvalid);
	printf("\tdyna %d (%d%%), ",
		txtinfo.tdyn, (txtinfo.tdyn * 100) / txtinfo.tvalid);
	if (vrdata && vrdata < vtext) {
		printf("rdata %d (%d%%), ", txtinfo.trdata,
			(txtinfo.trdata * 100) / txtinfo.tvalid);
	}
	printf("text %d (%d%%) of code (%d%%)\n",
		txtinfo.ttext, (txtinfo.ttext * 100) / txtinfo.tvalid,
		(txtinfo.ttext * 100) / txtinfo.tsize);
	if (Tflg) {
		printf("\ttotal code bytes %d, # pages if accumulated ~%d\n",
			Ttotal, (Ttotal + pagesize - 1) / pagesize);
	}
	printf("Data info - total # of pages %d, # valid %d, # dirty %d\n",
		ldinfo.bufsize / pagesize, pvalid, pdirty);
	printf("\t# of pages dirty with <= %d%% usage %d\n",
		threshold, pdmatch);
	printf("\ttotal symbol bytes %d, # pages if accumulated ~%d\n",
		btotal, (btotal + pagesize - 1) / pagesize);
	printf("Data pages (show symbols for those with <= %d%% usage) -\n",
		threshold);
	for (pgn = 0, pgd = ldinfo.tpbuf->pr_data, cbuf = ldinfo.cbuf;
	     pgn < ldinfo.bufsize / pagesize;
	     pgn++, pgd++, cbuf += pagesize / sizeof (int)) {
		struct symbol *sym = NULL;

		/* Get # of differences */
		bytes = pgd->pr_value;
		vaddr = ldinfo.vaddr + pgn * pagesize;

		/* Print header */
		printf("    vaddr 0x%x, dirty count %d, flags=<",
			vaddr, bytes);
                flags = pgd->pr_flags & PGF_NONHIST;
                FLAGSPRF(PGF_VALHISTORY,PGF_VALHISTORY,"VALID");
                FLAGSPRF(PGF_REFHISTORY,PGF_REFHISTORY,"REFERENCE");
                FLAGSPRF(PGF_WRTHISTORY,PGF_WRTHISTORY,"WRITTEN");
                printf(">\n");

		/* Skip non-dirty pages */
		if ((pgd->pr_flags & PGF_WRTHISTORY) == 0)
			continue;

		/* Check threshold and dump symbol names */
		if (bytes > 0 && bytes <= cutoff) {
			prprettysym(NULL, vaddr, 0);
			pvalid = 0;
			for (i = 0; i < pagesize; ) {
				/* Skip over inactive space */
				if (cbuf[i / sizeof (int)] == 0) {
					i += sizeof (int);
					continue;
				}

				/* Hex address mode? */
				if (Hflg) {
					if (++pvalid > 5) {
						printf("\n\t");
						pvalid = 1;
					}
					printf("0x%x ", vaddr + i);
					i += sizeof (int);
					continue;
				}

				/* Find symbol */
				if ((sym = prfindsym(vaddr + i)) != NULL) {
					offset = 0;
					if (sym->vaddr < (vaddr+i))
						offset = (vaddr+i) - sym->vaddr;

					prprettysym(sym, vaddr, sym->usage);
#if DEBUG
					printf("\ni %d size 0x%x vaddr 0x%x at 0x%x\n",
						i, sym->size, sym->vaddr, vaddr + i);
#endif
					i += sym->size - offset;
				} else {
					i += sizeof (int);
				}
			}
			printf("\ncopies of page %d\n", ldinfo.dbuf[pgn]);
			totsquanderpages += ldinfo.dbuf[pgn];
		} else if (bytes == 0) {
			printf("\t<anonymous writer at unknown offset>\n");
			printf("copies of page %d\n", ldinfo.dbuf[pgn]);
			/* be conservative */
			totsquanderpages += ldinfo.dbuf[pgn];
		}
	}
	printf("total pages used for less than threshold bytes %d\n", totsquanderpages);

	exit(0);
}
