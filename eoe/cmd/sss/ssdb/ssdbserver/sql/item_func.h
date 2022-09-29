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

/* Function items used by mysql */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#ifdef HAVE_IEEEFP_H
extern "C"				/* Bug in BSDI include file */
{
#include <ieeefp.h>
}
#endif

class Item_func :public Item_result_field
{
protected:
  Item **args;
  uint arg_count;
public:
  table_map used_tables_cache;
  enum Functype { UNKNOWN_FUNC,EQ_FUNC,NE_FUNC,LT_FUNC,LE_FUNC,GE_FUNC,GT_FUNC,
		  LIKE_FUNC,NOTLIKE_FUNC,ISNULL_FUNC,ISNOTNULL_FUNC,
		  COND_AND_FUNC,COND_OR_FUNC,BETWEEN,IN_FUNC,INTERVAL_FUNC};
  enum optimize_type { OPTIMIZE_NONE,OPTIMIZE_KEY,OPTIMIZE_OP };
  enum Type type() const { return FUNC_ITEM; }
  virtual enum Functype functype() const   { return UNKNOWN_FUNC; }
  Item_func(void)
  {
    arg_count=0;
  }
  Item_func(Item *a)
  {
    arg_count=1;
    if ((args=(Item**) sql_alloc(sizeof(Item*))))
    {
      args[0]=a;
      with_sum_func=a->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b)
  {
    arg_count=2;
    if ((args=(Item**) sql_alloc(sizeof(Item*)*2)))
    {
      args[0]=a; args[1]=b;
      with_sum_func=a->with_sum_func || b->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b,Item *c)
  {
    arg_count=3;
    if ((args=(Item**) sql_alloc(sizeof(Item*)*3)))
    {
      args[0]=a; args[1]=b; args[2]=c;
      with_sum_func=a->with_sum_func || b->with_sum_func || c->with_sum_func;
    }
  }
  Item_func(Item *a,Item *b,Item *c,Item *d)
  {
    arg_count=4;
    if ((args=(Item**) sql_alloc(sizeof(Item*)*4)))
    {
      args[0]=a; args[1]=b; args[2]=c; args[3]=d;
      with_sum_func=a->with_sum_func || b->with_sum_func || c->with_sum_func ||
	d->with_sum_func;
    }
  }
  Item_func(List<Item> &list);
  ~Item_func();
  bool fix_fields(THD *,struct st_table_list *);
  void make_field(Send_field *field);
  table_map used_tables() const;
  void update_used_tables();
  virtual void fix_length_and_dec()=0;
  virtual optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  virtual Item *key_item() const { return args[0]; }
  virtual const char *func_name() const { return "?"; }
  inline Item **arguments() const { return args; }
  inline uint argument_count() const { return arg_count; }
  inline void remove_arguments() { arg_count=0; }
  virtual void split_sum_func(List<Item> &fields);
  void print(String *str);
  void print_op(String *str);
  void fix_num_length_and_dec();
  bool get_date(TIME *ltime,bool date_only=0);
  bool get_time(TIME *ltime);
};


class Item_real_func :public Item_func
{
public:
  Item_real_func() :Item_func() {}
  Item_real_func(Item *a) :Item_func(a) {}
  Item_real_func(Item *a,Item *b) :Item_func(a,b) {}
  Item_real_func(List<Item> &list) :Item_func(list) {}
  String *str(String*str);
  longlong val_int() { return (longlong) val(); }
  enum Item_result result_type () const { return REAL_RESULT; }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};

class Item_num_func :public Item_func
{
 protected:
  Item_result hybrid_type;
public:
  Item_num_func(Item *a) :Item_func(a),hybrid_type(REAL_RESULT) {}
  Item_num_func(Item *a,Item *b) :Item_func(a,b),hybrid_type(REAL_RESULT) {}
  String *str(String*str);
  longlong val_int() { return (longlong) val(); }
  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_num_op :public Item_func
{
 protected:
  Item_result hybrid_type;
 public:
  Item_num_op(Item *a,Item *b) :Item_func(a,b),hybrid_type(REAL_RESULT) {}
  String *str(String*str);
  void print(String *str) { print_op(str); }
  enum Item_result result_type () const { return hybrid_type; }
  void fix_length_and_dec() { fix_num_length_and_dec(); find_num_type(); }
  void find_num_type(void);
};


class Item_int_func :public Item_func
{
public:
  Item_int_func() :Item_func() {}
  Item_int_func(Item *a) :Item_func(a) {}
  Item_int_func(Item *a,Item *b) :Item_func(a,b) {}
  Item_int_func(Item *a,Item *b,Item *c) :Item_func(a,b,c) {}
  Item_int_func(List<Item> &list) :Item_func(list) {}
  double val() { return (double) val_int(); }
  String *str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};


class Item_func_plus :public Item_num_op
{
public:
  Item_func_plus(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "+"; }
  double val();
  longlong val_int();
};

class Item_func_minus :public Item_num_op
{
public:
  Item_func_minus(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "-"; }
  double val();
  longlong val_int();
};

class Item_func_mul :public Item_num_op
{
public:
  Item_func_mul(Item *a,Item *b) :Item_num_op(a,b) {}
  const char *func_name() const { return "*"; }
  double val();
  longlong val_int();
};


class Item_func_div :public Item_num_op
{
public:
  Item_func_div(Item *a,Item *b) :Item_num_op(a,b) {}
  double val();
  longlong val_int();
  const char *func_name() const { return "/"; }
  void fix_length_and_dec();
};


class Item_func_mod :public Item_num_op
{
public:
  Item_func_mod(Item *a,Item *b) :Item_num_op(a,b) {}
  double val();
  longlong val_int();
  const char *func_name() const { return "%"; }
  void fix_length_and_dec();
};


class Item_func_neg :public Item_num_func
{
public:
  Item_func_neg(Item *a) :Item_num_func(a) {}
  double val();
  longlong val_int();
  const char *func_name() const { return "-"; }
  void fix_length_and_dec();
};


class Item_func_abs :public Item_num_func
{
public:
  Item_func_abs(Item *a) :Item_num_func(a) {}
  const char *func_name() const { return "abs"; }
  double val();
  longlong val_int();
  enum Item_result result_type () const 
  { return args[0]->result_type() == INT_RESULT ? INT_RESULT : REAL_RESULT; }
  void fix_length_and_dec();
};

// A class to handle logaritmic and trigometric functions

class Item_dec_func :public Item_real_func
{
 public:
  Item_dec_func(Item *a) :Item_real_func(a) {}
  Item_dec_func(Item *a,Item *b) :Item_real_func(a,b) {}
  void fix_length_and_dec()
  {
    decimals=6; max_length=float_length(decimals);
    maybe_null=1;
  }
  inline double fix_result(double value)
  {
#ifndef HAVE_FINITE
    return value;
#else
    if (finite(value) && value != POSTFIX_ERROR)
      return value;
    null_value=1;
    return 0.0;
#endif
  }
};

class Item_func_exp :public Item_dec_func
{
public:
  Item_func_exp(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "exp"; }
};

class Item_func_log :public Item_dec_func
{
public:
  Item_func_log(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "log"; }
};


class Item_func_log10 :public Item_dec_func
{
public:
  Item_func_log10(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "log10"; }
};


class Item_func_sqrt :public Item_dec_func
{
public:
  Item_func_sqrt(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "sqrt"; }
};


class Item_func_pow :public Item_dec_func
{
public:
  Item_func_pow(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val();
  const char *func_name() const { return "pow"; }
};


class Item_func_acos :public Item_dec_func
{
 public:
  Item_func_acos(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "acos"; }
};

class Item_func_asin :public Item_dec_func
{
 public:
  Item_func_asin(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "asin"; }
};

class Item_func_atan :public Item_dec_func
{
 public:
  Item_func_atan(Item *a) :Item_dec_func(a) {}
  Item_func_atan(Item *a,Item *b) :Item_dec_func(a,b) {}
  double val();
  const char *func_name() const { return "atan"; }
};

class Item_func_cos :public Item_dec_func
{
 public:
  Item_func_cos(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "cos"; }
};

class Item_func_sin :public Item_dec_func
{
 public:
  Item_func_sin(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "sin"; }
};

class Item_func_tan :public Item_dec_func
{
 public:
  Item_func_tan(Item *a) :Item_dec_func(a) {}
  double val();
  const char *func_name() const { return "tan"; }
};

class Item_func_integer :public Item_int_func
{
public:
  Item_func_integer(Item *a) :Item_int_func(a) {}
  void fix_length_and_dec();
};


class Item_func_ceiling :public Item_func_integer
{
public:
  Item_func_ceiling(Item *a) :Item_func_integer(a) {}
  const char *func_name() const { return "ceiling"; }
  longlong val_int();
};

class Item_func_floor :public Item_func_integer
{
public:
  Item_func_floor(Item *a) :Item_func_integer(a) {}
  const char *func_name() const { return "floor"; }
  longlong val_int();
};

/* This handles round and truncate */

class Item_func_round :public Item_real_func
{
  bool truncate;
public:
  Item_func_round(Item *a,Item *b,bool trunc_arg)
    :Item_real_func(a,b),truncate(trunc_arg) {}
  const char *func_name() const { return truncate ? "truncate" : "round"; }
  double val();
  void fix_length_and_dec();
};


class Item_func_rand :public Item_real_func
{
public:
  Item_func_rand(Item *a) :Item_real_func(a) {}
  Item_func_rand()	  :Item_real_func()  {}
  double val();
  const char *func_name() const { return "rand"; }
  void fix_length_and_dec() { max_length=6; decimals=4; }
};


class Item_func_sign :public Item_int_func
{
public:
  Item_func_sign(Item *a) :Item_int_func(a) {}
  const char *func_name() const { return "sign"; }
  longlong val_int();
};


class Item_func_units :public Item_real_func
{
  char *name;
  double mul,add;
 public:
  Item_func_units(char *name_arg,Item *a,double mul_arg,double add_arg)
    :Item_real_func(a),name(name_arg),mul(mul_arg),add(add_arg) {}
  double val();
  const char *func_name() const { return name; }
  void fix_length_and_dec() { decimals=6; max_length=float_length(decimals); }
};


class Item_func_min_max :public Item_func
{
  Item_result cmp_type;
  String tmp_value;
  int cmp_sign;
public:
  Item_func_min_max(List<Item> &list,int type) :Item_func(list),
    cmp_sign(type) {}
  double val();
  longlong val_int();
  String *str(String *);
  void fix_length_and_dec();
  enum Item_result result_type () const { return cmp_type; }
};

class Item_func_min :public Item_func_min_max
{
public:
  Item_func_min(List<Item> &list) :Item_func_min_max(list,1) {}
  const char *func_name() const { return "least"; }
};

class Item_func_max :public Item_func_min_max
{
public:
  Item_func_max(List<Item> &list) :Item_func_min_max(list,-1) {}
  const char *func_name() const { return "greatest"; }
};


class Item_func_length :public Item_int_func
{
  String value;
public:
  Item_func_length(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "length"; }
  void fix_length_and_dec() { max_length=10; }
};


class Item_func_locate :public Item_int_func
{
  String value1,value2;
public:
  Item_func_locate(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_func_locate(Item *a,Item *b,Item *c) :Item_int_func(a,b,c) {}
  const char *func_name() const { return "locate"; }
  longlong val_int();
  void fix_length_and_dec() { maybe_null=0; max_length=11; }
};


class Item_func_field :public Item_int_func
{
  Item *item;
  String value,tmp;
public:
  Item_func_field(Item *a,List<Item> &list) :Item_int_func(list),item(a) {}
  ~Item_func_field() { delete item; }
  longlong val_int();
  bool fix_fields(THD *thd,struct st_table_list *tlist)
  {
    return (item->fix_fields(thd,tlist) || Item_func::fix_fields(thd,tlist));
  }
  void update_used_tables()
  {
    item->update_used_tables() ; Item_func::update_used_tables();
    used_tables_cache|=item->used_tables();
  }
  const char *func_name() const { return "field"; }
  void fix_length_and_dec()
  { maybe_null=0; max_length=2; used_tables_cache|=item->used_tables();}
};


class Item_func_ascii :public Item_int_func
{
  String value;
public:
  Item_func_ascii(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "ascii"; }
  void fix_length_and_dec() { max_length=3; }
};


class Item_func_find_in_set :public Item_int_func
{
  String value,value2;
  uint enum_value;
  ulonglong enum_bit;
public:
  Item_func_find_in_set(Item *a,Item *b) :Item_int_func(a,b),enum_value(0) {}
  longlong val_int();
  const char *func_name() const { return "find_in_set"; }
  void fix_length_and_dec();
};


class Item_func_bit_or :public Item_int_func
{
public:
  Item_func_bit_or(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "|"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_bit_and :public Item_int_func
{
public:
  Item_func_bit_and(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "&"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_bit_count :public Item_int_func
{
public:
  Item_func_bit_count(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "bit_count"; }
  void fix_length_and_dec() { decimals=0; max_length=2; }
};

class Item_func_shift_left :public Item_int_func
{
public:
  Item_func_shift_left(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "<<"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_shift_right :public Item_int_func
{
public:
  Item_func_shift_right(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return ">>"; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};

class Item_func_set_last_insert_id :public Item_int_func
{
public:
  Item_func_set_last_insert_id(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "last_insert_id"; }
  void fix_length_and_dec() { decimals=0; max_length=args[0]->max_length; }
};


#ifdef HAVE_DLOPEN

class Item_udf_func :public Item_func
{
 protected:
  String *buffers;
  UDF_ARGS f_args;
  UDF_INIT initid;
  char *num_buffer;
  udf_func *u_d;
  uchar error;
  bool initialized;

public:
  Item_udf_func(udf_func *udf)	:Item_func(), buffers(0), u_d(udf),
    error(0), initialized(0) {}
  Item_udf_func(udf_func *udf, List<Item> &list)
    :Item_func(list), buffers(0), u_d(udf),error(0),initialized(0) {}
  ~Item_udf_func();
  const char *func_name() const { return u_d ? u_d->name : "?"; }
  bool fix_fields(THD *thd,struct st_table_list *tlist);
  Item_result result_type () const { return u_d ? u_d->returns : STRING_RESULT;}
  bool get_arguments();
};


class Item_func_udf_float :public Item_udf_func
{
 public:
  Item_func_udf_float(udf_func *udf) :Item_udf_func(udf) {}
  Item_func_udf_float(udf_func *udf, List<Item> &list)
    :Item_udf_func(udf,list) {}
  ~Item_func_udf_float() {}
  longlong val_int() { return (longlong) Item_func_udf_float::val(); }
  double val();
  String *str(String*str);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_func_udf_int :public Item_udf_func
{
public:
  Item_func_udf_int(udf_func *udf) :Item_udf_func(udf) {}
  Item_func_udf_int(udf_func *udf, List<Item> &list)
    :Item_udf_func(udf,list) {}
  ~Item_func_udf_int() {}
  longlong val_int();
  double val() { return (double) Item_func_udf_int::val_int(); }
  String *str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};


class Item_func_udf_str :public Item_udf_func
{
public:
  Item_func_udf_str(udf_func *udf) :Item_udf_func(udf) {}
  Item_func_udf_str(udf_func *udf, List<Item> &list)
    :Item_udf_func(udf,list) {}
  ~Item_func_udf_str() {}
  String *str(String *);
  double val()
  {
    String *res;  res=str(&str_value);
    return res ? atof(res->c_ptr()) : 0.0;
  }
  longlong val_int()
  {
    String *res;  res=str(&str_value);
    return res ? strtoll(res->c_ptr(),NULL,10) : (longlong) 0;
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
};

#else /* Dummy functions to get sql_yacc.cc compiled */

class Item_func_udf_float :public Item_real_func
{
 public:
  Item_func_udf_float(udf_func *udf) :Item_real_func() {}
  Item_func_udf_float(udf_func *udf, List<Item> &list) :Item_real_func(list) {}
  ~Item_func_udf_float() {}
  double val() { return 0.0; }
};


class Item_func_udf_int :public Item_int_func
{
public:
  Item_func_udf_int(udf_func *udf) :Item_int_func() {}
  Item_func_udf_int(udf_func *udf, List<Item> &list) :Item_int_func(list) {}
  ~Item_func_udf_int() {}
  longlong val_int() { return 0; }
};


class Item_func_udf_str :public Item_func
{
public:
  Item_func_udf_str(udf_func *udf) :Item_func() {}
  Item_func_udf_str(udf_func *udf, List<Item> &list)  :Item_func(list) {}
  ~Item_func_udf_str() {}
  String *str(String *) { null_value=1; return 0; }
  double val() { null_value=1; return 0.0; }
  longlong val_int() { null_value=1; return 0; }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() { maybe_null=1; max_length=0; }
};

#endif /* HAVE_DLOPEN */

/*
** User level locks
*/

class ULL;
void item_user_lock_init(void);
void item_user_lock_release(ULL *ull);
void item_user_lock_free(void);

class Item_func_get_lock :public Item_int_func
{
  String value;
 public:
  Item_func_get_lock(Item *a,Item *b) :Item_int_func(a,b) {}
  longlong val_int();
  const char *func_name() const { return "get_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=1;}
};

class Item_func_release_lock :public Item_int_func
{
  String value;
 public:
  Item_func_release_lock(Item *a) :Item_int_func(a) {}
  longlong val_int();
  const char *func_name() const { return "release_lock"; }
  void fix_length_and_dec() { decimals=0; max_length=1; maybe_null=1;}
};
