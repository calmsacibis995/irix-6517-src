/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */
/* This file is originally from the mysql distribution. Coded by monty */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#ifdef HAVE_FCONVERT
#include <floatingpoint.h>
#endif

extern gptr sql_alloc(unsigned size);
extern void sql_element_free(void *ptr);

#include "sql_string.h"

/*****************************************************************************
** String functions
*****************************************************************************/

bool String::real_alloc(uint32 length)
{
  length=ALIGN_SIZE(length+1);
  if (Alloced_length < length)
  {
    free();
    if (!(Ptr=my_malloc(length,MYF(MY_WME))))
    {
      str_length=0;
      return TRUE;
    }
    Alloced_length=length;
    alloced=1;
  }
  Ptr[0]=0;
  str_length=0;
  return FALSE;
}


/*
** Check that string is big enough. Set string[alloc_length] to 0
** (for C functions)
*/

bool String::realloc(uint32 alloc_length)
{
  uint32 length=ALIGN_SIZE(alloc_length+1);
  if (Alloced_length < length)
  {
    char *new_ptr;
    if (alloced)
    {
      if ((new_ptr= (char*) my_realloc(Ptr,length,MYF(MY_WME))))
      {
	Ptr=new_ptr;
	Alloced_length=length;
      }
      else
	return TRUE;				// Signal error
    }
    else if ((new_ptr= (char*) my_malloc(length,MYF(MY_WME))))
    {
      memcpy(new_ptr,Ptr,str_length);
      new_ptr[str_length]=0;
      Ptr=new_ptr;
      Alloced_length=length;
      alloced=1;
    }
    else
      return TRUE;			// Signal error
  }
  Ptr[alloc_length]=0;			// This make other funcs shorter
  return FALSE;
}


#ifdef NOT_NEEDED
void String::set(long num)
{
  if (!alloc(14))
    str_length=(uint32) (int2str(num,Ptr,-10)-Ptr);
}
#endif

void String::set(longlong num)
{
  if (!alloc(21))
    str_length=(uint32) (longlong2str(num,Ptr,-10)-Ptr);
}

void String::set(double num,uint decimals)
{
#ifdef HAVE_FCONVERT
  int decpt,sign;
  char *pos,*to,buff[331];

  VOID(fconvert(num,(int) decimals,&decpt,&sign,buff+1));
  if (!isdigit(buff[1]))
  {						// Nan or Inf
    pos=buff+1;
    if (sign)
    {
      buff[0]='-';
      pos=buff;
    }
    copy(pos,strlen(pos));
    return;
  }
  if (alloc((uint32) ((uint32) decpt+3+decimals)))
    return;
  to=Ptr;
  if (sign)
    *to++='-';

  pos=buff+1;
  if (decpt < 0)
  {					/* value is < 0 */
    *to++='0';
    if (!decimals)
      goto end;
    *to++='.';
    if ((uint32) -decpt > decimals)
      decpt= - (int) decimals;
    decimals=(uint32) ((int) decimals+decpt);
    while (decpt++ < 0)
      *to++='0';
  }
  else if (decpt == 0)
  {
    *to++= '0';
    if (!decimals)
      goto end;
    *to++='.';
  }
  else
  {
    while (decpt-- > 0)
      *to++= *pos++;
    if (!decimals)
      goto end;
    *to++='.';
  }
  while (decimals--)
    *to++= *pos++;

end:
  *to=0;
  str_length=(uint32) (to-Ptr);
#else
  char buff[330];
  sprintf(buff,"%.*lf",decimals,num);
  copy(buff,strlen(buff));
#endif
}


void String::copy()
{
  if (!alloced)
  {
    Alloced_length=0;				// Force realloc
    (void) realloc(str_length);
  }
}

void String::copy(const String &str)
{
  if (!alloc(str.str_length))
  {
    str_length=str.str_length;
    bmove(Ptr,str.Ptr,str_length);		// May be overlapping
    Ptr[str_length]=0;
  }
}

