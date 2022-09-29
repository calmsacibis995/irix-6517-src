/*
 * Initialize the symbol table in the kernel to allow symbolic kernel
 * debugging.
 *					H.N 3/87
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <a.out.h> /* includes #include <filehdr.h> #include <syms.h> */
#include <ldfcn.h>
#include <sys/errno.h>
#include <sex.h>

int maxdbgsyms;
unsigned int maxdbgsymsaddr;
int namesiz;

struct dbstbl {
	unsigned int	addr;		/* kernel address */
	unsigned int  	noffst;		/* offset into name table */
} *dbstab;

char *nametab;
int nameoffst;			/* index to next available byte in nametab */
int dbnum;			/* total number of entries went into dbstab */
struct nlist n_list[3];

int symcnt;			/* number of symbols */
int symMax;			
SYMR *stbl;			/* start of symbol table */
char *namelist = "unix";	/* namelist file */
extern int errno;

int Verbose = 0;		/* verbose flag */

main(argc,argv)
char **argv;
{
	int c;
	extern char *optarg;
	extern int optind;
	int dumpit = 0;

	while ((c = getopt(argc, argv, "vd")) != -1) {
		switch (c) {
		case 'v':
			Verbose++;
			break;
		case 'd':
			dumpit++;
			break;
		case '?':
			usage();
			break;
		}
	}
	if (argc - optind != 1)
		usage();
	else
		namelist = argv[optind];

	/* set up dbstab from info in namelist's symtab */
	setupdbstab();

	if (dumpit) {
		getdbstab();
		exit(0);
	}

	/* read symtab into an array */
	rdsymtab();
	/* setup symtab for dbgmon */
	setdbstab();
	/* write dbstab into obj file */
	wrdbstab();
	exit(0);
}

usage()
{
	fprintf(stderr,"Usage: setsym [-vd] filename\n");
	exit(1);
}

error(s)
char *s;
{
	perror(s);
	exit(1);
}

compar(x, y)
	register  struct dbstbl  *x, *y;
{
	if(x->addr > y->addr)
		return(1);
	else if(x->addr == y->addr)
		return(0);
	return(-1);
}

/* 
 *	read symbol table from object file into memory 
 */
rdsymtab()
{
	LDFILE	*ldptr = NULL;
	int	i;

	if((ldptr = ldopen(namelist, ldptr)) == NULL)
		error("cannot open namelist file");
	/* external symbols start at index isymMax and are iextMax long */
	/* read local and external symbols */
	symMax = SYMHEADER(ldptr).isymMax;
	symcnt = SYMHEADER(ldptr).isymMax + SYMHEADER(ldptr).iextMax; 
	if((stbl = (SYMR *) malloc (symcnt * sizeof(SYMR)))
		== NULL)
		error("cannot allocate space for namelist");
	for(i = 0; i < symcnt; i++) {
		if(ldtbread(ldptr, i , &stbl[i]) 
			!= SUCCESS) 
			error("cannot read symbol from namelist");
	}
	if(ldclose(ldptr) != SUCCESS)
		error("cannot ldclose namelist file");
}

/* 
	build dbstab with data from stbl, eliminate uninteresting data
*/
setdbstab()
{
	LDFILE	*ldptr = NULL;
	char *np;
	SYMR *sp;
	struct dbstbl *db;
	int stop = 0;
	int count = 0;
	int i;

	if((ldptr = ldopen(namelist, ldptr)) == NULL)
		error("cannot open namelist file");

	db = &dbstab[0];
	for(sp = stbl, i=0; i<symcnt; sp++, i++) {
		if(db >= &dbstab[maxdbgsyms]) {
			if (!stop)
				fprintf(stderr,"Symbol table overflow with %d entries left!\n",symcnt-i);
				stop =1;
		}
		if (i < symMax &&
		    sp->st != stStatic &&
		    sp->st != stStaticProc)
		    /* statics allowed, throw away other locals */
		    continue;
		else if (sp->sc == scUndefined || sp->sc == scSUndefined)
		    /* we don't want undefineds */
		    continue;
#ifdef NEVER
		/* only use scText */
		if( (sp->st!= stProc) &&
		    (sp->st!= stStaticProc) &&
		    (sp->st!= stStatic) &&
		    (sp->st!= stGlobal)
		)
			continue;
		if ( i<symMax && 
		     sp->st != stStatic &&
		     sp->st != stStaticProc
		)
			continue;
#endif

		if (sp->value == 0)
			continue;
		if (stop) {
			/* out of space, count how many missing entries */
			count++;
			continue;
		}

		/* get name, put into nametab[] */
		np = (char *)ldgetname(ldptr, sp);
		if (np == (char *) NULL) {
			continue;
		}
		/* setup dbstab */
		db->addr = sp->value;
		db->noffst = nameoffst;
		if ( (nameoffst + strlen(np)) >= namesiz) {
			fprintf(stderr,"Nametab overflow with %d entries left!\n",symcnt-i);
			break;
		}
		while (*np != '\0')
			nametab[nameoffst++] = *np++;
		nametab[nameoffst++] = '\0';
		db++;
	}
	if (stop)
		fprintf(stderr,"Missing %d symbol entries\n",count);
	/* sort in ascending addr */
	dbnum = db - dbstab;
	qsort(dbstab, dbnum, sizeof (struct dbstbl), compar);
	
	if (Verbose) {
		printf("First symbol %s @ 0x%x Last %s @ 0x%x\n",
			&nametab[dbstab[0].noffst], dbstab[0].addr,
			&nametab[dbstab[dbnum-1].noffst], dbstab[dbnum-1].addr);
	}
	/* swizzle words in dbstab if target is opposite endian */
	if (LDSWAP(ldptr))
		for (db = dbstab; db < &dbstab[dbnum]; db++) {
			db->addr = swap_word(db->addr);
			db->noffst = swap_word(db->noffst);
		}

	if(ldclose(ldptr) != SUCCESS)
		error("cannot ldclose namelist file");
}
		

