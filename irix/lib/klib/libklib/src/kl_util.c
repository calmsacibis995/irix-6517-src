#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <klib/klib.h>

/*
 * kl_shift_value()
 */
int
kl_shift_value(k_uint_t value)
{
	int i;

	if (value == 0) {
		return(-1);
	}

	for (i = 0; i < 64; i++) {
		if (value & (((k_uint_t)1) << i)) {
			break;
		}
	}
	return(i);
}

/*
 * kl_get_bit_value()
 *
 * x = byte_size, y = bit_size, z = bit_offset
 */
k_uint_t
kl_get_bit_value(k_ptr_t ptr, int x, int y, int z)
{
	k_uint_t value, mask;
	int right_shift, upper_bits;

	right_shift = ((x * 8) - (y + z)) + ((8 - x) * 8);

	if (y >= 32) {
		int upper_bits = y - 32;
		mask = ((1 << upper_bits) - 1);
		mask = (mask << 32) | 0xffffffff;
	}
	else {
		mask = ((1 << y) - 1);
	}
	bcopy(ptr, &value, x);
	value = value >> right_shift;
	return (value & mask);
}

/*
 * string_compare()
 */
int
string_compare(char *s1, char *s2)
{
	int len_1 = strlen(s1);
	int len_2 = strlen(s2);

	if (len_1 > len_2) {
		return(1);
	}
	else if (len_2 > len_1) {
		return(-1);
	}
	return(strcmp(s1, s2));
}

/*
 * string_match()
 */
int
string_match(char *s1, char *s2)
{
	if (s1) {
		if (!s2) {
			return(0);
		}
		else if (strcmp(s1, s2)) {
			return(0);
		}
	}
	else if (s2) {
		return(0);
	}
	return(1);
}


#define SECONDS_IN_DAY 86400
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_MINUTE 60

/*
 * str_to_ctime()
 */
time_t
str_to_ctime(char *timestr)
{
	int i, tzadjust;
	int	days, months, years;
	int year_len, len;
	char *c, *day, *month, *year;
	char tstring[11];
	time_t ctime = 0;
	struct tm ltime, gmtime;

	strcpy(tstring, timestr);
	month = tstring;
	if (c = strchr(month, '/')) {
		len = c - month;
		month[len] = 0;
		day = c + 1;
		if (c = strchr(day, '/')) {
			len = c - day;
			day[len] = 0;
			year = c + 1;

			years = atoi(year);
			year_len = strlen(year);
			if (year_len == 2) {
				if (years > 70) {
					years += 1900;
				}
				else {
					years += 2000;
				}
			}
			else if (year_len != 4) {
				return(0);
			}
			for (i = 1970; i < years; i++) {
				if (i % 4) {
					ctime += 365 * SECONDS_IN_DAY;
				}
				else {
					ctime += 366 * SECONDS_IN_DAY;
				}
			}

			months = atoi(month);
			for (i = 1; i < months; i++) {
				switch (i) {

					case 2:
						if (years % 4) {
							ctime += 28 * SECONDS_IN_DAY;
						}
						else {
							ctime += 29 * SECONDS_IN_DAY;
						}
						break;

					case 4:
					case 6:
					case 9:
					case 11:
						ctime += 30 * SECONDS_IN_DAY;
						break;

					case 1:
					case 3:
					case 5:
					case 7:
					case 8:
					case 10:
						ctime += 31 * SECONDS_IN_DAY;
						break;

				}
			}

			/* Add the seconds for all full days in the month
			 */
			days = atoi(day);
			ctime += (days - 1) * SECONDS_IN_DAY;

			/* XXX -- Add the time for the hours, minutes, and seconds 
			 */

			/* Adjust for local time and daylight savings time. We have
			 * to make sure that we watch for the case where we are at 
			 * the start of a month or year...
			 */
			localtime_r(&ctime, &ltime);
			gmtime_r(&ctime, &gmtime);
			if ((ltime.tm_year < gmtime.tm_year) ||
					(ltime.tm_mon < gmtime.tm_mon) || 
					(ltime.tm_mday < gmtime.tm_mday)) {
				tzadjust = 24 - ltime.tm_hour;
			}
			else {
				tzadjust = (gmtime.tm_mday - ltime.tm_mday);
			}
			ctime += (tzadjust * SECONDS_IN_HOUR);
			return(ctime);
		}
	}
	return(0);
}
