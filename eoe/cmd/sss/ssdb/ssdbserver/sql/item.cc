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

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>

/*****************************************************************************
** Item functions
*****************************************************************************/

/* Init all special items */

void item_init(void)
{
  item_user_lock_init();
}

Item::Item()
{
  marker=0;
  binary=maybe_null=null_value=with_sum_func=0;
  name=0;
  decimals=0; max_length=0;
  next=current_thd->free_list;			// Put in free list
  current_thd->free_list=this;
}

void Item::set_name(char *str,uint length)
{
  if (!length)
    name=str;					// Used by AS
  else
  {
    while (length && !isgraph(*str))
    {						// Fix problem with yacc
      length--;
      str++;
    }
    name=sql_strmake(str,length);
  }
}

bool Item::eq(const Item *item) const		// Only doing this on conds
{
  return type() == item->type() && name && item->name &&
    !my_strcasecmp(name,item->name);
}


Item_field::Item_field(Field *f) :Item_ident(NullS,f->table_name,f->field_name)
{
  set_field(f);
}


void Item_field::set_field(Field *field_par)
{
  field=result_field=field_par;			// for easy coding with fields
  maybe_null=field->maybe_null();
  max_length=field_par->field_length;
  decimals= ((field->result_type() == REAL_RESULT) ?
	     ((Field_num*) field)->decimals : 0);
  table_name=field_par->table_name;
  field_name=field_par->field_name;
  binary=field_par->binary();
}

const char *Item_ident::full_name() const
{
  char *tmp;
  if (!table_name)
    return field_name;
  if (db_name)
  {
    tmp=(char*) sql_alloc(strlen(db_name)+strlen(table_name)+
			  strlen(field_name)+3);
    strxmov(tmp,table_name,".",field_name,"@",db_name,NullS);
  }
  else
  {
    tmp=(char*) sql_alloc(strlen(table_name)+strlen(field_name)+2);
    strxmov(tmp,table_name,".",field_name,NullS);
  }
  return tmp;
}

/* ARGSUSED */
String *Item_field::str(String *str)
{
  if ((null_value=field->is_null()))
    return 0;
  return field->val_str(str,&str_value);
}

double Item_field::val()
{
  if ((null_value=field->is_null()))
    return 0.0;
  return field->val_real();
}

longlong Item_field::val_int()
{
  if ((null_value=field->is_null()))
    return 0;
  return field->val_int();
}


String *Item_field::str_result(String *str)
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_str(str,&str_value);
}

double Item_field::val_result()
{
  if ((null_value=result_field->is_null()))
    return 0.0;
  return result_field->val_real();
}

longlong Item_field::val_int_result()
{
  if ((null_value=result_field->is_null()))
    return 0;
  return result_field->val_int();
}

bool Item_field::eq(const Item *item) const
{
  return item->type() == type() && ((Item_field*) item)->field == field;
}

table_map Item_field::used_tables() const
{
  if (field->table->const_table)
    return 0;					// const item
  return field->table->map;
}


String *Item_int::str(String *str)
{
  str->set(value);
  return str;
}

void Item_int::print(String *str)
{
  if (!name)
  {
    str_value.set(value);
    name=str_value.c_ptr();
  }
  str->append(name);
}


String *Item_real::str(String *str)
{
  str->set(value,decimals);
  return str;
}

void Item_string::print(String *str)
{
  str->append('\'');
  str->append(full_name());
  str->append('\'');
}

bool Item_null::eq(const Item *item) const { return item->type() == type(); }
double Item_null::val() { null_value=1; return 0.0; }
longlong Item_null::val_int() { null_value=1; return 0; }
/* ARGSUSED */
String *Item_null::str(String *str __attribute__((unused)))
{ null_value=1; return 0;}


void Item_copy_string::copy()
{
  String *res=item->str(&str_value);
  if (res && res != &str_value)
    str_value.copy(*res);
  null_value=item->null_value;
}

/* ARGSUSED */
String *Item_copy_string::str(String *str __attribute__((unused)))
{
  if (null_value)
    return (String*) 0;
  return &str_value;
}


Item_avg_field::Item_avg_field(Item_sum_avg *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  field=item->result_field;
  maybe_null=1;
}

double Item_avg_field::val()
{
  double nr;
  longlong count;
  doubleget(nr,field->ptr);
  char *res=(field->ptr+sizeof(double));
  longlongget(count,res);

  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  return nr/(double) count;
}

