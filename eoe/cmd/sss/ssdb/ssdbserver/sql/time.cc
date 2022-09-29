/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.
   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* Functions to handle date and time */

#include "mysql_priv.h"
#include <m_ctype.h>

static ulong const days_at_timestart=719528;	/* daynr at 1970.01.01 */
uchar *days_in_month= (uchar*) "\037\034\037\036\037\036\037\037\036\037\036\037";


	/* Init some variabels neaded when using my_local_time */
	/* Currently only my_time_zone is inited */

static long my_time_zone=0;
void init_time(void)
{
  time_t seconds;
  struct tm *l_time,tm_tmp;;
  TIME my_time;

  seconds= (time_t) time((time_t*) 0);
  localtime_r(&seconds,&tm_tmp);
  l_time= &tm_tmp;
  my_time_zone=0;
  my_time.year=		(uint) l_time->tm_year;
  my_time.month=	(uint) l_time->tm_mon+1;
  my_time.day=		(uint) l_time->tm_mday;
  my_time.hour=		(uint) l_time->tm_hour;
  my_time.minute=	(uint) l_time->tm_min;
  my_time.second=		(uint) l_time->tm_sec;
  VOID(my_gmt_sec(&my_time));		/* Init my_time_zone */
}

	/* Convert current time to sec. since 1970.01.01 */

long my_gmt_sec(TIME *t)
{
  time_t tmp;
  struct tm *l_time,tm_tmp;
  long diff;

  if (t->hour >= 24)
  {					/* Fix for time-loop */
    t->day+=t->hour/24;
    t->hour%=24;
  }
  tmp=(time_t) ((calc_daynr((uint) t->year,(uint) t->month,(uint) t->day) -
		 (long) days_at_timestart)*86400L + (long) t->hour*3600L +
		(long) (t->minute*60 + t->second)) + (time_t) my_time_zone;
  localtime_r(&tmp,&tm_tmp);
  l_time=&tm_tmp;
  for (uint loop=0; loop < 3 && t->hour != (uint) l_time->tm_hour ; loop++)
  {					/* One check should be enough ? */
    diff=3600L*(long) ((((int) (t->hour - l_time->tm_hour)+36) % 24)-12);
    my_time_zone+=diff;
    tmp+=(time_t) diff;
    localtime_r(&tmp,&tm_tmp);
    l_time=&tm_tmp;
  }
  if ((my_time_zone >=0 ? my_time_zone: -my_time_zone) > 3600L*12)
    my_time_zone=0;			/* Wrong date */
  return tmp;
} /* my_gmt_sec */


	/* Some functions to calculate dates */

	/* Calculate nr of day since year 0 in new date-system (from 1615) */

long calc_daynr(uint year,uint month,uint day)
{
  long delsum;
  int temp;
  DBUG_ENTER("calc_daynr");

  if (year == 0 && month == 0 && day == 0)
    DBUG_RETURN(0);				/* Skipp errors */
  if (year < 200)
  {
    if ((year=year+1900) < 1970)
      year+=100;
  }
  delsum= (long) (365L * year+ 31*(month-1) +day);
  if (month <= 2)
      year--;
  else
    delsum-= (long) (month*4+23)/10;
  temp=(int) ((year/100+1)*3)/4;
  DBUG_PRINT("exit",("year: %d  month: %d  day: %d -> daynr: %ld",
		     year+(month <= 2),month,day,delsum+year/4-temp));
  DBUG_RETURN(delsum+(int) year/4-temp);
} /* calc_daynr */


	/* Calc weekday from daynr */
	/* Returns 0 for monday, 1 for tuesday .... */

int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  DBUG_ENTER("calc_weekday");
  DBUG_RETURN ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
}

	/* Calc days in one year. works with 0 <= year <= 99 */

uint calc_days_in_year(uint year)
{
  return (year & 3) == 0 && (year%100 || (year%400 == 0 && year)) ?
    366 : 365;
}

	/* Change a daynr to year, month and day */
	/* Daynr 0 is returned as date 00.00.00 */