wrdbstab()
{
	SCNHDR	shdr;
	LDFILE *ldptr = NULL;
	int fd, maxsyms;

	/* get addr of dbstab in memory */
	n_list[0].n_name = &"dbstab"[0];
	nlist(namelist,&n_list[0]);

	if (n_list[0].n_value == 0) {
		if (Verbose)
			fprintf(stderr,
		 "setsym: object file not compiled with kernel debug option\n");
		exit(0);
	}

	/* get file offset of .data */
	if((ldptr = ldopen(namelist, ldptr)) == NULL)
		error("cannot open namelist file");
	ldnshread(ldptr,".data",&shdr);
	if(ldclose(ldptr) != SUCCESS)
		error("cannot ldclose namelist file");

	if (dbnum > maxdbgsyms) {
		fprintf(stderr,
			"setsym: too many symbols for symbol entry table\n");
		exit(1);
	}
	if (Verbose) {
		printf("Using %d out of %d available symbol entries.\n",
			dbnum,maxdbgsyms);
		printf("Seeking to 0x%x, writing %d bytes of dbstab.\n",
			shdr.s_scnptr+n_list[0].n_value-shdr.s_vaddr,
			sizeof(struct dbstbl)*maxdbgsyms);
	}

	if ((fd = open(namelist,O_WRONLY)) == -1)
		error("cannot open namelist file");
	if (lseek(fd,shdr.s_scnptr + n_list[0].n_value - shdr.s_vaddr,0) == -1)
		error("cannot lseek namelist file");
	if (write(fd,dbstab,sizeof(struct dbstbl)*maxdbgsyms) !=
	    sizeof(struct dbstbl)*maxdbgsyms ) {
		error("dbstab write failed!");
	}

	n_list[0].n_name = &"nametab"[0];
	nlist(namelist,&n_list[0]);
	if (n_list[0].n_value == 0) {
		fprintf(stderr,
		 "setsym: object file not compiled with kernel debug option\n");
		exit(1);
	}
	if (lseek(fd,shdr.s_scnptr + n_list[0].n_value - shdr.s_vaddr,0) == -1)
		error("cannot lseek namelist file");

	if (nameoffst > namesiz) {
	       fprintf(stderr,
	       "setsym: not enough room in name table to store symbol names\n");
	       exit(1);
	}
	if (Verbose) {
		printf("Using %d out of %d bytes available for symbol names.\n",
			nameoffst,namesiz);
		printf("Seeking to 0x%x, writing %d bytes of nametab.\n",
			shdr.s_scnptr+n_list[0].n_value-shdr.s_vaddr,
			sizeof(char )*namesiz);
	}
	if (write(fd,nametab,sizeof(char)*namesiz) != sizeof(char)*namesiz)
		error("nametab write failed!");

	/* now re-write max symbols "symmax" so that binary search
	 * won't find a bucket of zeros
	 */
	if (lseek(fd, maxdbgsymsaddr, 0) == -1)
		error("cannot lseek namelist file");
	if (LDSWAP(ldptr))
		maxsyms = swap_word(dbnum);
	else
		maxsyms = dbnum;
	if (write(fd, &maxsyms, sizeof(maxsyms)) != sizeof(maxsyms))
		error("symmax write failed!");

	close(fd);
}
	