String *Item_avg_field::str(String *str)
{
  double nr=Item_avg_field::val();
  if (null_value)
    return 0;
  str->set(nr,decimals);
  return str;
}

Item_std_field::Item_std_field(Item_sum_std *item)
{
  name=item->name;
  decimals=item->decimals;
  max_length=item->max_length;
  field=item->result_field;
  maybe_null=1;
}

double Item_std_field::val()
{
  double nr,nr2;
  ulong count;
  doubleget(nr,field->ptr);
  doubleget(nr2,(field->ptr+sizeof(double)));
  ulongget(count,(field->ptr+sizeof(double)*2));

  if (!count)
  {
    null_value=1;
    return 0.0;
  }
  return sqrt((nr2-nr*nr/(double) count)/(double) count);
}

String *Item_std_field::str(String *str)
{
  double nr=val();
  if (null_value)
    return 0;
  str->set(nr,decimals);
  return str;
}

/*
** Functions to convert item to field (for send_fields)
*/

/* ARGSUSED */
bool Item::fix_fields(THD *thd __attribute__((unused)),
		      struct st_table_list *list __attribute__((unused)))
{
  return 0;
}

bool Item_field::fix_fields(THD *thd,TABLE_LIST *tables)
{
  if (!field)
  {
    Field *tmp;
    if (!(tmp=find_field_in_tables(thd,this,tables)))
      return 1;
    set_field(tmp);
  }
  return 0;
}


void Item::init_make_field(Send_field *tmp_field,enum enum_field_types type)
{
  tmp_field->table_name="";
  tmp_field->col_name=name;
  tmp_field->flags=maybe_null ? 0 : NOT_NULL_FLAG;
  tmp_field->type=type;
  tmp_field->length=max_length;
  tmp_field->decimals=decimals;
}

/* ARGSUSED */
void Item_field::make_field(Send_field *tmp_field __attribute__((unused)))
{
  field->make_field(tmp_field);
  if (name)
    tmp_field->col_name=name;			// Use user supplied name
}

void Item_int::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_LONGLONG);
}

void Item_real::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

void Item_string::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}

void Item_null::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_NULL);
  tmp_field->length=4;
}

void Item_func::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field, ((result_type() == STRING_RESULT) ?
			      FIELD_TYPE_VAR_STRING :
			      (result_type() == INT_RESULT) ?
			      FIELD_TYPE_LONGLONG : FIELD_TYPE_DOUBLE));
}

void Item_avg_field::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

void Item_std_field::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_DOUBLE);
}

/*
** Set a field:s value from a item
*/


void Item_field::save_org_in_field(Field *to)
{
  if (field->is_null())
  {
    null_value=1;
    set_field_to_null(to);
  }
  else
  {
    to->set_notnull();
    field_conv(to,field);
    null_value=0;
  }
}

bool Item_field::save_in_field(Field *to)
{
  if (result_field->is_null())
  {
    null_value=1;
    return set_field_to_null(to);
  }
  else
  {
    to->set_notnull();
    field_conv(to,result_field);
    null_value=0;
  }
  return 0;
}


bool Item_null::save_in_field(Field *field)
{
  return set_field_to_null(field);
}


bool Item::save_in_field(Field *field)
{
  if (result_type() == STRING_RESULT ||
      result_type() == REAL_RESULT &&
      field->result_type() == STRING_RESULT)
  {
    String *result;
    char buff[MAX_FIELD_WIDTH];		// Alloc buffer for small columns
    str_value.set_quick(buff,sizeof(buff));
    result=str(&str_value);
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    field->store(result->ptr(),result->length());
  }
  else if (result_type() == REAL_RESULT)
  {
    double nr=val();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    field->store(nr);
  }
  else
  {
    longlong nr=val_int();
    if (null_value)
      return set_field_to_null(field);
    field->set_notnull();
    field->store(nr);
  }
  return 0;
}

bool Item_string::save_in_field(Field *field)
{
  String *result;
  result=str(&str_value);
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  field->store(result->ptr(),result->length());
  return 0;
}

bool Item_int::save_in_field(Field *field)
{
  longlong nr=val_int();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  field->store(nr);
  return 0;
}

bool Item_real::save_in_field(Field *field)
{
  double nr=val();
  if (null_value)
    return set_field_to_null(field);
  field->set_notnull();
  field->store(nr);
  return 0;
}

/****************************************************************************
** varbinary item
** In string context this is a binary string
** In number context this is a longlong value.
****************************************************************************/

