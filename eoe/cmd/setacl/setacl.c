/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***************************************************************************
 * Command: setacl
 *
 ***************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/acl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <locale.h>
#include <pfmt.h>

#define AREAD		0x4	/* read permission */
#define AWRITE		0x2	/* write permission */
#define AEXEC		0x1	/* execute permission */

#define NOMATCH		0	/* no entry match found in ACL */
#define MRECLEN		256	/* maximum record length for "-f" option file */

#define NENTRIES	128	/* initial size of ACL buffer */

#define BADMALLOC	":66:malloc failed\n"
#define NOPERM		":63:permission denied for \"%s\"\n"
#define NOINSTALL	":57:system service not installed\n"
#define ACLFAIL		":80:acl failed for \"%s\", %s\n"
#define INCOMPTBLOPT	":81:incompatible options specified\n"
#define BADUSAGE	":48:incorrect usage\n"
#define DUPENTRY	":82:duplicate entries: \"%s\"\n"
#define ILLEGALOPT	":84:illegal entry specification, \"%s\"\n"
#define ILLEGALOPT2	":50:program error\n"
#define NOENTRY		":85:required entry for file owner, file group, \"class\", or \"other\" not specified\n"
#define ACLSORTFAIL	":86:aclsort call failed\n"
#define ACLSORTFAIL1	":87:aclsort call failed for \"%s\"\n"
#define NOFILEFOUND	":64:file \"%s\" not found\n"  
#define OPENFAIL	":88:can't open file \"%s\", %s\n" 
#define BADACLENTRY	":89:\"%s\", line %d; invalid ACL entry\n"
#define DONOTDELETE	":90:file owner, file group, \"class\", and \"other\" entries may not be deleted\n"
#define UNKTYPE		":91:unknown type \"%s\"\n"
#define UNKUSER		":92:unknown user-id \"%s\"\n"
#define UNKGROUP	":93:unknown group-id \"%s\"\n"
#define UNKPERM		":94:unknown permission \"%s\"\n"
#define NOMATCHFOUND	":95:matching entry not found in ACL for \"%s\": \"%s\"\n"
#define BADENTRYTYPE	":96:only file owner, file group, \"class\", or \"other\" may be specified for \"%s\"\n"
#define NODEFAULT	":97:default ACL entries may only be set on directories, not on \"%s\"\n"

int	lno;		/* aid in printing the file and line number */
char	*curfile;	/* when the -f option is specified */
int	fflag = 0;

#define	FILE_PRINT() { \
	if (fflag) \
		pfmt(stderr, MM_ERROR, BADACLENTRY, curfile, lno); \
}

struct acl_entry 	aclbuf[NENTRIES];	/* initial ACL buffer */

/*
 *
 *	The ACL operations buffer will be filled up with ACL entries specified
 *	as option arguments to the "-d" and "-m" options.  If the same ACL
 *	entry is specified in multiple option argument lists, only the last
 *	one specified will take affect.  In order to accomplish this, as each
 *	"-m" or "-d" option argument list is processed, all previously specified
 *	entries, contained in the operations buffer,  will be scanned for duplicates.  
 *	If a duplicate entry is found,	the most recent one will replace 
 *	the previous one in the ACL operations	buffer.
 *	This work is done by routine dmtoacl().  Once all option arguments are
 *	processed, the ACL from the current file is obtained via routine getacl(),
 * 	and routines modacl() and delacl() are called to modify entries and delete
 *	entries from the ACL, based on the entries in the ACL operations buffer.
 *
 */
struct  aclops {		/* buffer of ACL operations for "-d" & "-m" */
	int	a_op;		/* operation to perform */
	struct acl_entry a_acl;	/* entry to perform it on */
}  aclopbuf[NENTRIES];		/* initial ACL operations buffer */

/* flags for aclops.a_op */
#define ACL_MOD		0x1	/* modify this entry */
#define	ACL_DEL		0x2	/* delete this entry */
#define ACL_ADD		0x4	/* add this entry */


/*
 *	Macro GROWACLBUF is used to make the ACL buffer larger
 *			when necessary.  
 *
 *	NEWENTRIES is the number of entries the new buffer must hold.
 *	BUFENTRIES is the number of entries the current buffer holds.
 *	NBUFP is a pointer for the new ACL buffer.
 *	OBUFP is a pointer to the old ACL buffer.
 *	OLDENTRIES is the number of actual entries in the old buffer
 *		(it may not be full).
 *
 *	If OLDENTRIES is not zero, the actual entries in the old buffer
 *		are copied into the new buffer.
 *	The old buffer OBUFP is free()'ed if it was obtained via malloc().
 *
 */
#define	GROWACLBUF(NEWENTRIES, BUFENTRIES, NBUFP, OBUFP, OLDENTRIES)	\
	{	\
		int		buflen;	\
		int		i;	\
		struct acl_entry *SRCACLP;	\
		struct acl_entry *TGTACLP;	\
		while (NEWENTRIES > BUFENTRIES)	\
			BUFENTRIES *= 2;	\
		buflen = BUFENTRIES * sizeof(struct acl_entry);	\
		if ((NBUFP = (struct acl_entry *)malloc(buflen)) == NULL) { \
			pfmt(stderr, MM_ERROR, BADMALLOC); \
			exit(32);	\
		}	\
		if (OLDENTRIES) {	\
			SRCACLP = OBUFP;	\
			TGTACLP = NBUFP;	\
			for (i = 0; i < OLDENTRIES; i++) 	\
				*TGTACLP++ = *SRCACLP++;	\
		}	\
		if (OBUFP != aclbuf)	\
			free(OBUFP);	\
		OBUFP = NBUFP;	\
	}	


void usage();			/* print "usage: " message */

extern int optind;		/* for getopt */
extern char *optarg;		/* for getopt */
extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;

