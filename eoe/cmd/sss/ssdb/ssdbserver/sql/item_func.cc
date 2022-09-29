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

/* This file defines all numerical functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <hash.h>
#include <time.h>
#include <errno.h>

/* return TRUE if item is a constant */


bool
eval_const_cond(COND *cond)
{
  return ((Item_func*) cond)->val_int() ? TRUE : FALSE;
}


Item_func::Item_func(List<Item> &list)
{
  arg_count=list.elements;
  if ((args=(Item**) sql_alloc(sizeof(Item*)*arg_count)))
  {
    uint i=0;
    List_iterator<Item> li(list);
    Item *item;

    while ((item=li++))
    {
      args[i++]= item;
      with_sum_func|=item->with_sum_func;
    }
  }
  list.empty();					// Fields are used
}

Item_func::~Item_func()
{
  /* Nothing to do; Items are freed automaticly */
}

bool
Item_func::fix_fields(THD *thd,TABLE_LIST *tables)
{
  Item **arg,**arg_end;
  char buff[sizeof(double)];			// Max argument in function
  binary=0;
  used_tables_cache=0;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error flag is set!
  if (arg_count)
  {						// Print purify happy
    for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
    {
      if ((*arg)->fix_fields(thd,tables))
	return 1;				/* purecov: inspected */
      if ((*arg)->maybe_null)
	maybe_null=1;
      if ((*arg)->binary)
	binary=1;
      with_sum_func= with_sum_func || (*arg)->with_sum_func;
      used_tables_cache|=(*arg)->used_tables();
    }
  }
  fix_length_and_dec();
  return 0;
}


void Item_func::split_sum_func(List<Item> &fields)
{
  Item **arg,**arg_end;
  for (arg=args, arg_end=args+arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->with_sum_func && (*arg)->type() != SUM_FUNC_ITEM)
      (*arg)->split_sum_func(fields);
    else if ((*arg)->used_tables() || (*arg)->type() == SUM_FUNC_ITEM)
    {
      fields.push_front(*arg);
      *arg=new Item_ref((Item**) fields.head_ref(),0,(*arg)->name);
    }
  }
}


void Item_func::update_used_tables()
{
  used_tables_cache=0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    args[i]->update_used_tables();
    used_tables_cache|=args[i]->used_tables();
  }
}


table_map Item_func::used_tables() const
{
  return used_tables_cache;
}

void Item_func::print(String *str)
{
  str->append(func_name());
  str->append('(');
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str);
  }
  str->append(')');
}


void Item_func::print_op(String *str)
{
  str->append('(');
  for (uint i=0 ; i < arg_count-1 ; i++)
  {
    args[i]->print(str);
    str->append(' ');
    str->append(func_name());
    str->append(' ');
  }
  args[arg_count-1]->print(str);
  str->append(')');
}


String *Item_real_func::str(String *str)
{
  double nr=val();
  if (null_value)
    return 0; /* purecov: inspected */
  else
    str->set(nr,decimals);
  return str;
}


String *Item_num_func::str(String *str)
{
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr);
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals);
  }
  return str;
}


void Item_func::fix_num_length_and_dec()
{
  decimals=0;
  for (uint i=0 ; i < arg_count ; i++)
    set_if_bigger(decimals,args[i]->decimals);
  max_length=float_length(decimals);
}


String *Item_int_func::str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else
    str->set(nr);
  return str;
}

/* Change from REAL_RESULT (default) to INT_RESULT if both arguments are integers */

void Item_num_op::find_num_type(void)
{
  if (args[0]->result_type() == INT_RESULT && args[1]->result_type() == INT_RESULT)
    hybrid_type=INT_RESULT;
}

String *Item_num_op::str(String *str)
{
  if (hybrid_type == INT_RESULT)
  {
    longlong nr=val_int();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr);
  }
  else
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals);
  }
  return str;
}


double Item_func_plus::val()
{
  double val=args[0]->val()+args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return val;
}

longlong Item_func_plus::val_int()
{
  longlong val=args[0]->val_int()+args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return val;
}

double Item_func_minus::val()
{
  double val=args[0]->val() - args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  return val;
}

longlong Item_func_minus::val_int()
{
  longlong val=args[0]->val_int() - args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0;
  return val;
}

double Item_func_mul::val()
{
  double val=args[0]->val()*args[1]->val();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return val;
}