static inline uint char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}

Item_varbinary::Item_varbinary(const char *str, uint str_length)
{
  name=(char*) str-2;				// Lex makes this start with 0x
  max_length=(str_length+1)/2;
  char *ptr=sql_alloc(max_length+1);
  if (!ptr)
    return;
  str_value.set(ptr,max_length);
  char *end=ptr+max_length;
  if (max_length*2 != str_length)
    *ptr++=char_val(*str++);			// Not even, assume 0 prefix
  while (ptr != end)
  {
    *ptr++= (char) (char_val(str[0])*16+char_val(str[1]));
    str+=2;
  }
  *ptr=0;					// Keep purify happy
  binary=1;					// Binary is default
}

longlong Item_varbinary::val_int()
{
  char *end=(char*) str_value.ptr()+str_value.length(),
       *ptr=end-min(str_value.length(),sizeof(longlong));

  ulonglong value=0;
  for (; ptr != end ; ptr++)
    value=(value << 8)+ (ulonglong) (uchar) *ptr;
  return (longlong) value;
}


bool Item_varbinary::save_in_field(Field *field)
{
  field->set_notnull();
  if (field->result_type() == STRING_RESULT)
  {
    field->store(str_value.ptr(),str_value.length());
  }
  else
  {
    longlong nr=val_int();
    field->store(nr);
  }
  return 0;
}


void Item_varbinary::make_field(Send_field *tmp_field)
{
  init_make_field(tmp_field,FIELD_TYPE_STRING);
}

/*
** pack data in buffer for sending
*/

bool Item::send(String *packet)
{
  char buff[MAX_FIELD_WIDTH];
  String s(buff,sizeof(buff)),*res;
  if (!(res=str(&s)))
    return net_store_null(packet);
  CONVERT *convert;
  if ((convert=current_thd->convert_set))
    return convert->store(packet,res->ptr(),res->length());
  return net_store_data(packet,res->ptr(),res->length());
}

bool Item_null::send(String *packet)
{
  return net_store_null(packet);
}

/*
  This is used for HAVING clause
  Find field in select list having the same name
 */

bool Item_ref::fix_fields(THD *thd,TABLE_LIST *tables __attribute__((unused)))
{
  if (!ref)
  {
    if (!(ref=find_item_in_list(this,thd->item_list)))
      return 1;
    max_length= (*ref)->max_length;
    maybe_null= (*ref)->maybe_null;
    decimals=	(*ref)->decimals;
    binary=	(*ref)->binary;
  }
  return 0;
}

/*
** If item is a const function, calculate it and return a const item
** The original item is freed if not returned
*/

Item_result item_cmp_type(Item_result a,Item_result b)
{
  if (a == STRING_RESULT && b == STRING_RESULT)
    return STRING_RESULT;
  else if (a == INT_RESULT && b == INT_RESULT)
    return INT_RESULT;
  else
    return REAL_RESULT;
}


Item *resolve_const_item(Item *item,Item *cmp_item)
{
  if (item->basic_const_item())
    return item;				// Can't be better
  Item_result res_type=item_cmp_type(cmp_item->result_type(),
				     item->result_type());
  char *name=item->name;			// Alloced by sql_alloc

  if (res_type == STRING_RESULT)
  {
    char buff[MAX_FIELD_WIDTH];
    String tmp(buff,sizeof(buff)),*result;
    result=item->str(&tmp);
    if (item->null_value)
    {
#ifdef DELETE_ITEMS
      delete item;
#endif
      return new Item_null(name);
    }
    uint length=result->length();
    char *tmp_str=sql_strmake(result->ptr(),length);
    delete item;
    return new Item_string(name,tmp_str,length);
  }
  if (res_type == INT_RESULT)
  {
    longlong result=item->val_int();
    uint length=item->max_length;
    bool null_value=item->null_value;
#ifdef DELETE_ITEMS
    delete item;
#endif
    return (null_value ? (Item*) new Item_null(name) :
	    (Item*) new Item_int(name,result,length));
  }
  else
  {						// It must REAL_RESULT
    double result=item->val();
    uint length=item->max_length,decimals=item->decimals;
    bool null_value=item->null_value;
#ifdef DELETE_ITEMS
    delete item;
#endif
    return (null_value ? (Item*) new Item_null(name) :
	    (Item*) new Item_real(name,result,decimals,length));
  }
}



/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item>;
template class List_iterator<Item>;
template class List<List_item>;
#endif
