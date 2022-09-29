/* Copyright (C) 1979-1997 TcX AB & Monty Program KB & Detron HB

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

/*
  NOTES:
  Some of the number class uses the system functions strtol(), strtoll()...
  To avoid patching the end \0 or copying the buffer unnecessary, all calls
  to system functions are wrapped to a String object that adds the end null
  if it only if it isn't there.
  This adds some overhead when assigning numbers from strings but makes
  everything simpler.
  */

/*****************************************************************************
** This file implements classes defined in field.h
*****************************************************************************/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"
#include <m_ctype.h>
#include <errno.h>
#ifdef HAVE_FCONVERT
#include <floatingpoint.h>
#endif

/*****************************************************************************
** Instansiate templates and static variables
*****************************************************************************/

#ifdef __GNUC__
template class List<create_field>;
template class List_iterator<create_field>;
#endif

struct decstr {
  uint nr_length,nr_dec,sign,extra;
  char sign_char;
};

uchar Field_null::null[1]={1};
const char field_separator=',';

/*****************************************************************************
** Static help functions
*****************************************************************************/

	/*
	** Calculate length of number and it's parts
	** Increment cuted_fields if wrong number
	*/

static bool
number_dec(struct decstr *sdec, const char *str, const char *end)
{
  sdec->sign=sdec->extra=0;
  if (str == end)
  {
    current_thd->cuted_fields++;
    sdec->nr_length=sdec->nr_dec=sdec->sign=0;
    sdec->extra=1;				// We must put one 0 before .
    return 1;
  }

  if (*str == '-' || *str == '+')		/* sign */
  {
    sdec->sign_char= *str;
    sdec->sign=1;
    str++;
  }
  const char *start=str;
  while (str != end && isdigit(*str))
    str++;
  if (!(sdec->nr_length=(uint) (str-start)))
    sdec->extra=1;				// We must put one 0 before .
  start=str;
  if (str != end && *str == '.')
  {
    str++;
    start=str;
    while (str != end && isdigit(*str))
      str++;
  }
  sdec->nr_dec=(uint) (str-start);
  if (current_thd->count_cuted_fields)
  {
    while (str != end && isspace(*str))
      str++; /* purecov: inspected */
    if (str != end)
    {
      current_thd->cuted_fields++;
      return 1;
    }
  }
  return 0;
}


void Field_num::prepend_zeros(String *value)
{
  int diff;
  if ((diff= (int) (field_length - value->length())) > 0)
  {
    bmove_upp((char*) value->ptr()+field_length,value->ptr()+value->length(),
	      value->length());
    bfill((char*) value->ptr(),diff,'0');
    value->length(field_length);
    (void) value->c_ptr_quick();		// Avoid warnings in purify
  }
}

/*
** Test if given number is a int (or a fixed format float with .000)
** This is only used to give warnings in ALTER TABLE or LOAD DATA...
*/

static bool test_if_int(const char *str,int length)
{
  const char *end=str+length;

  while (str != end && isspace(*str))	// Allow start space
    str++; /* purecov: inspected */
  if (str != end && (*str == '-' || *str == '+'))
    str++;
  if (str == end)
    return 0;					// Error: Empty string
  for ( ; str != end ; str++)
  {
    if (!isdigit(*str))
    {
      if (*str == '.')
      {						// Allow '.0000'
	for (str++ ; str != end && *str == '0'; str++) ;
	if (str == end)
	  return 1;
      }
      if (!isspace(*str))
	return 0;
      for (str++ ; str != end ; str++)
	if (!isspace(*str))
	  return 0;
      return 1;
    }
  }
  return 1;
}


static bool test_if_real(const char *str,int length)
{
  while (length && isspace(*str))
  {						// Allow start space
    length--; str++;
  }
  if (!length)
    return 0;
  if (*str == '+' || *str == '-')
  {
    length--; str++;
    if (!length || !(isdigit(*str) || *str == '.'))
      return 0;
  }
  while (length && isdigit(*str))
  {
    length--; str++;
  }
  if (!length)
    return 1;
  if (*str == '.')
  {
    length--; str++;
    while (length && isdigit(*str))
    {
      length--; str++;
    }
  }
  if (!length)
    return 1;
  if (*str == 'E' || *str == 'e')
  {
    if (length < 3 || (str[1] != '+' && str[1] != '-') || !isdigit(str[2]))
      return 0;
    length-=3;
    str+=3;
    while (length && isdigit(*str))
    {
      length--; str++;
    }
  }
  for ( ; length ; length--, str++)
  {						// Allow end space
    if (!isspace(*str))
      return 0;
  }
  return 1;
}


/****************************************************************************
** Functions for the base classes
** This is a unpacked number.
****************************************************************************/

Field::Field(char *ptr_arg,uint32 length_arg,uchar *null_ptr_arg,
	     uint null_bit_arg,
	     utype unireg_check_arg,char *field_name_arg,
	     struct st_table *table_arg)
  :ptr(ptr_arg),null_ptr(null_ptr_arg),null_bit(null_bit_arg),
   table(table_arg),query_id(0),key_start(0),part_of_key(0),
   table_name(table_arg ? table_arg->table_name : 0),
   field_name(field_name_arg), unireg_check(unireg_check_arg),
   field_length(length_arg)
{
  flags=null_ptr ? 0: NOT_NULL_FLAG;
}

uint Field::offset()
{
  return (uint) (ptr - (char*) table->record[0]);
}


void Field::copy_from_tmp(int offset)
{
  memcpy(ptr,ptr+offset,pack_length());
  if (null_ptr)
  {
    *null_ptr= ((null_ptr[0] & (uchar) ~(uint) null_bit) |
		null_ptr[offset] & (uchar) null_bit);
  }
}


bool Field::send(String *packet)
{
  if (is_null())
    return net_store_null(packet);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff));
  val_str(&tmp,&tmp);
  CONVERT *convert;
  if ((convert=current_thd->convert_set))
    return convert->store(packet,tmp.ptr(),tmp.length());
  return net_store_data(packet,tmp.ptr(),tmp.length());
}


void Field_num::add_zerofill_and_unsigned(String &res) const
{
  res.length(strlen(res.ptr()));		// Fix length
  if (unsigned_flag)
    res.append(" unsigned");
  if (zerofill)
    res.append(" zerofill");
}

void Field_num::make_field(Send_field *field)
{
  field->table_name=table_name;
  field->col_name=field_name;
  field->length=field_length;
  field->type=type();
  field->flags=table->maybe_null ? (flags & ~NOT_NULL_FLAG) : flags;
  field->decimals=decimals;
}


void Field_str::make_field(Send_field *field)
{
  field->table_name=table_name;
  field->col_name=field_name;
  field->length=field_length;
  field->type=type();
  field->flags=table->maybe_null ? (flags & ~NOT_NULL_FLAG) : flags;
  field->decimals=0;
}


uint Field::fill_cache_field(CACHE_FIELD *copy)
{
  copy->str=ptr;
  copy->length=pack_length();
  copy->blob_field=0;
  if (flags & BLOB_FLAG)
  {
    copy->blob_field=(Field_blob*) this;
    copy->strip=0;
    copy->length-=sizeof(char*);
    return copy->length;
  }
  else if (!zero_pack() && (type() == FIELD_TYPE_STRING && copy->length > 4 ||
			    type() == FIELD_TYPE_VAR_STRING))
    copy->strip=1;				/* Remove end space */
  else
    copy->strip=0;
  return copy->length+(int) copy->strip;
}

/****************************************************************************
** Functions for the Field_decimal class
** This is a unpacked number.
****************************************************************************/

void
Field_decimal::reset(void)
{
  Field_decimal::store("0",1);
}

void Field_decimal::overflow(bool negative)
{
  uint len=field_length;
  char *to=ptr;
  if (negative && !unsigned_flag)
  {
    *to++ = '-';
    len--;
  }
  bfill(to,len,negative && unsigned_flag ? '0' : '9');
  if (decimals)
    ptr[field_length-decimals-1]='.';
  return;
}


void Field_decimal::store(const char *from,uint len)
{
  reg3 int i;
  uint dec;
  char fyllchar;
  const char *end=from+len;
  struct decstr index;
  bool error;

  if ((dec= decimals))
    dec++;					// Calculate pos of '.'
  while (from != end && isspace(*from))
    from++;
  if (zerofill)
  {
    fyllchar = '0';
    if (from != end)
      while (*from == '0' && from != end-1)	// Skipp prezero
	from++;
  }
  else
    fyllchar=' ';
  error=number_dec(&index,from,end);
  if (index.sign)
  {
    from++;
    if (unsigned_flag)				// No sign with zerofill
    {
      if (!error)
	current_thd->cuted_fields++;
      Field_decimal::overflow(1);
      return;
    }
  }
  /*
  ** Remove pre-zeros if too big number
  */
  for (i= (int) (index.nr_length+index.extra -(field_length-dec)+index.sign) ;
       i > 0 ;
       i--)
  {
    if (*from == '0')
    {
      from++;
      index.nr_length--;
      continue;
    }
    if (index.sign && index.sign_char == '+' && i == 1)
    {						// Remove pre '+'
      index.sign=0;
      break;
    }
    current_thd->cuted_fields++;
    // too big number, change to max or min number
    Field_decimal::overflow(index.sign && index.sign_char == '-');
    return;
  }
  char *to=ptr;
  for (i=(int) (field_length-dec-index.nr_length-index.extra - index.sign) ;
       i-- > 0 ;)
    *to++ = fyllchar;
  if (index.sign)
    *to++= index.sign_char;
  if (index.extra)
    *to++ = '0';
  for (i=(int) index.nr_length ; i-- > 0 ; )
    *to++ = *from++;
  if (dec--)
  {
    *to++ ='.';
    if (index.nr_dec) from++;			// Skipp '.'
    for (i=(int) min(index.nr_dec,dec) ; i-- > 0 ; ) *to++ = *from++;
    for (i=(int) (dec-min(index.nr_dec,dec)) ; i-- > 0 ; ) *to++ = '0';
  }

  /*
  ** Check for incorrect string if in batch mode (ALTER TABLE/LOAD DATA...)
  */
  if (!error && current_thd->count_cuted_fields && from != end)
  {						// Check if number was cuted
    for (; from != end ; from++)
    {
      if (*from != '0')
      {
	if (!isspace(*from))			// Space is ok
	  current_thd->cuted_fields++;
	break;
      }
    }
  }
}


