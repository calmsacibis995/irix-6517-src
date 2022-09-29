/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  Functions to convert to lover_case and to upper_case in scandinavia.

  case_sort converts a character string to a representaion that can
  be compared by strcmp to find with is alfabetical bigger.
  (lower- and uppercase letters is compared as the same)
*/

#include "mysys_priv.h"
#include <m_ctype.h>

	/* string to uppercase */

void caseup_str(my_string str)
{
  while ((*str = toupper(*str)) != 0)
    str++;
} /* caseup_str */

	/* string to lowercase */

void casedn_str(my_string str)
{
  while ((*str= tolower(*str)) != 0)
    str++;
} /* casedn_str */


	/* to uppercase */

void caseup(my_string str, uint length)
{
  for ( ; length>0 ; length--, str++)
    *str= toupper(*str);
} /* caseup */

	/* to lowercase */

void casedn(my_string str, uint length)
{
  for ( ; length>0 ; length--, str++)
    *str= tolower(*str);
} /* casedn */

	/* to sort-string that can be compared to get text in order */

void case_sort(my_string str, uint length)
{
  for ( ; length>0 ; length--, str++)
    *str= (char) my_sort_order[(uchar) *str];
} /* case_sort */

	/* find string in another with no case_sensivity */

my_string strcasestr(const char *str, const char *search)
{
 uchar *i,*j,*pos;

 pos=(uchar*) str;
skipp:
  while (*pos != '\0') {
    if (toupper((uchar) *pos++) == toupper((uchar) *search)) {
      i=(uchar*) pos; j=(uchar*) search+1;
      while (*j)
	if (toupper(*i++) != toupper(*j++)) goto skipp;
      return ((char*) pos-1);
    }
  }
  return ((my_string) 0);
} /* strcstr */


	/* compare strings without regarding to case */

int my_strcasecmp(const char *s, const char *t)
{
  while (toupper((uchar) *s) == toupper((uchar) *t++))
    if (!*s++) return 0;
  return ((int) toupper((uchar) s[0]) - (int) toupper((uchar) t[-1]));
}


int my_casecmp(const char *s, const char *t, uint len)
{
  while (len-- != 0 && toupper(*s++) == toupper(*t++)) ;
  return (int) len+1;
}


int my_strsortcmp(const char *s, const char *t)
{
#ifdef USE_STRCOLL
	return my_strcoll((unsigned char *)s, (unsigned char *)t);
#else
  while (my_sort_order[(uchar) *s] == my_sort_order[(uchar) *t++])
    if (!*s++) return 0;
  return ((int) my_sort_order[(uchar) s[0]] - (int) my_sort_order[(uchar) t[-1]]);
#endif
}

int my_sortcmp(const char *s, const char *t, uint len)
{
#ifndef USE_STRCOLL
  while (len--)
  {
    if (my_sort_order[(uchar) *s++] != my_sort_order[(uchar) *t++])
      return ((int) my_sort_order[(uchar) s[-1]] -
	      (int) my_sort_order[(uchar) t[-1]]);
  }
  return 0;
#else
	return my_strnncoll((unsigned char *)s, len, (unsigned char
	*)t, len);
#endif
}
