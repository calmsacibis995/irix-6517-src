/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Utility functions for interfacing with locale structures.
 */

#include <stdio.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <wchar.h>
#include <unistd.h>
#include "utils.h"
#include "regex2.h"
#include "_wchar.h"
#include "_locale.h"
#include "_locutils.h"
#include "colldata.h"




/* Static functions */

/* compare routine used in __lu_sort                      */
static int comparesit(const void *first, const void *second);

/* collation support - default C collation definition 	*/
/* libc/src/colldata.c 					*/
static xnd     *colltbl = (xnd *) __colldata;

#define COLLTBL_SIZE    256

/* static function prototypes */
static wchar_t wcconv_code (wchar_t, int);

wchar_t wcconv_code ( wchar_t wch, int codeset )
{
  if ((wch < _wcptr[codeset]->cmin) || (wch > _wcptr[codeset]->cmax))
      return 0;
  else
      return _wctbl[codeset].code[wch - _wcptr[codeset]->cmin];
}

int comparesit(const void *first, const void *second)
{
    if ( *(wchar_t *)first > *(wchar_t *)second )
	return 1;
    if ( *(wchar_t *)first < *(wchar_t *)second )
	return -1;

    return 0;
}

void __lu_sort(wchar_t *wcset, size_t sizeofset)
{
#ifdef REDEBUG
   int count;
#endif    
   
   qsort(wcset, sizeofset, sizeof(wchar_t), comparesit);
   
#ifdef REDEBUG
   for (count = 0; count < sizeofset; count+=1)
     printf("Sorted Value: %X\n",*(wcset+count));
#endif    
}



/*****************************************************************
 * General utility for determining if the given substitution string
 * exists in the current locale.
 * 
 * Return values -
 * 
 * returns 0 if successful and 1 if failure
 * 
 * returns associated value in char *value
 ******************************************************************/

int __lu_doessubexist(char *value, char *substring, size_t len)
{
        register int  i=0;
	int subst_cnt;
	int found = 0;
	struct hd *header;

	subnd *subtbl=NULL;

       /*
	* The __initcoll() is called here only when there is a collation element.
	* The intent is to optimize for regular regex process.
	*/

	/* TODO: Is this call protected by a LOCKLOCALE ??? */

        if ((subtbl =  __initcoll(&colltbl)) == (subnd *) -1L )
		return 1;

	/* Are we pointing to the static C locale table ? */
	if ( colltbl == (xnd *) __colldata )
	    /* Yes. In that case, there are no substitution
	     * This is required because the header structure is not available
	     * for the C locale */
	    return 1;

	/* Set up pointers to substitution information */

	header    = (struct hd *) ((char *) colltbl - sizeof(struct hd)); 
	subst_cnt = header->sub_cnt;
	
	if (len != 1) {	  
	  /* look up string in substitution table */
	  for (i = 0; i < subst_cnt && !found; i++) {
	     
	    if (strncmp(subtbl[i].repl, substring, len) == 0) {
		  /* get associated value into value */
		  *value = *(subtbl[i].exp);
		  found = 1;
	    }
	  }
	}

   if (found)
     return 0; /* success */
   else
     return 1; /* failure */
}


 /*****************************************************************
 * General utility for determining if the given substitution string
 * exists in the current locale.
 * 
 * Return values -
 * 
 * returns 0 if successful and 1 if failure
 * 
 * returns assiciated sub string in char *substring
 ******************************************************************/

int __lu_getstrfromsubcode(char value, char *substring, size_t *len)
{
        register int  i=0;
	int subst_cnt;
	int found = 0;
	struct hd *header;
	subnd *subtbl=NULL;


       /*
	* The __initcoll() is called here only when there is a collation element.
	* The intent is to optimize for regular regex process.
	*/

        if ((subtbl =  __initcoll(&colltbl)) == (subnd *) -1L)
                return 1;


	/* Are we pointing to the static C locale table */
	if ( colltbl == (xnd *) __colldata )
	    /* Yes. In that case, there are no substitution
	     * This is required because the header structure is not available
	     * for the C locale */
	    return 1;

	/* Set up pointers to substitution information */

	header    = (struct hd *) ((char *) colltbl - sizeof(struct hd));  
	subst_cnt = header->sub_cnt;

	  /* look up string in substitution table */
	  for (i = 0; i < subst_cnt && !found; i++) {
	     
	    if (*(subtbl[i].exp) == value) {
		  /* get associated string into substring */
		  strcpy(substring, subtbl[i].repl);
	          *len = strlen(substring);
		  found = 1;
	    }
	  }


   if (found)
     return 0; /* success */
   else
     return 1; /* failure */
}