void Field_decimal::store(double nr)
{
  if (unsigned_flag && nr < 0)
  {
    overflow(1);
    current_thd->cuted_fields++;
    return;
  }
  reg4 uint i,length;
  char fyllchar,*to;
  char buff[320];

  fyllchar = zerofill ? (char) '0' : (char) ' ';
  sprintf(buff,"%.*f",decimals,nr);
  length=strlen(buff);

  if (length > field_length)
  {
    overflow(nr < 0.0);
    current_thd->cuted_fields++;
  }
  else
  {
    to=ptr;
    for (i=field_length-length ; i-- > 0 ;)
      *to++ = fyllchar;
    memcpy(to,buff,length);
  }
}


void Field_decimal::store(longlong nr)
{
  if (unsigned_flag && nr < 0)
  {
    overflow(1);
    current_thd->cuted_fields++;
    return;
  }
  char buff[22];
  uint length=(uint) (longlong2str(nr,buff,-10)-buff);
  uint int_part=field_length- (decimals  ? decimals+1 : 0);

  if (length > int_part)
  {
    overflow(test(nr < 0L));			/* purecov: inspected */
    current_thd->cuted_fields++;		/* purecov: inspected */
  }
  else
  {
    char fyllchar = zerofill ? (char) '0' : (char) ' ';
    char *to=ptr;
    for (uint i=int_part-length ; i-- > 0 ;)
      *to++ = fyllchar;
    memcpy(to,buff,length);
    if (decimals)
    {
      to[length]='.';
      bfill(to+length+1,decimals,'0');
    }
  }
}


double Field_decimal::val_real(void)
{
  char temp= *(ptr+field_length); *(ptr+field_length) = '\0';
  double nr=atod(ptr);
  *(ptr+field_length)=temp;
  return(nr);
}

longlong Field_decimal::val_int(void)
{
  char temp= *(ptr+field_length); *(ptr+field_length) = '\0';
  longlong nr;
  if (unsigned_flag)
    nr=(longlong) strtoull(ptr,NULL,10);
  else
    nr=strtoll(ptr,NULL,10);
  *(ptr+field_length)=temp;
  return(nr);
}

String *Field_decimal::val_str(String *val_buffer,String *val_ptr)
{
  char *str;
  for (str=ptr ; *str == ' ' ; str++) ;
  uint tmp_length=(uint) (str-ptr);
  if (field_length < tmp_length)		// Error in data
    val_ptr->length(0);
  else
    val_ptr->set((const char*) str,field_length-tmp_length);
  return val_ptr;
}

/*
** Should be able to handle at least the following fixed decimal formats:
** 5.00 , -1.0,  05,  -05, +5 with optional pre/end space
*/

int Field_decimal::cmp(const char *a_ptr,const char *b_ptr)
{
  const char *end;
  /* First remove prefixes '0', ' ', and '-' */
  for (end=a_ptr+field_length;
       a_ptr != end &&
	 (*a_ptr == *b_ptr ||
	  ((isspace(*a_ptr)  || *a_ptr == '+' || *a_ptr == '0') &&
	   (isspace(*b_ptr) || *b_ptr == '+' || *b_ptr == '0')));
       a_ptr++,b_ptr++) ;

  if (a_ptr == end)
    return 0;
  int swap=0;
  if (*a_ptr == '-')
  {
    if (*b_ptr != '-')
      return -1;
    swap= -1 ^ 1;				// Swap result
    a_ptr++, b_ptr++;
  } else if (*b_ptr == '-')
    return 1;

  while (a_ptr != end)
  {
    if (*a_ptr++ != *b_ptr++)
      return swap ^ (a_ptr[-1] < b_ptr[-1] ? -1 : 1); // compare digits
  }
  return 0;
}


void Field_decimal::sort_string(char *to,uint length)
{
  char *str,*end;
  for (str=ptr,end=ptr+length;
       str != end &&
	 ((isspace(*str) || *str == '+' || *str == '0')) ;

       str++)
    *to++=' ';
  if (str == end)
    return; /* purecov: inspected */

  if (*str == '-')
  {
    *to++=1;					// Smaller than any number
    str++;
    while (str != end)
      if (isdigit(*str))
	*to++= (char) ('9' - *str++);
      else
	*to++= *str++;
  }
  else memcpy(to,str,(uint) (end-str));
}

void Field_decimal::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"decimal(%d,%d)",field_length,decimals);
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
** tiny int
****************************************************************************/

void Field_tiny::store(const char *from,uint len)
{
  String tmp_str(from,len);
  long tmp= strtol(tmp_str.c_ptr(),NULL,10);

  if (unsigned_flag)
  {
    if (tmp < 0)
    {
      tmp=0; /* purecov: inspected */
      current_thd->cuted_fields++; /* purecov: inspected */
    }
    else if (tmp > 255)
    {
      tmp= 255;
      current_thd->cuted_fields++;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len))
      current_thd->cuted_fields++;
  }
  else
  {
    if (tmp < -128)
    {
      tmp= -128;
      current_thd->cuted_fields++;
    }
    else if (tmp >= 128)
    {
      tmp= 127;
      current_thd->cuted_fields++;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len))
      current_thd->cuted_fields++;
  }
  ptr[0]= (char) tmp;
}


void Field_tiny::store(double nr)
{
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0.0)
    {
      *ptr=0;
      current_thd->cuted_fields++;
    }
    else if (nr > 255.0)
    {
      *ptr=(char) 255;
      current_thd->cuted_fields++;
    }
    else
      *ptr=(char) nr;
  }
  else
  {
    if (nr < -128.0)
    {
      *ptr= (char) -128;
      current_thd->cuted_fields++;
    }
    else if (nr > 127.0)
    {
      *ptr=127;
      current_thd->cuted_fields++;
    }
    else
      *ptr=(char) nr;
  }
}

void Field_tiny::store(longlong nr)
{
  if (unsigned_flag)
  {
    if (nr < 0L)
    {
      *ptr=0;
      current_thd->cuted_fields++;
    }
    else if (nr > 255L)
    {
      *ptr= (char) 255;
      current_thd->cuted_fields++;
    }
    else
      *ptr=(char) nr;
  }
  else
  {
    if (nr < -128L)
    {
      *ptr= (char) -128;
      current_thd->cuted_fields++;
    }
    else if (nr > 127L)
    {
      *ptr=127;
      current_thd->cuted_fields++;
    }
    else
      *ptr=(char) nr;
  }
}


double Field_tiny::val_real(void)
{
  int tmp= unsigned_flag ? (int) ((uchar*) ptr)[0] :
    (int) ((signed char*) ptr)[0];
  return (double) tmp;
}

longlong Field_tiny::val_int(void)
{
  int tmp= unsigned_flag ? (int) ((uchar*) ptr)[0] :
    (int) ((signed char*) ptr)[0];
  return (longlong) tmp;
}

String *Field_tiny::val_str(String *val_buffer,String *val_ptr)
{
  uint length;
  val_buffer->alloc(max(field_length+1,5));
  char *to=(char*) val_buffer->ptr();
  if (unsigned_flag)
    length= (uint) (int2str((long) *((uchar*) ptr),to,10)-to);
  else
    length=(int2str((long) *((signed char*) ptr),to,-10)-to);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_tiny::cmp(const char *a_ptr, const char *b_ptr)
{
  signed char a,b;
  a=(signed char) a_ptr[0]; b= (signed char) b_ptr[0];
  if (unsigned_flag)
    return ((uchar) a < (uchar) b) ? -1 : ((uchar) a > (uchar) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_tiny::sort_string(char *to,uint length)
{
  if (unsigned_flag)
    *to= *ptr;
  else
    to[0] = (char) ((uchar) ptr[0] ^ (uchar) 128);	/* Revers signbit */
}

void Field_tiny::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"tinyint(%d)",field_length);
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** short int
****************************************************************************/


// Note:  Sometimes this should be fixed to use one strtol() to use
// len and check for garbage after number.

void Field_short::store(const char *from,uint len)
{
  String tmp_str(from,len);
  long tmp= strtol(tmp_str.c_ptr(),NULL,10);
  if (unsigned_flag)
  {
    if (tmp < 0)
    {
      tmp=0;
      current_thd->cuted_fields++;
    }
    else if (tmp > (uint16) ~0)
    {
      tmp=(uint16) ~0;
      current_thd->cuted_fields++;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len))
      current_thd->cuted_fields++;
  }
  else
  {
    if (tmp < INT_MIN16)
    {
      tmp= INT_MIN16;
      current_thd->cuted_fields++;
    }
    else if (tmp > INT_MAX16)
    {
      tmp=INT_MAX16;
      current_thd->cuted_fields++;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len))
      current_thd->cuted_fields++;
  }
  shortstore(ptr,(short) tmp);
}


void Field_short::store(double nr)
{
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      shortstore(ptr,0);
      current_thd->cuted_fields++;
    }
    else if (nr > (double) (uint16) ~0)
    {
      shortstore(ptr,(uint16) ~0);
      current_thd->cuted_fields++;
    }
    else
      shortstore(ptr,(uint16) nr);
  }
  else
  {
    if (nr < (double) INT_MIN16)
    {
      shortstore(ptr,INT_MIN16);
      current_thd->cuted_fields++;
    }
    else if (nr > (double) INT_MAX16)
    {
      shortstore(ptr,INT_MAX16);
      current_thd->cuted_fields++;
    }
    else
      shortstore(ptr,(int16) nr);
  }
}

void Field_short::store(longlong nr)
{
  if (unsigned_flag)
  {
    if (nr < 0L)
    {
      shortstore(ptr,0);
      current_thd->cuted_fields++;
    }
    else if (nr > (longlong) (uint16) ~0)
    {
      shortstore(ptr,(uint16) ~0);
      current_thd->cuted_fields++;
    }
    else
      shortstore(ptr,(uint16) nr);
  }
  else
  {
    if (nr < INT_MIN16)
    {
      shortstore(ptr,INT_MIN16);
      current_thd->cuted_fields++;
    }
    else if (nr > INT_MAX16)
    {
      shortstore(ptr,INT_MAX16);
      current_thd->cuted_fields++;
    }
    else
      shortstore(ptr,(int16) nr);
  }
}


double Field_short::val_real(void)
{
  short j;
  shortget(j,ptr)
  return unsigned_flag ? (double) (unsigned short) j : (double) j;
}