longlong Item_func_mul::val_int()
{
  longlong val=args[0]->val_int()*args[1]->val_int();
  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return val;
}


double Item_func_div::val()
{
  double val=args[0]->val();
  double val2=args[1]->val();
  if ((null_value= val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0;
  return val/val2;
}

longlong Item_func_div::val_int()
{
  longlong val=args[0]->val_int();
  longlong val2=args[1]->val_int();
  if ((null_value= val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0;
  return val/val2;
}

void Item_func_div::fix_length_and_dec()
{
  decimals=max(args[0]->decimals,args[1]->decimals)+2;
  max_length=args[0]->max_length - args[0]->decimals + decimals;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  maybe_null=1;
}

double Item_func_mod::val()
{
  double val= floor(args[0]->val()+0.5);
  double val2=floor(args[1]->val()+0.5);
  if ((null_value=val2 == 0.0 || args[0]->null_value || args[1]->null_value))
    return 0.0; /* purecov: inspected */
  return fmod(val,val2);
}

longlong Item_func_mod::val_int()
{
  longlong val=  args[0]->val_int();
  longlong val2= args[1]->val_int();
  if ((null_value=val2 == 0 || args[0]->null_value || args[1]->null_value))
    return 0; /* purecov: inspected */
  return val % val2;
}

void Item_func_mod::fix_length_and_dec()
{
  max_length=args[1]->max_length;
  decimals=0;
  maybe_null=1;
  find_num_type();
}


double Item_func_neg::val()
{
  double val=args[0]->val();
  null_value=args[0]->null_value;
  return -val;
}

longlong Item_func_neg::val_int()
{
  longlong val=args[0]->val_int();
  null_value=args[0]->null_value;
  return -val;
}

void Item_func_neg::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT;
}

double Item_func_abs::val()
{
  double val=args[0]->val();
  null_value=args[0]->null_value;
  return fabs(val);
}

longlong Item_func_abs::val_int()
{
  longlong val=args[0]->val_int();
  null_value=args[0]->null_value;
  return val >= 0 ? val : -val;
}

void Item_func_abs::fix_length_and_dec()
{
  decimals=args[0]->decimals;
  max_length=args[0]->max_length;
  hybrid_type= args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT;
}

double Item_func_log::val()
{
  double val=args[0]->val();
  if ((null_value=(args[0]->null_value || val <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log(val);
}

double Item_func_log10::val()
{
  double val=args[0]->val();
  if ((null_value=(args[0]->null_value || val <= 0.0)))
    return 0.0; /* purecov: inspected */
  return log10(val);
}

double Item_func_exp::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0; /* purecov: inspected */
  return exp(val);
}

double Item_func_sqrt::val()
{
  double val=args[0]->val();
  if ((null_value=(args[0]->null_value || val < 0)))
    return 0.0; /* purecov: inspected */
  return sqrt(val);
}

double Item_func_pow::val()
{
  double val=args[0]->val();
  double val2=args[1]->val();
  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0.0; /* purecov: inspected */
  return pow(val,val2);
}

// Trigonometric functions

double Item_func_acos::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(acos(val));
}

double Item_func_asin::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(asin(val));
}

double Item_func_atan::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  if (arg_count == 2)
  {
    double val2= args[1]->val();
    if ((null_value=args[1]->null_value))
      return 0.0;
    return fix_result(atan2(val,val2));
  }
  return fix_result(atan(val));
}

double Item_func_cos::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(cos(val));
}

double Item_func_sin::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(sin(val));
}

double Item_func_tan::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0.0;
  return fix_result(tan(val));
}


// Shift-functions, same as << and >> in C/C++


longlong Item_func_shift_left::val_int()
{
  uint shift;
  ulonglong res= ((ulonglong) args[0]->val_int() <<
		  (shift=(uint) args[1]->val_int()));
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (shift < sizeof(longlong)*8 ? (longlong) res : LL(0));
}

longlong Item_func_shift_right::val_int()
{
  ulonglong res= (ulonglong) args[0]->val_int() >> (uint) args[1]->val_int();
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) res;
}


// Conversion functions

void Item_func_integer::fix_length_and_dec()
{
  max_length=args[0]->max_length - args[0]->decimals+1;
  uint tmp=float_length(decimals);
  set_if_smaller(max_length,tmp);
  decimals=0;
}

longlong Item_func_ceiling::val_int()
{
  double val=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) ceil(val);
}