/* === The Locale Interface Layer === 
 *
 *  - int __lu_eclass()
 *    returns number of elements of the eq class and passes a pointer to
 *  - int __lu_cclass()
 *    returns number of elements of the ch class, etc.
 *  - int __lu_hascase()
 *    predicate: returns non-zero if input class has case info
 *  - int __lu_collorder(MB min, MB max)
 *    maintains structure with collating order info in it for use determining 
 *    if code in a given range; returns index into range array
 *  - WC __lu_charmap(MB symbol)
 *    returns code that matches symbol in current charmap
 *  - WC __lu_collsymbol(MB symbol)
 *    returns code that matches collating symbol in colltbl
 *
 * ===> Consistently, caller must free buffers whose addresses are passed to
 *      these functions.
 */

/*
 * __lu_cclass - retrieve character class set from a wchrtbl-based locale
 *
 *  wcset is the address of a pointer to an array of wide characters,
 *  which is allocated here and freed by the caller
 *
 == int __lu_cclass(register WC *wcset);
 */

int __lu_cclass(cset *cs, char *class_name )
{
  register int codeset, num_elements = 0, bufsize = 0;
  register wchar_t j;
  wchar_t  start, finish;
  wctype_t wc_type = wctype(class_name);
  int	   mask;

  wchar_t wc, case_equiv;

  /* handle AT&T supplemental character set information */

  if ( wc_type == 0 )
      /* Unknown class name */
      return -1;

  for (codeset = 0; codeset < 3; codeset++)   /* for each wctype table */ 
  {

    /* efficiency question:  is it possible to preprocess type information
     * so that if this class not there skip the processing?
     */
      mask = codeset==0 ? P11 : codeset==1 ? P01 : P10 ;

      if ( _wcptr[codeset] == 0 || (start = _wcptr[codeset]->tmin) == 0)
	continue;
      
      finish = _wcptr[codeset]->tmax;
      
      bufsize += sizeof(wchar_t) * ((int) (finish - start));

      /* Allocate space for all the possible wchars.  We'll shrink the 
       * size of this buffer before leaving the routine
        */
      if ((cs->wcs = realloc(cs->wcs, bufsize)) == NULL)
	  return(-1);

      for (j = start; j <= finish; j++)

	  if ( _wctbl[codeset].type[ _wctbl[codeset].index[ j - start ] ] & wc_type )
	  {
	      wc = j | mask;

	      cs->wcs[num_elements] = wc;
	      /* if this character has a case equivalent, keep an index to it */
	      if ((case_equiv = wcconv_code(j, codeset)) != (wchar_t) 0)
	      {
		  cs->case_eq = realloc(cs->case_eq, (cs->nc+1) * sizeof(wchar_t)); 
		  cs->case_eq[cs->nc++] = case_equiv | mask ;
	      }
	      num_elements++;
	  }
  }

  /* buffer is too big, shrink it */
  if (num_elements == 0)
    {
      free(cs->wcs);
      cs->wcs = NULL;
      return(0);
    }
  else
    {
      realloc(cs->wcs, num_elements*sizeof(wchar_t));
       

      /*      if(num_elements >= LU_SORT_SET){
       *	 __lu_sort(cs->wcs, num_elements);
       *      }
       */
              
      return(num_elements);
    }
}

/*
 - __lu_eclass - get members of an equivalence-class.
   NB: Because the AT&T structure only handles 256 elements in a collation
       sequence, no supplementary characters are processed by this routine.
 == int __lu_eclass(register cset *cs, register char *name, int len);
	return 1 when sucess, 
	return 0 when failure
 */

