#ifndef lint
static  char sccsid[] = "@(#)xdrtoc.c	1.1 88/06/08 4.0NFSSRC; from 1.6 88/03/15 D/NFS; from 1.5 88/02/08 Copyr 1987 Sun Micro";
#endif

/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */
/*
 * xdrtoc - decode xdr toc input to ascii readable form
 *
 */

#include "mktp.h"

FILE *ip;

main(argc,argv)
int argc;
char **argv;
{

	if(argc == 1)
		ip = stdin;
	else {
		ip = fopen(*++argv,"r");
		if (ip == NULL) {
			perror(*argv);
			exit(1);
		}
	}
	bzero((char *)entries,NENTRIES * sizeof (entries[0]));
	get_toc();
	ascii_toc();
	exit(0);
}

static
get_toc()
{
	register amt = read_xdr_toc(ip,&dinfo,&vinfo,entries,NENTRIES);

	if(amt <= 0)
		exit(1);

	ep = entries + amt;
}
/*
 * ascii_toc
 */

static
ascii_toc()
{
	ptapeaddress();
}

static
char *gtypnam(type)
file_type type;
{
	switch(type)
	{
	default:
	case UNKNOWN :
		return "unknown";
	case TOC:
		return "toc";
	case IMAGE:
		return "image";
	case TAR:
		return "tar";
	case CPIO:
		return "cpio";
	}
}

static int
ptapeaddress()
{
	register toc_entry *eptr;
	char *gtypnam(), *find_dist_arch();

	(void) fprintf(stdout,"%s %s of %s from %s\n",dinfo.Title,
		dinfo.Version,dinfo.Date,dinfo.Source);
	(void) fprintf(stdout,"ARCH %s\n",find_dist_arch());
	(void) fprintf(stdout,"VOLUME %d\n",
		vinfo.vaddr.Address_u.tape.volno);
        (void) fprintf(stdout,
		" Vol File             Name       Size\tType\n");
	for(eptr = entries; eptr < ep; eptr++)
	{
		(void) fprintf(stdout,"%4d %4d %16s %10d\t%s\n",
			address_vol(eptr),address_entry(eptr),
			eptr->Name,Size_of_Entry(eptr),
			gtypnam(eptr->Type));
	}
	return(ferror(stdout));
}
address_vol(eptr)
toc_entry *eptr;
{
        tapeaddress *tp = &eptr->Where.Address_u.tape;
        return(tp->volno);
}

address_entry(eptr)
toc_entry *eptr;
{
        tapeaddress *tp = &eptr->Where.Address_u.tape;
        return(tp->file_number);
}



static char *
find_dist_arch()
{
	register string_list *sp = (string_list *) 0;
	register toc_entry *eptr;

	for(eptr = entries; eptr < ep; eptr++)
	{
		if(eptr->Info.kind == STANDALONE)
		{
			if(eptr->Info.Information_u.Standalone.arch.string_list_len > 0)
			{
				sp = &eptr->Info.Information_u.Standalone.arch;
				break;
			}
		}
		else if(eptr->Info.kind == EXECUTABLE)
		{
			if(eptr->Info.Information_u.Exec.arch.string_list_len > 0)
			{
				sp = &eptr->Info.Information_u.Exec.arch;
				break;
			}
		}
		else if(eptr->Info.kind == INSTALLABLE)
		{
			if(eptr->Info.Information_u.Install.arch_for.string_list_len > 0)
			{
				sp = &eptr->Info.Information_u.Install.arch_for;
				break;
			}
		}
	}

	if(sp)
		return(*sp->string_list_val);

	(void) fprintf(stderr,
		"Unable to determine intended architecture for distribution\n");
	exit(1);
	/*NOTREACHED*/
}

Size_of_Entry(eptr)
toc_entry *eptr;
{
	if(eptr->Where.dtype == TAPE)
		return(eptr->Where.Address_u.tape.size);
	else if(eptr->Where.dtype == DISK)
		return(eptr->Where.Address_u.disk.size);
	else
		return(0);
}