void String::copy(const char *str,uint32 length)
{
  if (!alloc(length))
  {
    str_length=length;
    memcpy(Ptr,str,length);
    Ptr[length]=0;
  }
}

/* This is used by mysql.cc */

void String::fill(uint32 max_length,char fill)
{
  if (str_length > max_length)
    Ptr[str_length=max_length]=0;
  else
  {
    if (!realloc(max_length))
    {
      bfill(Ptr+str_length,max_length-str_length,fill);
      str_length=max_length;
    }
  }
}

void String::strip_sp()
{
   while (str_length && isspace(Ptr[str_length-1]))
    str_length--;
}

void String::append(const String &s)
{
  if (!realloc(str_length+s.length()))
  {
    memcpy(Ptr+str_length,s.ptr(),s.length());
    str_length+=s.length();
  }
}

void String::append(const char *s,uint32 length)
{
  if (!length)
    length=strlen(s);
  if (!realloc(str_length+length))
  {
    memcpy(Ptr+str_length,s,length);
    str_length+=length;
  }
}


int String::strstr(const String &s,uint32 offset)
{
  if (s.length()+offset <= str_length)
  {
    if (!s.length())
      return offset;				// Empty string is always found

    register const char *str = Ptr+offset;
    register const char *search=s.ptr();
    const char *end=Ptr+str_length-s.length()+1;
    const char *search_end=s.ptr()+s.length();
skipp:
    while (str != end)
    {
      if (*str++ == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search+1;
	while (j != search_end)
	  if (*i++ != *j++) goto skipp;
	return (int) (str-Ptr) -1;
      }
    }
  }
  return -1;
}


/*
** Search string from end. Offset is offset to the end of string
*/

int String::strrstr(const String &s,uint32 offset)
{
  if (s.length() <= offset && offset <= str_length)
  {
    if (!s.length())
      return offset;				// Empty string is always found
    register const char *str = Ptr+offset-1;
    register const char *search=s.ptr()+s.length()-1;

    const char *end=Ptr+s.length()-1;
    const char *search_end=s.ptr()-1;
skipp:
    while (str != end)
    {
      if (*str-- == *search)
      {
	register char *i,*j;
	i=(char*) str; j=(char*) search-1;
	while (j != search_end)
	  if (*i-- != *j--) goto skipp;
	return (int) (i-Ptr) +1;
      }
    }
  }
  return -1;
}

/*
** replace substring with string
** If wrong parameter or not enough memory, do nothing
*/


void String::replace(uint32 offset,uint32 length,const String &to)
{
  long diff = (long) (to.length()-length);
  if (offset+length <= str_length &&
      (diff <= 0 || !realloc(str_length+(uint32) diff)))
  {
    if (diff < 0)
    {
      memcpy(Ptr+offset,to.ptr(),to.length());
      bmove(Ptr+offset+to.length(),Ptr+offset+length,
	    str_length-offset-length);
    }
    else
    {
      if (diff)
	bmove_upp(Ptr+str_length+diff,Ptr+str_length,
		  str_length-offset-length);
      memcpy(Ptr+offset,to.ptr(),to.length());
    }
    str_length+=(uint32) diff;
  }
}


int sortcmp(const String *x,const String *y)
{
  const char *s= x->ptr();
  const char *t= y->ptr();
  uint32 x_len=x->length(),y_len=y->length(),len=min(x_len,y_len);

#ifdef USE_STRCOLL
#ifndef CMP_ENDSPACE
  {
    while (x_len && isspace(s[x_len-1]))
      x_len--;
    while (y_len && isspace(t[y_len-1]))
      y_len--;
  }
#endif
  return my_strnncoll((unsigned char *)s,x_len,(unsigned char *)t,y_len);
#else
  x_len-=len;					// For easy end space test
  y_len-=len;
  while (len--)
  {
    if (my_sort_order[(uchar) *s++] != my_sort_order[(uchar) *t++])
      return ((int) my_sort_order[(uchar) s[-1]] -
	      (int) my_sort_order[(uchar) t[-1]]);
  }
#ifndef CMP_ENDSPACE
  /* Don't compare end space in strings */
  {
    if (y_len)
    {
      const char *end=t+y_len;
      for (; t != end ; t++)
	if (!isspace(*t))
	  return -1;
    }
    else
    {
      const char *end=s+x_len;
      for (; s != end ; s++)
	if (!isspace(*s))
	  return 1;
    }
    return 0;
  }
#else
  return (int) (x_len-y_len);
#endif /* CMP_ENDSPACE */
#endif /* USE_STRCOLL */
}