int __lu_eclass(cset *cs, char *name, int len)
{
        register int  i=0, save_pwt;
	register char ch;
	int subst_cnt;
	int found = 0;
	struct hd *header;

	subnd *subtbl=NULL;

       /*
	* The __initcoll() is called here only when there is a collation element.
	* The intent is to optimize for regular regex process.
	*/

        if ((subtbl =  __initcoll(&colltbl)) == (subnd *) -1L)
               return 0; 

	/* Are we pointing to the static C locale table */
	if ( colltbl == (xnd *) __colldata )
	    /* Yes. In that case, there are no substitution
	     * This is required because the header structure is not available
	     * for the C locale */
	    subst_cnt = 0;
	else
	{
	    
	    /* Set up pointers to substitution information */
	
	    header    = (struct hd *) ((char *) colltbl - sizeof(struct hd));  
	    subst_cnt = header->sub_cnt;
	}

	if (len != 1) {
	  /* look up string in substitution table */
	  for (i = 0; i < subst_cnt && !found; i++)
	    if (strncmp(subtbl[i].repl, name, len) == 0)
		{
		/* get associated value into ch */
		ch = *(subtbl[i].exp);
		found = 1;
		}
	    else /* if not there, assume it's a symbol */
	      /* have to look in order_seq !!!Here */ 
	      ;

	  if (!found) 
	    return(0);

	}
	else 
	  ch = *name;

       /*
	* This is the collation solution based on the existing colltbl structure.
	* Once we get the primary weight from the character, we search
	* for all characters that have the same primary weight and and them to cset.
	* Since the current colltbl only allows 256 entries based on the strxfrm() call, 
	* the for loop is hard coded with COLLTBL_SIZE.
	* The ultimate solution is to make the localedef to generate the weight list
	* that contains the characters that has the same weight.  The for-loop should
	* be eliminated once the weight list is generated.  This enhancement will require
	* an update to all the locale.
	*/


	save_pwt = colltbl[ch].pwt;
#ifdef DEBUGI18N
printf("Found %x with wgt %x\n", ch, colltbl[ch].pwt);
#endif


        CHadd(cs, ch);

	/* Because of a subtle design flaw/feature, at most one character with
	 * primary weight of zero should be saved in the character set.
	 * Otherwise, find and add all characters with same primary weight.
	 */

	if (save_pwt != 0) {
	  for (i=0; i < COLLTBL_SIZE; i++) {
	    if (colltbl[i].pwt == save_pwt && (char) i != ch) {
	      int j;
#ifdef DEBUGI18N
printf("Found %x with wgt %x\n", i, colltbl[i].pwt);
#endif

               /* if this char is substituted */
               for (j = 0, found = 0; j < subst_cnt && !found; j++)
		 if ((*(subtbl[j].exp) == (char) i) && 
		     (subtbl[j].explen == 1))
		   {
		   /* add subst string to multi-character element store */
                   /* MCadd(cs, subtbl[j].repl); */
		   size_t oldend = cs->smultis,
		          len    = strlen(subtbl[j].repl);
		   cs->smultis += len + 1;
		   if (cs->multis == NULL)
		     cs->multis = malloc(cs->smultis+1);
		   else
		     cs->multis = realloc(cs->multis, cs->smultis+1);
		   if (cs->multis == NULL) {
		       return(0);
		   }

		   memcpy(cs->multis + oldend, subtbl[j].repl, len);
		   cs->multis[cs->smultis - 1] = '\0';
		   cs->multis[cs->smultis]     = '\0';
		   cs->nmultis++;

		   found = 1;
		   } 

	       if (!found) 
       		 CHadd(cs, (char) i); 
 	    }
	  } 
	} 

	return(1);
} 


/* 
 * - __lu_hascase - predicate: returns non-zero if input class has case info
 * == int __lu_hascase(char *name)
 */

/* NOT YET USED
int __lu_hascase(char *class_name)
{
  return(1);
}
*/

/*  - int __lu_collorder(cset *, wchar_t *, wchar_t *)
 *    determines collating order info for use in determining if characters
 *    in a given range; stores characters in range in cset
 *    NOTE: the input wide character codes are bogus for AT&T collation table
 *    but the interface must reflect possibility of going to multibyte.
 *	return 1 when sucess; return 0 when failure
 */

int __lu_collorder(cset *cs, wchar_t wcmin, wchar_t wcmax)
{
  register int i, min_pwt, max_pwt;
  char min[2], max[2], save_ch;

  /* This test ensures that we have single byte inputs only */
  if ((wctomb(min, wcmin) == 1) && (wctomb(max, wcmax) == 1))
    {
    min[1] = max[1] = '\0';
    }
  else
    return(0); /* bad input */
  
  /* initialize collating structure if not already */
 /* subtbl is not being used here 	          */		

   __initcoll(&colltbl);

  /* verify that min, max in proper relation in this locale */
  min_pwt = colltbl[*min].pwt;
  max_pwt = colltbl[*max].pwt;

  if (min_pwt > max_pwt)
      return(0);

  /* search table for all primary weights between min and max */
  for (i = 0, save_ch = *min; i < COLLTBL_SIZE; i++)
    {
    if ((min_pwt < colltbl[i].pwt) && (colltbl[i].pwt < max_pwt))
       {
       if ((save_ch+1) == i)
	 save_ch = i;
       else
	 save_ch = 0;
       /* No big thing to go ahead and save it here */
       CHadd(cs, i);
       }
    }
  
  /* don't forget to add the endpoints */
  CHadd(cs, *min);
  CHadd(cs, *max);

  /* fast search option enabled if range characters are consecutive encodings */
  if (save_ch != 0)
    {
      cs->range = 1;
      cs->range_min = (wchar_t) *min;
      cs->range_max = (wchar_t) *max;
    }
    
  return(1);
}

/*  - WC __lu_charmap(MB symbol)
 *    returns code that matches symbol in current charmap
 */

/* NOT YET USED
wchar_t __lu_charmap(char *symbol)
{
  return(WEOF);
}
*/

/*  - WC __lu_collsymbol(MB symbol)
 *    returns code that matches collating symbol in colltbl
 */

int __lu_collsymbol(char *value, char *symbol, int length)
{
  return(__lu_doessubexist(value, symbol, length));
}
