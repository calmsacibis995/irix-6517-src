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

/* Sum functions (COUNT, MIN...) */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"


void Item_sum::make_field(Send_field *tmp_field)
{
  if (item->type() == Item::FIELD_ITEM && keep_field_type())
    ((Item_field*) item)->field->make_field(tmp_field);
  else
  {
    tmp_field->flags=0;
    if (!maybe_null)
      tmp_field->flags|= NOT_NULL_FLAG;
    tmp_field->length=max_length;
    tmp_field->decimals=decimals;
    tmp_field->type=(result_type() == INT_RESULT ? FIELD_TYPE_LONG :
		     result_type() == REAL_RESULT ? FIELD_TYPE_DOUBLE :
		     FIELD_TYPE_VAR_STRING);
  }
  tmp_field->table_name="";
  tmp_field->col_name=name;
}

void Item_sum::print(String *str)
{
  str->append(func_name());
  str->append('(');
  item->print(str);
  str->append(')');
}


String *
Item_sum_num::str(String *str)
{
  double nr=val();
  if (null_value)
    return 0;
  str->set(nr,decimals);
  return str;
}


String *
Item_sum_int::str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  char buff[21];
  uint length= (uint) (longlong2str(nr,buff,-10)-buff);
  str->copy(buff,length);
  return str;
}


bool
Item_sum_num::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (!thd->allow_sum_func)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE,MYF(0));
    return 1;
  }
  thd->allow_sum_func=0;			// No included group funcs
  if (item->fix_fields(thd,tables))
    return 1;
  decimals=item->decimals;
  result_field=0;
  max_length=float_length(decimals);
  maybe_null=item->maybe_null;
  null_value=1;
  fix_length_and_dec();
  thd->allow_sum_func=1;			// Allow group functions
  return 0;
}


bool
Item_sum_hybrid::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (!thd->allow_sum_func)
  {
    my_error(ER_INVALID_GROUP_FUNC_USE,MYF(0));
    return 1;
  }
  thd->allow_sum_func=0;			// No included group funcs
  if (item->fix_fields(thd,tables))
    return 1;
  hybrid_type=item->result_type();
  if (hybrid_type == INT_RESULT)
    max_length=21;
  else if (hybrid_type == REAL_RESULT)
    max_length=float_length(decimals);
  else
    max_length=item->max_length;
  decimals=item->decimals;
  maybe_null=item->maybe_null;
  binary=item->binary;
  result_field=0;
  null_value=1;
  fix_length_and_dec();
  thd->allow_sum_func=1;			// Allow group functions
  return 0;
}


/***********************************************************************
** reset and add of sum_func
***********************************************************************/

void Item_sum_sum::reset()
{
  null_value=0; sum=item->val();
}

void Item_sum_sum::add()
{
  sum+=item->val();
}

double Item_sum_sum::val()
{
  return sum;
}


void Item_sum_count::reset()
{
  count=0; null_value=0; add();
}

void Item_sum_count::add()
{
  if (!item->maybe_null)
    count++;
  else
  {
    (void) item->val_int();
    if (!item->null_value)
      count++;
  }
}

longlong Item_sum_count::val_int()
{
  return (longlong) count;
}

/*
** Avgerage
*/

void Item_sum_avg::reset()
{
  sum=0.0; count=0; Item_sum_avg::add();
}

void Item_sum_avg::add()
{
  double nr=item->val();
  if (!item->null_value)
  {
    sum+=nr;
    count++;
  }
}

double Item_sum_avg::val()
{
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  return sum/ulonglong2double(count);
}


/*
** Standard deviation
*/

void Item_sum_std::reset()
{
  sum=sum_sqr=0.0; count=0; Item_sum_std::add();
}

void Item_sum_std::add()
{
  double nr=item->val();
  if (!item->null_value)
  {
    sum+=nr;
    sum_sqr+=nr*nr;
    count++;
  }
}

double Item_sum_std::val()
{
  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  null_value=0;
  double tmp=ulonglong2double(count);
  return sqrt((sum_sqr - sum*sum/tmp)/tmp);
}


void Item_sum_std::reset_field()
{
  double nr=item->val();
  char *res=result_field->ptr;

  if (item->null_value)
    bzero(res,sizeof(double)*2+sizeof(long));
  else
  {
    doublestore(res,nr);
    nr*=nr;
    doublestore(res+sizeof(double),nr);
    longstore(res+sizeof(double)*2,1);
  }
}

