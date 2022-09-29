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

/* This file defines all compare functions */

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>

/*
** Test functions
** These returns 0LL if false and 1LL if true and null if some arg is null
** 'AND' and 'OR' never return null
*/

longlong Item_func_not::val_int()
{
  double val=args[0]->val();
  null_value=args[0]->null_value;
  return !null_value && val == 0 ? 1 : 0;
}

void Item_bool_func2::fix_length_and_dec()
{
  decimals=0; max_length=1;
  // Make a special case of compare with fields to get nicer DATE comparisons

  if (args[0]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[0])->field;
    if (field->store_for_compare() && args[1]->const_item())
    {
      Item *item=args[1];
      item->save_in_field(field);
      if (!item->null_value)
      {
	args[1]=new Item_int(field->val_int());
#ifdef DELETE_ITEMS
	delete item;
#endif
	cmp_type=INT_RESULT;			// Works for all types.
	return;
      }
    }
  }
  if (args[1]->type() == FIELD_ITEM)
  {
    Field *field=((Item_field*) args[1])->field;
    if (field->store_for_compare() && args[0]->const_item())
    {
      Item *item=args[0];
      item->save_in_field(field);
      if (!item->null_value)
      {
	args[0]=new Item_int(field->val_int());
#ifdef DELETE_ITEMS
	delete item;
#endif
	cmp_type=INT_RESULT;			// Works for all types.
	return;
      }
    }
  }
  cmp_type=item_cmp_type(args[0]->result_type(),args[1]->result_type());
}


