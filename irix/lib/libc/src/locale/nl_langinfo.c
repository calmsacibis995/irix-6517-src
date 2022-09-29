/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/nl_langinfo.c	1.8"

#ifdef __STDC__
	#pragma weak nl_langinfo = _nl_langinfo
#endif
#include "synonyms.h"
#include "mplib.h"
#include <stdlib.h>
#include <limits.h>
#include <nl_types.h>
#include <langinfo.h>
#include <locale.h>
#include <time.h>
#include <string.h>

#include <wchar.h>
#include <iconv_cnv.h>
#include <iconv_int.h>

#define MAX 128

#define LOCALE_CS_MAP "/usr/lib/locale/locale.alias"

static char *old_locale;

char *
nl_langinfo( item )
nl_item      item;
{
struct tm tm;
static char *buf;
char buf2[MAX];
struct lconv *currency;
char *s;
size_t size;
const char *rptr;

	LOCKDECL(l);
	if (!buf && (buf = malloc(MAX)) == NULL)
		return "";

	LOCKINIT(l, LOCKLOCALE);
	switch (item) {
		/*
		 * The seven days of the week in their full beauty
		 */

		case DAY_1 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=0;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Sunday";
			break;

		case DAY_2 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=1;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Monday";
			break;

		case DAY_3 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=2;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Tuesday";
			break;

		case DAY_4 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=3;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Wednesday";
			break;

		case DAY_5 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=4;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Thursday";
			break;

		case DAY_6 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=5;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Friday";
			break;

		case DAY_7 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=6;
			size = strftime(buf,MAX,"%A",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Saturday";
			break;


		/*
		 * The abbreviated seven days of the week
		 */
		case ABDAY_1 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=0;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Sun";
			break;

		case ABDAY_2 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=1;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Mon";
			break;

		case ABDAY_3 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=2;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Tue";
			break;

		case ABDAY_4 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=3;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Wed";
			break;

		case ABDAY_5 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=4;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Thur";
			break;

		case ABDAY_6 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=5;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Fri";
			break;

		case ABDAY_7 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_wday=6;
			size = strftime(buf,MAX,"%a",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Sat";
			break;



		/*
		 * The full names of the twelve months...
		 */
		case MON_1 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=0;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "January";
			break;

		case MON_2 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=1;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Feburary";
			break;

		case MON_3 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=2;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "March";
			break;

		case MON_4 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=3;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "April";
			break;

		case MON_5 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=4;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "May";
			break;

		case MON_6 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=5;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "June";
			break;

		case MON_7 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=6;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "July";
			break;

		case MON_8 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=7;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "August";
			break;

		case MON_9 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=8;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "September";
			break;

		case MON_10 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=9;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "October";
			break;

		case MON_11 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=10;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "November";
			break;

		case MON_12 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=11;
			size = strftime(buf,MAX,"%B",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "December";
			break;

		/*
		 * ... and their abbreviated form
		 */
		case ABMON_1 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=0;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Jan";
			break;

		case ABMON_2 :
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=1;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Feb";
			break;

		case ABMON_3 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=2;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Mar";
			break;

		case ABMON_4 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=3;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Apr";
			break;

		case ABMON_5 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=4;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "May";
			break;

		case ABMON_6 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=5;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Jun";
			break;

		case ABMON_7 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=6;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Jul";
			break;

		case ABMON_8 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=7;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Aug";
			break;

		case ABMON_9 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=8;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Sep";
			break;

		case ABMON_10 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=9;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Oct";
			break;

		case ABMON_11 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=10;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Nov";
			break;

		case ABMON_12 : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_mon=11;
			size = strftime(buf,MAX,"%b",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "Dec";
			break;

		case AM_STR : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_hour=1;
			size = strftime(buf,MAX,"%p",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "AM";
			break;

		case PM_STR : 
			(void) memset((void*)&tm,sizeof (struct tm),0);
			tm.tm_hour=13;
			size = strftime(buf,MAX,"%p",&tm);
			if (size)
				rptr = (const char *) buf;
			else
				rptr = "PM";
			break;


		/*
		 * plus some special strings you might need to know
		 */

		case RADIXCHAR :
		case THOUSEP :
		case CRNCYSTR :

			currency = localeconv();
			switch (item) {

				case THOUSEP :
					rptr = (const char *) currency->thousands_sep;
					break;

				case RADIXCHAR :
					rptr = (const char *) currency->decimal_point;
					break;

				case CRNCYSTR : 
					if (currency->p_cs_precedes == CHAR_MAX || *(currency->currency_symbol) == '\0') {
						rptr = "";
						break;
					}
					if (currency->p_cs_precedes == 1)
						buf[0] = '-';
					else
						buf[0] = '+';
					(void) strcpy(&buf[1], currency->currency_symbol);
					rptr = (const char *) buf;
					break;
			}
			break;

		/*
		 * Default string used to format date and time
		 *	e.g. Sunday, August 24 21:08:38 MET 1986
		 */

		case T_FMT :
			old_locale = setlocale(LC_MESSAGES,(char*)NULL);
			(void) strcpy(buf,old_locale);
			(void)setlocale(LC_MESSAGES,setlocale(LC_TIME,(char*)NULL));
			s = gettxt("Xopen_info:1","%H:%M:%S");
			(void) setlocale(LC_MESSAGES,buf);
			if (strcmp(s,"Message not found!!\n"))
				rptr = (const char *) s;
			else 
				rptr = "%H:%M:%S";
			break;

		case D_FMT :
			old_locale = setlocale(LC_MESSAGES,(char*)NULL);
			(void) strcpy(buf,old_locale);
			(void)setlocale(LC_MESSAGES,setlocale(LC_TIME,(char*)NULL));
			s = gettxt("Xopen_info:2","%m/%d/%y");
			(void) setlocale(LC_MESSAGES,buf);
			if (strcmp(s,"Message not found!!\n"))
				rptr = (const char *) s;
			else 
				rptr = "%m/%d/%y";
			break;

		case D_T_FMT :
			old_locale = setlocale(LC_MESSAGES,(char*)NULL);
			(void) strcpy(buf,old_locale);
			(void)setlocale(LC_MESSAGES,setlocale(LC_TIME,(char*)NULL));
			s = gettxt("Xopen_info:3","%a %b %e %H:%M:%S %Y");
			(void) setlocale(LC_MESSAGES,buf);
			if (strcmp(s,"Message not found!!\n"))
				rptr = (const char *) s;
			else 
				rptr = "%a %b %e %H:%M:%S %Y";
			break;

		case T_FMT_AMPM:
			old_locale = setlocale(LC_MESSAGES,(char*)NULL);
			(void) strcpy(buf,old_locale);
			(void)setlocale(LC_MESSAGES,setlocale(LC_TIME,(char*)NULL));
			s = gettxt("Xopen_info:8","%I:%M:%S %p");
			(void) setlocale(LC_MESSAGES,buf);
			if (strcmp(s,"Message not found!!\n"))
				rptr = (const char *) s;
			else 
				rptr = "%I:%M:%S %p";
			break;

		case YESSTR :
			old_locale = setlocale(LC_MESSAGES,(char*)NULL);
			(void) strcpy(buf,old_locale);
			old_locale=setlocale(LC_ALL,(char*)NULL);
			if (*old_locale == '/') {
				/*
				 * composite locale
				 */
				old_locale++;
				s = buf2;
				while (*old_locale != '/')
					*s++ = *old_locale++;
				*s = '\0';
			} else
				(void) strcpy(buf2,old_locale);
			old_locale = setlocale(LC_MESSAGES,buf2);
			s = gettxt("Xopen_info:4","yes");
			(void) setlocale(LC_MESSAGES,buf);
			if (strcmp(s,"Message not found!!\n"))
				rptr = (const char *) s;
			else 
				rptr = "yes";
			break;

		case NOSTR :
			old_locale = setlocale(LC_MESSAGES,(char*)NULL);
			(void) strcpy(buf,old_locale);
			old_locale=setlocale(LC_ALL,(char*)NULL);
			if (*old_locale == '/') {
				/*
				 * composite locale
				 */
				old_locale++;
				s = buf2;
				while (*old_locale != '/')
					*s++ = *old_locale++;
				*s = '\0';
			} else
				(void) strcpy(buf2,old_locale);
			old_locale = setlocale(LC_MESSAGES,buf2);
			(void)setlocale(LC_MESSAGES,old_locale);
			s = gettxt("Xopen_info:5","no");
			(void) setlocale(LC_MESSAGES,buf);
			if (strcmp(s,"Message not found!!\n"))
				rptr = (const char *) s;
			else 
				rptr = "no";
			break;

               case CODESET :
                 {
                        char    *codeset;

                        old_locale = setlocale(LC_CTYPE, (char *) NULL);

			if ( codeset = __mbwc_locale_codeset ( old_locale ) )
			    rptr = codeset;
			else
			    rptr = "ISO8859-1";

                        break;

                 }


		default :
			rptr = "";
			break;

	    }

	UNLOCKLOCALE(l);
	return (char *) rptr;
}