void Item_sum_std::update_field(int offset)
{
  double nr,old_nr,old_sqr;
  longlong field_count;
  char *res=result_field->ptr;

  doubleget(old_nr,res+offset);
  doubleget(old_sqr,res+offset+sizeof(double));
  longlongget(field_count,res+offset+sizeof(double)*2);

  nr=item->val();
  if (!item->null_value)
  {
    old_nr+=nr;
    old_sqr+=nr*nr;
    field_count++;
  }
  doublestore(res,old_nr);
  doublestore(res+sizeof(double),old_sqr);
  longlongstore(res+sizeof(double)*2,field_count);
}

/* min & max */

double Item_sum_hybrid::val()
{
  if (null_value)
    return 0.0;
  if (hybrid_type == STRING_RESULT)
  {
    String *res;  res=str(&str_value);
    return res ? atof(res->c_ptr()) : 0.0;
  }
  return sum;
}


String *
Item_sum_hybrid::str(String *str)
{
  if (null_value)
    return 0;
  if (hybrid_type == STRING_RESULT)
    return &value;
  str->set(sum,decimals);
  return str;
}


void Item_sum_min::add()
{
  if (hybrid_type != STRING_RESULT)
  {
    double nr=item->val();
    if (!item->null_value && (null_value || nr < sum))
    {
      sum=nr;
      null_value=0;
    }
  }
  else
  {
    String *result=item->str(&tmp_value);
    if (!item->null_value &&
	(null_value ||
	 (binary ? stringcmp(&value,result) : sortcmp(&value,result)) > 0))
    {
      value.copy(*result);
      null_value=0;
    }
  }
}


void Item_sum_max::add()
{
  if (hybrid_type != STRING_RESULT)
  {
    double nr=item->val();
    if (!item->null_value && (null_value || nr > sum))
    {
      sum=nr;
      null_value=0;
    }
  }
  else
  {
    String *result=item->str(&tmp_value);
    if (!item->null_value &&
	(null_value ||
	 (binary ? stringcmp(&value,result) : sortcmp(&value,result)) < 0))
    {
      value.copy(*result);
      null_value=0;
    }
  }
}


/* bit_or and bit_and */

longlong Item_sum_bit::val_int()
{
  return (longlong) bits;
}

void Item_sum_bit::reset()
{
  bits=reset_bits; add();
}

void Item_sum_or::add()
{
  ulonglong value= (ulonglong) item->val_int();
  if (!item->null_value)
    bits|=value;
}

void Item_sum_and::add()
{
  ulonglong value= (ulonglong) item->val_int();
  if (!item->null_value)
    bits&=value;
}


/************************************************************************
** reset result of a Item_sum with is saved in a tmp_table
*************************************************************************/

void Item_sum_num::reset_field()
{
  double nr=item->val();
  char *res=result_field->ptr;

  if (maybe_null)
  {
    if (item->null_value)
    {
      nr=0.0;
      result_field->set_null();
    }
    else
      result_field->set_notnull();
  }
  memcpy(res,(char*) &nr,sizeof(double));
}


void Item_sum_hybrid::reset_field()
{
  if (hybrid_type == STRING_RESULT)
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff)),*res;

    res=item->str(&tmp);
    if (item->null_value)
    {
      result_field->set_null();
      result_field->reset();
    }
    else
    {
      result_field->set_notnull();
      result_field->store(res->ptr(),res->length());
    }
  }
  else if (hybrid_type == INT_RESULT)
  {
    longlong nr=item->val_int();

    if (maybe_null)
    {
      if (item->null_value)
      {
	nr=0;
	result_field->set_null();
      }
      else
	result_field->set_notnull();
    }
    result_field->store(nr);
  }
  else						// REAL_RESULT
  {
    double nr=item->val();

    if (maybe_null)
    {
      if (item->null_value)
      {
	nr=0.0;
	result_field->set_null();
      }
      else
	result_field->set_notnull();
    }
    result_field->store(nr);
  }
}


void Item_sum_sum::reset_field()
{
  double nr=item->val();			// Nulls also return 0
  memcpy(result_field->ptr,(char*) &nr,sizeof(double));
}