getdbstab()
{
	SCNHDR	shdr;
	LDFILE *ldptr = NULL;
	int fd;
	int sbss_scnptr,sbss_size;

	/* get addr of dbstab in memory */
	n_list[0].n_name = &"dbstab"[0];
	nlist(namelist,&n_list[0]);
	/* get file offset of .sbss */
	if((ldptr = ldopen(namelist, ldptr)) == NULL)
		error("cannot open namelist file");
	ldnshread(ldptr,".data",&shdr);
	sbss_scnptr = shdr.s_scnptr;
	if ( (fd = open(namelist,O_RDONLY)) == -1 )
		error("cannot open namelist file");
	if ( lseek(fd,sbss_scnptr + n_list[0].n_value - shdr.s_vaddr,0) == -1 )
		error("cannot lseek namelist file");
	if ( read(fd,dbstab,sizeof(struct dbstbl)*maxdbgsyms) !=
			sizeof(struct dbstbl)*maxdbgsyms )
		error("dbstab read failed!");
	
	n_list[0].n_name = &"nametab"[0];
	nlist(namelist,&n_list[0]);
	if ( lseek(fd,sbss_scnptr + n_list[0].n_value - shdr.s_vaddr,0) == -1 )
		error("cannot lseek namelist file");
	if ( read(fd,nametab,sizeof(char)*namesiz) !=
			sizeof(char)*namesiz )
		error("nametab read failed!");
	if (close(fd) == -1)
		error("cannot close obj file");
	if(ldclose(ldptr) != SUCCESS)
		error("cannot ldclose namelist file");
	for (fd = 0; fd < maxdbgsyms; fd++) {
		printf("addr=%x noffst=%d name=%s\n",dbstab[fd].addr,dbstab[fd].noffst,&nametab[dbstab[fd].noffst]);
	}
	
}
	
/* 
 * allocate space for the symbol table according
 * to symbols compiled into kernel file
 * - "symmax" for maximum number of symbols
 * - "namesize" for maximum size of symbol table
 */
setupdbstab()
{
	SCNHDR	shdr;
	LDFILE *ldptr = NULL;
	int maxsyms, maxnametab = 0;
	int fd;

	/* get addr of symmax in memory */
	n_list[0].n_name = &"symmax"[0];
	n_list[1].n_name = &"namesize"[0];
	nlist(namelist,&n_list[0]);
	if (n_list[0].n_value == 0) {
		if (Verbose)
			fprintf(stderr,
		 "setsym: object file not compiled with kernel debug option\n");
		exit(0);
	}
	if (n_list[1].n_value == 0 && Verbose)
		fprintf (stderr, "Warning: old style symbol table\n");

	/* get file offset of .data */
	if((ldptr = ldopen(namelist, ldptr)) == NULL)
		error("cannot open namelist file");
	ldnshread(ldptr,".data",&shdr);

	/* read size of symbol table and name list */
	if ((fd = open(namelist,O_RDONLY)) == -1)
		error("cannot open namelist file");
	maxdbgsymsaddr = shdr.s_scnptr + n_list[0].n_value - shdr.s_vaddr;
	if (lseek(fd, maxdbgsymsaddr, 0) == -1)
		error("cannot lseek namelist file");
	if (read(fd,&maxsyms,sizeof(maxsyms)) != sizeof(maxsyms))
		error("symmax read failed!");
	if (n_list[1].n_value) {
		if (lseek(fd,shdr.s_scnptr+n_list[1].n_value - shdr.s_vaddr,0) 
		    == -1)
			error("cannot lseek namelist file");
		if (read(fd,&maxnametab,sizeof(maxnametab))!=sizeof(maxnametab))
			error("namesize read failed!");
	} 
	if (close (fd) == -1)
		error("cannot close namelist file");

	if (LDSWAP(ldptr)) {
		maxsyms = swap_word(maxsyms);
		maxnametab = swap_word(maxnametab);
	}
	if(ldclose(ldptr) != SUCCESS)
		error("cannot ldclose namelist file");
	/* maxsyms must be read from object file */
	/* size of name list defaults to 10 * maxsyms */
	if (maxsyms == 0) {
		if (Verbose)
			fprintf(stderr,
			    "setsym: object file not compiled"
			    " with kernel debug option, syms = 0\n");
		exit(0);
	}
	maxdbgsyms = maxsyms;
	namesiz = maxnametab ? maxnametab : maxsyms * 10;
	dbstab = (struct dbstbl *) calloc (maxdbgsyms, sizeof(struct dbstbl));
	if (dbstab == 0)
		error ("cannot malloc symbol table");
	nametab = (char *) calloc (namesiz, sizeof(char));
	if (nametab == 0)
		error ("cannot malloc name table");

	if (Verbose)  {
		printf ("Maximum number of symbols: %d\n", maxdbgsyms);
		printf ("Maximum size of name table: %d\n", namesiz);
	}
}