longlong Item_func_floor::val_int()
{
  double val=args[0]->val();
  null_value=args[0]->null_value;
  return (longlong) floor(val);
}

void Item_func_round::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  decimals=args[0]->decimals;
  if (args[1]->const_item())
  {
    int tmp=(int) args[1]->val_int();
    if (tmp < 0)
      decimals=0;
    else
      decimals=tmp;
  }
}

double Item_func_round::val()
{
  double val=args[0]->val();
  int dec=(int) args[1]->val_int();
  uint abs_dec=abs(dec);

  if ((null_value=args[0]->null_value || args[1]->null_value))
    return 0.0;
  double tmp=(abs_dec < array_elements(log_10) ?
	      log_10[abs_dec] : pow(10.0,(double) abs_dec));

  if (truncate)
    return dec < 0 ? floor(val/tmp)*tmp : floor(val*tmp)/tmp;
  return dec < 0 ? rint(val/tmp)*tmp : rint(val*tmp)/tmp;
}


double Item_func_rand::val()
{
  if (arg_count)
  {					// Only use argument once in query
    ulong tmp=((ulong) args[0]->val_int())+55555555L;
    randominit(&current_thd->rand,tmp,tmp/2);
#ifdef DELETE_ITEMS
    delete args[0];
#endif
    arg_count=0;
  }
  return rnd(&current_thd->rand);
}

longlong Item_func_sign::val_int()
{
  double val=args[0]->val();
  null_value=args[0]->null_value;
  return val < 0.0 ? -1 : (val	> 0 ? 1 : 0);
}


double Item_func_units::val()
{
  double val=args[0]->val();
  if ((null_value=args[0]->null_value))
    return 0;
  return val*mul+add;
}


void Item_func_min_max::fix_length_and_dec()
{
  decimals=0;
  max_length=0;
  maybe_null=1;
  binary=0;
  cmp_type=args[0]->result_type();
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (max_length < args[i]->max_length)
      max_length=args[i]->max_length;
    if (decimals < args[i]->decimals)
      decimals=args[i]->decimals;
    if (!args[i]->maybe_null)
      maybe_null=0;
    cmp_type=item_cmp_type(cmp_type,args[i]->result_type());
    if (args[i]->binary)
      binary=1;
  }
}


String *Item_func_min_max::str(String *str)
{
  switch (cmp_type) {
  case INT_RESULT:
  {
    longlong nr=val_int();
    if (null_value)
      return 0;
    else
      str->set(nr);
    return str;
  }
  case REAL_RESULT:
  {
    double nr=val();
    if (null_value)
      return 0; /* purecov: inspected */
    else
      str->set(nr,decimals);
    return str;
  }
  case STRING_RESULT:
  {
    String *res;
    LINT_INIT(res);
    null_value=1;
    for (uint i=0; i < arg_count ; i++)
    {
      if (null_value)
      {
	res=args[i]->str(str);
	null_value=args[i]->null_value;
      }
      else
      {
	String *res2;
	res2= args[i]->str(res == str ? &tmp_value : str);
	if (res2)
	{
	  int cmp=binary ? stringcmp(res,res2) : sortcmp(res,res2);
	  if ((cmp_sign < 0 ? cmp : -cmp) < 0)
	    res=res2;
	}
      }
    }
    return res;
  }
  }
  return 0;					// Keep compiler happy
}


double Item_func_min_max::val()
{
  double value=0.0;
  null_value=1;
  for (uint i=0; i < arg_count ; i++)
  {
    if (null_value)
    {
      value=args[i]->val();
      null_value=args[i]->null_value;
    }
    else
    {
      double tmp=args[i]->val();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
  }
  return value;
}


longlong Item_func_min_max::val_int()
{
  longlong value=0;
  null_value=1;
  for (uint i=0; i < arg_count ; i++)
  {
    if (null_value)
    {
      value=args[i]->val_int();
      null_value=args[i]->null_value;
    }
    else
    {
      longlong tmp=args[i]->val_int();
      if (!args[i]->null_value && (tmp < value ? cmp_sign : -cmp_sign) > 0)
	value=tmp;
    }
  }
  return value;
}


longlong Item_func_length::val_int()
{
  String *res=args[0]->str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) res->length();
}