/*
 * Procedure:     main
 *
 * Restrictions:
                 getopt: none 
                 pfmt: none
*/
main(argc,argv)
char *argv[];
{
	struct aclops 	*aclopbp = aclopbuf;	/* ACL operations buffer */
	struct acl_entry 	*aclbp = aclbuf;/* pointer to ACL buffer */
	void 		ftoacl(); 		/* processes -f option args */
 	void		acltochar();		/* converts acl to printable format */
	int		opbuf_entries = NENTRIES;/* ACL operations buf size */
	int		buf_entries = NENTRIES;	/* ACL buffer size */
	int 		mflag = 0; 		/* option flags */
	int 		dflag = 0; 
	int		sflag = 0; 	
	int		rflag = 0;
	int 		dmentries = 0; 		/* args of "-d" & "-m" */
	int 		nentries = 0;		/* entries to set */
	int 		i,j,k,c;
	int 		ret;			/* return value from aclsort */
	int		error = 0;		/* final return code to user */
	char 		*ffile;			/* file for "-f" option */
	char 		*argp;
	char 		aclentry[MRECLEN];


	(void)setlocale(LC_ALL,"");
	(void)setcat("uxes");
	(void)setlabel("UX:setacl");

	while ((c=getopt(argc,argv,"rm:d:s:f:"))!=EOF) {
		argp = optarg;
		switch(c) {
		case 'm':
			if (fflag || sflag) {
				pfmt(stderr, MM_ERROR, INCOMPTBLOPT);
				usage();
				exit(1);
			}
			if ((ret = dmtoacl(argp, &aclopbp, &opbuf_entries, 
				&dmentries, ACL_MOD)) == -1) {
				pfmt(stderr, MM_ERROR, BADUSAGE);
				usage();
				exit(1);
			}
			mflag++;
			break;
		case 'd':
			if (fflag || sflag) {
				pfmt(stderr, MM_ERROR, INCOMPTBLOPT);
				usage();
				exit(1);
			}
			if ((ret = dmtoacl(argp, &aclopbp, &opbuf_entries,
				&dmentries, ACL_DEL)) == -1) {
				pfmt(stderr, MM_ERROR, BADUSAGE);
				usage();	 
				exit(1);
			}
			dflag++;
			break;
		case 's':
			if (mflag || dflag || fflag) {
				pfmt(stderr, MM_ERROR, INCOMPTBLOPT);
				usage();
				exit(1);
			}
			if ((ret = stoacl(argp, &aclbp, &buf_entries, 
				&nentries)) == -1) {
				pfmt(stderr, MM_ERROR, BADUSAGE);
				usage();
				exit(1);
			}
			sflag++;
			break;
		case 'f':
			if (sflag || mflag || dflag) {
				pfmt(stderr, MM_ERROR, INCOMPTBLOPT );
				usage();
				exit(1);
			}
			ffile = optarg;
			fflag++;
		 	ftoacl(ffile, &aclbp, &buf_entries, &nentries);
			break;
		case 'r':
			rflag++;
			break;
		case '?':
			usage();
			exit(1);
			break;
		}	 /* end switch */
	} 	/* end while getopt */

	if (optind == argc) {	/* no file name given */
		pfmt(stderr, MM_ERROR, BADUSAGE);
		usage();
		exit(1);
	}
	if (!dflag && !mflag && !sflag && !fflag) {
		pfmt(stderr, MM_ERROR, BADUSAGE);
		usage();
		exit(1);
	}
	/*
	 * Fixed ACL for -s and -f options; sort once only.
	 */
	if (sflag || fflag) {
		/* sort the entries and update the ACL */
		if ((ret = aclsort(nentries, rflag, aclbp)) != 0) {
			if (ret > 0) {
				acltochar(aclbp + ret, aclentry);
				pfmt(stderr, MM_ERROR, DUPENTRY, aclentry);
				exit(32);
			} 
			if ((ret == -1) && (ckmandacls(nentries, aclbp) != 0)) {
				/* check that all base entries exist */
				pfmt(stderr, MM_ERROR, NOENTRY);
				usage();
				exit(1);
			}
			pfmt(stderr, MM_ERROR, ACLSORTFAIL);
			exit(32);
		}	/* end "if ((ret = aclsort( ..." */
	}
	for (i = optind; i < argc; i++) {
		/* loop for each file specified */
		if (dflag || mflag) {
			/* 
			 * get all existing ACL entries for the file 
			 */
			 if (getacl(argv[i], &aclbp, &buf_entries,
				&nentries) < 0) {
				error = 2;
				continue;
			}

			if (dflag) {
				/*
				 * 1st delete all entries marked for deletion 
				 * in the operations buffer from the file's
				 * ACL buffer.
				 */
				if (delacl(aclopbp, dmentries, aclbp, &nentries, argv[i]) < 0){
					error = 2;
					continue;
				}
			}
			if (mflag) {
				/*
				 * then add or modify all entries marked
				 * for modification in the operations buffer 
				 */
				if (modacl(aclopbp, dmentries, &aclbp, 
					&buf_entries, &nentries, argv[i]) < 0) {
					error = 2;
					continue;
				}
			}
			/*
			 * if we deleted entries, and now only have four base
			 * entries, make sure the CLASS_OBJ & GROUP_OBJ
			 * permissions match now.
			 */
			if (dflag && (nentries == NACLBASE))
				rflag++;

			/* sort the entries and update the ACL */
			if ((ret = aclsort(nentries, rflag, aclbp)) != 0) {
				pfmt(stderr, MM_ERROR, ACLSORTFAIL1, argv[i]);
				error = 2;
				continue;
			}	/* end "if ((ret = aclsort( ..." */
		}
		if (putacls(argv[i], aclbp, nentries, (dflag || mflag)) != 0)
			error = 2;
	}	/* end for */

	/*
	 * if we had to allocate a new operations buffer
	 * or ACL buffer, make sure to free them now
	 */
	if (aclbp != aclbuf)
		free(aclbp);
	if (aclopbp != aclopbuf)
		free(aclopbp);
	exit(error);

}

/*
 * Procedure:     stoacl
 *
 * Restrictions:
*/
/*
 *							
 *	stoacl - converts operands of the "-s" option	
 *		which are expressions of the form 	
 *
 *		[d[efault]:]u[ser]:[uid]:operm | perm
 *			or
 *		[d[efault]:]g[roup]:[gid]:operm | perm
 *			or
 *		[d[efault]:]c[lass]:operm | perm
 *			or
 *		[d[efault]:]o[ther]:operm | perm
 *		
 *		to a buffer containing an ACL
 *
 *	input - 1. Pointer to the argument expression	
 *		2. Pointer to the buffer to store the ACL in
 *		3. Pointer to number of entries which will
 *		   fit in the buffer.
 *		4. Pointer to the number of entries actually
 *		   stored in the buffer.
 *							
 *	output - 0 on success, -1 on failure
 *							
 */