int Item_bool_func2::compare()
{
  null_value=0;
  if (cmp_type == STRING_RESULT)
  {
    String *res1,*res2;
    res1=args[0]->str(&tmp_value1);
    res2=args[1]->str(&tmp_value2);
    if (!args[0]->null_value && !args[1]->null_value)
    {
      return binary ? stringcmp(res1,res2) : sortcmp(res1,res2);
    }
  }
  else if (cmp_type == REAL_RESULT)
  {
    double val1=args[0]->val();
    double val2=args[1]->val();

    if (!args[0]->null_value && !args[1]->null_value)
    {
      if (val1 < val2)	return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  else
  {
    longlong val1=args[0]->val_int();
    longlong val2=args[1]->val_int();

    if (!args[0]->null_value && !args[1]->null_value)
    {
      if (val1 < val2)	return -1;
      if (val1 == val2) return 0;
      return 1;
    }
  }
  null_value=1;
  return 0;
}


longlong Item_func_eq::val_int()
{
  int val=compare();
#ifdef mSQL_COMPLIANT
  // mSQL returns true if one compares NULL_value=NULL constant
  if (null_value)
  {
    if (args[0]->type() == NULL_ITEM ||
	args[1]->type() == NULL_ITEM)
    {						// Compare with NULL const
      if (args[0]->null_value && args[1]->null_value)
      {
	null_value=0;
	return 1;
      }
    }
    return 0;
  }
#endif
  return val == 0 && !null_value ? 1 : 0;
}


longlong Item_func_ne::val_int()
{
  int val=compare();
#ifdef mSQL_COMPLIANT
  // mSQL returns true if one compares NOT NULL_value <> NULL constant
  if (null_value)
  {
    if (args[0]->type() == NULL_ITEM ||
	args[1]->type() == NULL_ITEM)
    {						// Compare with NULL const
      null_value=0;
      return (args[0]->null_value && args[1]->null_value) ? 0 : 1;
    }
    return 0;
  }
#endif
  return val != 0 && !null_value ? 1 : 0;
}


longlong Item_func_ge::val_int()
{
  int val=compare();
  return val >= 0 && !null_value ? 1 : 0;
}


longlong Item_func_gt::val_int()
{
  int val=compare();
  return val > 0 ? 1 : 0;
}

longlong Item_func_le::val_int()
{
  int val=compare();
  return val <= 0 && !null_value ? 1 : 0;
}


longlong Item_func_lt::val_int()
{
  int val=compare();
  return val < 0 ? 1 : 0;
}


longlong Item_func_strcmp::val_int()
{
  String *a=args[0]->str(&tmp_value1);
  String *b=args[1]->str(&tmp_value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  int val=stringcmp(a,b);
  null_value=0;
  return !val ? 0 : (val < 0 ? (longlong) -1 : (longlong) 1);
}


void Item_func_interval::fix_length_and_dec()
{
  bool nums=1;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (args[i]->type() != Item::INT_ITEM &&
	args[i]->type() != Item::REAL_ITEM)
    {
      nums=0;
      break;
    }
  }
  if (nums && arg_count >= 8)
  {
    intervals=(double*) sql_alloc(sizeof(double)*arg_count);
    for (uint i=0 ; i < arg_count ; i++)
      intervals[i]=args[i]->val();
  }
  maybe_null=0; max_length=2;
  used_tables_cache|=item->used_tables();
}

/*
  return -1 if null value,
	  0 if lower than lowest
	  1 - arg_count if between args[n] and args[n+1]
	  arg_count+1 if higher than biggest argument
*/

longlong Item_func_interval::val_int()
{
  double value=item->val();
  if (item->null_value)
    return -1;				// -1 if null /* purecov: inspected */
  if (intervals)
  {					// Use binary search to find interval
    uint start,end;
    start=0; end=arg_count-1;
    while (start != end)
    {
      uint mid=(start+end+1)/2;
      if (intervals[mid] <= value)
	start=mid;
      else
	end=mid-1;
    }
    return (value < intervals[start]) ? 0 : start+1;
  }
  if (args[0]->val() > value)
    return 0;
  for (uint i=1 ; i < arg_count ; i++)
  {
    if (args[i]->val() > value)
      return i;
  }
  return (longlong) arg_count;
}


void Item_func_interval::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
}

void Item_func_between::fix_length_and_dec()
{
  max_length=1;
  if (args[0]->binary)
    string_compare=stringcmp;
  else
    string_compare=sortcmp;
}

longlong Item_func_between::val_int()
{						// ANSI BETWEEN
  if (args[0]->result_type() == STRING_RESULT)
  {
    String *value,*a,*b;
    value=args[0]->str(&value0);
    if ((null_value=args[0]->null_value))
      return 0;
    a=args[1]->str(&value1);
    b=args[2]->str(&value2);
    if (!args[1]->null_value && !args[2]->null_value)
      return (string_compare(value,a) >= 0 && string_compare(value,b) <= 0) ?
	1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= string_compare(value,b) <= 0; // not null if false range.
    }
    else
    {
      null_value= string_compare(value,a) >= 0; // not null if false range.
    }
  }
  else if (args[0]->result_type() == INT_RESULT)
  {
    longlong value=args[0]->val_int(),a,b;
    if ((null_value=args[0]->null_value))
      return 0; /* purecov: inspected */
    a=args[1]->val_int();
    b=args[2]->val_int();
    if (!args[1]->null_value && !args[2]->null_value)
      return (value >= a && value <= b) ? 1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  else
  {
    double value=args[0]->val(),a,b;
    if ((null_value=args[0]->null_value))
      return 0; /* purecov: inspected */
    a=args[1]->val();
    b=args[2]->val();
    if (!args[1]->null_value && !args[2]->null_value)
      return (value >= a && value <= b) ? 1 : 0;
    if (args[1]->null_value && args[2]->null_value)
      null_value=1;
    else if (args[1]->null_value)
    {
      null_value= value <= b;			// not null if false range.
    }
    else
    {
      null_value= value >= a;
    }
  }
  return 0;
}

void
Item_func_ifnull::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null;
  max_length=max(args[0]->max_length,args[1]->max_length);
  decimals=max(args[0]->decimals,args[1]->decimals);
  cached_result_type=args[0]->result_type();
}

double
Item_func_ifnull::val()
{
  double value=args[0]->val();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val();
  if ((null_value=args[1]->null_value))
    return 0.0;
  return value;
}

