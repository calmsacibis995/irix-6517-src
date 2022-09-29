#ident "$Revision: 1.6 $"

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)dumpitime.c	5.2 (Berkeley) 5/28/86";
 */

#include "dump.h"

static struct itime *ithead;	/* head of the list version */

static int getrecord(FILE *, struct idates *);
static int makeidate(struct idates *, char *);
static void readitimes(FILE *);
static void recout(FILE *, struct idates *);

char *
prdate(time_t d)
{
	char *p;

	if (d == 0)
		return("the epoch");
	p = ctime(&d);
	p[24] = 0;
	return(p);
}

void
inititimes(void)
{
	FILE *df;

	if ((df = fopen(increm, "r")) == NULL) {
		if (errno != ENOENT)	/* don't panic the operator */
			perror(increm);
		return;
	}
	(void) flock(fileno(df), LOCK_SH);
	readitimes(df);
	fclose(df);
}

static void
readitimes(FILE *df)
{
	int		i;
	struct	itime	*itwalk;

	for (;;) {
		itwalk = (struct itime *)calloc(1, sizeof (struct itime));
		if (getrecord(df, &(itwalk->it_value)) < 0)
			break;
		nidates++;
		itwalk->it_next = ithead;
		ithead = itwalk;
	}

	/*
	 *	arrayify the list, leaving enough room for the additional
	 *	record that we may have to add to the idate structure
	 */
	idatev = (struct idates **)calloc(nidates + 1,sizeof (struct idates *));
	itwalk = ithead;
	for (i = nidates - 1; i >= 0; i--, itwalk = itwalk->it_next)
		idatev[i] = &itwalk->it_value;
}

void
getitime(void)
{
	struct	idates	*ip;
	int		i;
	char		*fname;

	fname = disk;
#ifdef FDEBUG
	msg("Looking for name %s in increm = %s for delta = %c\n",
		fname, increm, incno);
#endif
	spcl.c_ddate = 0;
	lastincno = '0';

	inititimes();
	if (idatev == 0)
		return;
	/*
	 *	Go find the entry with the same name for a lower increment
	 *	and older date
	 */
	ITITERATE(i, ip) {
		if (strncmp(fname, ip->id_name, sizeof (ip->id_name)) != 0)
			continue;
		if (ip->id_incno >= incno)
			continue;
		if (ip->id_ddate <= spcl.c_ddate)
			continue;
		spcl.c_ddate = ip->id_ddate;
		lastincno = ip->id_incno;
	}
}

void
putitime(void)
{
	FILE		*df;
	struct	idates	*itwalk;
	int		i;
	int		fd;
	char		*fname;
	mode_t		omask;

	if(uflag == 0)
		return;
	if ((df = fopen(increm, "r+")) == NULL) {
		if (errno == ENOENT) {
			fprintf(stderr, "  DUMP: creating file %s\n", increm);
			omask = umask((mode_t)0022);
			if ((df = fopen(increm, "w")) == NULL) {
				perror(increm);
				fprintf(stderr, "Couldn't update %s\n", increm);
				return;
			}
			(void) umask(omask);
		} else {
			perror(increm);
			fprintf(stderr, "Couldn't update %s\n", increm);
			return;
		}
	}
	fd = fileno(df);
	(void) flock(fd, LOCK_EX);
	fname = disk;
	if (idatev)
		free(idatev);
	idatev = 0;
	nidates = 0;
	ithead = 0;
	readitimes(df);
	if (fseek(df,0L,0) < 0) {   /* rewind() was redefined in dumptape.c */
		perror("fseek");
		dumpabort();
	}
	spcl.c_ddate = 0;
	ITITERATE(i, itwalk){
		if (strncmp(fname, itwalk->id_name,
				sizeof (itwalk->id_name)) != 0)
			continue;
		if (itwalk->id_incno != incno)
			continue;
		goto found;
	}
	/*
	 *	construct the new upper bound;
	 *	Enough room has been allocated.
	 */
	itwalk = idatev[nidates] =
		(struct idates *)calloc(1, sizeof(struct idates));
	nidates += 1;
  found:
	strncpy(itwalk->id_name, fname, sizeof (itwalk->id_name));
	itwalk->id_incno = incno;
	itwalk->id_ddate = spcl.c_date;

	ITITERATE(i, itwalk){
		recout(df, itwalk);
	}
	if (ftruncate(fd, ftell(df))) {
		perror("ftruncate");
		dumpabort();
	}
	(void) fclose(df);
	msg("level %c dump on %s\n", incno, prdate(spcl.c_date));
}

static void
recout(FILE *file, struct idates *what)
{
	fprintf(file, DUMPOUTFMT,
		what->id_name,
		what->id_incno,
		ctime(&(what->id_ddate))
	);
}

int	recno;

static int
getrecord(FILE *df, struct idates* idatep)
{
	char		buf[BUFSIZ];

	recno = 0;
	if ( (fgets(buf, BUFSIZ, df)) != buf)
		return(-1);
	recno++;
	if (makeidate(idatep, buf) < 0)
		msg("Unknown intermediate format in %s, line %d\n",
			increm, recno);

#ifdef FDEBUG
	msg("getrecord: %s %c %s\n",
		idatep->id_name, idatep->id_incno, prdate(idatep->id_ddate));
#endif
	return(0);
}

static int
makeidate(struct idates *ip, char *buf)
{
	char	un_buf[128];

	if (sscanf(buf, DUMPINFMT, ip->id_name, &ip->id_incno, un_buf) != 3)
		return(-1);
	ip->id_ddate = unctime(un_buf);
	if (ip->id_ddate < 0)
		return(-1);
	return(0);
}

/*
 * This is an estimation of the number of TP_BSIZE blocks in the file.
 * It estimates the number of blocks in files with holes by assuming
 * that all of the blocks accounted for by di_blocks are data blocks
 * (when some of the blocks are usually used for indirect pointers);
 * hence the estimate may be high.
 */
void
est(struct efs_dinode *ip)
{
	unsigned long s;

	/*
	 * ip->di_size is the size of the file in bytes.
	 * ip->di_blocks stores the number of sectors actually in the file.
	 * If there are more sectors than the size would indicate, this just
	 *	means that there are indirect blocks in the file or unused
	 *	sectors in the last file block; we can safely ignore these
	 *	(s = t below).
	 * If the file is bigger than the number of sectors would indicate,
	 *	then the file has holes in it.	In this case we must use the
	 *	block count to estimate the number of data blocks used, but
	 *	we use the actual size for estimating the number of indirect
	 *	dump blocks (t vs. s in the indirect block calculation).
	 */
	esize++;
	/*
	 * No holes in EFS.
	 */
	s = howmany(ip->di_size, TP_BSIZE);
	esize += s;
}

void
bmapest(char *map)
{
	int	i, n;

	n = -1;
	for (i = 0; i < msiz; i++)
		if(map[i])
			n = i;
	if(n < 0)
		return;
	n++;
	esize++;
	esize += howmany(n * sizeof map[0], TP_BSIZE);
}
