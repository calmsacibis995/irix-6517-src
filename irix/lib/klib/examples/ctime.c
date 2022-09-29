#ident  "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/examples/RCS/ctime.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

/*
#define TIME_TEST
int print_all = 1;
*/


/*
 * print_ctime()
 */
void
print_time(time_t ctime, FILE *ofp)
{
    char *tbuf;

    tbuf = (char *)kl_alloc_block(100, K_TEMP);
    cftime(tbuf, "%m/%d/%Y", &ctime);
    fprintf(ofp, "cime=%u (%s)\n", ctime, tbuf);
    kl_free_block(tbuf);
}

/*
 * compare_time()
 */
int
compare_time(char *ts, time_t ctime)
{
	int ret;
    char *tbuf;

    tbuf = (char *)kl_alloc_block(100, K_TEMP);
    cftime(tbuf, "%m/%d/%Y", &ctime);
	ret = string_compare(ts, tbuf);
    kl_free_block(tbuf);
	return(ret);
}

/*
 * main()
 */
void
main(int argc, char **argv)
{
	int day, month, year;
	time_t ctime;
	char time_str[16];

#ifdef TIME_TEST
	year = 1970;
	while (year <= 2020) {
		month = 1;
		while (month < 13) {
			day = 1;
			while (1) {
				switch(month) {

					case 2:
						if (year % 4) {
							if (day > 28) {
								goto next_month;
							}
						}
						else {
							if (day > 28) {
								goto next_month;
							}
						}
						break;

					case 1:
					case 3:
					case 5:
					case 7:
					case 8:
					case 10:
					case 12:
						if (day > 31) {
							goto next_month;
						}
						break;


					case 4:
					case 6:
					case 9:
					case 11:
						if (day > 30) {
							goto next_month;
						}
						break;
				}
				sprintf(time_str, "%02d/%02d/%d", month, day, year);
				ctime = str_to_ctime(time_str);
				if (print_all || compare_time(time_str, ctime)) {
					printf("%s -- ", time_str);
					print_time(ctime, stdout);
				}
				day++;
			}
next_month:
			month++;
		}
		year++;
	}
#else
	if (argc > 1) {
		ctime = str_to_ctime(argv[1]);
		print_time(ctime, stdout);
	}
#endif /* TIME_TEST */
	exit(0);
}
