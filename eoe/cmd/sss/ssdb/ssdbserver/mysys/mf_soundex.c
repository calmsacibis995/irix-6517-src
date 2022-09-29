/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/****************************************************************
*	SOUNDEX ALGORITHM in C					*
*								*
*	The basic Algorithm source is taken from EDN Nov.	*
*	14, 1985 pg. 36.					*
*								*
*	As a test Those in Illinois will find that the		*
*	first group of numbers in their drivers license		*
*	number is the soundex number for their last name.	*
*								*
*	RHW  PC-IBBS ID. #1230					*
*								*
*	As an extension if remove_garbage is set then all non-	*
*	alpha characters are skipped				*
****************************************************************/

#include "mysys_priv.h"
#include <m_ctype.h>
#include "my_static.h"

static char get_scode(char **ptr,pbool remove_garbage);

		/* outputed string is 4 byte long */
		/* out_pntr can be == in_pntr */

void soundex(register my_string out_pntr, my_string in_pntr,
	     pbool remove_garbage)
{
  char ch,last_ch;
  reg3 my_string end;

  if (remove_garbage)
  {
    while (*in_pntr && isspace(*in_pntr))	/* Skipp pre-space */
      in_pntr++;
  }
  *out_pntr++ = toupper(*in_pntr);	/* Copy first letter		 */
  last_ch = get_scode(&in_pntr,0);	/* code of the first letter	 */
					/* for the first 'double-letter  */
					/* check.			 */
  end=out_pntr+3;			/* Loop on input letters until	 */
					/* end of input (null) or output */
					/* letter code count = 3	 */

  in_pntr++;
  while (out_pntr < end && (ch = get_scode(&in_pntr,remove_garbage)) != 0)
  {
    in_pntr++;
    if ((ch != '0') && (ch != last_ch)) /* if not skipped or double */
    {
      *out_pntr++ = ch;			/* letter, copy to output */
    }					/* for next double-letter check */
    last_ch = ch;			/* save code of last input letter */
  }
  while (out_pntr < end)
    *out_pntr++ = '0';
  *out_pntr=0;				/* end string */
  return;
} /* soundex */


  /*
    If alpha, map input letter to soundex code.
    If not alpha and remove_garbage is set then skipp to next char
    else return 0
    */

static char get_scode(char **ptr, pbool remove_garbage)
{
  uchar ch;

  if (remove_garbage)
  {
    while (**ptr && !isalpha(**ptr))
      (*ptr)++;
  }
  ch=toupper(**ptr);
  if (ch < 'A' || ch > 'Z')
  {
    if (isalpha(ch))			/* If exetended alfa (country spec) */
      return '0';			/* threat as vokal */
    return 0;				/* Can't map */
  }
  return(soundex_map[ch-'A']);
} /* get_scode */
