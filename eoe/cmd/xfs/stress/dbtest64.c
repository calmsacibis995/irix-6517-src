

#include <sys/types.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <ndbm.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* #define WorkDir	"/xfs" */
#define DBNAME	"DBtest"
#define DBRECS	100000
#define LOOPS	100

typedef struct _myDB {
	char		data[976];
	unsigned short	checksum;
	long		align;
} myDB;	

int InitDbmLookup(int);
int DoDbmLookup(void);
void CleanupDbmLookup(void);
void DoSysError(char *, int);
int creatDbRec(myDB *);
unsigned short calccksum(char *, int);
void CkSumError(char *, unsigned short, unsigned short);
int InsertKey(unsigned short *, int, unsigned short);
void DumpKeys(int, unsigned short);

DBM64 *dbm;
int numDbmEntries, keyIdx, Dups = 0, Errors = 0;
unsigned short *KeyArray, *DupsArray;
int debugflg = 0;	/* are we in debugging mode? */
int errflg = 0;		/* usage option errors */
int ignoreflg = 1;	/* ignore errors in lookup; default = on */
int loops = LOOPS;
int randseed = 0;

main(int argc, char *argv[])
{
	int	numrecs = DBRECS, c, l;

	while ((c = getopt(argc, argv, "idn:l:s:")) != EOF) {
		switch(c) {
			case 'i':
				ignoreflg++; break;
			case 'd':
				debugflg++; break;
			case 'n':
				numrecs = atoi(optarg);
				numrecs = (numrecs < 1) ? DBRECS : numrecs;
				break;
			case 'l':
				loops = atoi(optarg);
				loops = (loops < 1) ? LOOPS : loops;
				break;
			case 's':
				randseed = atoi(optarg);
				randseed = (randseed < 0) ? 0 : randseed;
				break;
			case '?':
				errflg++;
				break;
		}
	}
	if (errflg) {
		printf("Usage: dbtest [-di] [-n number] [-l loop] [-s randseed]\n");
		exit(0);
	}
	if (optind > argc) {
		printf("Extra arguments ignored\n");
		for ( ; optind<argc; optind++)
			printf("\t%s\n", argv[optind]);
	}

	printf("dbtest v1.0\n\n");
	printf("Creating database containing %d records...\n", numrecs);
	printf("\tperforming lookups for %d iterations...\n", loops);
	if (randseed)
		printf("\tusing %d as seed for srandom()...\n\n", randseed);
	InitDbmLookup(numrecs);
	printf("\t\nThere were %d duplicate checksums generated\n", Dups);
	for (l=0; l<loops; l++) {
		printf("\nPerforming lookups on database...\n");
		if (DoDbmLookup() != numrecs)
			printf("\nError performing lookups!\n");
		else
			printf("\nLookups succeeded...\n");
	}
	printf("\nCleaning up database...\n");
	printf("\t\nThere were %d duplicate checksums generated\n", Dups);
	CleanupDbmLookup();
	if (debugflg)
		for (l=0; l<Dups; l++) {
			if ((l % 8) == 0)
				putchar('\n');
			printf("0x%x\t", DupsArray[l]);
		}

	return 0;
}


int InitDbmLookup(int howmany)
{
	char	filename[100];
	datum	key, content;
	int	i, rc;
	myDB	dbRec;

	sprintf(filename, "%s-%d", DBNAME, getpid());
	dbm = dbm_open64(filename, O_WRONLY|O_CREAT, 0644);
	if(dbm == NULL) DoSysError("\ndbm_open", (int)dbm);

	if ((KeyArray = (unsigned short *)calloc( howmany,
				sizeof(unsigned short))) == NULL)
		DoSysError("\ncalloc:KeyArray", -1);
	if ((DupsArray = (unsigned short *)calloc( 100,
				sizeof(unsigned short))) == NULL)
		DoSysError("\ncalloc:DupsArray", -1);

	keyIdx = 0;
	for(i=0; i<howmany; i++) {

		if ( creatDbRec(&dbRec) )
			DoSysError("\ncreatDbRec", -1);
		if (debugflg)
			printf("created rec #%-7d\t%x\r", i+1, dbRec.checksum);

		if (InsertKey(KeyArray, keyIdx, dbRec.checksum))
			keyIdx++;

		key.dptr = (char *)&(dbRec.checksum);
		key.dsize = sizeof(dbRec.checksum);
		content.dptr = (char *)&dbRec;
		content.dsize = sizeof(dbRec);
write(2, NULL, 0);
		rc = dbm_store64(dbm, key, content, DBM_INSERT);
		if (rc < 0)
			DoSysError("\ndbm_store", rc);
		else if (rc == 1) {
			keyIdx--;	/* key is already in database */
			DupsArray[Dups++] = dbRec.checksum;
		}
	}
	numDbmEntries = i;
	dbm_close64(dbm); /* close to eliminate chance of in-memory caching */
	dbm = dbm_open64(filename, O_RDONLY, 0);
	if(dbm == NULL) DoSysError("\ndbm_open", (int)dbm);
	return 0;
}