longlong
Item_func_ifnull::val_int()
{
  longlong value=args[0]->val_int();
  if (!args[0]->null_value)
  {
    null_value=0;
    return value;
  }
  value=args[1]->val_int();
  if ((null_value=args[1]->null_value))
    return 0;
  return value;
}

String *
Item_func_ifnull::str(String *str)
{
  String *res  =args[0]->str(str);
  if (!args[0]->null_value)
  {
    null_value=0;
    return res;
  }
  res=args[1]->str(str);
  if ((null_value=args[1]->null_value))
    return 0;
  return res;
}

void
Item_func_if::fix_length_and_dec()
{
  maybe_null=args[1]->maybe_null || args[2]->maybe_null;
  max_length=max(args[1]->max_length,args[2]->max_length);
  decimals=max(args[0]->decimals,args[1]->decimals);
  cached_result_type = args[1]->result_type();
}


double
Item_func_if::val()
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  double value=arg->val();
  null_value=arg->null_value;
  return value;
}

longlong
Item_func_if::val_int()
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  longlong value=arg->val_int();
  null_value=arg->null_value;
  return value;
}

String *
Item_func_if::str(String *str)
{
  Item *arg= args[0]->val_int() ? args[1] : args[2];
  String *res=arg->str(str);
  null_value=arg->null_value;
  return res;
}

/****************************************************************************
** classes and function for the IN operator
****************************************************************************/

static int cmp_longlong(longlong *a,longlong *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

static int cmp_double(double *a,double *b)
{
  return *a < *b ? -1 : *a == *b ? 0 : 1;
}

int in_vector::find(Item *item)
{
  byte *result=get_value(item);
  if (!result || !used_count)
    return 0;				// Null value

  uint start,end;
  start=0; end=used_count-1;
  while (start != end)
  {
    uint mid=(start+end+1)/2;
    int res;
    if ((res=(*compare)(base+mid*size,result)) == 0)
      return 1;
    if (res < 0)
      start=mid;
    else
      end=mid-1;
  }
  return (int) ((*compare)(base+start*size,result) == 0);
}


in_string::in_string(uint elements,qsort_cmp cmp_func)
  :in_vector(elements,sizeof(String),cmp_func),tmp(buff,sizeof(buff))
{}

in_string::~in_string()
{
  for (uint i=0 ; i < count ; i++)
    ((String*) base)[i].free();
}

void in_string::set(uint pos,Item *item)
{
  String *str=((String*) base)+pos;
  String *res=item->str(str);
  if (res && res != str)
    *str= *res;
}

byte *in_string::get_value(Item *item)
{
  return (byte*) item->str(&tmp);
}


in_longlong::in_longlong(uint elements)
  :in_vector(elements,sizeof(longlong),(qsort_cmp) cmp_longlong)
{}

void in_longlong::set(uint pos,Item *item)
{
  ((longlong*) base)[pos]=item->val_int();
}

byte *in_longlong::get_value(Item *item)
{
  tmp=item->val_int();
  if (item->null_value)
    return 0; /* purecov: inspected */
  return (byte*) &tmp;
}


in_double::in_double(uint elements)
  :in_vector(elements,sizeof(double),(qsort_cmp) cmp_double)
{}

void in_double::set(uint pos,Item *item)
{
  ((double*) base)[pos]=item->val();
}

byte *in_double::get_value(Item *item)
{
  tmp=item->val();
  if (item->null_value)
    return 0; /* purecov: inspected */
  return (byte*) &tmp;
}


void Item_func_in::fix_length_and_dec()
{
  if (const_item())
  {
    switch (item->result_type()) {
    case STRING_RESULT:
      if (item->binary)
	array=new in_string(arg_count,(qsort_cmp) stringcmp); /* purecov: inspected */
      else
	array=new in_string(arg_count,(qsort_cmp) sortcmp);
      break;
    case INT_RESULT:
      array= new in_longlong(arg_count);
      break;
    case REAL_RESULT:
      array= new in_double(arg_count);
      break;
    }
    uint j=0;
    for (uint i=0 ; i < arg_count ; i++)
    {
      array->set(j,args[i]);
      if (!args[i]->null_value)			// Skipp NULL values
	j++;
    }
    if ((array->used_count=j))
      array->sort();
  }
  else
  {
    switch (item->result_type()) {
    case STRING_RESULT:
      if (item->binary)
	in_item= new cmp_item_binary_string;
      else
	in_item= new cmp_item_sort_string;
      break;
    case INT_RESULT:
      in_item=	  new cmp_item_int;
      break;
    case REAL_RESULT:
      in_item=	  new cmp_item_real;
      break;
    }
  }
  maybe_null=0; max_length=2;
  used_tables_cache|=item->used_tables();
}


void Item_func_in::print(String *str)
{
  str->append('(');
  item->print(str);
  Item_func::print(str);
  str->append(')');
}


longlong Item_func_in::val_int()
{
  if (array)
    return array->find(item);

  in_item->store_value(item);
  if (item->null_value)
    return 0;
  for (uint i=0 ; i < arg_count ; i++)
  {
    if (!in_item->cmp(args[i]) && !args[i]->null_value)
      return 1;					// Would maybe be nice with i ?
  }
  return 0;
}


void Item_func_in::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
}