void get_date_from_daynr(long daynr,uint *ret_year,uint *ret_month,
			 uint *ret_day)
{
  uint year,temp,leap_day,day_of_year,days_in_year;
  uchar *month_pos;
  DBUG_ENTER("get_date_from_daynr");

  if (daynr <= 365L || daynr >= 3652500)
  {						/* Fix if wrong daynr */
    *ret_year= *ret_month = *ret_day =0;
  }
  else
  {
    year= (uint) (daynr*100 / 36525L);
    temp=(((year-1)/100+1)*3)/4;
    day_of_year=(uint) (daynr - (long) year * 365L) - (year-1)/4 +temp;
    while (day_of_year > (days_in_year= calc_days_in_year(year)))
    {
      day_of_year-=days_in_year;
      (year)++;
    }
    leap_day=0;
    if (days_in_year == 366)
    {
      if (day_of_year > 31+28)
      {
	day_of_year--;
	if (day_of_year == 31+28)
	  leap_day=1;		/* Handle leapyears leapday */
      }
    }
    *ret_month=1;
    for (month_pos= days_in_month ;
	 day_of_year > (uint) *month_pos ;
	 day_of_year-= *(month_pos++), (*ret_month)++)
      ;
    *ret_year=year;
    *ret_day=day_of_year+leap_day;
  }
  DBUG_VOID_RETURN;
}

/*	find date from string and put it in vektor
	Input: pos = "YYMMDD" OR "YYYYMMDD" in any order or
	"xxxxx YYxxxMMxxxDD xxxx" where xxx is anything exept
	a number. Month or day mustn't exeed 2 digits, year may be 4 digits.
*/


#ifdef NOT_NEEDED

void find_date(string pos,uint *vek,uint flag)
{
  uint length,value;
  string start;
  DBUG_ENTER("find_date");
  DBUG_PRINT("enter",("pos: '%s'  flag: %d",pos,flag));

  bzero((char*) vek,sizeof(int)*4);
  while (*pos && !isdigit(*pos))
    pos++;
  length=strlen(pos);
  for (uint i=0 ; i< 3; i++)
  {
    start=pos; value=0;
    while (isdigit(pos[0]) &&
	   ((pos-start) < 2 || ((pos-start) < 4 && length >= 8 &&
				!(flag & 3))))
    {
      value=value*10 + (uint) (uchar) (*pos - '0');
      pos++;
    }
    vek[flag & 3]=value; flag>>=2;
    while (*pos && (ispunct(*pos) || isspace(*pos)))
      pos++;
  }
  DBUG_PRINT("exit",("year: %d  month: %d  day: %d",vek[0],vek[1],vek[2]));
  DBUG_VOID_RETURN;
} /* find_date */


	/* Outputs YYMMDD if input year < 100 or YYYYMMDD else */

static long calc_daynr_from_week(uint year,uint week,uint day)
{
  long daynr;
  int weekday;

  daynr=calc_daynr(year,1,1);
  if ((weekday= calc_weekday(daynr,0)) >= 3)
    daynr+= (7-weekday);
  else
    daynr-=weekday;

  return (daynr+week*7+day-8);
}

void convert_week_to_date(string date,uint flag,uint *res_length)
{
  string format;
  uint year,vek[4];

  find_date(date,vek,(uint) (1*4+2*16));		/* YY-WW-DD */
  year=vek[0];

  get_date_from_daynr(calc_daynr_from_week(vek[0],vek[1],vek[2]),
		      &vek[0],&vek[1],&vek[2]);
  *res_length=8;
  format="%04d%02d%02d";
  if (year < 100)
  {
    vek[0]= vek[0]%100;
    *res_length=6;
    format="%02d%02d%02d";
  }
  sprintf(date,format,vek[flag & 3],vek[(flag >> 2) & 3],
	  vek[(flag >> 4) & 3]);
  return;
}

	/* returns YYWWDD or YYYYWWDD according to input year */
	/* flag only reflects format of input date */