longlong Item_func_locate::val_int()
{
  String *a=args[0]->str(&value1);
  String *b=args[1]->str(&value2);
  if (!a || !b)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  uint start=0;
  if (arg_count == 3)
  {
    start=(uint) args[2]->val_int()-1;
    if (start > a->length() || start+b->length() > a->length())
      return 0;
  }
  if (!b->length())				// Found empty string at start
    return (longlong) (start+1);
  return (longlong) (a->strstr(*b,start)+1) ;
}


longlong Item_func_field::val_int()
{
  String *field;
  if (!(field=item->str(&value)))
    return 0;					// -1 if null ?
  for (uint i=0 ; i < arg_count ; i++)
  {
    String *tmp_value=args[i]->str(&tmp);
    if (tmp_value && field->length() == tmp_value->length() &&
	!memcmp(field->ptr(),tmp_value->ptr(),tmp_value->length()))
      return (longlong) (i+1);
  }
  return 0;
}


longlong Item_func_ascii::val_int()
{
  String *res=args[0]->str(&value);
  if (!res)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) (res->length() ? (uchar) (*res)[0] : (uchar) 0);
}


static char nbits[256] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

	/* Search after a string in a string of strings separated by ',' */
	/* Returns number of found type >= 1 or 0 if not found */
	/* This optimizes searching in enums to bit testing! */

void Item_func_find_in_set::fix_length_and_dec()
{
  decimals=0;
  max_length=3;					// 1-999
  if (args[0]->const_item() && args[1]->type() == FIELD_ITEM)
  {
    Field *field= ((Item_field*) args[1])->field;
    if (field->real_type() == FIELD_TYPE_SET)
    {
      String *find=args[0]->str(&value);
      enum_value=find_enum(((Field_enum*) field)->typelib,find->ptr(),
			   find->length());
      enum_bit=0;
      if (enum_value)
	enum_bit=LL(1) << (enum_value-1);
    }
  }
}

static const char separator=',';

longlong Item_func_find_in_set::val_int()
{
  if (enum_value)
  {
    ulonglong tmp=(ulonglong) args[1]->val_int();
    if (!(null_value=args[1]->null_value || args[0]->null_value))
    {
      if (tmp & enum_bit)
	return enum_value;
    }
    return 0L;
  }

  String *find=args[0]->str(&value);
  String *buffer=args[1]->str(&value2);
  if (!find || !buffer)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;

  int diff;
  if ((diff=buffer->length() - find->length()) >= 0)
  {
    const char *f_pos=find->ptr();
    const char *f_end=f_pos+find->length();
    const char *str=buffer->ptr();
    const char *end=str+diff+1;
    const char *real_end=str+buffer->length();
    uint position=1;
    do
    {
      const char *pos= f_pos;
      while (pos != f_end)
      {
	if (toupper(*str) != toupper(*pos))
	  goto not_found;
	str++;
	pos++;
      }
      if (str == real_end || str[0] == separator)
	return (longlong) position;
  not_found:
      while (str < end && str[0] != separator)
	str++;
      position++;
    } while (++str <= end);
  }
  return 0;
}


longlong Item_func_bit_count::val_int()
{
  ulonglong value= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulong v1=(ulong) value;
#if SIZEOF_LONG_LONG > 4
  ulong v2=(ulong) (value >> 32);
#else
  ulong v2=0;
#endif
  uint bits= (uint) (uchar) (nbits[(uchar) v1] +
			     nbits[(uchar) (v1 >> 8)] +
			     nbits[(uchar) (v1 >> 16)] +
			     nbits[(uchar) (v1 >> 24)] +
			     nbits[(uchar) v2] +
			     nbits[(uchar) (v2 >> 8)] +
			     nbits[(uchar) (v2 >> 16)] +
			     nbits[(uchar) (v2 >> 24)]);
  return (longlong) bits;
}


/****************************************************************************
** Functions to handle dynamic loadable functions
** Original source by: Alexis Mikhailov <root@medinf.chuvashia.su>
** Rewritten by: Monty.
****************************************************************************/

#ifdef HAVE_DLOPEN

Item_udf_func::~Item_udf_func()
{
  if (initialized)
  {
    if (u_d->func_deinit != NULL)
    {
      void (*deinit)(UDF_INIT *) = (void (*)(UDF_INIT*))
	u_d->func_deinit;
      (*deinit)(&initid);
    }
    free_udf(u_d);
  }
  delete [] buffers;
}