longlong Item_func_bit_or::val_int()
{
  ulonglong arg1= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  return (longlong) (arg1 | arg2);
}


longlong Item_func_bit_and::val_int()
{
  ulonglong arg1= (ulonglong) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  ulonglong arg2= (ulonglong) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  return (longlong) (arg1 & arg2);
}


bool
Item_cond::fix_fields(THD *thd,TABLE_LIST *tables)
{
  List_iterator<Item> li(list);
  Item *item;
  char buff[sizeof(char*)];			// Max local vars in function
  used_tables_cache=0;

  if (thd && check_stack_overrun(thd,buff))
    return 0;					// Fatal error flag is set!
  while ((item=li++))
  {
    while (item->type() == Item::COND_ITEM &&
	   ((Item_cond*) item)->functype() == functype())
    {						// Identical function
      li.replace(((Item_cond*) item)->list);
      ((Item_cond*) item)->list.empty();
#ifdef DELETE_ITEMS
      delete (Item_cond*) item;
#endif
      item= *li.ref();				// new current item
    }
    if (item->fix_fields(thd,tables))
      return 1; /* purecov: inspected */
    used_tables_cache|=item->used_tables();
    with_sum_func= with_sum_func || item->with_sum_func;
  }
  if (thd)
    thd->cond_count+=list.elements;
  fix_length_and_dec();
  return 0;
}


void Item_cond::split_sum_func(List<Item> &fields)
{
  List_iterator<Item> li(list);
  Item *item;
  while ((item=li++))
  {
    if (item->with_sum_func && item->type() != SUM_FUNC_ITEM)
      item->split_sum_func(fields);
    else if (item->used_tables() || item->type() == SUM_FUNC_ITEM)
    {
      fields.push_front(item);
      li.replace(new Item_ref((Item**) fields.head_ref(),0,item->name));
    }
    item->update_used_tables();
    used_tables_cache|=item->used_tables();
  }
}


ulong
Item_cond::used_tables() const
{						// This caches used_tables
  return used_tables_cache;
}

void Item_cond::update_used_tables()
{
  used_tables_cache=0;
  List_iterator<Item> li(list);
  Item *item;
  while ((item=li++))
  {
    item->update_used_tables();
    used_tables_cache|=item->used_tables();
  }
}


void Item_cond::print(String *str)
{
  str->append('(');
  List_iterator<Item> li(list);
  Item *item;
  if ((item=li++))
    item->print(str);
  while ((item=li++))
  {
    str->append(' ');
    str->append(func_name());
    str->append(' ');
    item->print(str);
  }
  str->append(')');
}


longlong Item_cond_and::val_int()
{
  List_iterator<Item> li(list);
  Item *item;
  while ((item=li++))
    if (item->val_int() == 0)
      return 0;
  return 1;
}