int
stoacl(argp, aclpp, bufentriesp, sentriesp)
char 			*argp;
struct acl_entry 	**aclpp;
int			*bufentriesp;
int			*sentriesp;
{
	register int 		commas, i, buflen;
	struct acl_entry	*aclp = *aclpp;
	struct acl_entry	*tgtaclp = aclp; 
	char			*malloc(), *scanugo();
	char 			*argstart;
	long			bentries = *bufentriesp;

	if (argp == NULL || *argp=='\0')
		return(-1);
	argstart = argp;

	/* count number of acl entries to modify */

	commas = 1;
	while (*argp && *argp != '\n') {
		if (*argp == ',')
			commas++;
		++argp;
	}

	if (commas > bentries)
		GROWACLBUF(commas, bentries, tgtaclp, aclp, 0); 
	argp = argstart;
	for (i = 0; i < commas; i++, tgtaclp++) {

		/* validate ae_type, ae_id portion of ACL entry */
		argp = scanugo(argp, &tgtaclp->ae_type, &tgtaclp->ae_id, ':'); 

		/* validate ae_perm portion of ACL entry */
		(void)scanperms(&argp, &tgtaclp->ae_perm, ',');

	}	/* end for */
	*sentriesp = commas;
	*bufentriesp = bentries;
	*aclpp = aclp;
	return(0);
}

/*
 * Procedure:     ftoacl
 *
 * Restrictions:
                 fopen: none
                 fgets: none
                 rewind: none
                 pfmt: none
*/
/*
 *							
 *	ftoacl - reads file argument of "-f" option	
 *		which contains expressions of the form 	
 *
 *		[d[efault]:]u[ser]:[uid]:operm | perm
 *			or
 *		[d[efault]:]g[roup]:[gid]:operm | perm
 *			or
 *		[d[efault]:]c[lass]:operm | perm
 *			or
 *		[d[efault]:]o[ther]:operm | perm
 *			or
 *		comments starting with "#"
 *		and converts them to ACL entries.	
 *							
 *	input - 1. Pointer to the file name		
 *		2. Pointer to the ACL buffer.
 *		3. Pointer to the number of entries which
 *		   will fit in the ACL buffer.
 *		4. Pointer to the number of ACL entries
 *		   stored in the ACL buffer.
 *							
 *	output - none
 *							
 */

void
ftoacl(filep, aclpp, bufentriesp, nentriesp)
register char 		*filep;
struct acl_entry	**aclpp;
int			*bufentriesp;
int			*nentriesp;
{
	struct acl_entry	*aclp = *aclpp;
	struct acl_entry 	*tgtaclp = aclp;
	FILE 		*fd, *fopen();
	char 		*malloc(), *fgets(), *scanugo();
	char 		*argp, rec[MRECLEN];
	int 		c, entries, buflen;
	int		ret;
	int		bentries = *bufentriesp;
	int		oentries;
	void 		rewind();
	
	if ((fd = fopen(filep, "r")) == NULL) {
		if (errno == EACCES) 
			pfmt(stderr, MM_ERROR, NOPERM, filep);
		else if (errno == ENOENT)
			pfmt(stderr, MM_ERROR, NOFILEFOUND, filep);
		else 
			pfmt(stderr, MM_ERROR, OPENFAIL, filep, strerror(errno));
		exit(32);
	}
	entries = 0;
	lno = 0;
	curfile = filep;
	while (fgets(rec, MRECLEN, fd) != NULL) {
		lno++;
		argp = rec;
		while (*argp == ' ')
			argp++;
		if (!*argp)
			continue;	/* if blank or null line, skip it */
		if (*argp == '#') 	/* if line start w/ comment, skip it */
			continue;
		entries++;
		if (entries > bentries) {
			oentries = entries - 1;
			GROWACLBUF(entries, bentries, tgtaclp, aclp, oentries);
			tgtaclp += oentries;
		}

		/* validate type and id in entry */
		argp = scanugo(argp, &tgtaclp->ae_type, &tgtaclp->ae_id, ':');

		/* validate permissions in entry */
		if ((ret = scanperms(&argp, &tgtaclp->ae_perm, ' ')) != -1) {
			/* make sure only white space or comments follow */
			argp++;
			while ((*argp == ' ') || (*argp == '\t'))
				argp++;
			if ((*argp != '#') && (*argp != '\n')) {
				pfmt(stderr, MM_ERROR, BADACLENTRY, filep, lno);
				exit(32);
			}
		}
		tgtaclp++;
	}	/* end while "fscanf" */
	*nentriesp = entries;
	*bufentriesp = bentries;
	*aclpp = aclp;
	return;
}

/*
 * Procedure:     dmtoacl
 *
 * Restrictions:
                 pfmt: none
*/
/*
 *							
 *	dmtoacl - converts operands of the "-d" & "-m" options	
 *		which are expressions of the form 	
 *
 *		[d[efault]:]u[ser]:[uid][:operm | perm]
 *			or
 *		[d[efault]:]g[roup]:[gid][:operm | perm]
 *			or
 *		[d[efault]:]c[lass]:operm | perm
 *			or
 *		[d[efault]:]o[ther]:operm | perm
 *		
 *		to a buffer containing an ACL
 *
 *	input - 1. Pointer to the argument expression	
 *		2. current buffer containing entries to be
 *		   modified or deleted.
 *		3. Pointer to number of entries which will
 *		   fit in current buffer.
 *		4. Pointer to number of entries to be
 *		   modified or deleted.
 *		5. Flag indicating whether processing "-d" or "-m"
 *							
 *	output - 0 on success, -1 on failure
 *							
 */