int stringcmp(const String *x,const String *y)
{
  const char *s= x->ptr();
  const char *t= y->ptr();
  uint32 x_len=x->length(),y_len=y->length(),len=min(x_len,y_len);

  while (len--)
  {
    if (*s++ != *t++)
      return ((int) (uchar) s[-1] - (int) (uchar) t[-1]);
  }
  return (int) (x_len-y_len);
}


String *copy_if_not_alloced(String *to,String *from,uint32 length)
{
  if (from->Alloced_length >= length)
    return from;
  if (from->alloced || !to || from == to)
  {
    (void) from->realloc(length);
    return from;
  }
  if (to->realloc(length))
    return from;				// Actually an error
  to->str_length=min(from->str_length,length);
  memcpy(to->Ptr,from->Ptr,to->str_length);
  return to;
}

/* Make it easier to handle different charactersets */

#if defined(USE_BIG5CODE)
#define INC_PTR(A,B) A=(A+1 == B || !isbig5code(*A,*(A+1))) ? A+1 : A+2
#elif defined(USE_MB)
#define INC_PTR(A,B) A+=(ismbchar(A,B)?ismbchar(A,B):1)
#else
#define INC_PTR(A,B) A++
#endif

/*
** Compare string against string with wildcard
**	0 if matched
**	-1 if not matched with wildcard
**	 1 if matched with wildcard
*/

static int wild_case_compare(const char *str,const char *strend,
			     const char *wildstr,const char *wildend,
			     char escape)
{
  int result= -1;				// Not found, using wildcards
  while (wildstr != wildend)
  {
    while (*wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
#ifdef USE_BIG5CODE
      if (wildstr+1 != wildend && isbig5code(*wildstr,*(wildstr+1)))
      {
	if (*str != *wildstr || str+1 == strend || str[1] != wildstr[1])
	  return (1);
	str+=2;
	wildstr+=2;
      }
      else
#endif
#ifdef USE_MB
      int l;
      if ((l = ismbchar(wildstr, wildend))) {
	  if (str+l > strend || memcmp(str, wildstr, l) != 0)
	      return 1;
	  str += l;
	  wildstr += l;
      } else
#endif
      if (str == strend || toupper(*wildstr++) != toupper(*str++))
	return(1);				// No match
      if (wildstr == wildend)
	return (str != strend);			// Match if both are at end
      result=1;					// Found an anchor char
    }
    if (*wildstr == wild_one)
    {
      do
      {
	if (str == strend)			// Skipp one char if possible
	  return (result);
	INC_PTR(str,strend);
      } while (++wildstr < wildend && *wildstr == wild_one);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == wild_many)
    {						// Found wild_many
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for ( ; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == wild_many)
	  continue;
	if (*wildstr == wild_one)
	{
	  if (str == strend)
	    return (result);
	  INC_PTR(str,strend);
	  continue;
	}
	break;					// Not a wild character
      }
      if (wildstr == wildend)
	return(0);				// Ok if wild_many is last
      if (str == strend)
	return result;

      char cmp;
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
#ifdef USE_BIG5CODE
      bool cmp_is_code=wildstr+1 != wildend && isbig5code(cmp,*(wildstr));
#endif
#ifdef USE_MB
      int mblen = ismbchar(wildstr, wildend);
      const char* mb = wildstr;
#endif
      INC_PTR(wildstr,wildend);			// This is compared trough cmp
#if !defined(USE_BIG5CODE)
      cmp=toupper(cmp);
#endif
      do
      {
#ifdef USE_BIG5CODE
	for (;;)
	{
	  if (cmp_is_code)
	  {
	    if (*str == cmp && str+1 != strend && str[1] == wildstr[-1])
	    {
	      str+=2;				// Point at char after cmp
	      break;
	    }
	    else if (str+1 != strend && isbig5code(*str,*(str+1)))
	      str++;
	  }
	  else if (str+1 != strend && isbig5code(*str,*(str+1)) && *str != cmp)
	    str++;				// Skip extra shar
	  else if (toupper(*str) == toupper(cmp))
	  {
	    str++;
	    break;				// Found it
	  }
	  if (str++ == strend)
	    return (result);
	}
#else
#ifdef USE_MB
	for (;;)
	{
	  if (str >= strend)
	    return result;
	  if (mblen)
	  {
	    if (str+mblen <= strend && memcmp(str, mb, mblen) == 0)
	    {
	      str += mblen;
	      break;
	    }
	  }
	  else if (!ismbchar(str, strend) && toupper(*str) == cmp)
	  {
	    str++;
	    break;
	  }
	  INC_PTR(str, strend);
	}
#else
	while (str != strend && toupper(*str) != cmp)
	  str++;
	if (str++ == strend) return (result);
#endif
#endif
	{
	  int tmp=wild_case_compare(str,strend,wildstr,wildend,escape);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != strend && wildstr[0] != wild_many);
      return(result);
    }
  }
  return (str != strend ? 1 : 0);
}


