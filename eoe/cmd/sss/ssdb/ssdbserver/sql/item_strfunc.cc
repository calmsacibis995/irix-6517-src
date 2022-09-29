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

/* This file defines all string functions
** Warning: Some string functions doesn't always put and end-null on a String
** (This shouldn't be neaded)
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

String empty_string("");

uint nr_of_decimals(const char *str)
{
  if ((str=strchr(str,'.')))
  {
    const char *start= ++str;
    for ( ; isdigit(*str) ; str++) ;
    return (uint) (str-start);
  }
  return 0;
}

double Item_str_func::val()
{
  String *res;
  res=str(&str_value);
  return res ? atof(res->c_ptr()) : 0.0;
}

longlong Item_str_func::val_int()
{
  String *res;
  res=str(&str_value);
  return res ? strtoll(res->c_ptr(),NULL,10) : (longlong) 0;
}


/*
** Concatinate args with the following premissess
** If only one arg which is ok, return value of arg
** Don't reallocate str() if not absolute necessary.
*/

String *Item_func_concat::str(String *str)
{
  String *res,*res2,*use_as_buff;
  uint i;

  null_value=0;
  if (!(res=args[0]->str(str)))
    goto null;
  use_as_buff= &tmp_value;
  for (i=1 ; i < arg_count ; i++)
  {
    if (res->length() == 0)
    {
      if (!(res=args[i]->str(str)))
	goto null;
    }
    else
    {
      if (!(res2=args[i]->str(use_as_buff)))
	goto null;
      if (res2->length() == 0)
	continue;
      if (res->length()+res2->length() > max_allowed_packet)
	goto null;				// Error check
      if (res->alloced_length() >= res->length()+res2->length())
      {						// Use old buffer
	res->append(*res2);
      }
      else if (str->alloced_length() >= res->length()+res2->length())
      {
	str->copy(*res);
	str->append(*res2);
	res=str;
      }
      else if (res == &tmp_value)
      {
	res->append(*res2);			// Must be a blob
      }
      else if (res2 == &tmp_value)
      {						// This can happend only 1 time
	tmp_value.replace(0,0,*res);
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
	       res2->ptr() <= tmp_value.ptr() + tmp_value.alloced_length())
      {						// This happens real seldom
	tmp_value.copy(*res);			// Move res to start of tmp
	tmp_value.replace(0,0,*res);
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else
      {						// Two big const strings
	if (tmp_value.alloc(max_length))
	  return &empty_string;			/* purecov: inspected */
	tmp_value.copy(*res);
	tmp_value.append(*res2);
	res= &tmp_value;
	use_as_buff=str;
      }
    }
  }
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat::fix_length_and_dec()
{
  max_length=0;
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_reverse::str(String *str)
{
  String *res = args[0]->str(str);
  char *ptr,*end;

  if ((null_value=args[0]->null_value))
    return 0;
  res=copy_if_not_alloced(str,res,res->length());
  ptr = (char *) res->ptr();
  end=ptr+res->length()-1;
  for (; ptr < end ; ptr++,end--)
  {
    char tmp = *ptr;
    *ptr = *end;
    *end=tmp;
  }
  return res;
}


void Item_func_reverse::fix_length_and_dec()
{
  max_length = args[0]->max_length;
}

/*
** Replace all occurences of string2 in string1 with string3.
** Don't reallocate str() if not neaded
*/


String *Item_func_replace::str(String *str)
{
  String *res,*res2,*res3;
  int offset;
  uint from_length,to_length;
  bool alloced=0;

  null_value=0;
  res=args[0]->str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  if (res2->length() == 0)
    return res;
  if ((offset=res->strstr(*res2)) < 0)
    return res;
  if (!(res3=args[2]->str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

  do
  {
    if (res->length()-from_length + to_length > max_allowed_packet)
      goto null;
    if (!alloced)
    {
      alloced=1;
      res=copy_if_not_alloced(str,res,res->length()+to_length);
    }
    res->replace((uint) offset,from_length,*res3);
    offset+=(int) to_length;
  }
  while ((offset=res->strstr(*res2,(uint) offset)) >0);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_replace::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  int diff=(int) (args[2]->max_length - args[1]->max_length);
  if (diff > 0 && args[1]->max_length)
  {						// Calculate of maxreplaces
    max_length= max_length/args[1]->max_length;
    max_length= (max_length+1)*(uint) diff;
  }
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_insert::str(String *str)
{
  String *res,*res2;
  uint start,length;

  null_value=0;
  res=args[0]->str(str);
  res2=args[3]->str(&tmp_value);
  start=(uint) args[1]->val_int()-1;
  length=(uint) args[2]->val_int();
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null; /* purecov: inspected */
  if (start > res->length()+1)
    return res;					// Wrong param; skipp insert
  if (length > res->length()-start)
    length=res->length()-start;
  if (res->length() - length + res2->length() > max_allowed_packet)
    goto null;					// OOM check
  res=copy_if_not_alloced(str,res,res->length());
  res->replace(start,length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}


void Item_func_insert::fix_length_and_dec()
{
  max_length=args[0]->max_length+args[3]->max_length;
  if (max_length > MAX_BLOB_WIDTH)
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lcase::str(String *str)
{
  String *res;
  if (!(res=args[0]->str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->casedn();
  return res;
}


String *Item_func_ucase::str(String *str)
{
  String *res;
  if (!(res=args[0]->str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  res->caseup();
  return res;
}


String *Item_func_left::str(String *str)
{
  String *res  =args[0]->str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0;
  if (length <= 0)
    return &empty_string;
  if (res->length() > (ulong) length)
  {						// Safe even if const arg
    if (!res->alloced_length())
    {						// Don't change const str
      str_value= *res;				// Not malloced string
      res= &str_value;
    }
    res->length((uint) length);
  }
  return res;
}


void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int();
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_left::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_right::str(String *str)
{
  String *res  =args[0]->str(str);
  long length  =(long) args[1]->val_int();

  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  if (length <= 0)
    return &empty_string; /* purecov: inspected */
  if (res->length() <= (uint) length)
    return res; /* purecov: inspected */
  tmp_value.set(*res,(res->length()- (uint) length),(uint) length);
  return &tmp_value;
}


void Item_func_right::fix_length_and_dec()
{
  left_right_max_length();
}


String *Item_func_substr::str(String *str)
{
  String *res  = args[0]->str(str);
  int32 start   = (int32) args[1]->val_int();
  int32 length  = arg_count == 3 ? (int32) args[2]->val_int() : INT_MAX32;
  int32 tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */
  if (start <= 0 || (uint) start > res->length() || length <= 0)
    return &empty_string;

  start--;
  tmp_length=(int32) res->length()-start;
  length=min(length,tmp_length);

  if (!start && res->length() == (uint) length)
    return res;
  tmp_value.set(*res,(uint) start,(uint) length);
  return &tmp_value;
}


void Item_func_substr::fix_length_and_dec()
{
  max_length=args[0]->max_length;

  if (args[1]->const_item())
  {
    int32 start=(int32) args[1]->val_int()-1;
    if (start < 0 || start >= (int32) max_length)
      max_length=0; /* purecov: inspected */
    else
      max_length-= (uint) start;
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32 length= (int32) args[2]->val_int();
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
}


String *Item_func_substr_index::str(String *str)
{
  String *res =args[0]->str(str);
  String *delimeter =args[1]->str(&tmp_value);
  int32 count = (int32) args[2]->val_int();
  uint offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint delimeter_length=delimeter->length();
  if (!res->length() || !delimeter_length || !count)
    return &empty_string;		// Wrong parameters

  if (count > 0)
  {					// start counting from the beginning
    for (offset=0 ;; offset+=delimeter_length)
    {
      if ((int) (offset=res->strstr(*delimeter,offset)) < 0)
	return res;			// Didn't find, return org string
      if (!--count)
      {
	tmp_value.set(*res,0,offset);
	break;
      }
    }
  }
  else
  {					// Start counting at end
    for (offset=res->length() ; ; offset-=delimeter_length)
    {
      if ((int) (offset=res->strrstr(*delimeter,offset)) < 0)
	return res;			// Didn't find, return org string
      if (!++count)
      {
	offset+=delimeter_length;
	tmp_value.set(*res,offset,res->length()- offset);
	break;
      }
    }
  }
  return (&tmp_value);
}

/*
** The trim functions are extension to ANSI SQL because they trim substrings
** They ltrim() and rtrim() functions are optimized for 1 byte strings
** They also return the original string if possible, else they return
** a substring that points at the original string.
*/


String *Item_func_ltrim::str(String *str)
{
  String *res  =args[0]->str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove=args[1]->str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove || (remove_length=remove->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  if (remove_length == 1)
  {
    char chr=(*remove)[0];
    while (ptr != end && *ptr == chr)
      ptr++;
  }
  else
  {
    const char *r_ptr=remove->ptr();
    end-=remove_length;
    while (ptr < end && !memcmp(ptr,r_ptr,remove_length))
      ptr+=remove_length;
    end+=remove_length;
  }
  if (ptr == res->ptr())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_rtrim::str(String *str)
{
  String *res  =args[0]->str(str);
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove=args[1]->str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove || (remove_length=remove->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  if (remove_length == 1)
  {
    char chr=(*remove)[0];
    while (ptr != end  && end[-1] == chr)
      end--;
  }
  else
  {
    const char *r_ptr=remove->ptr();
    while (ptr + remove_length < end &&
	   !memcmp(end-remove_length,r_ptr,remove_length))
      end-=remove_length;
  }
  if (end == res->ptr()+res->length())
    return res;
  tmp_value.set(*res,0,(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_trim::str(String *str)
{
  String *res  =args[0]->str(str);
  if ((null_value=args[0]->null_value))
    return 0;					/* purecov: inspected */
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  String *remove=args[1]->str(&tmp);
  uint remove_length;
  LINT_INIT(remove_length);

  if (!remove || (remove_length=remove->length()) == 0 ||
      remove_length > res->length())
    return res;

  char *ptr=(char*) res->ptr();
  char *end=ptr+res->length();
  const char *r_ptr=remove->ptr();
  while (ptr+remove_length < end && !memcmp(ptr,r_ptr,remove_length))
    ptr+=remove_length;
  while (ptr + remove_length < end &&
	 !memcmp(end-remove_length,r_ptr,remove_length))
    end-=remove_length;

  if (ptr == res->ptr() && end == ptr+res->length())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}



String *Item_func_password::str(String *str)
{
  String *res  =args[0]->str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;
  make_scrambled_password(tmp_value,res->c_ptr());
  str->set(tmp_value,16);
  return str;
}

#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::str(String *str)
{
  String *res  =args[0]->str(str);

#ifdef HAVE_CRYPT
  char salt[2];
 if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &empty_string;

  if (arg_count == 1)
  {					// generate random salt
    time_t timestamp=current_thd->query_start();
    salt[0] = bin_to_ascii(timestamp & 0x3f);
    salt[1] = bin_to_ascii((timestamp >> 5) & 0x3f);
  }
  else
  {					// obtain salt from the first two bytes
    String *salt_str=args[1]->str(&tmp_value);
    if ((null_value= (args[1]->null_value || salt_str->length() < 2)))
      return 0;
    salt[0]=(*salt_str)[0];
    salt[1]=(*salt_str)[1];
  }
  str->set(crypt(res->c_ptr(),salt),13);
  return str;
#else
  null_value=1;
  return 0;
#endif	/* HAVE_CRYPT */
}

void Item_func_encode::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  maybe_null=args[0]->maybe_null;
}

String *Item_func_encode::str(String *str)
{
  String *res;
  if (!(res=args[0]->str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.encode((char*) res->ptr(),res->length());
  return res;
}

String *Item_func_decode::str(String *str)
{
  String *res;
  if (!(res=args[0]->str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  sql_crypt.init();
  sql_crypt.decode((char*) res->ptr(),res->length());
  return res;
}


String *Item_func_database::str(String *str)
{
  if (!current_thd->db)
    str->length(0);
  else
    str->set((const char*) current_thd->db,strlen(current_thd->db));
  return str;
}

String *Item_func_user::str(String *str)
{
  THD *thd=current_thd;
  str->copy((const char*) thd->user,strlen(thd->user));
  str->append('@');
  str->append(thd->host ? thd->host : thd->ip ? thd->ip : "");
  return str;
}

void Item_func_soundex::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  set_if_bigger(max_length,4);
}


  /*
    If alpha, map input letter to soundex code.
    If not alpha and remove_garbage is set then skipp to next char
    else return 0
    */

extern "C" {
extern char *soundex_map;		// In mysys/static.c
}

static char get_scode(char *ptr)
{
  uchar ch=toupper(*ptr);
  if (ch < 'A' || ch > 'Z')
  {
					// Thread extended alfa (country spec)
    return '0';				// as vokal
  }
  return(soundex_map[ch-'A']);
}


String *Item_func_soundex::str(String *str)
{
  String *res  =args[0]->str(str);
  char last_ch,ch;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */

  if (str_value.alloc(max(res->length(),4)))
    return str; /* purecov: inspected */
  char *to= (char *) str_value.ptr();
  char *from= (char *) res->ptr(), *end=from+res->length();

  while (from != end && isspace(*from)) // Skipp pre-space
    from++; /* purecov: inspected */
  if (from == end)
    return &empty_string;		// No alpha characters.
  *to++ = toupper(*from);		// Copy first letter
  last_ch = get_scode(from);		// code of the first letter
					// for the first 'double-letter check.
					// Loop on input letters until
					// end of input (null) or output
					// letter code count = 3
  for (from++ ; from < end ; from++)
  {
    if (!isalpha(*from))
      continue;
    ch=get_scode(from);
    if ((ch != '0') && (ch != last_ch)) // if not skipped or double
    {
       *to++ = ch;			// letter, copy to output
       last_ch = ch;			// save code of last input letter
    }					// for next double-letter check
  }
  for (end=(char*) str_value.ptr()+4 ; to < end ; to++)
    *to = '0';
  *to=0;				// end string
  str_value.length((uint) (to-str_value.ptr()));
  return &str_value;
}


/*
** Change a number to format '3,333,333,333.000'
** This should be 'internationalized' sometimes.
*/

Item_func_format::Item_func_format(Item *org,int dec) :Item_str_func(org)
{
  decimals=(uint) set_zone(dec,0,30);
}


String *Item_func_format::str(String *str)
{
  double nr	=args[0]->val();
  uint32 diff,length,str_length;
  uint dec;
  if ((null_value=args[0]->null_value))
    return 0; /* purecov: inspected */
  dec= decimals ? decimals+1 : 0;
  str->set(nr,decimals);
  str_length=str->length();
  if (nr < 0)
    str_length--;				// Don't count sign
  length=str->length()+(diff=(str_length- dec-1)/3);
  if (diff)
  {
    char *tmp,*pos;
    str=copy_if_not_alloced(&tmp_str,str,length);
    str->length(length);
    tmp=(char*) str->ptr()+length - dec-1;
    for (pos=(char*) str->ptr()+length ; pos != tmp; pos--)
      pos[0]=pos[- (int) diff];
    while (diff)
    {
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=pos[-(int) diff]; pos--;
      pos[0]=',';
      pos--;
      diff--;
    }
  }
  return str;
}


void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;
  for (uint i=1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
  used_tables_cache|=item->used_tables();
}


void Item_func_elt::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
}


double Item_func_elt::val()
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  return args[tmp-1]->val();
}

longlong Item_func_elt::val_int()
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return args[tmp-1]->val_int();
}

String *Item_func_elt::str(String *str)
{
  uint tmp;
  if ((tmp=(uint) item->val_int()) == 0 || tmp > arg_count)
  {
    null_value=1;
    return NULL;
  }
  null_value=0;
  return args[tmp-1]->str(str);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;
  decimals=0;
  for (uint i=1 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;
  used_tables_cache|=item->used_tables();
}


void Item_func_make_set::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
}


String *Item_func_make_set::str(String *str)
{
  ulonglong bits;
  bool first_found=0;
  Item **ptr=args;
  String *result=&empty_string;

  bits=item->val_int();
  if ((null_value=item->null_value))
    return NULL;

  if (arg_count < 64)
    bits &= ((ulonglong) 1 << arg_count)-1;

  for (; bits; bits >>= 1, ptr++)
  {
    if (bits & 1)
    {
      String *res= (*ptr)->str(str);
      if (res)					// Skipp nulls
      {
	if (!first_found)
	{					// First argument
	  first_found=1;
	  if (res != str)
	    result=res;				// Use original string
	  else
	  {
	    tmp_str.copy(*res);			// Don't use 'str'
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    tmp_str.alloc(result->length()+res->length()+1);
	    tmp_str.copy(*result);
	    result= &tmp_str;
	  }
	  tmp_str.append(',');
	  tmp_str.append(*res);
	}
      }
    }
  }
  return result;
}


String *Item_func_char::str(String *str)
{
  str->length(0);
  for (uint i=0 ; i < arg_count ; i++)
  {
    char num= (char) args[i]->val_int();
    if (!args[i]->null_value)
      str->append(num);
  }
  str->realloc(str->length());			// Add end 0 (for Purify)
  return str;
}


inline String* alloc_buffer(String *res,String *str,String *tmp_value,
			    ulong length)
{
  if (res->alloced_length() < length)
  {
    if (str->alloced_length() >= length)
    {
      str->copy(*res);
      str->length(length);
      return str;
    }
    else
    {
      if (tmp_value->alloc(length))
	return 0;
      tmp_value->copy(*res);
      tmp_value->length(length);
      return tmp_value;
    }
  }
  res->length(length);
  return res;
}


void Item_func_repeat::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    max_length=(long) (args[0]->max_length * args[1]->val_int());
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}

/*
** Item_func_repeat::str is carefully written to avoid reallocs
** as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::str(String *str)
{
  uint length,tot_length;
  char *to;
  long count= (long) args[1]->val_int();
  String *res =args[0]->str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value=0;
  if (count <= 0)			// For nicer SQL code
    return &empty_string;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  if (length > max_allowed_packet/count)// Safe length check
    goto err;				// Probably an error
  tot_length= length*(uint) count;
  if (!(res= alloc_buffer(res,str,&tmp_value,tot_length)))
    goto err;

  to=(char*) res->ptr()+length;
  while (--count)
  {
    memcpy(to,res->ptr(),length);
    to+=length;
  }
  return (res);

err:
  null_value=1;
  return 0;
}


void Item_func_rpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_rpad::str(String *str)
{
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  int32 count= (int32) args[1]->val_int();
  String *res =args[0]->str(str);
  String *rpad = args[2]->str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;
  null_value=0;
  if (count <= (int32) (res_length=res->length()))
    return (res);				// String to pad is big enough
  length_pad= rpad->length();
  if ((ulong) count > max_allowed_packet || args[2]->null_value || !length_pad)
    goto err;
  if (!(res= alloc_buffer(res,str,&tmp_value,count)))
    goto err;

  to= (char*) res->ptr()+res_length;
  ptr_pad=rpad->ptr();
  for (count-= res_length; (uint32) count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


void Item_func_lpad::fix_length_and_dec()
{
  if (args[1]->const_item())
  {
    uint32 length= (uint32) args[1]->val_int();
    max_length=max(args[0]->max_length,length);
    if (max_length >= MAX_BLOB_WIDTH)
    {
      max_length=MAX_BLOB_WIDTH;
      maybe_null=1;
    }
  }
  else
  {
    max_length=MAX_BLOB_WIDTH;
    maybe_null=1;
  }
}


String *Item_func_lpad::str(String *str)
{
  uint32 res_length,length_pad;
  char *to;
  const char *ptr_pad;
  ulong count= (long) args[1]->val_int();
  String *res= args[0]->str(str);
  String *lpad= args[2]->str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;
  null_value=0;
  if (count <= (res_length=res->length()))
    return (res);				// String to pad is big enough
  length_pad= lpad->length();
  if (count > max_allowed_packet || args[2]->null_value || !length_pad)
    goto err;

  if (res->alloced_length() < count)
  {
    if (str->alloced_length() >= count)
    {
      memcpy((char*) str->ptr()+(count-res_length),res->ptr(),res_length);
      res=str;
    }
    else
    {
      if (tmp_value.alloc(count))
	goto err;
      memcpy((char*) tmp_value.ptr()+(count-res_length),res->ptr(),res_length);
      res=&tmp_value;
    }
  }
  else
    bmove_upp((char*) res->ptr()+count,res->ptr()+res_length,res_length);
  res->length(count);

  to= (char*) res->ptr();
  ptr_pad= lpad->ptr();
  for (count-= res_length; count > length_pad; count-= length_pad)
  {
    memcpy(to,ptr_pad,length_pad);
    to+= length_pad;
  }
  memcpy(to,ptr_pad,(size_t) count);
  return (res);

 err:
  null_value=1;
  return 0;
}


String *Item_func_conv::str(String *str)
{
  String *res= args[0]->str(str);
  char *endptr,ans[65],*ptr;
  longlong dec;
  int from_base= (int) args[1]->val_int();
  int to_base= (int) args[2]->val_int();

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (from_base < 0)
    dec= strtoll(res->c_ptr(),&endptr,-from_base);
  else
    dec= (longlong) strtoull(res->c_ptr(),&endptr,from_base);
  ptr= longlong2str(dec,ans,to_base);
  res->copy(ans,(uint32) (ptr-ans));
  return res;
}
