#ident "$Revision: 1.3 $"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <time.h>


main(int argc, char **argv)
{
    struct tms	t; /* not used */
    time_t	uptime;
    time_t	now;
    time_t	atboot;
    time_t	crtime;
    struct tm	tm;
    char	*Date = "Day Mon 00 00:00:00 0000\n";
    char	*Day  = "SunMonTueWedThuFriSat";
    char	*Month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char	*p, c;
    int		i;
    extern int	daylight;

    if (argc == 1)
	printf("%d\n", time(0));
    else if ((argc == 2) && (strcmp(argv[1], "-i") == 0)) {
	uptime = times(&t) / HZ; /* time in seconds */
	now = time(NULL);
	atboot = now - uptime;
	printf("START|%d|%s", atboot, ctime(&atboot));
    } else if ((argc == 3) && (strcmp(argv[1], "-d") == 0)) {
	crtime = atol(argv[2]);
	printf("%s", ctime(&crtime));
    } else if ((argc == 7) && (strcmp(argv[1], "-t") == 0)) {
	if (p = strstr(Month, argv[3]))
	    tm.tm_mon = (p - Month) / 3;
	else
	    fprintf(stderr, "Incorrect date string\n");

	tm.tm_mday = atoi(argv[4]);
	tm.tm_hour = (argv[5][0] - '0') * 10 + argv[5][1] - '0';
	tm.tm_min = (argv[5][3] - '0') * 10 + argv[5][4] - '0';
	tm.tm_sec = (argv[5][6] - '0') * 10 + argv[5][7] - '0';
	tm.tm_year = atoi(argv[6]) - 1900;
	tm.tm_isdst = -1;
	crtime = mktime(&tm);
	printf("%d\n", crtime);
    } else
	fprintf(stderr, "Error: incorrect arguments.\n");
}