void convert_date_to_week(string date,uint flag,uint *res_length)
{
  uint vek[4],weekday,days,year,week,day;
  long daynr,first_daynr;
  char buff[256],*format;

  if (! date[0])
  {
    get_date(buff,0,0L);			/* Use current date */
    find_date(buff+2,vek,(uint) (1*4+2*16));	/* YY-MM-DD */
  }
  else
    find_date(date,vek,flag);

  year= vek[0];
  daynr=      calc_daynr(year,vek[1],vek[2]);
  first_daynr=calc_daynr(year,1,1);

	/* Caculate year and first daynr of year */
  if (vek[1] == 1 && (weekday=calc_weekday(first_daynr,0)) >= 3 &&
      vek[2] <= 7-weekday)
  {
    if (!year--)
      year=99;
    first_daynr=first_daynr-calc_days_in_year(year);
  }
  else if (vek[1] == 12 &&
	   (weekday=calc_weekday(first_daynr+calc_days_in_year(year)),0) < 3 &&
	   vek[2] > 31-weekday)
  {
    first_daynr=first_daynr+calc_days_in_year(year);
    if (year++ == 99)
      year=0;
  }

	/* Calulate daynr of first day of week 1 */
  if ((weekday= calc_weekday(first_daynr,0)) >= 3)
    first_daynr+= (7-weekday);
  else
    first_daynr-=weekday;

  days=(int) (daynr-first_daynr);
  week=days/7+1 ; day=calc_weekday(daynr,0)+1;

  *res_length=8;
  format="%04d%02d%02d";
  if (year < 100)
  {
    *res_length=6;
    format="%02d%02d%02d";
  }
  sprintf(date,format,year,week,day);
  return;
}

#endif

	/* Functions to handle periods */

ulong convert_period_to_month(ulong period)
{
  ulong a,b;
  if (period == 0)
    return 0L;
  if ((a=period/100) < 70)
    a+=2000;
  else if (a < 100)
    a+=1900;
  b=period%100;
  return a*12+b-1;
}

ulong convert_month_to_period(ulong month)
{
  ulong year;
  if (month == 0L)
    return 0L;
  if ((year=month/12) < 100)
  {
    year+=(year < 70) ? 2000 : 1900;
  }
  return year*100+month%12+1;
}

#ifdef NOT_NEEDED

ulong add_to_period(ulong period,int months)
{
  if (period == 0L)
    return 0L;
  return convert_month_to_period(convert_period_to_month(period)+months);
}
#endif


/*****************************************************************************
** convert a timestamp string to a TIME value.
** At least the following formats are recogniced (based on number of digits)
** YYMMDD, YYYYMMDD, YYMMDDHHMMSS, YYYYMMDDHHMMSS
** YY-MM-DD, YYYY-MM-DD, YY-MM-DD HH.MM.SS
** Returns the type of string
*****************************************************************************/

