
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <nlist.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysmp.h>

char		*vmunix	= "/unix";
char		*mem = "/dev/kmem";
/*
 * The old way of reading numbers from the kernel was using this interface,
 * which looks up the "namelist".
 */

# define MAXNL		10

int		memfd = -1;

#if (_MIPS_SZLONG == 64)
#define NLIST nlist64
#else
#define NLIST nlist
#endif

static struct NLIST	nmlist[MAXNL] = {
	{ "ecc_num_1bit", 0, 0, 0 },
	{ "ecc_recent_1bit", 0, 0, 0 },
	{ "ecc_last_1bit", 0, 0, 0 },
	{ "lbolt", 0, 0, 0 },
	{ 0, 0, 0, 0 }
};

static void
stg_open(void)
{
   int		i;
   int		err;

	if (memfd >= 0)
		return;
	if ((memfd = open(mem, O_RDONLY)) == -1) {
		fprintf(stderr, "can't open %s\n", mem);
		exit(1);
	}
	if (NLIST(vmunix, nmlist) == -1) {
		fprintf(stderr, "can't open namelist file\n");
		exit(1);
	}
	for (err = 0, i = 0; nmlist[i].n_name != 0; i++)
		if (nmlist[i].n_value == 0) {
			fprintf(stderr, "can't get value of \"%s\"\n",
				nmlist[i].n_name);
			err++;
		}
	if (err)
		exit(1);
}

long
stg_getval(char *s)
{
   int		i;

	stg_open();
	for (i = 0; nmlist[i].n_name != 0; i++)
		if (strcmp(s, nmlist[i].n_name) == 0)
			return((long)nmlist[i].n_value);
	if (i >= MAXNL) {
		fprintf(stderr, "out of namelist storage space\n");
		exit(1);
	}
	nmlist[i+1].n_name = 0;
	nmlist[i].n_name = s;
	nmlist[i].n_value = 0;

	if (NLIST(vmunix, &nmlist[i]) == -1) {
		fprintf(stderr, "can't re-open namelist file\n");
		exit(1);
	}
	if (nmlist[i].n_value == 0) {
		fprintf(stderr, "can't get value of \"%s\"\n",
			nmlist[i].n_name);
		exit(1);
	}
	return((long)nmlist[i].n_value);
}

void
stg_sread(char *s, void *buf, int len)
{
   long	offset;

	stg_open();
	offset = stg_getval(s);
	if (lseek(memfd, offset, 0) == -1) {
		fprintf(stderr, "error seeking to %#x on memory\n", offset);
		exit(1);
	}
	if (read(memfd, buf, len) != len) {
		fprintf(stderr, "error reading from memory\n");
		exit(1);
	}
}

void
stg_vread(__psunsigned_t addr, void *buf, int len)
{
	stg_open();
	if (lseek(memfd, (off_t)addr, 0) == -1) {
		fprintf(stderr, "error seeking to %#x on memory\n", addr);
		exit(1);
	}
	if (read(memfd, buf, len) != len) {
		fprintf(stderr, "error reading from memory\n");
		exit(1);
	}
}


main() {

	ulong ecc_num_1bit;
	ulong ecc_recent_1bit;
	time_t ecc_last_1bit;
	time_t lbolt;

	stg_sread("ecc_num_1bit", &ecc_num_1bit, sizeof(ecc_num_1bit));
	stg_sread("ecc_recent_1bit", &ecc_recent_1bit, sizeof(ecc_recent_1bit));
	stg_sread("ecc_last_1bit", &ecc_last_1bit, sizeof(ecc_last_1bit));
	stg_sread("lbolt", &lbolt, sizeof(lbolt));

	printf("Number of single bit errors since boot: %ld\n", ecc_num_1bit);
	printf("Number of single bit errors since last reported: %ld\n", 
		ecc_recent_1bit);
	printf("Time since last single bit error reported: %d seconds\n", 
		(ecc_last_1bit > 0 ? (lbolt-ecc_last_1bit)/100 : -1));
	return 0;
}

