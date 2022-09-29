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

/* This file defines all string functions */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class Item_str_func :public Item_func
{
public:
  Item_str_func() :Item_func() {}
  Item_str_func(Item *a) :Item_func(a) {}
  Item_str_func(Item *a,Item *b) :Item_func(a,b) {}
  Item_str_func(Item *a,Item *b,Item *c) :Item_func(a,b,c) {}
  Item_str_func(Item *a,Item *b,Item *c,Item *d) :Item_func(a,b,c,d) {}
  Item_str_func(List<Item> &list) :Item_func(list) {}
  longlong val_int();
  double val();
  enum Item_result result_type () const { return STRING_RESULT; }
  void left_right_max_length();
};

class Item_func_concat :public Item_str_func
{
  String tmp_value;
public:
  Item_func_concat(List<Item> &list) :Item_str_func(list) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "concat"; }
};

class Item_func_reverse :public Item_str_func
{
public:
  Item_func_reverse(Item *a) :Item_str_func(a) {}
  String *str(String *);
  void fix_length_and_dec();
};


class Item_func_replace :public Item_str_func
{
  String tmp_value,tmp_value2;
public:
  Item_func_replace(Item *org,Item *find,Item *replace)
    :Item_str_func(org,find,replace) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "replace"; }
};


class Item_func_insert :public Item_str_func
{
  String tmp_value;
public:
  Item_func_insert(Item *org,Item *start,Item *length,Item *new_str)
    :Item_str_func(org,start,length,new_str) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "insert"; }
};


class Item_str_conv :public Item_str_func
{
public:
  Item_str_conv(Item *item) :Item_str_func(item) {}
  void fix_length_and_dec() { max_length = args[0]->max_length; }
};


class Item_func_lcase :public Item_str_conv
{
public:
  Item_func_lcase(Item *item) :Item_str_conv(item) {}
  String *str(String *);
  const char *func_name() const { return "lcase"; }
};

class Item_func_ucase :public Item_str_conv
{
public:
  Item_func_ucase(Item *item) :Item_str_conv(item) {}
  String *str(String *);
  const char *func_name() const { return "ucase"; }
};


class Item_func_left :public Item_str_func
{
public:
  Item_func_left(Item *a,Item *b) :Item_str_func(a,b) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "left"; }
};


class Item_func_right :public Item_str_func
{
  String tmp_value;
public:
  Item_func_right(Item *a,Item *b) :Item_str_func(a,b) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "right"; }
};


class Item_func_substr :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr(Item *a,Item *b) :Item_str_func(a,b) {}
  Item_func_substr(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "substr"; }
};


class Item_func_substr_index :public Item_str_func
{
  String tmp_value;
public:
  Item_func_substr_index(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  String *str(String *);
  void fix_length_and_dec() { max_length= args[0]->max_length; }
  const char *func_name() const { return "substr_index"; }
};


class Item_func_ltrim :public Item_str_func
{
  String tmp_value;
public:
  Item_func_ltrim(Item *a,Item *b) :Item_str_func(a,b) {}
  String *str(String *);
  void fix_length_and_dec() { max_length= args[0]->max_length; }
  const char *func_name() const { return "ltrim"; }
};


class Item_func_rtrim :public Item_str_func
{
  String tmp_value;
public:
  Item_func_rtrim(Item *a,Item *b) :Item_str_func(a,b) {}
  String *str(String *);
  void fix_length_and_dec() { max_length= args[0]->max_length; }
  const char *func_name() const { return "rtrim"; }
};

class Item_func_trim :public Item_str_func
{
  String tmp_value;
public:
  Item_func_trim(Item *a,Item *b) :Item_str_func(a,b) {}
  String *str(String *);
  void fix_length_and_dec() { max_length= args[0]->max_length; }
  const char *func_name() const { return "trim"; }
};


class Item_func_password :public Item_str_func
{
  char tmp_value[17];
public:
  Item_func_password(Item *a) :Item_str_func(a) {}
  String *str(String *);
  void fix_length_and_dec() { max_length = 16; }
  const char *func_name() const { return "password"; }
};

class Item_func_encrypt :public Item_str_func
{
  String tmp_value;
public:
  Item_func_encrypt(Item *a) :Item_str_func(a) {}
  Item_func_encrypt(Item *a, Item *b): Item_str_func(a,b) {}
  String *str(String *);
  void fix_length_and_dec() { maybe_null=1; max_length = 13; }
};

#include "sql_crypt.h"

class Item_func_encode :public Item_str_func
{
 protected:
  SQL_CRYPT sql_crypt;
public:
  Item_func_encode(Item *a, char *seed):
    Item_str_func(a),sql_crypt(seed) {}
  String *str(String *);
  void fix_length_and_dec();
};

class Item_func_decode :public Item_func_encode
{
public:
  Item_func_decode(Item *a, char *seed): Item_func_encode(a,seed) {}
  String *str(String *);
};


class Item_func_database :public Item_str_func
{
public:
  Item_func_database() {}
  String *str(String *);
  void fix_length_and_dec() { max_length= MAX_FIELD_NAME; }
  const char *func_name() const { return "database"; }
};

class Item_func_user :public Item_str_func
{
public:
  Item_func_user() {}
  String *str(String *);
  void fix_length_and_dec() { max_length= USERNAME_LENGTH+HOSTNAME_LENGTH+1; }
  const char *func_name() const { return "user"; }
};


class Item_func_soundex :public Item_str_func
{
public:
  Item_func_soundex(Item *a) :Item_str_func(a) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "soundex"; }
};