int
dmtoacl(argp, dmaclopbpp, bufentriesp, nentriesp, dmflag)
char 		*argp;			/* argument string */
struct aclops 	**dmaclopbpp;		/* ptr to buffer of -d & -m entries */
int		*bufentriesp;		/* max number of entries in buffer */
int		*nentriesp;		/* current num. of entries in buffer */
int		dmflag;			/* ACL_DEL for "-d", ACL_MOD for "-m" */
{
	char 		*argstart, *malloc(), *scanugo();
	struct aclops 	*newaclopbp;
	struct aclops 	*srcaclopbp;	
	struct aclops 	*tgtaclopbp;
	struct aclops	*dmaclopbp = *dmaclopbpp;
	struct acl_entry	dmacl;
	register int 	commas, i, j, buflen;
	int		entries = *nentriesp;
	int		bentries = *bufentriesp;
	char		delim;

	if ((argp == NULL) || (*argp == '\0'))
		return (-1);
	argstart = argp;

	/* count number of acl entries to modify */

	commas = 1;
	while (*argp && *argp != '\n') {
		if (*argp == ',')
			commas++;
		++argp;
	}
	/*
	 * if the new argument entries, added to the entries currently
	 * in the operations buffer, will exceed the buffer size,
 	 * we've got to  allocate a new operations buffer
	 */
	if ((entries + commas) > bentries) {

		/* existing operations buffer not big enough */
		while ((entries + commas) > bentries)
			bentries *= 2;

		/* get storage space for acl entries to modify */
		buflen = bentries * sizeof(struct aclops);
		if ((newaclopbp = (struct aclops *)malloc(buflen)) == NULL) {
			pfmt(stderr, MM_ERROR, BADMALLOC);
			exit(32);
		}
		/* now copy existing operations buffer to new buffer */
		srcaclopbp = dmaclopbp;
		tgtaclopbp = newaclopbp;
		for (i = 0; i < entries; i++) 
			*tgtaclopbp++ = *srcaclopbp++;

		/* if the previous buffer was malloc'ed, free it */
		if (dmaclopbp != aclopbuf)
			free(dmaclopbp);
		dmaclopbp = newaclopbp;
	}	/* end if */

	/* 
	 * now scan each argument entry, and for each, try to find it in
	 * the existing operations buffer of entries.
	 */
	tgtaclopbp = dmaclopbp;
	argp = argstart;
	for (i = 0; i < commas; i++) {

		/* validate ae_type, ae_id portion of ACL entry */
		if (dmflag & ACL_DEL)
			delim = ',';
		else
			delim = ':';
		argp = scanugo(argp, &dmacl.ae_type, &dmacl.ae_id, delim); 
		
		if (dmflag & ACL_DEL) {
			/* can't delete these types */
			if ((dmacl.ae_type == USER_OBJ) || 
				(dmacl.ae_type == GROUP_OBJ) || 
				(dmacl.ae_type == CLASS_OBJ) || 
				(dmacl.ae_type == OTHER_OBJ)) {
				pfmt(stderr, MM_ERROR, DONOTDELETE);
				exit (32);
			}
		} else {
			/* get permissions to modify */
			(void)scanperms(&argp, &dmacl.ae_perm, ',');
		}

		for (j = 0; j < entries; j++, tgtaclopbp++) {
			if ((tgtaclopbp->a_acl.ae_type == dmacl.ae_type) &&
				(tgtaclopbp->a_acl.ae_id == dmacl.ae_id)) {
				/* 
				 * the entry has been previously specified
				 * as an arg to -d or -m. 
				 * mark entry for modification or deletion
				 */
				if (dmflag & ACL_MOD)
					/* if "-m", set new permissions */
					tgtaclopbp->a_acl.ae_perm = dmacl.ae_perm;
				tgtaclopbp->a_op = dmflag;
				break;
			}
		}	/* end for j */
		if (j == entries) {
			/*
			 * no match  - this is the first time this entry
			 * has been specified. add it to the buffer for 
			 * modification or deletion
			 */
			tgtaclopbp->a_acl.ae_type = dmacl.ae_type;
			tgtaclopbp->a_acl.ae_id = dmacl.ae_id;
			if (dmflag & ACL_MOD)
				/* if "-m", set new permissions */
				tgtaclopbp->a_acl.ae_perm = dmacl.ae_perm;
			tgtaclopbp->a_op = dmflag;
			entries++;
		}
		tgtaclopbp = dmaclopbp;
	}	/* end for i */
	*nentriesp = entries;
	*bufentriesp = bentries;
	*dmaclopbpp = dmaclopbp;
	return(0);
}

/*
 * Procedure:     scanugo
 *
 * Restrictions:
                 getpwnam: P_MACREAD
                 getgrnam: P_MACREAD
                 pfmt: none
                 sscanf: none
*/
/*
 *							
 *	scanugo - scans for an expression of the form	
 *		"user::", "user:uid:", "group::",
 *		"group:gid:", "class:", or "other:"
 *		Each of the expressions may optionally be preceded
 *		by the default specifier "default:" or "d:"
 *							
 *	input - argp : a pointer to the argument string 
 *		type : pointer to a variable to store the entry type in
 *		id   : pointer to a variable to store  the id in
 *		delim: the character ending the "type:id" expression -
 *		       currently either ':' for arguments of the
 *		       -f, -m, and -s option, or ',' for arguments
 *		       of the -d option.
 *							
 *	output - the updated pointer to the arg string	
 *							
 */