bool
Item_udf_func::fix_fields(THD *thd,TABLE_LIST *tables)
{
  char buff[sizeof(double)];			// Max argument in function
  DBUG_ENTER("Item_udf_func::fix_fields");

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error flag is set!

  udf_func *udf=find_udf(u_d->name,strlen(u_d->name),1);

  if (!udf)
  {
    my_printf_error(ER_CANT_FIND_UDF,ER(ER_CANT_FIND_UDF),MYF(0),u_d->name,
		    errno);
    DBUG_RETURN(1);
  }
  u_d=udf;

  /* Fix all arguments */
  binary=maybe_null=0;
  used_tables_cache=0;
  if ((f_args.arg_count=arg_count))
  {
    if (!(f_args.arg_type= (Item_result*)
	  sql_alloc(arg_count*sizeof(Item_result))))

    {
      free_udf(u_d);
      DBUG_RETURN(1);
    }
    uint i;
    Item **arg,**arg_end;
    for (i=0, arg=args, arg_end=args+arg_count; arg != arg_end ; arg++,i++)
    {
      if ((*arg)->fix_fields(thd,tables))
	return 1;
      if ((*arg)->binary)
	binary=1;
      if ((*arg)->maybe_null)
	maybe_null=1;
      with_sum_func= with_sum_func || (*arg)->with_sum_func;
      used_tables_cache|=(*arg)->used_tables();
      f_args.arg_type[i]=(*arg)->result_type();
    }
    if (!(buffers=new String[arg_count]) ||
	!(f_args.args= (char**) sql_alloc(arg_count * sizeof(char *))) ||
	!(f_args.lengths=(ulong*) sql_alloc(arg_count * sizeof(long))) ||
	!(f_args.maybe_null=(char*) sql_alloc(arg_count * sizeof(char))) ||
	!(num_buffer= sql_alloc(ALIGN_SIZE(sizeof(double))*arg_count)))
    {
      free_udf(u_d);
      DBUG_RETURN(1);
    }
  }
  fix_length_and_dec();
  initid.max_length=max_length;
  initid.maybe_null=maybe_null;
  initid.decimals=decimals;
  initid.ptr=0;

  if (u_d->func_init)
  {
    char *to=num_buffer;
    for (uint i=0; i < arg_count; i++)
    {
      f_args.args[i]=0;
      f_args.lengths[i]=args[i]->max_length;
      f_args.maybe_null[i]=(char) args[i]->maybe_null;

      switch(args[i]->type()) {
      case Item::STRING_ITEM:			// Constant string !
      {
	String *res=args[i]->str((String *) 0);
	if (args[i]->null_value)
	  continue;
	f_args.args[i]=    (char*) res->ptr();
	break;
      }
      case Item::INT_ITEM:
	*((longlong*) to) = args[i]->val_int();
	if (!args[i]->null_value)
	{
	  f_args.args[i]=to;
	  to+= ALIGN_SIZE(sizeof(longlong));
	}
	break;
      case Item::REAL_ITEM:
	*((double*) to) = args[i]->val();
	if (!args[i]->null_value)
	{
	  f_args.args[i]=to;
	  to+= ALIGN_SIZE(sizeof(double));
	}
	break;
      default:					// Skipp these
	break;
      }
    }
    thd->net.last_error[0]=0;
    my_bool (*init)(UDF_INIT *, UDF_ARGS *, char *)=
      (my_bool (*)(UDF_INIT *, UDF_ARGS *,  char *))
      u_d->func_init;
    if ((error=(uchar) init(&initid, &f_args, thd->net.last_error)))
    {
      my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		      u_d->name,thd->net.last_error);
      free_udf(u_d);
      DBUG_RETURN(1);
    }
    max_length=min(initid.max_length,MAX_BLOB_WIDTH);
    maybe_null=initid.maybe_null;
    decimals=min(initid.decimals,31);
  }
  initialized=1;
  if (error)
  {
    my_printf_error(ER_CANT_INITIALIZE_UDF,ER(ER_CANT_INITIALIZE_UDF),MYF(0),
		    u_d->name, ER(ER_UNKNOWN_ERROR));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


bool Item_udf_func::get_arguments()
{
  if (error)
  {
    null_value=1;				// If something goes wrong
    return 1;
  }
  null_value=0;
  char *to= num_buffer;
  uint str_count=0;
  for (uint i=0; i < arg_count; i++)
  {
    f_args.args[i]=0;
    switch (f_args.arg_type[i]) {
    case STRING_RESULT:
      {
	String *res=args[i]->str(&buffers[str_count++]);
	if (!(args[i]->null_value))
	{
	  f_args.args[i]=    (char*) res->ptr();
	  f_args.lengths[i]= res->length();
	  break;
	}
      }
    case INT_RESULT:
      *((longlong*) to) = args[i]->val_int();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(longlong));
      }
      break;
    case REAL_RESULT:
      *((double*) to) = args[i]->val();
      if (!args[i]->null_value)
      {
	f_args.args[i]=to;
	to+= ALIGN_SIZE(sizeof(double));
      }
      break;
    }
  }
  return 0;
}