longlong Item_cond_or::val_int()
{
  List_iterator<Item> li(list);
  Item *item;
  while ((item=li++))
    if (item->val_int() != 0)
      return 1;
  return 0;
}

longlong Item_func_isnull::val_int()
{
  (void) args[0]->val();
  return (args[0]->null_value) ? 1 : 0;
}

longlong Item_func_isnotnull::val_int()
{
  (void) args[0]->val();
  return !(args[0]->null_value) ? 1 : 0;
}


void Item_func_like::fix_length_and_dec()
{
  decimals=0; max_length=1;
  cmp_type=STRING_RESULT;			// For quick select
}


longlong Item_func_like::val_int()
{
  String *res,*res2;
  res=args[0]->str(&tmp_value1);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  res2=args[1]->str(&tmp_value2);
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  if (binary)
    return wild_compare(*res,*res2,escape) ? 0 : 1;
  else
    return wild_case_compare(*res,*res2,escape) ? 0 : 1;
}


/* We can optimize a where if first character isn't a wildcard */

Item_func::optimize_type Item_func_like::select_optimize() const
{
  if (args[1]->type() == STRING_ITEM)
  {
    if (((Item_string *) args[1])->str_value[0] != wild_many)
    {
      if ((args[0]->result_type() != STRING_RESULT) ||
	  ((Item_string *) args[1])->str_value[0] != wild_one)
	return OPTIMIZE_OP;
    }
  }
  return OPTIMIZE_NONE;
}

#ifdef USE_REGEX

bool
Item_func_regex::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (args[0]->fix_fields(thd,tables) || args[1]->fix_fields(thd,tables))
    return 1;					/* purecov: inspected */
  with_sum_func=args[0]->with_sum_func || args[1]->with_sum_func;;
  max_length=1; decimals=0;
  used_tables_cache=args[0]->used_tables() | args[1]->used_tables();
  if (!regex_compiled && args[1]->const_item())
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff));
    String *res=args[1]->str(&tmp);
    if (args[1]->null_value)
    {
      my_error(ER_INVALID_USE_OF_NULL,MYF(0));
      return 1;
    }
    int error;
    if ((error=regcomp(&preg,res->c_ptr(),REG_EXTENDED | REG_NOSUB)))
    {
      char buff[100];
      (void) regerror(error,&preg,buff,sizeof(buff));
      my_printf_error(ER_REGEXP_ERROR,ER(ER_REGEXP_ERROR),MYF(0),buff);
      return 1;
    }
    regex_compiled=regex_is_const=1;
    maybe_null=args[0]->maybe_null;
  }
  else
    maybe_null=1;
  return 0;
}


longlong Item_func_regex::val_int()
{
  char buff[MAX_FIELD_WIDTH];
  String *res, tmp(buff,sizeof(buff));

  res=args[0]->str(&tmp);
  if (args[0]->null_value)
  {
    null_value=1;
    return 0;
  }
  if (!regex_is_const)
  {
    char buff2[MAX_FIELD_WIDTH];
    String *res2, tmp2(buff2,sizeof(buff2));

    res2= args[1]->str(&tmp2);
    if (args[1]->null_value)
    {
      null_value=1;
      return 0;
    }
    if (!regex_compiled || stringcmp(res2,&prev_regexp))
    {
      prev_regexp.copy(*res2);
      if (regex_compiled)
      {
	regfree(&preg);
	regex_compiled=0;
      }
      if (regcomp(&preg,res2->c_ptr(),REG_EXTENDED | REG_NOSUB))
      {
	null_value=1;
	return 0;
      }
      regex_compiled=1;
    }
  }
  null_value=0;
  return regexec(&preg,res->c_ptr(),0,(regmatch_t*) 0,0) ? 0 : 1;
}


Item_func_regex::~Item_func_regex()
{
  if (regex_compiled)
  {
    regfree(&preg);
    regex_compiled=0;
  }
}

#endif /* USE_REGEX */