char *
scanugo(argp, type, id, delim)
char 	*argp;
int	*type;
uid_t	*id;
char	delim;
{
	char 		*p, *unamep, *gnamep;
	struct passwd 	*getpwnam(),*pwd_p;
	struct group 	*getgrnam(),*grp_p;
	uid_t 		uid, gid;
	int		ret;
	char 		name[50];	/* for sscanf call; error if filled */
	char		*tnamep;

	tnamep = unamep = argp;
	ret = skipc(&argp, ':');
	
	/*
	 *	check if this is a default entry
	 *	scan for "default" or "d"
	 */

	if ((strcmp(unamep, "default") == 0) || (strcmp(unamep, "d") == 0)) {
		*type = ACL_DEFAULT;
		unamep = argp;
		*(unamep-1) = ':';	/* for tnamep */
		ret = skipc(&argp, ':');
	} else
		*type = 0;

	/*
	 *	scan for "user", "u", "group", "g", "class", "c", "other", or "o"
	 */
	
	if ((strcmp(unamep, "user") == 0) || (strcmp(unamep, "u") == 0)) { 
		/* 
		 *	ae_type is USER_OBJ or USER
		 */
		if (ret == 0)
			*(argp-1) = ':';	/* for tnamep */
		if (*argp == '\0') {
			FILE_PRINT();
			pfmt(stderr, MM_ERROR, ILLEGALOPT, tnamep);
			usage();
			exit(1);
		}
		unamep = argp;
		if (*unamep == ':') {
			if (delim == ',') {	/* -d option */
				unamep++;
				if (*unamep && *unamep != ',') {
					FILE_PRINT();
					pfmt(stderr, MM_ERROR, ILLEGALOPT,
						tnamep);
					usage();
					exit(1);
				}

			}
			*type |= USER_OBJ;
			return ++unamep;
		}
		ret = skipc(&argp, delim);
		pwd_p=getpwnam(unamep);
		if (pwd_p == NULL) {
			/*
		 	* uname not in passwd file.  if valid numeric 
		 	* uid, ok.  Otherwise, error 		       
		 	*/
			p = unamep;
			if ((sscanf(unamep, "%d%s", &uid, name) == 1) &&
				(uid < MAXUID) && (uid >= 0)) {
				*id = (ushort)uid;
				*type |= USER;
				return argp;
			} else {
				FILE_PRINT();
				pfmt(stderr, MM_ERROR, UNKUSER, unamep);
				exit(32);
			}
		} else {
			*id = (ushort) pwd_p->pw_uid;
			*type |= USER;
			return argp;
		}
	} else if ((strcmp(unamep,"group") == 0) || (strcmp(unamep,"g") == 0)) {
		/* 
		 *	ae_type is GROUP_OBJ or GROUP
		 */
		if (ret == 0)
			*(argp-1) = ':';	/* for tnamep */
		if (*argp == '\0') {
			FILE_PRINT();
			pfmt(stderr, MM_ERROR, ILLEGALOPT, tnamep);
			usage();
			exit(1);
		}
		gnamep = argp;
		if (*gnamep == ':') {
			if (delim == ',') {	/* -d option */
				gnamep++;
				if (*gnamep && *gnamep != ',') {
					FILE_PRINT();
					pfmt(stderr, MM_ERROR, ILLEGALOPT,
						tnamep);
					usage();
					exit(1);
				}

			}
			*type |= GROUP_OBJ;
			return ++gnamep;
		}
		ret = skipc(&argp, delim);
		grp_p=getgrnam(gnamep);
		if (grp_p == NULL) {
		/*
		 * gname not in group file.  if valid numeric 
		 * gid, ok.  Otherwise, error 		       
		 */
			if ((sscanf(gnamep, "%d%s", &gid, name) == 1) &&
				(gid < MAXUID) && (gid >= 0)) {
				*id = (ushort)gid;
				*type |= GROUP;
				return argp;
			} else {
				FILE_PRINT();
		 		pfmt(stderr, MM_ERROR, UNKGROUP, gnamep);
				exit(32);
			}
		} else {
			*id = (ushort)grp_p->gr_gid;
			*type |= GROUP;
			return argp;
		}
        } else if ((strcmp(unamep,"class") == 0) || (strcmp(unamep,"c") == 0)) {                /*
                 *      ae_type is CLASS_OBJ
                 */
		if (ret == 0)
			*(argp-1) = ':';	/* for tnamep */
		if (ret || (delim == ',' && *argp && *argp != ',')) {
			FILE_PRINT();
			pfmt(stderr, MM_ERROR, ILLEGALOPT, tnamep);
			usage();
			exit(1);
		}
		if (delim == ',')
			argp++;
		*type |= CLASS_OBJ;
		return argp;
	} else if ((strcmp(unamep,"other") == 0) || (strcmp(unamep,"o") == 0)) {
		/* 
		 *	ae_type is OTHER_OBJ
		 */
		if (ret == 0)
			*(argp-1) = ':';	/* for tnamep */
		if (ret || (delim == ',' && *argp && *argp != ',')) {
			FILE_PRINT();
			pfmt(stderr, MM_ERROR, ILLEGALOPT, tnamep);
			usage();
			exit(1);
		}
		if (delim == ',')
			argp++;
		*type |= OTHER_OBJ;
		return argp;
	}

	/*
	 *	If we get this far, illegal option
	 */

	if (*argp == '\0') {
		FILE_PRINT();
		pfmt(stderr, MM_ERROR, ILLEGALOPT, tnamep);
		usage();
		exit(1);
	}
	FILE_PRINT();
	pfmt(stderr, MM_ERROR, UNKTYPE, tnamep);
	exit(32);
}

/*
 * Procedure:     scanperms
 *
 * Restrictions:
                 pfmt: none
*/
/*
 *						
 *	scanperms - scan the argument string	
 *		for an expression specifying	
 *		permissions for the file or 	
 *		the ACL entry.			
 *						
 *	input - a ptr to a ptr to the argument string,	
 *		a ptr to the variable which	
 *		will receive the permissions.
 *		the trailing delimiter of the permissions:
 *		 currently either a comma or a blank
 *						
 *	output - return code from skipc() routine:
 *		0 if trailing delimiter found,
 *		1 if space or tab found,
 *		2 if null found,
 *		-1 if newline found
 *						
 */
int
scanperms(argpp, permp, delim)
char 	**argpp;  
ushort 	*permp;
char	delim;
{
	char		*argp = *argpp;
	char 		*p;		/* for scanning characters */
	char 		*cmodep;	/* ptr to start of perm characters */
	int 		permchars = 0;	/* # of perm characters so far */
	int 		rchar = 0;	/* non-zero if "r" specified */
	int 		wchar = 0;	/* non-zero if "w" specified */
	int 		xchar = 0;	/* non-zero if "x" specified */
	int 		ret;
	ushort 		perms = 0;

	if (argp && *argp) {
		if ((*argp >= '0') && (*argp <= '7')) {
			ret = omode(&argp, permp, delim);
			*argpp = argp;
			return (ret);
		}
		p = cmodep = argp;
		ret = skipc(&argp, delim);
		while (*p) {
			switch (*p++) {		/* scan for "perm" */  
			case 'r':
				if (rchar) {
					FILE_PRINT();
					pfmt(stderr, MM_ERROR, UNKPERM, cmodep);
					exit(32);
				}
				perms |= AREAD;
				rchar++;
				break;
			case 'w':
				if (wchar) {
					FILE_PRINT();
					pfmt(stderr, MM_ERROR, UNKPERM, cmodep);
					exit(32);
				}
				wchar++;
				perms |= AWRITE;
				break;
			case 'x':
				if (xchar) {
					FILE_PRINT();
					pfmt(stderr, MM_ERROR, UNKPERM, cmodep);
					exit(32);
				}
				xchar++;
				perms |= AEXEC;
				break;
			case '-':
				break;
			default:
				FILE_PRINT();
				pfmt(stderr, MM_ERROR, UNKPERM, cmodep);
				exit(32);
				break;
			}	/* end switch to scan for "perm" */
			permchars++;
		}	/* end while */
		if (permchars < 1 || permchars > 3) {
			FILE_PRINT();
			pfmt(stderr,  MM_ERROR, UNKPERM, cmodep);
			exit(32);
		}
		*permp = perms;
		*argpp = argp;
		return (ret);
	}	/* end if argp */
	else {
		FILE_PRINT();
		pfmt(stderr, MM_ERROR, UNKPERM, argp);
		exit(32);
	}
}