class Item_func_elt :public Item_str_func
{
  Item *item;

public:
  Item_func_elt(Item *a,List<Item> &list) :Item_str_func(list),item(a) {}
  ~Item_func_elt() { delete item; }
  double val();
  longlong val_int();
  String *str(String *str);
  bool fix_fields(THD *thd,struct st_table_list *tlist)
  {
    return (item->fix_fields(thd,tlist) || Item_func::fix_fields(thd,tlist));
  }
  void fix_length_and_dec();
  void update_used_tables();
  const char *func_name() const { return "elt"; }
};


class Item_func_make_set :public Item_str_func
{
  Item *item;
  String tmp_str;

public:
  Item_func_make_set(Item *a,List<Item> &list) :Item_str_func(list),item(a) {}
  ~Item_func_make_set() { delete item; }
  String *str(String *str);
  bool fix_fields(THD *thd,struct st_table_list *tlist)
  {
    return (item->fix_fields(thd,tlist) || Item_func::fix_fields(thd,tlist));
  }
  void fix_length_and_dec();
  void update_used_tables();
  const char *func_name() const { return "make_set"; }
};


class Item_func_format :public Item_str_func
{
  String tmp_str;
public:
  Item_func_format(Item *org,int dec);
  String *str(String *);
  void fix_length_and_dec()
  {
    max_length=args[0]->max_length+(args[0]->max_length-args[0]->decimals)/3;
  }
  const char *func_name() const { return "format"; }
};


class Item_func_char :public Item_str_func
{
public:
  Item_func_char(List<Item> &list) :Item_str_func(list) {}
  String *str(String *);
  void fix_length_and_dec() { maybe_null=0; max_length=arg_count; }
  const char *func_name() const { return "char"; }
};


class Item_func_repeat :public Item_str_func
{
  String tmp_value;
public:
  Item_func_repeat(Item *arg1,Item *arg2) :Item_str_func(arg1,arg2) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "repeat"; }
};


class Item_func_rpad :public Item_str_func
{
  String tmp_value;
public:
  Item_func_rpad(Item *arg1,Item *arg2,Item *arg3) 
    :Item_str_func(arg1,arg2,arg3) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "rpad"; }
};


class Item_func_lpad :public Item_str_func
{
  String tmp_value;
public:
  Item_func_lpad(Item *arg1,Item *arg2,Item *arg3) 
    :Item_str_func(arg1,arg2,arg3) {}
  String *str(String *);
  void fix_length_and_dec();
  const char *func_name() const { return "lpad"; }
};


class Item_func_conv :public Item_str_func
{
public:
  Item_func_conv(Item *a,Item *b,Item *c) :Item_str_func(a,b,c) {}
  const char *func_name() const { return "conv"; }
  String *str(String *);
  void fix_length_and_dec() { decimals=0; max_length=64; }
};