longlong Field_short::val_int(void)
{
  short j;
  shortget(j,ptr)
  return unsigned_flag ? (longlong) (unsigned short) j : (longlong) j;
}

String *Field_short::val_str(String *val_buffer,String *val_ptr)
{
  uint length;
  val_buffer->alloc(max(field_length+1,7));
  char *to=(char*) val_buffer->ptr();
  short j; shortget(j,ptr);

  if (unsigned_flag)
    length=(uint) (int2str((long) (uint16) j,to,10)-to);
  else
    length=(uint) (int2str((long) j,to,-10)-to);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_short::cmp(const char *a_ptr, const char *b_ptr)
{
  short a,b;
  shortget(a,a_ptr);
  shortget(b,b_ptr);

  if (unsigned_flag)
    return ((unsigned short) a < (unsigned short) b) ? -1 :
    ((unsigned short) a > (unsigned short) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_short::sort_string(char *to,uint length)
{
#ifdef WORDS_BIGENDIAN
  if (unsigned_flag)
    to[0] = ptr[0];
  else
    to[0] = ptr[0] ^ 128;			/* Revers signbit */
  to[1]   = ptr[1];
#else
  if (unsigned_flag)
    to[0] = ptr[1];
  else
    to[0] = ptr[1] ^ 128;			/* Revers signbit */
  to[1]   = ptr[0];
#endif
}

void Field_short::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"smallint(%d)",field_length);
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
** medium int
****************************************************************************/

// Note:  Sometimes this should be fixed to use one strtol() to use
// len and check for garbage after number.

void Field_medium::store(const char *from,uint len)
{
  String tmp_str(from,len);
  long tmp= strtol(tmp_str.c_ptr(),NULL,10);

  if (unsigned_flag)
  {
    if (tmp < 0)
    {
      tmp=0;
      current_thd->cuted_fields++;
    }
    else if (tmp >= (long) (1L << 24))
    {
      tmp=(long) (1L << 24)-1L;
      current_thd->cuted_fields++;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len))
      current_thd->cuted_fields++;
  }
  else
  {
    if (tmp < INT_MIN24)
    {
      tmp= INT_MIN24;
      current_thd->cuted_fields++;
    }
    else if (tmp > INT_MAX24)
    {
      tmp=INT_MAX24;
      current_thd->cuted_fields++;
    }
    else if (current_thd->count_cuted_fields && !test_if_int(from,len))
      current_thd->cuted_fields++;
  }

  int3store(ptr,tmp);
}


void Field_medium::store(double nr)
{
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      int3store(ptr,0);
      current_thd->cuted_fields++;
    }
    else if (nr >= (double) (long) (1L << 24))
    {
      ulong tmp=(ulong) (1L << 24)-1L;
      int3store(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      int3store(ptr,(ulong) nr);
  }
  else
  {
    if (nr < (double) INT_MIN24)
    {
      long tmp=(long) INT_MIN24;
      int3store(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else if (nr > (double) INT_MAX24)
    {
      long tmp=(long) INT_MAX24;
      int3store(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      int3store(ptr,(long) nr);
  }
}

void Field_medium::store(longlong nr)
{
  if (unsigned_flag)
  {
    if (nr < 0L)
    {
      int3store(ptr,0);
      current_thd->cuted_fields++;
    }
    else if (nr >= (longlong) (long) (1L << 24))
    {
      long tmp=(long) (1L << 24)-1L;;
      int3store(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      int3store(ptr,(ulong) nr);
  }
  else
  {
    if (nr < (longlong) INT_MIN24)
    {
      long tmp=(long) INT_MIN24;
      int3store(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else if (nr > (longlong) INT_MAX24)
    {
      long tmp=(long) INT_MAX24;
      int3store(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      int3store(ptr,(long) nr);
  }
}


double Field_medium::val_real(void)
{
  long j= unsigned_flag ? (long) uint3korr(ptr) : sint3korr(ptr);
  return (double) j;
}

longlong Field_medium::val_int(void)
{
  long j= unsigned_flag ? (long) uint3korr(ptr) : sint3korr(ptr);
  return (longlong) j;
}

String *Field_medium::val_str(String *val_buffer,String *val_ptr)
{
  uint length;
  val_buffer->alloc(max(field_length+1,10));
  char *to=(char*) val_buffer->ptr();
  long j= unsigned_flag ? (long) uint3korr(ptr) : sint3korr(ptr);

  length=(uint) (int2str(j,to,-10)-to);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer); /* purecov: inspected */
  return val_buffer;
}


int Field_medium::cmp(const char *a_ptr, const char *b_ptr)
{
  long a,b;
  if (unsigned_flag)
  {
    a=uint3korr(a_ptr);
    b=uint3korr(b_ptr);
  }
  else
  {
    a=sint3korr(a_ptr);
    b=sint3korr(b_ptr);
  }
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_medium::sort_string(char *to,uint length)
{
  if (unsigned_flag)
    to[0] = ptr[2];
  else
    to[0] = (uchar) (ptr[2] ^ 128);		/* Revers signbit */
  to[1] = ptr[1];
  to[2] = ptr[0];
}


void Field_medium::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"mediumint(%d)",field_length);
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** long int
****************************************************************************/


// Note:  Sometimes this should be fixed to use one strtol() to use
// len and check for garbage after number.

void Field_long::store(const char *from,uint len)
{
  while (len && isspace(*from))
  {
    len--; from++;
  }
  long tmp;
  String tmp_str(from,len);
  errno=0;
  if (unsigned_flag)
  {
    if (!len || *from == '-')
    {
      tmp=0;					// Set negative to 0
      errno=ERANGE;
    }
    else
      tmp=(long) strtoul(tmp_str.c_ptr(),NULL,10);
  }
  else
    tmp=strtol(tmp_str.c_ptr(),NULL,10);
  if (errno || current_thd->count_cuted_fields && !test_if_int(from,len))
    current_thd->cuted_fields++;
  longstore(ptr,tmp);
}


void Field_long::store(double nr)
{
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      longstore(ptr,0);
      current_thd->cuted_fields++;
    }
    else if (nr > (double) (ulong) ~0L)
    {
      ulong tmp=(ulong) ~0L;
      longstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      longstore(ptr,(ulong) nr);
  }
  else
  {
    if (nr < (double) INT_MIN32)
    {
      long tmp=(long) INT_MIN32;
      longstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else if (nr > (double) INT_MAX32)
    {
      long tmp=(long) INT_MAX32;
      longstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      longstore(ptr,(long) nr);
  }
}


void Field_long::store(longlong nr)
{
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      longstore(ptr,0);
      current_thd->cuted_fields++;
    }
    else if (nr > (longlong) (uint32) ~0L)
    {
      uint32 tmp=(uint32) ~0L;
      longstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      longstore(ptr,(uint32) nr);
  }
  else
  {
    if (nr < (longlong) INT_MIN32)
    {
      int32 tmp=(int32) INT_MIN32;
      longstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else if (nr > (longlong) INT_MAX32)
    {
      int32 tmp=(int32) INT_MAX32;
      longstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
      longstore(ptr,(int32) nr);
  }
}


double Field_long::val_real(void)
{
  int32 j;
  longget(j,ptr);
  return unsigned_flag ? (double) (uint32) j : (double) j;
}

longlong Field_long::val_int(void)
{
  int32 j;
  longget(j,ptr)
  return unsigned_flag ? (longlong) (uint32) j : (longlong) j;
}

String *Field_long::val_str(String *val_buffer,String *val_ptr)
{
  uint length;
  val_buffer->alloc(max(field_length+1,12));
  char *to=(char*) val_buffer->ptr();
  int32 j; longget(j,ptr);

  length=(uint) (int2str((unsigned_flag ? (long) (uint32) j : (long) j),
			 to,
			 unsigned_flag ? 10 : -10)-to);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_long::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
  longget(a,a_ptr);
  longget(b,b_ptr);

  if (unsigned_flag)
    return ((ulong) a < (ulong) b) ? -1 : ((ulong) a > (ulong) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_long::sort_string(char *to,uint length)
{
#ifdef WORDS_BIGENDIAN
  if (unsigned_flag)
    to[0] = ptr[0];
  else
    to[0] = ptr[0] ^ 128;			/* Revers signbit */
  to[1]   = ptr[1];
  to[2]   = ptr[2];
  to[3]   = ptr[3];
#else
  if (unsigned_flag)
    to[0] = ptr[3];
  else
    to[0] = ptr[3] ^ 128;			/* Revers signbit */
  to[1]   = ptr[2];
  to[2]   = ptr[1];
  to[3]   = ptr[0];
#endif
}


void Field_long::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"int(%d)",field_length);
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** longlong int
****************************************************************************/

void Field_longlong::store(const char *from,uint len)
{
  while (len && isspace(*from))
  {						// For easy error check
    len--; from++;
  }
  longlong tmp;
  String tmp_str(from,len);
  errno=0;
  if (unsigned_flag)
  {
    if (!len || *from == '-')
    {
      tmp=0;					// Set negative to 0
      errno=ERANGE;
    }
    else
      tmp=(longlong) strtoull(tmp_str.c_ptr(),NULL,10);
  }
  else
    tmp=strtoll(tmp_str.c_ptr(),NULL,10);
  if (errno || current_thd->count_cuted_fields && !test_if_int(from,len))
    current_thd->cuted_fields++;
  longlongstore(ptr,tmp);
}


void Field_longlong::store(double nr)
{
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      bzero(ptr,pack_length());
      current_thd->cuted_fields++;
    }
    else if (nr > (double) ~ (ulonglong) 0)
    {
      bfill(ptr,pack_length(),(char) 255);
      current_thd->cuted_fields++;
    }
    else
    {
      ulonglong tmp=(ulonglong) nr;
      longlongstore(ptr,tmp);
    }
  }
  else
  {
    if (nr <= (double) LONGLONG_MIN)
    {
      longlong tmp=(longlong) LONGLONG_MIN;
      longlongstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else if (nr >= (double) LONGLONG_MAX)
    {
      longlong tmp=(longlong) LONGLONG_MAX;
      longlongstore(ptr,tmp);
      current_thd->cuted_fields++;
    }
    else
    {
      longlong tmp=(longlong) nr;
      longlongstore(ptr,tmp);
    }
  }
}


void Field_longlong::store(longlong nr)
{
  longlongstore(ptr,nr);
}


double Field_longlong::val_real(void)
{
  longlong j;
  longlongget(j,ptr);
  return unsigned_flag ? ulonglong2double(j) : (double) j;
}

longlong Field_longlong::val_int(void)
{
  longlong j;
  longlongget(j,ptr);
  return j;
}


String *Field_longlong::val_str(String *val_buffer,String *val_ptr)
{
  uint length;
  val_buffer->alloc(max(field_length+1,22));
  char *to=(char*) val_buffer->ptr();
  longlong j; longlongget(j,ptr);

  length=(uint) (longlong2str(j,to,unsigned_flag ? 10 : -10)-to);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_longlong::cmp(const char *a_ptr, const char *b_ptr)
{
  longlong a,b;
  longlongget(a,a_ptr);
  longlongget(b,b_ptr);

  if (unsigned_flag)
    return ((ulonglong) a < (ulonglong) b) ? -1 :
    ((ulonglong) a > (ulonglong) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_longlong::sort_string(char *to,uint length)
{
#ifdef WORDS_BIGENDIAN
  if (unsigned_flag)
    to[0] = ptr[0];
  else
    to[0] = ptr[0] ^ 128;			/* Revers signbit */
  to[1]   = ptr[1];
  to[2]   = ptr[2];
  to[3]   = ptr[3];
  to[4]   = ptr[4];
  to[5]   = ptr[5];
  to[6]   = ptr[6];
  to[7]   = ptr[7];
#else
  if (unsigned_flag)
    to[0] = ptr[7];
  else
    to[0] = ptr[7] ^ 128;			/* Revers signbit */
  to[1]   = ptr[6];
  to[2]   = ptr[5];
  to[3]   = ptr[4];
  to[4]   = ptr[3];
  to[5]   = ptr[2];
  to[6]   = ptr[1];
  to[7]   = ptr[0];
#endif
}


void Field_longlong::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"bigint(%d)",field_length);
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** single precision float
****************************************************************************/

void Field_float::store(const char *from,uint len)
{
  String tmp_str(from,len);
  errno=0;
  Field_float::store(atof(tmp_str.c_ptr()));
  if (errno || current_thd->count_cuted_fields && !test_if_real(from,len))
    current_thd->cuted_fields++;
}


void Field_float::store(double nr)
{
  if (decimals != 31)
    nr=floor(nr*log_10[decimals]+0.5)/log_10[decimals]; // To fixed point
  float j;
  if (nr < -FLT_MAX)
  {
    j= -FLT_MAX;
    current_thd->cuted_fields++;
  }
  else if (nr > FLT_MAX)
  {
    j=FLT_MAX;
    current_thd->cuted_fields++;
  }
  else
    j= (float) nr;
  memcpy(ptr,(byte*) &j,sizeof(j));
}


void Field_float::store(longlong nr)
{
  float j= (float) nr;
  memcpy(ptr,(byte*) &j,sizeof(j));
}


double Field_float::val_real(void)
{
  float j; memcpy((byte*) &j,ptr,sizeof(j));
  return ((double) j);
}

longlong Field_float::val_int(void)
{
  float j; memcpy((byte*) &j,ptr,sizeof(j));
  return ((longlong) j);
}


String *Field_float::val_str(String *val_buffer,String *val_ptr)
{
  float j;
  memcpy((byte*) &j,(char*) ptr,sizeof(j));
  val_buffer->alloc(max(field_length,70));
  char *to=(char*) val_buffer->ptr();

#ifdef HAVE_FCONVERT
  char buff[70],*pos=buff;
  int decpt,sign,dec=decimals;

  VOID(sfconvert(&j,dec,&decpt,&sign,buff));
  if (sign)
  {
    *to++='-';
  }
  if (decpt < 0)
  {					/* val_buffer is < 0 */
    *to++='0';
    if (!dec)
      goto end;
    *to++='.';
    if (-decpt > dec)
      decpt= - (int) dec;
    dec=(uint) ((int) dec+decpt);
    while (decpt++ < 0)
      *to++='0';
  }
  else if (decpt == 0)
  {
    *to++= '0';
    if (!dec)
      goto end;
    *to++='.';
  }
  else
  {
    while (decpt-- > 0)
      *to++= *pos++;
    if (!dec)
      goto end;
    *to++='.';
  }
  while (dec--)
    *to++= *pos++;
end:
#else
  sprintf(to,"%.*lf",decimals,j);
  to=strend(to);
#endif

  val_buffer->length((uint) (to-val_buffer->ptr()));
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_float::cmp(const char *a_ptr, const char *b_ptr)
{
  float a,b;
  memcpy(&a,a_ptr,sizeof(float));
  memcpy(&b,b_ptr,sizeof(float));
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

#define FLT_EXP_DIG (sizeof(float)*8-FLT_MANT_DIG)

void Field_float::sort_string(char *to,uint length)
{
  float nr;
  memcpy(&nr,ptr,sizeof(nr));

  uchar *tmp= (uchar*) to;
  if (nr == (float) 0.0)
  {						/* Change to zero string */
    tmp[0]=(uchar) 128;
    bzero((char*) tmp+1,sizeof(nr)-1);
  }
  else
  {
#ifdef WORDS_BIGENDIAN
    memcpy(tmp,&nr,sizeof(nr));
#else
    tmp[0]= ptr[3]; tmp[1]=ptr[2]; tmp[2]= ptr[1]; tmp[3]=ptr[0];
#endif
    if (tmp[0] & 128)				/* Negative */
    {						/* make complement */
      uint i;
      for (i=0 ; i < sizeof(nr); i++)
	tmp[i]=tmp[i] ^ (uchar) 255;
    }
    else
    {
      ushort exp=((ushort) tmp[0] << 8) | (ushort) tmp[1] | (ushort) 32768;
      exp+= (ushort) 1 << (16-1-FLT_EXP_DIG);
      tmp[0]= (uchar) (exp >> 8);
      tmp[1]= (uchar) exp;
    }
  }
}


void Field_float::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"float(%d,%d)",field_length,decimals);
  add_zerofill_and_unsigned(res);
}

/****************************************************************************
** double precision floating point numbers
****************************************************************************/

void Field_double::store(const char *from,uint len)
{
  String tmp_str(from,len);
  errno=0;
  double j= atof(tmp_str.c_ptr());
  if (errno || current_thd->count_cuted_fields && !test_if_real(from,len))
    current_thd->cuted_fields++;
  doublestore(ptr,j);
}


void Field_double::store(double nr)
{
  if (decimals != 31)
    nr=floor(nr*log_10[decimals]+0.5)/log_10[decimals]; // To fixed point
  doublestore(ptr,nr);
}


void Field_double::store(longlong nr)
{
  double j= (double) nr;
  doublestore(ptr,j);
}


double Field_double::val_real(void)
{
  double j; doubleget(j,ptr);
  return j;
}

longlong Field_double::val_int(void)
{
  double j; doubleget(j,ptr);
  return ((longlong) j);
}


String *Field_double::val_str(String *val_buffer,String *val_ptr)
{
  double j;
  doubleget(j,ptr);
  val_buffer->alloc(max(field_length,320));
  char *to=(char*) val_buffer->ptr();

#ifdef HAVE_FCONVERT
  char buff[320],*pos=buff;
  int decpt,sign,dec=decimals;

  VOID(fconvert(j,dec,&decpt,&sign,buff));
  if (sign)
  {
    *to++='-';
  }
  if (decpt < 0)
  {					/* val_buffer is < 0 */
    *to++='0';
    if (!dec)
      goto end;
    *to++='.';
    if (-decpt > dec)
      decpt= - (int) dec;
    dec=(uint) ((int) dec+decpt);
    while (decpt++ < 0)
      *to++='0';
  }
  else if (decpt == 0)
  {
    *to++= '0';
    if (!dec)
      goto end;
    *to++='.';
  }
  else
  {
    while (decpt-- > 0)
      *to++= *pos++;
    if (!dec)
      goto end;
    *to++='.';
  }
  while (dec--)
    *to++= *pos++;
 end:
#else
  sprintf(to,"%.*lf",decimals,j);
  to=strend(to);
#endif

  val_buffer->length((uint) (to-val_buffer->ptr()));
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


int Field_double::cmp(const char *a_ptr, const char *b_ptr)
{
  double a,b;
  memcpy(&a,a_ptr,sizeof(double));
  memcpy(&b,b_ptr,sizeof(double));
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


#define DBL_EXP_DIG (sizeof(double)*8-DBL_MANT_DIG)

/* The following should work for IEEE */

void Field_double::sort_string(char *to,uint length)
{
  double nr;
  memcpy(&nr,ptr,sizeof(nr));
  change_double_for_sort(nr, (byte*) to);
}


void Field_double::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"double(%d,%d)",field_length,decimals);
  add_zerofill_and_unsigned(res);
}


/****************************************************************************
** timestamp
** The first timestamp in the table is automaticly updated
** by handler.cc.  The form->timestamp points at the automatic timestamp.
****************************************************************************/

Field_timestamp::Field_timestamp(char *ptr_arg, uint32 len_arg,
				 enum utype unireg_check_arg,
				 char *field_name_arg,
				 struct st_table *table_arg)
    :Field_num(ptr_arg, len_arg, (uchar*) 0,0,
	       unireg_check_arg, field_name_arg, table_arg,
	       0, 1, 1)
{
  if (table && !table->timestamp_field)
  {
    table->timestamp_field= this;		// Automatic timestamp
    table->time_stamp=(uint) (ptr_arg - (char*) table->record[0])+1;
    flags|=TIMESTAMP_FLAG;
  }
}


void Field_timestamp::store(const char *from,uint len)
{
  long tmp=(long) str_to_timestamp(from,len);
  longstore(ptr,tmp);
}

void Field_timestamp::fill_and_store(char *from,uint len)
{
  uint res_length;
  if (len <= field_length)
    res_length=field_length;
  else if (len <= 12)
    res_length=12;				/* purecov: inspected */
  else if (len <= 14)
    res_length=14;				/* purecov: inspected */
  else
    res_length=(len+1)/2*2;			// must be even
  if (res_length != len)
  {
    bmove_upp(from+res_length,from+len,len);
    bfill(from,res_length-len,'0');
    len=res_length;
  }
  long skr=(long) str_to_timestamp(from,len);
  longstore(ptr,skr);
}


void Field_timestamp::store(double nr)
{
  if (nr < 0 || nr > 99991231235959.0)
  {
    nr=0;					// Avoid overflow on buff
    current_thd->cuted_fields++;
  }
  Field_timestamp::store((longlong) rint(nr));
}


/*
** Convert a datetime of formats YYMMDD, YYYYMMDD or YYMMDDHHMSS to
** YYYYMMDDHHMMSS.  The high date '99991231235959' is checked before this
** function.
*/

static longlong fix_datetime(longlong nr)
{
  if (nr == LL(0) || nr >= LL(10000101000000))
    return nr;					// Normal datetime >= Year 1000
  if (nr < 101)
    goto err;
  if (nr <= 691231L)
    return (nr+20000000L)*1000000L;		// YYMMDD, year: 2000-2069
  if (nr < 700101L)
    goto err;
  if (nr <= 991231L)
    return (nr+19000000L)*1000000L;		// YYMMDD, year: 1970-1999
  if (nr < 10000101L)
    goto err;
  if (nr <= 99991231L)
    return nr*1000000L;
  if (nr < 101000000L)
    goto err;
  if (nr <= LL(691231235959))
    return nr+LL(20000000000000);		// YYMMDDHHMMSS, 2000-2069
  if (nr < LL(700101000000))
    goto err;
  if (nr <= LL(991231235959))
    return nr+LL(19000000000000);		// YYMMDDHHMMSS, 1970-1999

 err:
  current_thd->cuted_fields++;
  return LL(0);
}


void Field_timestamp::store(longlong nr)
{
  char buff[22];
  fill_and_store(buff,longlong2str(fix_datetime(nr),buff,10)-buff);
}


double Field_timestamp::val_real(void)
{
  return (double) Field_timestamp::val_int();
}

longlong Field_timestamp::val_int(void)
{
  uint len,pos;
  int part_time;
  uint32 temp;
  time_t time_arg;
  struct tm *l_time;
  longlong res;
  struct tm tm_tmp;

  longget(temp,ptr);
  if (temp == 0L)				// No time
    return(0);					/* purecov: inspected */
  time_arg=(time_t) temp;
  localtime_r(&time_arg,&tm_tmp);
  l_time=&tm_tmp;
  res=(longlong) 0;
  for (pos=len=0; len+1 < (uint) field_length ; len+=2,pos++)
  {
    bool year_flag=0;
    switch (dayord.pos[pos]) {
    case 0: part_time=l_time->tm_year % 100; year_flag=1 ; break;
    case 1: part_time=l_time->tm_mon+1; break;
    case 2: part_time=l_time->tm_mday; break;
    case 3: part_time=l_time->tm_hour; break;
    case 4: part_time=l_time->tm_min; break;
    case 5: part_time=l_time->tm_sec; break;
    default: part_time=0; break; /* purecov: deadcode */
    }
    if (year_flag && (field_length == 8 || field_length == 14))
    {
      res=res*(longlong) 10000+(part_time+((part_time < 70) ? 2000 : 1900));
      len+=2;
    }
    else
      res=res*(longlong) 100+part_time;
  }
  return (longlong) res;
}


String *Field_timestamp::val_str(String *val_buffer,String *val_ptr)
{
  uint pos;
  int part_time;
  uint32 temp;
  time_t time_arg;
  struct tm *l_time;
  struct tm tm_tmp;

  val_buffer->alloc(field_length+1);
  char *to=(char*) val_buffer->ptr(),*end=to+field_length;
  longget(temp,ptr);
  if (temp == 0L)
  {				      /* Zero time is "000000" */
    VOID(strfill(to,field_length,'0'));
    val_buffer->length(field_length);
    return val_buffer;
  }
  time_arg=(time_t) temp;
  localtime_r(&time_arg,&tm_tmp);
  l_time=&tm_tmp;
  for (pos=0; to < end ; pos++)
  {
    bool year_flag=0;
    switch (dayord.pos[pos]) {
    case 0: part_time=l_time->tm_year % 100; year_flag=1; break;
    case 1: part_time=l_time->tm_mon+1; break;
    case 2: part_time=l_time->tm_mday; break;
    case 3: part_time=l_time->tm_hour; break;
    case 4: part_time=l_time->tm_min; break;
    case 5: part_time=l_time->tm_sec; break;
    default: part_time=0; break; /* purecov: deadcode */
    }
    if (year_flag && (field_length == 8 || field_length == 14))
    {
      if (part_time < 70)
      {
	*to++='2'; *to++='0'; /* purecov: inspected */
      }
      else
      {
	*to++='1'; *to++='9';
      }
    }
    *to++=(char) ('0'+((uint) part_time/10));
    *to++=(char) ('0'+((uint) part_time % 10));
  }
  *to=0;					// Safeguard
  val_buffer->length((uint) (to-val_buffer->ptr()));
  return val_buffer;
}


int Field_timestamp::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
  longget(a,a_ptr);
  longget(b,b_ptr);
  return ((uint32) a < (uint32) b) ? -1 : ((uint32) a > (uint32) b) ? 1 : 0;
}

void Field_timestamp::sort_string(char *to,uint length)
{
#ifdef WORDS_BIGENDIAN
  to[0] = ptr[0];
  to[1] = ptr[1];
  to[2] = ptr[2];
  to[3] = ptr[3];
#else
  to[0] = ptr[3];
  to[1] = ptr[2];
  to[2] = ptr[1];
  to[3] = ptr[0];
#endif
}


void Field_timestamp::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"timestamp(%d)",field_length);
  res.length(strlen(res.ptr()));
}

void Field_timestamp::set_time()
{
  long skr= current_thd->query_start();
  longstore(ptr,skr);
}

/****************************************************************************
** time type
** In string context: HH:MM:SS
** In number context: HHMMSS
** Stored as a 3 byte unsigned int
****************************************************************************/

void Field_time::store(const char *from,uint len)
{
  TIME ltime;
  long tmp;
  if (str_to_time(from,len,&ltime))
    tmp=0L;
  else
  {
    tmp=(ltime.day*24L+ltime.hour*10000L)+(ltime.minute*100+ltime.second);
    if (tmp > 8385959)
    {
      tmp=8385959;
      current_thd->cuted_fields++;
    }
  }
  if (ltime.neg)
    tmp= -tmp;
  Field_time::store((longlong) tmp);
}


void Field_time::store(double nr)
{
  long tmp;
  if (nr > 8385959.0)
  {
    tmp=8385959L;
    current_thd->cuted_fields++;
  }
  else if (nr < -8385959.0)
  {
    tmp= -8385959L;
    current_thd->cuted_fields++;
  }
  else
  {
    tmp=(long) rint(nr);
    if (tmp % 100 > 59 || tmp/100 % 100 > 59)
    {
      tmp=0;
      current_thd->cuted_fields++;
    }
  }
  int3store(ptr,tmp);
}


void Field_time::store(longlong nr)
{
  long tmp;
  if (nr > (longlong) 8385959L)
  {
    tmp=8385959L;
    current_thd->cuted_fields++;
  }
  else if (nr < (longlong) -8385959L)
  {
    tmp= -8385959L;
    current_thd->cuted_fields++;
  }
  else
  {
    tmp=(long) nr;
    if (tmp % 100 > 59 || tmp/100 % 100 > 59)
    {
      tmp=0;
      current_thd->cuted_fields++;
    }
  }
  int3store(ptr,tmp);
}


double Field_time::val_real(void)
{
  ulong j= (ulong) uint3korr(ptr);
  return (double) j;
}

longlong Field_time::val_int(void)
{
  return (longlong) sint3korr(ptr);
}

String *Field_time::val_str(String *val_buffer,String *val_ptr)
{
  val_buffer->alloc(16);
  long tmp=(long) sint3korr(ptr);
  char *sign="";
  if (tmp < 0)
  {
    tmp= -tmp;
    sign= "-";
  }
  sprintf((char*) val_buffer->ptr(),"%s%02d:%02d:%02d",
	  sign,(int) tmp/10000, (int) (tmp/100 % 100),
	  (int) (tmp % 100));
  val_buffer->length(strlen(val_buffer->ptr()));
  return val_buffer;
}


int Field_time::cmp(const char *a_ptr, const char *b_ptr)
{
  long a,b;
  a=(long) sint3korr(a_ptr);
  b=(long) sint3korr(b_ptr);
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_time::sort_string(char *to,uint length)
{
  to[0] = (uchar) (ptr[2] ^ 128);
  to[1] = ptr[1];
  to[2] = ptr[0];
}

void Field_time::sql_type(String &res) const
{
  res.set("time",4);
}

/****************************************************************************
** year type
** Save in a byte the year 0, 1901->2155
** Can handle 2 byte or 4 byte years!
****************************************************************************/

void Field_year::store(const char *from, uint len)
{
  String tmp_str(from,len);
  long nr= strtol(tmp_str.c_ptr(),NULL,10);

  if (nr < 0 || nr >= 100 && nr <= 1900 || nr > 2155)
  {
    *ptr=0;
    current_thd->cuted_fields++;
    return;
  }
  else if (current_thd->count_cuted_fields && !test_if_int(from,len))
    current_thd->cuted_fields++;
  if (nr != 0 || len != 4)
  {
    if (nr < 70)
      nr+=100;					// 2000 - 2069
    else if (nr > 1900)
      nr-= 1900;
  }
  *ptr= (char) (unsigned char) nr;
}

void Field_year::store(double nr)
{
  if (nr < 0.0 || nr >= 2155.0)
    Field_year::store((longlong) -1);
  else
    Field_year::store((longlong) nr);
}

void Field_year::store(longlong nr)
{
  if (nr < 0 || nr >= 100 && nr <= 1900 || nr > 2155)
  {
    *ptr=0;
    current_thd->cuted_fields++;
    return;
  }
  if (nr != 0 || field_length != 4)		// 0000 -> 0; 00 -> 2000
  {
    if (nr < 70)
      nr+=100;					// 2000 - 2069
    else if (nr > 1900)
      nr-= 1900;
  }
  *ptr= (char) (unsigned char) nr;
}


double Field_year::val_real(void)
{
  return (double) Field_year::val_int();
}

longlong Field_year::val_int(void)
{
  int tmp= (int) ((uchar*) ptr)[0];
  if (field_length != 4)
    tmp%=100;					// Return last 2 char
  else if (tmp)
    tmp+=1900;
  return (longlong) tmp;
}

String *Field_year::val_str(String *val_buffer,String *val_ptr)
{
  val_buffer->alloc(5);
  val_buffer->length(field_length);
  char *to=(char*) val_buffer->ptr();
  sprintf(to,field_length == 2 ? "%02d" : "%04d",(int) Field_year::val_int());
  return val_buffer;
}

void Field_year::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"year(%d)",field_length);
  res.length(strlen(res.ptr()));
}


/****************************************************************************
** date type
** In string context: YYYY-MM-DD
** In number context: YYYYMMDD
** Stored as a 4 byte unsigned int
****************************************************************************/

void Field_date::store(const char *from,uint len)
{
  TIME l_time;
  ulong tmp;
  if (str_to_TIME(from,len,&l_time) == TIMESTAMP_NONE)
    tmp=0;
  else
    tmp=(ulong) l_time.year*10000L + (ulong) (l_time.month*100+l_time.day);
  longstore(ptr,tmp);
}


void Field_date::store(double nr)
{
  long tmp;
  if (nr > 19000000000000.0 && nr <= 99991231235959.0)
    nr=floor(nr/1000000.0);			// Timestamp to date
  if (nr < 0.0 || nr > 99991231.0)
  {
    tmp=0L;
    current_thd->cuted_fields++;
  }
  else
    tmp=(long) rint(nr);
  longstore(ptr,tmp);
}


void Field_date::store(longlong nr)
{
  long tmp;
  if (nr > LL(19000000000000) && nr < LL(99991231235959))
    nr=nr/LL(1000000);			// Timestamp to date
  if (nr < 0 || nr > LL(99991231))
  {
    tmp=0L;
    current_thd->cuted_fields++;
  }
  else
    tmp=(long) nr;
  longstore(ptr,tmp);
}


double Field_date::val_real(void)
{
  int32 j;
  longget(j,ptr);
  return (double) (uint32) j;
}

longlong Field_date::val_int(void)
{
  int32 j;
  longget(j,ptr);
  return (longlong) (uint32) j;
}

String *Field_date::val_str(String *val_buffer,String *val_ptr)
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  int32 tmp;
  longget(tmp,ptr);
  sprintf((char*) val_buffer->ptr(),"%04d-%02d-%02d",
	  (int) ((uint32) tmp/10000L % 10000), (int) ((uint32) tmp/100 % 100),
	  (int) ((uint32) tmp % 100));
  return val_buffer;
}

int Field_date::cmp(const char *a_ptr, const char *b_ptr)
{
  int32 a,b;
  longget(a,a_ptr);
  longget(b,b_ptr);
  return ((uint32) a < (uint32) b) ? -1 : ((uint32) a > (uint32) b) ? 1 : 0;
}


void Field_date::sort_string(char *to,uint length)
{
#ifdef WORDS_BIGENDIAN
  to[0] = ptr[0];
  to[1] = ptr[1];
  to[2] = ptr[2];
  to[3] = ptr[3];
#else
  to[0] = ptr[3];
  to[1] = ptr[2];
  to[2] = ptr[1];
  to[3] = ptr[0];
#endif
}

void Field_date::sql_type(String &res) const
{
  res.set("date",4);
}

/****************************************************************************
** The new date type
** This is identical to the old date type, but stored on 3 bytes instead of 4
** In number context: YYYYMMDD
****************************************************************************/

void Field_newdate::store(const char *from,uint len)
{
  TIME l_time;
  long tmp;
  if (str_to_TIME(from,len,&l_time) == TIMESTAMP_NONE)
    tmp=0L;
  else
    tmp= l_time.day + l_time.month*32 + l_time.year*16*32;
  int3store(ptr,tmp);
}

void Field_newdate::store(double nr)
{
  if (nr < 0.0 || nr > 99991231235959.0)
    Field_newdate::store((longlong) -1);
  else
    Field_newdate::store((longlong) rint(nr));
}


void Field_newdate::store(longlong nr)
{
  long tmp;
  if (nr >= LL(100000000) && nr <= LL(99991231235959))
    nr=nr/LL(1000000);			// Timestamp to date
  if (nr < 0L || nr > 99991231L)
  {
    tmp=0;
    current_thd->cuted_fields++;
  }
  else
  {
    tmp=(long) nr;
    if (tmp)
    {
      if (tmp < 700000)				// Fix short dates
	tmp+=20000000;
      else if (tmp < 999999)
	tmp+=19000000;
    }
    uint month=((tmp/100) % 100);
    uint day= tmp%100;
    if (month > 12 || day > 31)
    {
      tmp=0L;					// Don't allow date to change
      current_thd->cuted_fields++;
    }
    else
      tmp= day + month*32 + (tmp/10000)*16*32;
  }
  int3store(ptr,tmp);
}


double Field_newdate::val_real(void)
{
  return (double) Field_newdate::val_int();
}

longlong Field_newdate::val_int(void)
{
  ulong j=uint3korr(ptr);
  j= (j % 32L)+(j / 32L % 16L)*100L + (j/(16L*32L))*10000L;
  return (longlong) j;
}

String *Field_newdate::val_str(String *val_buffer,String *val_ptr)
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  ulong tmp=(ulong) uint3korr(ptr);
  sprintf((char*) val_buffer->ptr(),"%04d-%02d-%02d",
	  (int) (tmp/(32*16L) % 10000), (int) (tmp/32 % 16),
	  (int) (tmp % 32));
  return val_buffer;
}


int Field_newdate::cmp(const char *a_ptr, const char *b_ptr)
{
  ulong a,b;
  a=(ulong) uint3korr(a_ptr);
  b=(ulong) uint3korr(b_ptr);
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_newdate::sort_string(char *to,uint length)
{
  to[0] = ptr[2];
  to[1] = ptr[1];
  to[2] = ptr[0];
}

void Field_newdate::sql_type(String &res) const
{
  res.set("date",4);
}


/****************************************************************************
** datetime type
** In string context: YYYY-MM-DD HH:MM:DD
** In number context: YYYYMMDDHHMMDD
** Stored as a 8 byte unsigned int. Should sometimes be change to a 6 byte int.
****************************************************************************/

void Field_datetime::store(const char *from,uint len)
{
  longlong tmp=str_to_datetime(from,len);
  longlongstore(ptr,tmp);
}


void Field_datetime::store(double nr)
{
  if (nr < 0.0 || nr > 99991231235959.0)
  {
    nr=0.0;
    current_thd->cuted_fields++;
  }
  Field_datetime::store((longlong) rint(nr));
}


void Field_datetime::store(longlong nr)
{
  if (nr < 0 || nr > LL(99991231235959))
  {
    nr=0;
    current_thd->cuted_fields++;
  }
  else
    nr=fix_datetime(nr);
  longlongstore(ptr,nr);
}


double Field_datetime::val_real(void)
{
  return (double) Field_datetime::val_int();
}

longlong Field_datetime::val_int(void)
{
  longlong j;
  longlongget(j,ptr);
  return j;
}

String *Field_datetime::val_str(String *val_buffer,String *val_ptr)
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  ulonglong tmp;
  longlongget(tmp,ptr);
 // We divide the year with 10000L to catch overflow errors
  sprintf((char*) val_buffer->ptr(),
	  "%04d-%02d-%02d %02d:%02d:%02d",
	  (int) ((ulong) (tmp/LL(10000000000) % 10000L)),
	  (int) ((tmp/LL(100000000)) % 100),
	  (int) ((tmp/LL(1000000)) % 100),
	  (int) ((tmp/LL(10000)) % 100),
	  (int) ((tmp/LL(100)) % 100),
	  (int) (tmp % LL(100)));
  return val_buffer;
}

int Field_datetime::cmp(const char *a_ptr, const char *b_ptr)
{
  longlong a,b;
  longlongget(a,a_ptr);
  longlongget(b,b_ptr);
  return ((ulonglong) a < (ulonglong) b) ? -1 :
    ((ulonglong) a > (ulonglong) b) ? 1 : 0;
}

void Field_datetime::sort_string(char *to,uint length)
{
#ifdef WORDS_BIGENDIAN
  to[0] = ptr[0];
  to[1] = ptr[1];
  to[2] = ptr[2];
  to[3] = ptr[3];
  to[4] = ptr[4];
  to[5] = ptr[5];
  to[6] = ptr[6];
  to[7] = ptr[7];
#else
  to[0] = ptr[7];
  to[1] = ptr[6];
  to[2] = ptr[5];
  to[3] = ptr[4];
  to[4] = ptr[3];
  to[5] = ptr[2];
  to[6] = ptr[1];
  to[7] = ptr[0];
#endif
}


void Field_datetime::sql_type(String &res) const
{
  res.set("datetime",8);
}

/****************************************************************************
** string type
** A string may be varchar or binary
****************************************************************************/

	/* Copy a string and fill with space */

void Field_string::store(const char *from,uint length)
{
#ifdef USE_TIS620
  if(!binary_flag) {
    ThNormalize((uchar *)ptr, field_length, (uchar *)from, length);
    if(length < field_length) {
      bfill(ptr + length, field_length - length, ' ');
    }
  }
#else
  if (length <= field_length)
  {
    memcpy(ptr,from,length);
    if (length < field_length)
      bfill(ptr+length,field_length-length,' ');
  }
  else
  {
    memcpy(ptr,from,field_length);
    if (current_thd->count_cuted_fields)
    {						// Check if we loosed some info
      const char *end=from+length;
      for (from+=field_length ; from != end ; from++)
      {
	if (!isspace(*from))
	{
	  current_thd->cuted_fields++;
	  break;
	}
      }
    }
  }
#endif /* USE_TIS620 */
}


void Field_string::store(double nr)
{
  char buff[MAX_FIELD_WIDTH];
  int width=min(field_length,DBL_DIG+5);
  sprintf(buff,"%-*.*g",width,max(width-5,0),nr);
  Field_string::store(buff,strlen(buff));
}


void Field_string::store(longlong nr)
{
  char buff[22];
  char *end=longlong2str(nr,buff,-10);
  Field_string::store(buff,end-buff);
}


double Field_string::val_real(void)
{
  double value;
  char save=ptr[field_length];			// Ok to patch record
  ptr[field_length]=0;
  value=atof(ptr);
  ptr[field_length]=save;
  return value;
}


longlong Field_string::val_int(void)
{
  longlong value;
  char save=ptr[field_length];			// Ok to patch record
  ptr[field_length]=0;
  value=strtoll(ptr,NULL,10);
  ptr[field_length]=save;
  return value;
}


String *Field_string::val_str(String *val_buffer,String *val_ptr)
{
  char *end=ptr+field_length;
  while (end > ptr && end[-1] == ' ')
    end--;
  val_ptr->set((const char*) ptr,(uint) (end - ptr));
  return val_ptr;
}


int Field_string::cmp(const char *a_ptr, const char *b_ptr)
{
  if (binary_flag)
    return memcmp(a_ptr,b_ptr,field_length);
  else
    return my_sortcmp(a_ptr,b_ptr,field_length);
}

void Field_string::sort_string(char *to,uint length)
{
  if (binary_flag)
    memcpy((byte*) to,(byte*) ptr,(size_t) field_length);
  else
#ifndef USE_STRCOLL
  {
    for (char *from=ptr,*end=ptr+field_length ; from != end ;)
    *to++=(char) my_sort_order[(uint) (uchar) *from++];
  }
#else
  {
    uint tmp=my_strnxfrm((unsigned char *)to,(unsigned char *)ptr,length,field_length);
    if (tmp < length)
      bzero(to+tmp,length-tmp);
  }
#endif
}


void Field_string::sql_type(String &res) const
{
  sprintf((char*) res.ptr(),"%s(%d)",
	  field_length > 3 &&
	  (table->db_create_options & HA_OPTION_PACK_RECORD) ?
	  "varchar" : "char",
	  field_length);
  res.length(strlen(res.ptr()));
  if (binary_flag)
    res.append(" binary");
}


/****************************************************************************
** blob type
** A blob is saved as a length and a pointer. The length is stored in the
** packlength slot and may be from 1-4.
****************************************************************************/

Field_blob::Field_blob(char *ptr_arg, uchar *null_ptr_arg, uint null_bit_arg,
		       enum utype unireg_check_arg, char *field_name_arg,
		       struct st_table *table_arg,uint blob_pack_length,
		       bool binary_arg)
  :Field_str(ptr_arg, (1L << min(blob_pack_length,3)*8)-1L,
	     null_ptr_arg, null_bit_arg, unireg_check_arg, field_name_arg,
	     table_arg),
   packlength(blob_pack_length),binary_flag(binary_arg)
{
  flags|= BLOB_FLAG;
  if (binary_arg)
    flags|=BINARY_FLAG;
  if (table)
    table->blob_fields++;
}


void Field_blob::store_length(ulong number)
{
  switch (packlength) {
  case 1:
    if (number > 255)
    {
      number=255;
      current_thd->cuted_fields++;
    }
    ptr[0]= (uchar) number;
    break;
  case 2:
    if (number > (uint16) ~0)
    {
      number= (uint16) ~0;
      current_thd->cuted_fields++;
    }
    shortstore(ptr,(unsigned short) number);
    break;
  case 3:
    if (number > (ulong) (1L << 24))
    {
      number= (ulong) (1L << 24)-1L;
      current_thd->cuted_fields++;
    }
    int3store(ptr,number);
    break;
  case 4:
    longstore(ptr,number);
  }
}

ulong Field_blob::get_length(void)
{
  switch (packlength) {
  case 1:
    return (ulong) (uchar) ptr[0];
  case 2:
    {
      uint16 tmp; shortget(tmp,ptr);
      return (ulong) tmp;
    }
  case 3:
    return (ulong) uint3korr(ptr);
  case 4:
    {
      uint32 tmp; longget(tmp,ptr);
      return (ulong) tmp;
    }
  }
  return 0;					// Impossible
}


void Field_blob::store(const char *from,uint len)
{
  if (!len)
  {
    bzero(ptr,Field_blob::pack_length());
  }
  else
  {
#ifdef USE_TIS620
    char *th_ptr=0;
#endif
    Field_blob::store_length(len);
    if (table->copy_blobs)
    {						// Must make a copy
#ifdef USE_TIS620
      if(!binary_flag)
      {
	/* If there isn't enough memory, use original string */
	if ((th_ptr=(char * ) my_malloc(sizeof(char) * len,MYF(0))))
	{
	  ThNormalize((uchar *) th_ptr, len, (uchar *) from, len);
	  from= (const char*) th_ptr;
	}
      }
#endif /* USE_TIS620 */
      value.set(from,len);
      value.copy();
      from=value.ptr();
    }
    bmove(ptr+packlength,(char*) &from,sizeof(char*));
#ifdef USE_TIS620
    my_free(th_ptr,MYF(MY_ALLOW_ZERO_PTR));
#endif
  }
}


void Field_blob::store(double nr)
{
  value.set(nr);
  Field_blob::store(value.ptr(),value.length());
}


void Field_blob::store(longlong nr)
{
  value.set(nr);
  Field_blob::store(value.ptr(),value.length());
}


double Field_blob::val_real(void)
{
  char *blob;

  memcpy(&blob,ptr+packlength,sizeof(char*));
  if (!blob)
    return 0.0;
  ulong length=get_length();

  char save=blob[length];			// Ok to patch blob in NISAM
  blob[length]=0;
  double nr=atof(blob);
  blob[length]=save;
  return nr;
}


longlong Field_blob::val_int(void)
{
  char *blob;
  memcpy(&blob,ptr+packlength,sizeof(char*));
  if (!blob)
    return 0;
  ulong length=get_length();

  char save=blob[length];			// Ok to patch blob in NISAM
  blob[length]=0;
  longlong nr=strtoll(blob,NULL,10);
  blob[length]=save;
  return nr;
}


String *Field_blob::val_str(String *val_buffer,String *val_ptr)
{
  char *blob;
  memcpy(&blob,ptr+packlength,sizeof(char*));
  if (!blob)
    val_ptr->length(0);
  else
    val_ptr->set((const char*) blob,get_length());
  return val_ptr;
}


int Field_blob::cmp(const char *a_ptr, const char *b_ptr)
{
  return 0;					// Never used
}

void Field_blob::sort_string(char *to,uint length)
{
  char *blob;
  uint blob_length=get_length();
#ifdef USE_STRCOLL
  uint blob_org_length=blob_length;
#endif
  if (!blob_length)
    bzero(to,length);
  else
  {
    if (blob_length > length)
      blob_length=length;
    memcpy(&blob,ptr+packlength,sizeof(char*));
    if (binary_flag)
    {
      memcpy(to,blob,blob_length);
      to+=blob_length;
    }
    else
#ifndef USE_STRCOLL
    {
      for (char *end=blob+blob_length ; blob != end ;)
	*to++=(char) my_sort_order[(uint) (uchar) *blob++];
    }
#else
    {
      blob_length=my_strnxfrm((unsigned char *)to,(unsigned char *)blob,length,blob_org_length);
      if (blob_length >= length)
	return;
      to+=blob_length;
    }
#endif
    bzero(to,length-blob_length);
  }
}


void Field_blob::sql_type(String &res) const
{
  char *str;
  switch (packlength) {
  default: str="tiny"; break;
  case 2:  str=""; break;
  case 3:  str="medium"; break;
  case 4:  str="long"; break;
  }
  res.set(str,strlen(str));
  res.append(binary_flag ? "blob" : "text");
}


/****************************************************************************
** enum type.
** This is a string which only can have a selection of different values.
** If one uses this string in a number context one gets the type number.
****************************************************************************/

enum ha_base_keytype Field_enum::key_type() const
{
  switch (packlength) {
  default: return HA_KEYTYPE_BINARY;
  case 2: return HA_KEYTYPE_USHORT_INT;
  case 3: return HA_KEYTYPE_UINT24;
  case 4: return HA_KEYTYPE_ULONG_INT;
  case 8: return HA_KEYTYPE_ULONGLONG;
  }
}

void Field_enum::store_type(ulonglong value)
{
  switch (packlength) {
  case 1: ptr[0]= (uchar) value;  break;
  case 2: shortstore(ptr,(unsigned short) value);  break;
  case 3: int3store(ptr,(long) value); break;
  case 4: longstore(ptr,(long) value); break;
  case 8: longlongstore(ptr,value); break;
  }
}


uint find_enum(TYPELIB *typelib,const char *x, uint length)
{
  const char *end=x+length;
  while (end > x && isspace(end[-1]))
    end--;

  const char *i;
  const char *j;
  for (uint pos=0 ; (j=typelib->type_names[pos]) ; pos++)
  {
    for (i=x ; i != end && toupper(*i) == toupper(*j) ; i++, j++) ;
    if (i == end && ! *j)
      return(pos+1);
  }
  return(0);
}


/*
** Note. Storing a empty string in a enum field gives a warning
** (if there isn't a empty value in the enum)
*/

void Field_enum::store(const char *from,uint length)
{
  uint tmp=find_enum(typelib,from,length);
  {
    if (!tmp)
    {
      current_thd->cuted_fields++;
      Field_enum::store_type((longlong) 0);
    }
    else
      store_type((ulonglong) tmp);
  }
}


void Field_enum::store(double nr)
{
  Field_enum::store((longlong) nr);
}


void Field_enum::store(longlong nr)
{
  if ((uint) nr > typelib->count || nr == 0)
  {
    current_thd->cuted_fields++;
    nr=0;
  }
  store_type((ulonglong) (uint) nr);
}


double Field_enum::val_real(void)
{
  return (double) Field_enum::val_int();
}


longlong Field_enum::val_int(void)
{
  switch (packlength) {
  case 1:
    return (longlong) (uchar) ptr[0];
  case 2:
    {
      uint16 tmp; shortget(tmp,ptr);
      return (longlong) tmp;
    }
  case 3:
    return (longlong) uint3korr(ptr);
  case 4:
    {
      uint32 tmp; longget(tmp,ptr);
      return (longlong) tmp;
    }
  case 8:
    {
      longlong tmp; longlongget(tmp,ptr);
      return tmp;
    }
  }
  return 0;					// impossible
}


String *Field_enum::val_str(String *val_buffer,String *val_ptr)
{
  uint tmp=(uint) Field_enum::val_int();
  if (!tmp)
    val_ptr->length(0);
  else
    val_ptr->set((const char*) typelib->type_names[tmp-1],
		 strlen(typelib->type_names[tmp-1]));
  return val_ptr;
}

int Field_enum::cmp(const char *a_ptr, const char *b_ptr)
{
  char *old=ptr;
  ptr=(char*) a_ptr;
  ulonglong a=Field_enum::val_int();
  ptr=(char*) b_ptr;
  ulonglong b=Field_enum::val_int();
  ptr=old;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_enum::sort_string(char *to,uint length)
{
  ulonglong value=Field_enum::val_int();
  to+=packlength-1;
  for (uint i=0 ; i < packlength ; i++)
  {
    *to-- = (uchar) (value & 255);
    value>>=8;
  }
}


void Field_enum::sql_type(String &res) const
{
  res.length(0);
  res.append("enum(");

  bool flag=0;
  for (char **pos=typelib->type_names; *pos ; pos++)
  {
    if (flag)
      res.append(',');
    res.append('\'');
    append_unescaped(&res,*pos);
    res.append('\'');
    flag=1;
  }
  res.append(')');
}


/****************************************************************************
** set type.
** This is a string which can have a collection of different values.
** Each string value is separated with a ','.
** For example "One,two,five"
** If one uses this string in a number context one gets the bits as a longlong
** number.
****************************************************************************/

ulonglong find_set(TYPELIB *typelib,const char *x,uint length)
{
  const char *end=x+length;
  while (end > x && isspace(end[-1]))
    end--;

  ulonglong found=0;
  if (x != end)
  {
    const char *start=x;
    bool error=0;
    for (;;)
    {
      const char *pos=start;
      for ( ; pos != end && *pos != field_separator ; pos++) ;
      uint find=find_enum(typelib,start,(uint) (pos-start));
      if (!find)
	error=1;
      else
	found|= ((longlong) 1 << (find-1));
      if (pos == end)
	break;
      start=pos+1;
    }
    if (error)
      current_thd->cuted_fields++;
  }
  return found;
}


void Field_set::store(const char *from,uint length)
{
  store_type(find_set(typelib,from,length));
}


void Field_set::store(longlong nr)
{
  if ((ulonglong) nr > (ulonglong) (((longlong) 1 << typelib->count) -
				    (longlong) 1))
  {
    nr&= (longlong) (((longlong) 1 << typelib->count) - (longlong) 1);
    current_thd->cuted_fields++;
  }
  store_type((ulonglong) nr);
}


String *Field_set::val_str(String *val_buffer,String *val_ptr)
{
  ulonglong tmp=(ulonglong) Field_enum::val_int();
  uint bitnr=0;

  val_buffer->length(0);
  while (tmp && bitnr < (uint) typelib->count)
  {
    if (tmp & 1)
    {
      if (val_buffer->length())
	val_buffer->append(field_separator);
      String str(typelib->type_names[bitnr],
		 strlen(typelib->type_names[bitnr]));
      val_buffer->append(str);
    }
    tmp>>=1;
    bitnr++;
  }
  return val_buffer;
}


void Field_set::sql_type(String &res) const
{
  res.length(0);
  res.append("set(");

  bool flag=0;
  for (char **pos=typelib->type_names; *pos ; pos++)
  {
    if (flag)
      res.append(',');
    res.append('\'');
    append_unescaped(&res,*pos);
    res.append('\'');
    flag=1;
  }
  res.append(')');
}


/*****************************************************************************
** Handling of field and create_field
*****************************************************************************/

/*
** Make a field from the .frm file info
*/

uint32 calc_pack_length(enum_field_types type,uint32 length)
{
  switch (type) {
  case FIELD_TYPE_STRING:
  case FIELD_TYPE_VAR_STRING:
  case FIELD_TYPE_DECIMAL: return (length);
  case FIELD_TYPE_YEAR:
  case FIELD_TYPE_TINY	: return 1;
  case FIELD_TYPE_SHORT : return 2;
  case FIELD_TYPE_INT24:
  case FIELD_TYPE_NEWDATE:
  case FIELD_TYPE_TIME:   return 3;
  case FIELD_TYPE_TIMESTAMP:
  case FIELD_TYPE_DATE:
  case FIELD_TYPE_LONG	: return 4;
  case FIELD_TYPE_FLOAT : return sizeof(float);
  case FIELD_TYPE_DOUBLE: return sizeof(double);
  case FIELD_TYPE_DATETIME:
  case FIELD_TYPE_LONGLONG: return 8;	/* Don't crash if no longlong */
  case FIELD_TYPE_NULL	: return 0;
  case FIELD_TYPE_TINY_BLOB:   return 1+sizeof(char*);
  case FIELD_TYPE_BLOB:  return 2+sizeof(char*);
  case FIELD_TYPE_MEDIUM_BLOB: return 3+sizeof(char*);
  case FIELD_TYPE_LONG_BLOB:   return 4+sizeof(char*);
  case FIELD_TYPE_SET:
  case FIELD_TYPE_ENUM: abort(); return 0;	// This shouldn't happen
  }
  return 0;					// This shouldn't happen
}


uint pack_length_to_packflag(uint type)
{
  switch (type) {
    case 1: return f_settype((uint) FIELD_TYPE_TINY);
    case 2: return f_settype((uint) FIELD_TYPE_SHORT);
    case 3: return f_settype((uint) FIELD_TYPE_INT24);
    case 4: return f_settype((uint) FIELD_TYPE_LONG);
    case 8: return f_settype((uint) FIELD_TYPE_LONGLONG);
  }
  return 0;					// This shouldn't happen
}


Field *make_field(char *ptr, uint32 field_length,
		  uchar *null_pos, uint null_bit,
		  uint pack_flag,
		  Field::utype unireg_check,
		  TYPELIB *interval,
		  char *field_name,
		  struct st_table *table)
{
  if (!f_maybe_null(pack_flag))
  {
    null_pos=0;
    null_bit=0;
  }
  if (f_is_alpha(pack_flag))
  {
    if (!f_is_packed(pack_flag))
      return new Field_string(ptr,field_length,null_pos,null_bit,
			      unireg_check, field_name, table,
			      f_is_binary(pack_flag) != 0);

    uint pack_length=calc_pack_length((enum_field_types)
				      f_packtype(pack_flag),
				      field_length);

    if (f_is_blob(pack_flag))
      return new Field_blob(ptr,null_pos,null_bit,
			    unireg_check, field_name, table,
			    pack_length,f_is_binary(pack_flag) != 0);
    if (interval)
    {
      if (f_is_enum(pack_flag))
	return new Field_enum(ptr,field_length,null_pos,null_bit,
				  unireg_check, field_name, table,
				  pack_length, interval);
      else
	return new Field_set(ptr,field_length,null_pos,null_bit,
			     unireg_check, field_name, table,
			     pack_length, interval);
    }
  }

  switch ((enum enum_field_types) f_packtype(pack_flag)) {
  case FIELD_TYPE_DECIMAL:
    return new Field_decimal(ptr,field_length,null_pos,null_bit,
			     unireg_check, field_name, table,
			     f_decimals(pack_flag),
			     f_is_zerofill(pack_flag) != 0,
			     f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_FLOAT:
    return new Field_float(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name, table,
			   f_decimals(pack_flag),
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag)== 0);
  case FIELD_TYPE_DOUBLE:
    return new Field_double(ptr,field_length,null_pos,null_bit,
			    unireg_check, field_name, table,
			    f_decimals(pack_flag),
			    f_is_zerofill(pack_flag) != 0,
			    f_is_dec(pack_flag)== 0);
  case FIELD_TYPE_TINY:
    return new Field_tiny(ptr,field_length,null_pos,null_bit,
			  unireg_check, field_name, table,
			  f_is_zerofill(pack_flag) != 0,
			  f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_SHORT:
    return new Field_short(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name, table,
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_INT24:
    return new Field_medium(ptr,field_length,null_pos,null_bit,
			    unireg_check, field_name, table,
			    f_is_zerofill(pack_flag) != 0,
			    f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_LONG:
    return new Field_long(ptr,field_length,null_pos,null_bit,
			   unireg_check, field_name, table,
			   f_is_zerofill(pack_flag) != 0,
			   f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_LONGLONG:
    return new Field_longlong(ptr,field_length,null_pos,null_bit,
			      unireg_check, field_name, table,
			      f_is_zerofill(pack_flag) != 0,
			      f_is_dec(pack_flag) == 0);
  case FIELD_TYPE_TIMESTAMP:
    return new Field_timestamp(ptr,field_length,
			       unireg_check, field_name, table);
  case FIELD_TYPE_YEAR:
    return new Field_year(ptr,field_length,null_pos,null_bit,
			  unireg_check, field_name, table);
  case FIELD_TYPE_DATE:
    return new Field_date(ptr,null_pos,null_bit,
			  unireg_check, field_name, table);
  case FIELD_TYPE_NEWDATE:
    return new Field_newdate(ptr,null_pos,null_bit,
			     unireg_check, field_name, table);
  case FIELD_TYPE_TIME:
    return new Field_time(ptr,null_pos,null_bit,
			  unireg_check, field_name, table);
  case FIELD_TYPE_DATETIME:
    return new Field_datetime(ptr,null_pos,null_bit,
			      unireg_check, field_name, table);
  case FIELD_TYPE_NULL:
    default:					// Impossible (Wrong version)
    return new Field_null(ptr,field_length,unireg_check,field_name,table);
  }
  return 0;					// Impossible (Wrong version)
}


/* Create a field suitable for create of table */

create_field::create_field(Field *old_field)
{
  field=      old_field;
  field_name=change=old_field->field_name;
  length=     old_field->field_length;
  flags=      old_field->flags;
  unireg_check=old_field->unireg_check;
  pack_length=old_field->pack_length();
  sql_type=   old_field->real_type();
  if (sql_type == FIELD_TYPE_STRING)
    sql_type=old_field->type();

  if (old_field->result_type() == REAL_RESULT)
    decimals= ((Field_num*) old_field)->decimals;
  else
    decimals=0;

  if (flags & ENUM_FLAG)
    interval= ((Field_enum*) old_field)->typelib;
  else
    interval=0;
  if (!old_field->is_null() && ! (flags & BLOB_FLAG) &&
      old_field->type() != FIELD_TYPE_TIMESTAMP)
  {
    char buff[MAX_FIELD_WIDTH],*pos;
    String tmp(buff,sizeof(buff));
    field->val_str(&tmp,&tmp);
    pos=sql_memdup(tmp.ptr(),tmp.length()+1);
    pos[tmp.length()]=0;
    def=new Item_string(pos,tmp.length());
  }
  else
    def=0;
}