void Item_sum_count::reset_field()
{
  char *res=result_field->ptr;
  longlong nr=0;

  if (!item->maybe_null)
    nr=1;
  else
  {
    (void) item->val_int();
    if (!item->null_value)
      nr=1;
  }
  longlongstore(res,nr);
}


void Item_sum_avg::reset_field()
{
  double nr=item->val();
  char *res=result_field->ptr;

  if (item->null_value)
    bzero(res,sizeof(double)+sizeof(longlong));
  else
  {
    doublestore(res,nr);
    res+=sizeof(double);
    longlong tmp=1;
    longlongstore(res,tmp);
  }
}

void Item_sum_bit::reset_field()
{
  char *res=result_field->ptr;
  ulonglong nr=(ulonglong) item->val_int();
  longlongstore(res,nr);
}

/*
** calc next value and merge it with field_value
*/

void Item_sum_sum::update_field(int offset)
{
  double old_nr,nr;
  char *res=result_field->ptr;

  doubleget(old_nr,res+offset);
  nr=item->val();
  if (!item->null_value)
    old_nr+=nr;
  doublestore(res,old_nr);
}


void Item_sum_count::update_field(int offset)
{
  longlong nr;
  char *res=result_field->ptr;

  longlongget(nr,res+offset);
  if (!item->maybe_null)
    nr++;
  else
  {
    (void) item->val_int();
    if (!item->null_value)
      nr++;
  }
  longlongstore(res,nr);
}


void Item_sum_avg::update_field(int offset)
{
  double nr,old_nr;
  longlong field_count;
  char *res=result_field->ptr;

  doubleget(old_nr,res+offset);
  longlongget(field_count,res+offset+sizeof(double));

  nr=item->val();
  if (!item->null_value)
  {
    old_nr+=nr;
    field_count++;
  }
  doublestore(res,old_nr);
  res+=sizeof(double);
  longlongstore(res,field_count);
}


void Item_sum_hybrid::update_field(int offset)
{
  if (hybrid_type == STRING_RESULT)
    min_max_update_str_field(offset);
  else if (hybrid_type == INT_RESULT)
    min_max_update_int_field(offset);
  else
    min_max_update_real_field(offset);
}


void
Item_sum_hybrid::min_max_update_str_field(int offset)
{
  String *res_str=item->str(&value);

  if (item->null_value)
    result_field->copy_from_tmp(offset);	// Use old value
  else
  {
    res_str->strip_sp();
    result_field->ptr+=offset;			// Get old max/min
    result_field->val_str(&tmp_value,&tmp_value);
    result_field->ptr-=offset;

    if (result_field->is_null() ||
	(cmp_sign * (binary ? stringcmp(res_str,&tmp_value) :
		 sortcmp(res_str,&tmp_value)) < 0))
      result_field->store(res_str->ptr(),res_str->length());
    else
    {						// Use old value
      char *res=result_field->ptr;
      memcpy(res,res+offset,result_field->pack_length());
    }
    result_field->set_notnull();
  }
}


void
Item_sum_hybrid::min_max_update_real_field(int offset)
{
  double nr,old_nr;

  result_field->ptr+=offset;
  old_nr=result_field->val_real();
  nr=item->val();
  if (!item->null_value)
  {
    if (result_field->is_null(offset) ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null(offset))
    result_field->set_null();
  result_field->ptr-=offset;
  result_field->store(old_nr);
}


void
Item_sum_hybrid::min_max_update_int_field(int offset)
{
  longlong nr,old_nr;

  result_field->ptr+=offset;
  old_nr=result_field->val_int();
  nr=item->val_int();
  if (!item->null_value)
  {
    if (result_field->is_null(offset) ||
	(cmp_sign > 0 ? old_nr > nr : old_nr < nr))
      old_nr=nr;
    result_field->set_notnull();
  }
  else if (result_field->is_null(offset))
    result_field->set_null();
  result_field->ptr-=offset;
  result_field->store(old_nr);
}


void Item_sum_or::update_field(int offset)
{
  ulonglong nr;
  char *res=result_field->ptr;

  ulongget(nr,res+offset);
  nr|= (ulonglong) item->val_int();
  longlongstore(res,nr);
}


void Item_sum_and::update_field(int offset)
{
  ulonglong nr;
  char *res=result_field->ptr;

  ulongget(nr,res+offset);
  nr&= (ulonglong) item->val_int();
  longlongstore(res,nr);
}
