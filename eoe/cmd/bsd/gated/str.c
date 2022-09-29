/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 */

#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/str.c,v 1.1 1989/09/18 19:02:24 jleong Exp $";
#endif	not lint

#include "include.h"

/* 
 * Thanks to Doug Elias, Cornell Theory Center for these string
 * routines.
 */
/******************************************************************
*name:        cap  <cut_and_paste>
*
*function:    take off next white-space-separated "word" from 
*             "string", resetting "string" to 1 character past 
*             "word" in original "string"
*
*arguments:   (char *)string
*
*algorithm:   skip over any leading "white-space";
*             if (string is blank)
*               return (NULL);
*             else
*               send string to substr() to take off next "word" 
*                <contiguous string of non-white-space characters>;
*               set string to 1 character past "word";
*               return("word");
*
*calls:       (char *)next_white()
*               returns ptr to next "white-space" character
*             (char *)substr()
*               returns a substring of characters according to 
*                 arguments given on starting and ending positions 
*                 in parent string
*
*returns:     (char *)NULL if at end of string
*             (char *)word if not
*
*bugs:
*
*to-do:
*
*history:
*******************************************************************/
#include <stdio.h>

#define EOL ('\0')


char *cap(string)
  char *string;
{
  static char word[80];
  char *substr();
  char *next_white();
  char *str = string;
  char *w_ptr;
  int start;
  int stop;

#ifdef  DEBUG
  printf("cap: rcvd \"%s\"\n", string);
#endif

  while (*str != EOL && (*str == ' ' || *str == '\t'))
    ++str;

  if (*str == EOL || *str == '\n')
    return(NULL);

  word[0] = EOL;
  start = str - string;
  stop = ((w_ptr = next_white(str)) == NULL) ? -1 :
						  w_ptr - string;
  if((str = substr(string, start, stop)) != NULL)
    (void) strcpy(word, str);
  if (stop == -1)
    string[0] = EOL;
  else
    (void) strcpy(string, string + stop + 1);

#ifdef DEBUG
  printf("cap: rtn \"%s\", string == \"%s\"\n", word, string);
#endif

  return(word);
}

/*******************************************************************
*name:        next_white
*
*function:    return ptr to next "white-space" character
*
*arguments:   string
*
*algorithm:   set ptr to beginning of string;
*             while (character under ptr not white-space or EOL)
*               skip to next character;
*             if (EOL)
*               return(NULL);
*             else
*               return(ptr);
*
*calls:       none
*
*returns:     (char *)NULL if no white space before EOL
*             (char *)ptr if white space exists
*
*bugs:
*
*to-do:
*
*history:
*******************************************************************/

char *next_white(string)
  char *string;
{
  char *ptr = string;

#ifdef DEBUG
  printf("next_white: rcvd \"%s\"\n", string);
#endif

  while (*ptr != ' ' && 
	 *ptr != '\t' &&
	 *ptr != '\n' && 
	 *ptr != EOL)
    ++ptr;

#ifdef DEBUG
  printf("next_white: rtn %d\n", (*ptr == '\0') ? 0 :
						  ptr - string);
#endif

  return((*ptr == EOL) ? NULL : ptr);
}

/******************************************************************
*name:        substr
*
*function:    return a string from the middle of another string
*
*arguments:   (char *) original string
*             (int) starting position
*             (int) ending position
*
*algorithm:   if (stop < 0)
*               set stop to length of string;
*             if (start >= stop)
*               return(NULL);
*             set substr to (string + start) thru (string + stop);
*             return(substr);
*
*calls:       none
*
*returns:     (char *)NULL if start >= stop
*             (char *)substr otherwise
*
*bugs:
*
*to-do:
*
*history:
******************************************************************/

char *substr(string, start, stop)
  char *string;
  int start;
  int stop;
{
  static char asubstr[80];

#ifdef DEBUG
  printf("asubstr: rcvd \"%s\"(string) %d(start) %d(stop)\n",
   string, start, stop);
#endif

  if (stop < 0)
    stop = strlen(string);

  if (start >= stop)
    return(NULL);

  asubstr[0] = EOL;
  (void) strncpy(asubstr, string + start, stop - start);
  asubstr[stop - start] = EOL;

#ifdef DEBUG
  printf("asubstr: rtn \"%s\"\n", asubstr);
#endif

  return(asubstr);
}


/*
 *	Thanks for Stuart Levy for the following token routing
 */
/*
 * Extract a white-space delimited token from a buffer, return its length.
 * \n, \r, \0 or # terminate the buffer.
 * *lpp is advanced to point at the first character following the token.
 * The token is copied into buf, guaranteed \0 terminated.
 */

gettoken(lpp, buf, buflen)
  char **lpp;
  char *buf;
  int buflen;	/* sizeof(buf) */
{
  register char *lp, *tailp;
  register int len;

  for(lp = *lpp; *lp == ' ' || *lp == '\t'; lp++)
    ;
  tailp = lp;
  for(;;) {
    switch(*tailp) {
    case ' ':
    case '\t':
    case '#':
    case '\0':
    case '\n':
    case '\r':
      len = tailp-lp;
      if(len >= buflen)
	len = buflen-1;
      bcopy(lp, buf, len);
      buf[len] = '\0';
      *lpp = tailp;
      return(tailp-lp);

    default:
	tailp++;
    }
  }
}


/*
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static char charmap[] = {
	'\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
	'\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
	'\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
	'\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
	'\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
	'\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
	'\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
	'\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
	'\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
	'\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

#ifndef sgi
strcasecmp(s1, s2)
	register char *s1, *s2;
{
	register char *cm = charmap;

	while (cm[*s1] == cm[*s2++])
		if (*s1++ == '\0')
			return(0);
	return(cm[*s1] - cm[*--s2]);
}

strncasecmp(s1, s2, n)
	register char *s1, *s2;
	register int n;
{
	register char *cm = charmap;

	while (--n >= 0 && cm[*s1] == cm[*s2++])
		if (*s1++ == '\0')
			return(0);
	return(n < 0 ? 0 : cm[*s1] - cm[*--s2]);
}
#endif sgi