/*
 * Procedure:     omode
 *
 * Restrictions:
                 pfmt: none
*/
/*
 *						
 *	omode - scan the argument string for an 
 *		octal permissions expresssion	
 *						
 *	input - a ptr to a ptr to the argument string	
 *		a ptr to the variable which 	
 *		will receive the permissions	
 *		the trailing delimiter of permissions:
 *		currently either a comma or a blank
 *						
 *	output - return value from skipc:
 *		0 if character found,
 *		1 if blank or tab found,
 *		2 if null found,
 *		-1 if newline found
 *						
 */
int
omode(argpp, permp, delim)
char 	**argpp;
ushort 	*permp;
char	delim;
{
	register int 	c;
 	char 		*argp = *argpp;
	char 		*p;
	int		ret;
	p = argp;		/* save pointer to octal mode */
	ret = skipc(&argp, delim);
	if (((c = *p) < '0') || (c > '7') || *(p+1)) {
		FILE_PRINT();
		pfmt(stderr, MM_ERROR, UNKPERM, p);
		exit(32);
	}
	*permp = (ushort)(c - '0');
	*argpp = argp;
	return(ret);
}


/*
 * Procedure:     skipc
 *
 * Restrictions:
*/
/* 
 *	skipc - skip characters in string pointed to by p 
 *		until the char in c or '\n' or '\0' 
 * 		when the char in c or '\n' is found, 
 *		replace it with a '\0' 
 *
 *	input - pointer to pointer to the argument string
 *		the character to search for
 *
 *	output - zero if character found, 
 *		 1 if space or tab found, 
 *		 2 if null found, 
 *		 -1 if newline found
 */
int
skipc(pp, c)
register char 	**pp;
char 		c;
{
	register char 	*p = *pp;
	int		ret;

	while (*p && *p != c && *p != '\n' && *p != '\t' && *p != ' ')
		++p;
	if (*p == '\n') {
		*p = '\0';
		ret = -1;
	} else if (*p == ' ' || *p == '\t') {
		*p = '\0';
		ret = 1;
	} else if (*p) {
		*p++ = '\0';
		ret = 0;
	} else
		ret = 2;
	*pp = p;
	return(ret);
}


/*
 * Procedure:     delacl
 *
 * Restrictions:
                 pfmt: none
*/
/*
 *	delacl - deletes the acl entries in the first list
 *		marked for deletion from the second list
 *
 *	input -	1. ACL operations buffer with entries 
 *		   to be deleted or modified
 *		2. count of entries to be deleted or modified
 *		3. ACL buffer with entries from file
 *		4. pointer to count of file's entries
 *		5. file name
 *
 *	output - zero on success, -1 on failure
 *
 */

int
delacl(daclopbp, dentries, aclp, nentriesp, fname)
struct aclops 		*daclopbp;
int			dentries;
struct acl_entry 	*aclp;
int 			*nentriesp;
char			*fname;
{
	struct aclops	*aclopsp = daclopbp;
	struct acl_entry 	*tgtaclp, *srcaclp;
	void		acltochar();
	int 		i, j, k, match;
	int 		entries = *nentriesp;
	char 		aclentry[MRECLEN];

	tgtaclp = aclp;
	for (i = 0; i < dentries; i++, aclopsp++) {
		if (aclopsp->a_op != ACL_DEL)
			continue;
		match = NOMATCH;
		for (j = 0; j < entries; j++, tgtaclp++) {
			if (aclopsp->a_acl.ae_type == tgtaclp->ae_type) {
				switch(aclopsp->a_acl.ae_type) {
				case USER_OBJ:
				case GROUP_OBJ:
				case CLASS_OBJ:
				case OTHER_OBJ:
				case DEF_USER_OBJ:
				case DEF_GROUP_OBJ:
				case DEF_CLASS_OBJ:
				case DEF_OTHER_OBJ:
					match++;
					break;
				case USER:
				case GROUP:
				case DEF_USER:
				case DEF_GROUP:
					if (aclopsp->a_acl.ae_id == 
						tgtaclp->ae_id)
						match++;
					break;
				default:
					/* This can't really happen */
					pfmt(stderr, MM_ERROR, ILLEGALOPT2);
					exit(32);
				}	/* end switch */
				if (match) {
					/* found a match !!  remove acl entry */
					/* and compress buffer */
					srcaclp = tgtaclp + 1;
					for (k = j + 1; k < entries; k++)
						*tgtaclp++ = *srcaclp++;
					entries--;
					break;	/* out of j-loop */
				}
			}	/* end if type equal */
		}	/* end for "j" */
		if (!match) {
			/* did not find a match ! */
			acltochar(&aclopsp->a_acl, aclentry);
			pfmt(stderr, MM_ERROR, NOMATCHFOUND, fname, aclentry);
			return (-1);
		}
		tgtaclp = aclp;
	}	/* end for "i" */
	*nentriesp = entries;
	return (0);
}


/*
 * Procedure:     modacl
 *
 * Restrictions:
                 pfmt: none
*/
/*
 *	modacl - All ACL entries in the first buffer which are marked
 *		 for modification are modified in second buffer or added
 *		 to the buffer.
 *
 *	input -	1. ACL operations buffer with entries 
 *		   to be deleted or modified
 *		2. count of entries to be deleted or modified
 *		3. Ptr to ptr to ACL buffer with entries from file
 *		4. Pointer to number of entries which will fit in ACL buffer.
 *		5. pointer to count of file's entries
 *
 *	output - zero on success, -1 on failure
 *
 */