double Item_func_udf_float::val()
{
  uchar is_null=0;
  DBUG_ENTER("Item_func_udf_float::val");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));

  if (get_arguments())
    DBUG_RETURN(0);
  double (*udf)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)=
    (double (*)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)) u_d->func;
  double tmp=udf(&initid, &f_args, &is_null, &error);
  if (is_null || error)
  {
    null_value=1;
    return 0.0;
  }
  DBUG_RETURN(tmp);
}


String *Item_func_udf_float::str(String *str)
{
  double nr=val();
  if (null_value)
    return 0; /* purecov: inspected */
  else
    str->set(nr,decimals);
  return str;
}


longlong Item_func_udf_int::val_int()
{
  uchar is_null=0;
  DBUG_ENTER("Item_func_udf_int::val_int");
  DBUG_PRINT("info",("result_type: %d  arg_count: %d",
		     args[0]->result_type(), arg_count));

  if (get_arguments())
    DBUG_RETURN(0);
  longlong (*udf)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)=
    (longlong (*)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)) u_d->func;
  longlong tmp=udf(&initid, &f_args, &is_null, &error);
  if (is_null || error)
  {
    null_value=1;
    return 0LL;
  }
  DBUG_RETURN(tmp);
}


String *Item_func_udf_int::str(String *str)
{
  longlong nr=val_int();
  if (null_value)
    return 0;
  else
    str->set(nr);
  return str;
}

/* Default max_length is max argument length */

void Item_func_udf_str::fix_length_and_dec()
{
  DBUG_ENTER("Item_func_udf_str::fix_length_and_dec");
  max_length=0;
  for (uint i = 0; i < arg_count; i++)
    set_if_bigger(max_length,args[i]->max_length);
  DBUG_VOID_RETURN;
}


String *Item_func_udf_str::str(String *str)
{
  uchar is_null=0;
  ulong res_length;
  if (get_arguments())
  {
    null_value=1;
    return 0;
  }
  char * (*udf)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *)=
    (char* (*)(UDF_INIT *, UDF_ARGS *, char *, ulong *, uchar *, uchar *))
    u_d->func;

  if ((res_length=str->alloced_length()) < MAX_FIELD_WIDTH)
  {						// This happens VERY seldom
    if (str->alloc(MAX_FIELD_WIDTH))
    {
      error=1;
      null_value=1;
      return 0;
    }
  }
  char *res=udf(&initid, &f_args, (char*) str->ptr(), &res_length, &is_null,
		&error);
  if (is_null || !res || error)			// The !res is for safety
  {
    null_value=1;
    return 0;
  }
  if (res == str->ptr())
  {
    str->length(res_length);
    return str;
  }
  str_value.set(res, res_length);
  return &str_value;
}

#endif /* HAVE_DLOPEN */

/*
** User level locks
*/

pthread_mutex_t LOCK_user_locks;
static HASH hash_user_locks;

class ULL
{
  char *key;
  uint key_length;

public:
  int count;
  bool locked;
  pthread_cond_t cond;
  pthread_t thread;

  ULL(const char *key_arg,uint length) :key_length(length),count(1),locked(1)
  {
    key=(char*) my_memdup((byte*) key_arg,length,MYF(0));
    pthread_cond_init(&cond,NULL);
    if (key)
    {
      if (hash_insert(&hash_user_locks,(byte*) this))
      {
	my_free((gptr) key,MYF(0));
	key=0;
      }
    }
  }
  ~ULL()
  {
    if (key)
    {
      hash_delete(&hash_user_locks,(byte*) this);
      my_free((gptr) key,MYF(0));
    }
    pthread_cond_destroy(&cond);
  }
  inline bool initialized() { return key != 0; }
  friend void item_user_lock_release(ULL *ull);
  friend char *ull_get_key(const ULL *ull,uint *length,my_bool not_used);
};