timestamp_type
str_to_TIME(const char *str, uint length, TIME *l_time)
{
  uint field_length,year_length,digits,i,number_of_fields,date[6];
  bool date_used=0;
  const char *pos;
  const char *end=str+length;
  DBUG_ENTER("str_to_timestamp");
  DBUG_PRINT("enter",("str: %.*s",length,str));

  for (; !isdigit(*str) && str != end ; str++) ; // Skipp garbage
  if (str == end)
    DBUG_RETURN(TIMESTAMP_NONE);
  /*
  ** calculate first number of digits.
  ** If length= 4, 8 or >= 14 then year is of format YYYY.
     (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos=str; pos != end && isdigit(*pos) ; pos++) ;
  digits= (uint) (pos-str);
  year_length= (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
  field_length=year_length-1;
  for (i=0 ; i < 6 && str != end && isdigit(*str) ; i++)
  {
    uint tmp_value=(uint) (uchar) (*str++ - '0');
    while (str != end && isdigit(str[0]) && field_length--)
    {
      tmp_value=tmp_value*10 + (uint) (uchar) (*str - '0');
      str++;
    }
    if ((date[i]=tmp_value))
      date_used=1;				// Found something
    while (str != end && (ispunct(*str) || isspace(*str)))
      str++;
    field_length=1;				// Rest fields can only be 2
  }
  if (year_length == 2)
    date[0]+= (date[0] < 70 ? 2000 : 1900);
  number_of_fields=i;
  while (i < 6)
    date[i++]=0;
  if (number_of_fields < 3 || !date_used || date[1] == 0 || date[1] > 12 ||
      date[2] == 0 || date[2] > 31 || date[3] > 23 || date[4] > 59 ||
      date[5] > 59)
  {
    current_thd->cuted_fields++;
    DBUG_RETURN(TIMESTAMP_NONE);
  }
  if (str != end && current_thd->count_cuted_fields)
  {
    for ( ; str != end ; str++)
    {
      if (!isspace(*str))
      {
	current_thd->cuted_fields++;
	break;
      }
    }
  }
  l_time->year=  date[0];
  l_time->month= date[1];
  l_time->day=	 date[2];
  l_time->hour=  date[3];
  l_time->minute=date[4];
  l_time->second=date[5];
  DBUG_RETURN(number_of_fields <= 3 ? TIMESTAMP_DATE : TIMESTAMP_FULL);
}


time_t str_to_timestamp(const char *str,uint length)
{
  TIME l_time;
  if (str_to_TIME(str,length,&l_time) == TIMESTAMP_NONE)
    return(0);
  if (l_time.year >= 2038 || l_time.year < 1970)
  {
    current_thd->cuted_fields++;
    return(0);
  }
  return(my_gmt_sec(&l_time));
}


longlong str_to_datetime(const char *str,uint length)
{
  TIME l_time;
  if (str_to_TIME(str,length,&l_time) == TIMESTAMP_NONE)
    return(0);
  return (longlong) (l_time.year*LL(10000000000) +
		     l_time.month*LL(100000000)+
		     l_time.day*LL(1000000)+
		     l_time.hour*LL(10000)+
		     (longlong) (l_time.minute*100+l_time.second));
}


/*****************************************************************************
** convert a time string to a (ulong) value.
** Can use all full timestamp formats and
** [-] DAYS [H]H:MM:SS, [H]H:MM:SS, [M]M:SS, [H]HMMSS, [M]MSS or [S]S
*****************************************************************************/

bool str_to_time(const char *str,uint length,TIME *l_time)
{
  long date[4];
  const char *end=str+length;
  l_time->neg=0;
  for (; !isdigit(*str) && *str != '-' && str != end ; str++) ;
  if (str != end && *str == '-')
  {
    l_time->neg=1;
    str++;
  }
  if (str == end)
    return 1;

  /* Check first if this is a full TIMESTAMP */
  if (length >= 12)
  {						// Probably full timestamp
    if (str_to_TIME(str,length,l_time) == TIMESTAMP_FULL)
      return 0;					// Was an ok timestamp
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */

  if (get_interval_info(str,(uint) (end-str), 4, date))
  {
    current_thd->cuted_fields++;
    return 1;
  }
  if (date[3] > 100 && date[2] == 0 && date[1] == 0 && date[0] == 0)
  {
    /* String given as one number; assume HHMMSS format */
    date[2]=date[3]/100 % 100;
    date[1]=date[3]/10000;
    date[3]%= 100;
  }
  if (date[2] >= 60 || date[3] >= 60)
  {
    current_thd->cuted_fields++;
    return 1;
  }
  l_time->day=date[0];
  l_time->hour=date[1];
  l_time->minute=date[2];
  l_time->second=date[3];
  return 0;
}