int
modacl(maclopbp, mentries, aclpp, bufentriesp, nentriesp)
struct aclops 	*maclopbp;	/* buffer of entries to modify */
int		mentries;	/* number of entries to modify */
struct acl_entry 	**aclpp;/* buffer of current ACL entries on file */
int		*bufentriesp;	/* max no. entries in current ACL buffer */
int		*nentriesp;	/* current no. entries in ACL buffer */
{
	struct aclops  	*aclopsp = maclopbp;	/* start of acls to modify */
	struct acl_entry	*aclp = *aclpp;
	struct acl_entry  	*srcaclp;
	struct acl_entry 	*tgtaclp = aclp;	
	struct acl_entry  	*newaclp;	/* new acl buffer with mods */
	char 		*malloc();
	int 		i, j; 
	int 		add_entries = 0;	/* ACL entries to add to file */
	int		entries = *nentriesp;
	int		bentries = *bufentriesp;
	int		newentries;
	int buflen;
	int match;			/* entry found in file's ACL */

	for (i = 0; i < mentries; i++, aclopsp++) {
		if (aclopsp->a_op != ACL_MOD) 
			/* this entry is marked for deletion. skip it. */
			continue;
		match = NOMATCH;
		for (j = 0; j < entries; j++, tgtaclp++) {
			if (aclopsp->a_acl.ae_type == tgtaclp->ae_type) {
				switch(aclopsp->a_acl.ae_type) {
				case USER_OBJ:
				case GROUP_OBJ:
				case CLASS_OBJ:
				case OTHER_OBJ:
				case DEF_USER_OBJ:
				case DEF_GROUP_OBJ:
				case DEF_CLASS_OBJ:
				case DEF_OTHER_OBJ:
					tgtaclp->ae_perm = aclopsp->a_acl.ae_perm;
					match++;
					break;
				case USER:
				case GROUP:
				case DEF_USER:
				case DEF_GROUP:
					if (aclopsp->a_acl.ae_id == 
						tgtaclp->ae_id){
						tgtaclp->ae_perm = 
							aclopsp->a_acl.ae_perm;
						match++;
					}
					break;
				default:
					/* This can't really happen */
					pfmt(stderr, MM_ERROR, ILLEGALOPT2);
					exit(32);
				}	/* end switch */
			} else
				continue;

			/* if match found, end search of file's ACL */
			if (match)
				break;

		}	/* end for "j" */
		
		/* if no match, this entry will be added as is to file */
		if (!match) {
			aclopsp->a_op |= ACL_ADD;
			add_entries++;
		}
		tgtaclp = aclp;
	}	/* end for "i" */
	
	/* now check if any acl entries should be added to acl buffer */
	if (add_entries) {
		/* 
		 * if the number of entries to be added will exceed
		 * the maximum number of entries that will fit in the
		 * buffer, allocate a new buffer 
		 */
		if ((add_entries + entries) > bentries) {
			newentries = add_entries + entries;
			GROWACLBUF(newentries, bentries, newaclp, aclp,entries);
		} 
		tgtaclp = aclp + entries;
		entries += add_entries;
		aclopsp = maclopbp;
		for (i = 0; i < mentries; i++, aclopsp++) 
			/* 
			 * search all -m entries, looking for those
			 * to be added to ACL. Add them to the buffer
			 * when they are found.
			 * Be sure to turn off "add" flag for next file.
			 */
			if ((aclopsp->a_op & ACL_ADD) != 0) {
				*tgtaclp++ = aclopsp->a_acl;
                                aclopsp->a_op &= ~ACL_ADD;
                        }
	}	/* end if (mentries) */
	*nentriesp = entries;
	*bufentriesp = bentries;
	*aclpp = aclp;
	return (0);
}

/*
 * Procedure:     ckmandacls
 *
 * Restrictions:
*/
/*
 *
 *	ckmandacls - validate that all required entries are in ACL buffer
 *
 *	input -  1. number of entries in ACL buffer
 *		 2. pointer to ACL buffer
 *
 *	output - 0 on success, -1 on failure
 */
int
ckmandacls(nentries, aclp)
int	nentries;
struct  acl_entry *aclp;
{
	struct acl_entry *p;
	int i;
	int u_obj = 0;
	int g_obj = 0;
	int c_obj = 0;
	int o_obj = 0;

	for (i = 0, p = aclp; i < nentries; i++, p++) {
		switch (p->ae_type) {
		case USER_OBJ:
			u_obj++;
			break;
		case GROUP_OBJ:
			g_obj++;
			break;
		case CLASS_OBJ:
			c_obj++;
			break;
		case OTHER_OBJ:
			o_obj++;
			break;
		default:
			break;
		}	/* end switch */
	}	/* end for */
	if (!u_obj || !g_obj || !c_obj || !o_obj)
		return -1;
	else
		return 0;
}

/*
 * Procedure:     getacl
 *
 * Restrictions:
                 acl(2): none
                 pfmt: none
*/
/*
 *
 *	getacl - get ACL entries from file
 *
 *	input -  1. pointer to file name
 *		 2. pointer to pointer to ACL buffer
 *		 3. pointer to max number of entries ACL buffer will hold
 *		 4. pointer to number of entries in buffer
 *
 *	output - zero on success, -1 on failure
 *
 */
int
getacl(filep, aclpp, bufentriesp, nentriesp)
char		*filep;
struct acl_entry	**aclpp;
int		*bufentriesp;
int		*nentriesp;
{
	struct acl_entry	*aclp = *aclpp;
	struct acl_entry	*newaclp;
	char 		*malloc();
	int		entries;
	int		bentries = *bufentriesp;
	int		newentries;
	ushort		bsize;
	  
	while ((*nentriesp = acl(filep, ACL_GET, bentries, aclp)) == -1) {
		switch (errno) {
		case ENOSPC:
			newentries = bentries + 1;
			GROWACLBUF(newentries, bentries, newaclp, aclp, 0);
			break;
		case EACCES:
			pfmt(stderr, MM_ERROR, NOPERM, filep);
			return (-1);
		case ENOENT:
			pfmt(stderr, MM_ERROR, NOFILEFOUND, filep);
			return (-1);
		case ENOPKG:
			pfmt(stderr, MM_ERROR, NOINSTALL);
			exit(3);
		default:
			pfmt(stderr, MM_ERROR, ACLFAIL, strerror(errno));
			return (-1);
		}
	}	/* end while */
	*bufentriesp = bentries;
	*aclpp = aclp;
	return (0);
}

