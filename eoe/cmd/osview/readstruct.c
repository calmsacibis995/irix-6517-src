# include	<stdio.h>
# include	<fcntl.h>
# include	<nlist.h>
# include	<string.h>
# include	<unistd.h>
# include 	<sys/sysmp.h>
# include	<sys/sysget.h>
# include	<sys/errno.h>
# include	"osview.h"

extern int errno;

struct kname {
	char *name;
	int  count;
};
struct kname knames[] = {
	KSYM_AVENRUN,		-1,
	KSYM_PSTART,		-1,
	KSYM_VN_VNUMBER,	-1,
	KSYM_VN_NFREE,		-1,
	KSYM_VN_EPOCH,		-1,
	KSYM_SYSSEGSZ,		-1,
	KSYM_GFXINFO, 		-1,
	KSYM_MIN_FILE_PAGES,	-1,
};
int num_knames = sizeof(knames) / sizeof(struct kname);

/* Routine to return data for 1 cpu */
int
stg_sysget_cpu1(int id, char *buf, int buflen, int cpu)
{
	sgt_cookie_t ck;
	int err;

	if (cellid >= 0) {

		/* Set up sysget to read the logical cpu from one cell */

		SGT_COOKIE_INIT(&ck);
		SGT_COOKIE_SET_CELL(&ck, cellid);
		SGT_COOKIE_SET_LCPU(&ck, cpu);
		err = sysget(id, buf, buflen, SGT_READ | SGT_CPUS, &ck);
	}
	else {
		err = (int)sysmp(MP_SAGET1, id, buf, buflen, cpu);
	}
	if (err < 0 && errno != EINVAL) {

		/* Anything other than EINVAL is something we didn't expect */

		if (cellid >= 0) {
			perror("sysget");
		}
		else {
			perror("sysmp");
		}
		fprintf(stderr,
			"stg_sysget_cpu1: SGT_READ of (%d) for cpu %d failed\n",
			id, cpu);
		pbyebye();
	}
	return(err);
}
	
/* Routine to return data for all cpus. For the default case we specify
 * the SGT_SUM flag since a copy of a cpu's data exists per-cell.
 */
void
stg_sysget_cpus(int id, char *buf, int buflen)
{
	sgt_cookie_t ck;
	int err, flags = SGT_READ | SGT_CPUS | SGT_SUM;

	SGT_COOKIE_INIT(&ck);
	if (cellid >= 0) {

		/* Set up sysget to read the cpus from one cell */

		SGT_COOKIE_SET_CELL(&ck, cellid);
		flags &= ~SGT_SUM;
	}
	err = sysget(id, buf, buflen, flags, &ck);
	if (err < 0) {
		perror("sysget");
		fprintf(stderr,
		   "stg_sysget_cpus: SGT_READ of (%d) failed\n", id);
		pbyebye();
	}
}

void
stg_sysget(int id, char *buf, int buflen)
{
	sgt_cookie_t ck;
	int err;

	if (cellid >= 0) {

		/* Set up sysget to read one cell */

		SGT_COOKIE_INIT(&ck);
		SGT_COOKIE_SET_CELL(&ck, cellid);
		err = sysget(id, buf, buflen, SGT_READ, &ck);
	}
	else {
		err = (int)sysmp(MP_SAGET, id, buf, buflen);
	}
	if (err < 0 && id != MPSA_TILEINFO) {
		if (cellid >= 0) {
			perror("sysget");
		}
		else {
			perror("sysmp");
		}
		fprintf(stderr, "stg_sysget: SGT_READ of (%d) failed\n", id);
		pbyebye();
	}
}

void
stg_sysget_cells(int id, char *buf, int buflen)
{
	sgt_cookie_t ck;
	int err;

	SGT_COOKIE_INIT(&ck);
	if (cellid >= 0) {

		/* Set up sysget to read one cell */

		SGT_COOKIE_SET_CELL(&ck, cellid);
	}
	err = sysget(id, buf, buflen, SGT_READ, &ck);
	if (err < 0 && id != MPSA_TILEINFO) {
		perror("sysget");
		fprintf(stderr, "stg_sysget_cells: SGT_READ of (%d) failed\n",
			 id);
		pbyebye();
	}
}

void
stg_sread(char *s, void *buf, int len)
{
	sgt_cookie_t ck;
	sgt_info_t info;
	int i;
	int flags;


	for (i = 0; i < num_knames; i++) {
		if (!strcmp(s, knames[i].name)) {
			break;
		}
	}
	if (i == num_knames) {
		fprintf(stderr, "stg_sread: %s is unknown\n", s);
		pbyebye();
	}
	
	if (knames[i].count < 0) {

		/* Find out how many of these exist */

		SGT_COOKIE_INIT(&ck);
		SGT_COOKIE_SET_KSYM(&ck, knames[i].name);
		if (sysget(SGT_KSYM, (char *)&info, sizeof(info), SGT_INFO, &ck) < 0) {
			perror("sysget");
			fprintf(stderr, "sgt_sread: SGT_INFO of %s failed\n",s);
			pbyebye();
		}
		knames[i].count = info.si_num;
	}
			
	flags = SGT_READ;
	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, knames[i].name);
	if (cellid > 0) {

		/* We are only interested in one cell. */

		SGT_COOKIE_SET_CELL(&ck, cellid);
	}
	else if (knames[i].count > 1) {

		/* There appear to be copies of this object on each cell.
		 * We will get the sum.
		 */

		flags |= SGT_SUM;
	}

	if (sysget(SGT_KSYM, buf, len, flags, &ck) < 0) {
		perror("sysget");
		fprintf(stderr, "stg_sread: SGT_READ for %s failed\n", s);
		pbyebye();
	}
}