char *ull_get_key(const ULL *ull,uint *length,my_bool not_used)
{
  *length=(uint) ull->key_length;
  return (char*) ull->key;
}

void item_user_lock_init(void)
{
  pthread_mutex_init(&LOCK_user_locks,NULL);
  hash_init(&hash_user_locks,16,0,0,(hash_get_key) ull_get_key,NULL,0);
}

void item_user_lock_free(void)
{
  hash_free(&hash_user_locks);
}

void item_user_lock_release(ULL *ull)
{
  ull->locked=0;
  if (--ull->count)
    pthread_cond_signal(&ull->cond);
  else
    delete ull;
}

/*
  Get a user level lock. If the thread has an old lock this is first released.
  Returns 1:  Got lock
  Returns 0:  Timeout
  Returns NULL: Error
*/

#ifndef ETIME
#define ETIME ETIMEDOUT				// For FreeBSD
#endif

longlong Item_func_get_lock::val_int()
{
  String *res=args[0]->str(&value);
  longlong timeout=args[1]->val_int();
  struct timespec abstime;
  THD *thd=current_thd;
  ULL *ull;
  int error;

  pthread_mutex_lock(&LOCK_user_locks);

  if (!res || !res->length())
  {
    pthread_mutex_unlock(&LOCK_user_locks);
    null_value=1;
    return 0;
  }
  null_value=0;

  if (thd->ull)
  {
    item_user_lock_release(thd->ull);
    thd->ull=0;
  }

  if (!(ull= ((ULL*) hash_search(&hash_user_locks,(byte*) res->ptr(),res->length()))))
  {
    ull=new ULL(res->ptr(),res->length());
    if (!ull || !ull->initialized())
    {
      delete ull;
      pthread_mutex_unlock(&LOCK_user_locks);
      null_value=1;				// Probably out of memory
      return 0;
    }
    ull->thread=thd->real_id;
    thd->ull=ull;
    pthread_mutex_unlock(&LOCK_user_locks);
    return 1;					// Got new lock
  }
  ull->count++;

  /* structure is now initialized.  Try to get the lock */
  /* Set up control struct to allow others to abort locks */
  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info="User lock";
  thd->mysys_var->current_mutex= &LOCK_user_locks;
  thd->mysys_var->current_cond=  &ull->cond;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  abstime.tv_sec=time((time_t*) 0)+(time_t) timeout;
  abstime.tv_nsec=0;
  while ((error=pthread_cond_timedwait(&ull->cond,&LOCK_user_locks,&abstime))
	 != ETIME && error != ETIMEDOUT && ull->locked)
  {
    if (thd->killed || abort_loop)
    {
      error=EINTR;				// Return NULL
      break;
    }
  }
  if (ull->locked)
  {
    if (!--ull->count)
      delete ull;				// Should never happen
    if (error != ETIME && error != ETIMEDOUT)
    {
      error=1;
      null_value=1;				// Return NULL
    }
  }
  else
  {
    ull->locked=1;
    ull->thread=thd->real_id;
    thd->ull=ull;
    error=0;
  }
  pthread_mutex_unlock(&LOCK_user_locks);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->proc_info=0;
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond=  0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  return !error ? 1 : 0;
}


/*
** Release a user level lock.
** Returns 1 if lock released
** 0 if lock wasn't held
** NULL if no such lock
*/

longlong Item_func_release_lock::val_int()
{
  String *res=args[0]->str(&value);
  ULL *ull;
  longlong result;
  if (!res || !res->length())
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  result=0;
  pthread_mutex_lock(&LOCK_user_locks);
  if (!(ull= ((ULL*) hash_search(&hash_user_locks,(byte*) res->ptr(),res->length()))))
  {
    null_value=1;
  }
  else
  {
    if (ull->locked && pthread_equal(pthread_self(),ull->thread))
    {
      result=1;					// Release is ok
      item_user_lock_release(ull);
      current_thd->ull=0;
    }
  }
  pthread_mutex_unlock(&LOCK_user_locks);
  return result;
}


longlong Item_func_set_last_insert_id::val_int()
{
  longlong value=args[0]->val_int();
  current_thd->insert_id(value);
  null_value=args[0]->null_value;
  return value;
}