/*
 * Procedure:     putacls
 *
 * Restrictions:
                 acl(2): none
                 pfmt: none
*/
/*
 *
 *	putacls - store ACLs on file
 *
 *	input   - 1. pointer to file name
 *		  2. pointer to ACL buffer
 *		  3. number of entries to store
 *		  4. continue flag on EINVAL from acl()
 *
 *	output  - zero on success, -1 on failure
 *
 */
int
putacls(filep, aclp, nentries, cont)
char 		*filep;
struct 	acl 	*aclp;
int		nentries;
int		cont;
{
	if (acl(filep, ACL_SET, nentries, aclp) == -1) {
		switch(errno) {
		case EPERM:
		case EACCES:
			pfmt(stderr, MM_ERROR, NOPERM, filep);
			break;
		case ENOSYS:
			pfmt(stderr, MM_ERROR, BADENTRYTYPE, filep);
			break;
		case ENOENT:
			pfmt(stderr, MM_ERROR, NOFILEFOUND, filep);
			break;
		case ENOTDIR:
			pfmt(stderr, MM_ERROR, NODEFAULT, filep);
			break;
		case ENOPKG:
			pfmt(stderr, MM_ERROR, NOINSTALL);
			exit(3);
			/* NOTREACHED */
			break;
		case EINVAL:
			pfmt(stderr, MM_ERROR, ACLFAIL, filep, strerror(errno));
			if (!cont)
				exit(32);
			break;
		default:
			pfmt(stderr, MM_ERROR, ACLFAIL, filep, strerror(errno));
			break;
		}	/* end switch */
		return (-1);
	}
	return (0);
}

/*
 * Procedure:     usage
 *
 * Restrictions:
                 pfmt: none
                 fprintf: none
*/
/* print Usage message */
void 
usage ()
{
	pfmt(stderr, MM_ACTION, ":98:\nusage: setacl [-r]\n");
	pfmt(stderr, MM_NOSTD, ":99:              [-m [[d[efault]:]u[ser]::operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":100:                  [[d[efault]:]u[ser]:uid:operm | perm[,...]]\n");
	pfmt(stderr, MM_NOSTD, ":101:                  [[d[efault]:]g[roup]::operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":102:                  [[d[efault]:]g[roup]:gid:operm | perm[,...]]\n");
	pfmt(stderr, MM_NOSTD, ":103:                  [[d[efault]:]c[lass]:operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":104:                  [[d[efault]:]o[ther]:operm | perm]]\n");
	pfmt(stderr, MM_NOSTD, ":105:              [-d [[d[efault]:]u[ser]:uid[,...]]\n");
	pfmt(stderr, MM_NOSTD, ":106:                  [[d[efault]:]g[roup]:gid[,...]]\n");
	pfmt(stderr, MM_NOSTD, ":107:                  [[d[efault]:]u[ser]::[,]]\n");
	pfmt(stderr, MM_NOSTD, ":108:                  [[d[efault]:]g[roup]::[,]]\n");
	pfmt(stderr, MM_NOSTD, ":109:                  [[d[efault]:]c[lass]:[,]]\n");
	pfmt(stderr, MM_NOSTD, ":110:                  [[d[efault]:]o[ther]:[,]]]\n");
	pfmt(stderr, MM_NOSTD, ":111:              object(s)\n");
	fprintf(stderr,"\n");
	pfmt(stderr, MM_NOSTD, ":112:        or\n");
	fprintf(stderr,"\n");
	pfmt(stderr, MM_NOSTD, ":113:       setacl [-r]\n");
	pfmt(stderr, MM_NOSTD, ":114:              -s u[ser]::operm | perm[,]\n");
	pfmt(stderr, MM_NOSTD, ":115:                 [d[efault]:u[ser]::operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":116:                 g[roup]::operm | perm[,]\n");
	pfmt(stderr, MM_NOSTD, ":117:                 [d[efault]:g[roup]::operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":118:                 c[lass]:operm | perm[,]\n");
	pfmt(stderr, MM_NOSTD, ":119:                 [d[efault]:c[lass]:operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":120:                 o[ther]:operm | perm[,]\n");
	pfmt(stderr, MM_NOSTD, ":121:                 [d[efault]:o[ther]:operm | perm[,]]\n");
	pfmt(stderr, MM_NOSTD, ":122:                 [[d[efault]:]u[ser]:uid:operm | perm[,...]]\n");
	pfmt(stderr, MM_NOSTD, ":123:                 [[d[efault]:]g[roup]:gid:operm | perm[,...]]\n");
	pfmt(stderr, MM_NOSTD, ":111:              object(s)\n");
	fprintf(stderr,"\n");
	pfmt(stderr, MM_NOSTD, ":112:        or\n");
	fprintf(stderr,"\n");
	pfmt(stderr, MM_NOSTD, ":124:       setacl [-r] -f file_name object(s)\n");
}

/*
 * Procedure:     acltochar
 *
 * Restrictions:
                 getpwuid: None
                 getgrgid: None
*/
/*
 *	
 *
 *	input	  - 1. pointer to ACL entry
 *		    2. pointer to character string
 *
 *	output	  - none
 *
 */
void
acltochar(aclp, entry)
struct acl_entry *aclp;
char *entry;
{
	struct passwd 	*getpwuid(),*pwd_p;
	struct group 	*getgrgid(),*grp_p;
	int		entry_type = aclp->ae_type;

	if (entry_type & ACL_DEFAULT) {
		entry += sprintf(entry, "default:");
		entry_type &= ~ACL_DEFAULT;
	}
	switch (entry_type) {
	case USER_OBJ:
		sprintf(entry, "user::");
		break;
	case USER:
		entry += sprintf(entry, "user:");
		if ((pwd_p = getpwuid(aclp->ae_id)) != NULL)
			sprintf(entry, "%s", pwd_p->pw_name);
		else
			sprintf(entry, "%d",aclp->ae_id);
		break;
	case GROUP_OBJ:
		sprintf(entry, "group::");
		break;
	case GROUP:
		entry += sprintf(entry, "group:");
		if ((grp_p = getgrgid(aclp->ae_id)) != NULL)
			sprintf(entry, "%s", grp_p->gr_name);
		else
			sprintf(entry, "%d", aclp->ae_id);
		break;
	case CLASS_OBJ:
		sprintf(entry, "class:");
		break;
	case OTHER_OBJ:
		sprintf(entry, "other:");
		break;
	default:
		break;
	}	/* end switch */
}