int wild_case_compare(String &match,String &wild, char escape)
{
  return wild_case_compare(match.ptr(),match.ptr()+match.length(),
			   wild.ptr(), wild.ptr()+wild.length(),escape);
}

/*
** The following is used when using LIKE on binary strings
*/

static int wild_compare(const char *str,const char *strend,
			const char *wildstr,const char *wildend,char escape)
{
  int result= -1;				// Not found, using wildcards
  while (wildstr != wildend)
  {
    while (*wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == escape && wildstr+1 != wildend)
	wildstr++;
      if (str == strend || *wildstr++ != *str++)
	return(1);
      if (wildstr == wildend)
	return (str != strend);			// Match if both are at end
      result=1;					// Found an anchor char
    }
    if (*wildstr == wild_one)
    {
      do
      {
	if (str == strend)			// Skipp one char if possible
	  return (result);
	INC_PTR(str,strend);
      } while (*++wildstr == wild_one && wildstr != wildend);
      if (wildstr == wildend)
	break;
    }
    if (*wildstr == wild_many)
    {						// Found wild_many
      wildstr++;
      /* Remove any '%' and '_' from the wild search string */
      for ( ; wildstr != wildend ; wildstr++)
      {
	if (*wildstr == wild_many)
	  continue;
	if (*wildstr == wild_one)
	{
	  if (str == strend)
	    return (result);
	  INC_PTR(str,strend);
	  continue;
	}
	break;					// Not a wild character
      }
      if (wildstr == wildend)
	return(0);				// Ok if wild_many is last
      if (str == strend)
	return result;

      char cmp;
      if ((cmp= *wildstr) == escape && wildstr+1 != wildend)
	cmp= *++wildstr;
      INC_PTR(wildstr,wildend);			// This is compared trough cmp
      do
      {
	while (str != strend && *str != cmp)
	  str++;
	if (str++ == strend) return (result);
	{
	  int tmp=wild_compare(str,strend,wildstr,wildend,escape);
	  if (tmp <= 0)
	    return (tmp);
	}
      } while (str != strend && wildstr[0] != wild_many);
      return(result);
    }
  }
  return (str != strend ? 1 : 0);
}


int wild_compare(String &match,String &wild, char escape)
{
  return wild_compare(match.ptr(),match.ptr()+match.length(),
		      wild.ptr(), wild.ptr()+wild.length(),escape);
}