int DoDbmLookup(void)
{
	datum key, content;
	int i, j, n;
	myDB *dbp;
	unsigned tmpck;

	printf("\n\tSequential lookups...\n");
	for(i=0, j=0; i<numDbmEntries; i++) {
		key.dptr = (char *)(KeyArray+j);
		key.dsize = sizeof(KeyArray[0]);

write(2, NULL, 0);
		content = dbm_fetch64(dbm, key);
		dbp = (myDB *)content.dptr;
		if ( !content.dptr ) {
			n = dbm_error(dbm);
			if (n) printf("\n\ndbm_error: %d\n", n);
			printf("\n%d: Sequential Lookup of key %4x failed!\n",
				i, KeyArray[j]);
			DumpKeys(i, KeyArray[j]);
			Errors++;
			if (ignoreflg) {
				j++;
				continue;
			}
			assert( dbp );
		}

		if (debugflg && dbp)
			printf("Seq rec #%-6d: checksum %4x (%4x)\r", i,
				dbp->checksum, KeyArray[j]);

		if (content.dsize == 0) {
			printf("\nrec #%-8d: key = %4x (%d)\n", i, KeyArray[j], j);
			DoSysError("\ndbm_fetch", content.dsize);
		}
		if (dbp->checksum != KeyArray[j])
			CkSumError("KeySequential", dbp->checksum, KeyArray[j]);
		if ((tmpck = calccksum(dbp->data, sizeof(dbp->data))) != KeyArray[j])
			CkSumError("DataSequential", tmpck, KeyArray[j]);
		if (++j >= keyIdx)
			j = 0;
	}
	printf("\n\n\tRandom lookups...\n");
	for(i=0; i<numDbmEntries; i++) {
		n = random() % keyIdx;
		key.dptr = (char *)(KeyArray+n);
		key.dsize = sizeof(KeyArray[0]);
		content = dbm_fetch64(dbm, key);
		dbp = (myDB *)content.dptr;
		if ( !content.dptr ) {
			n = dbm_error(dbm);
			if (n) printf("\n\ndbm_error: %d\n", n);
			printf("\n%d: Random Lookup of key %4x failed!\n",
				i, KeyArray[n]);
			DumpKeys(n, KeyArray[n]);
			Errors++;
			if (ignoreflg) {
				continue;
			}
			assert( dbp );
		}

		if (debugflg && dbp)
			printf("Rnd rec #%-6d: checksum %4x (%4x)\r", i,
				dbp->checksum, KeyArray[n]);

		if (content.dsize == 0)
			DoSysError("\ndbm_fetch", content.dsize);
		if (dbp->checksum != KeyArray[n])
			CkSumError("KeyRand", dbp->checksum, KeyArray[n]);
		if ((tmpck = calccksum(dbp->data, sizeof(dbp->data))) != KeyArray[n])
			CkSumError("DataRand", tmpck, KeyArray[n]);
	}
	return i;
}

void CleanupDbmLookup(void)
{
/*
	char filename[100];
	int rc;
*/

	dbm_close64(dbm);
/*
	sprintf(filename, "%s-%d.dir", DBNAME, getpid());
	rc = unlink(filename);
	if (rc) DoSysError("\nunlink", rc);
	sprintf(filename, "%s-%d.pag", DBNAME, getpid());
	rc = unlink(filename);
	if (rc) DoSysError("\nunlink", rc);
*/
}

void DoSysError(char *msg, int rc)
{
	perror(msg);
	fprintf(stderr, "Bailing out! status = %d\n", rc);
	exit(-1);
}

int creatDbRec(myDB *dbp)
{
	static int once = 0;
	int	i, j;
	long	*ptr;
	long    timeseed;

	if (!once) {
		once++;
		if ( !randseed ) {
			timeseed = time(0);
			printf("\tusing %d as seed for srandom()...\n\n",
				timeseed);
			srandom(timeseed);
		} else
			srandom(randseed);
	}
	ptr = (long *)&(dbp->data);
	j = sizeof(dbp->data) / sizeof(long);
	for (i=0; i < j; i++, ptr++) {
		*ptr = random();
	}
	for (i=(j*sizeof(long)); i<sizeof(dbp->data); i++)
		dbp->data[i] = (char)time(0);

	dbp->checksum = calccksum(dbp->data, sizeof(dbp->data));
	return 0;
}

unsigned short calccksum(char *ptr, int size)
{
	unsigned short sum;
	int i, n;

	sum = 0;
	n = ((size > 100) ? 100 : size);
	for (i=0; i<n && ptr; i++, ptr++) {
		if (sum & 01)
			sum = (sum >> 1) + 0x8000;
		else
			sum >>= 1;
		sum += (unsigned short)*ptr;
		sum &= 0xffff;
	}
	return sum;
}

/*ARGSUSED*/
void CkSumError(char *msg, unsigned short ck1, unsigned short ck2)
{
	Errors++;
	printf("%s\n\tChecksum error, %u != %u, Total errors = %d\n",
		msg, ck1, ck2, Errors);
	exit(1);
}

int InsertKey(unsigned short *k, int idx, unsigned short val)
{
/*
	int i, found=0;
*/

	return( (k[idx] = val) );

/*
	for (i=0; i<idx; i++) {
		if (k[i] == val) {
			found++;
			break;
		}
	}

	if (!found)
		k[idx] = val;

	return(!found);
*/
}

void DumpKeys(int n, unsigned short key)
{
/*
	int	i;
	unsigned short *p;
*/
	FILE	*f;

	if ((f = fopen("keys", "a")) == NULL) {
		perror("open(keys)");
		exit(1);
	}
/*
	for (i=0, p=arr; i<keyIdx && p; i++, p++)
*/
		fprintf(f, "%d: 0x%x\n", n, key);

	fclose(f);
}
