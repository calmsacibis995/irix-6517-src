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

/* classes for sum functions */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

class Item_sum :public Item_result_field
{
public:
  enum Sumfunctype {COUNT_FUNC,SUM_FUNC,AVG_FUNC,MIN_FUNC,MAX_FUNC,
		    UNIQUE_USERS_FUNC,STD_FUNC,SUM_BIT_FUNC };
  Item *item;
  bool quick_group;			/* for the future. Is now always 1 */

  Item_sum(Item *item_par) :item(item_par),quick_group(1)
  { with_sum_func=1; }
  ~Item_sum() { delete item; result_field=0;}
  enum Type type() const { return SUM_FUNC_ITEM; }
  virtual enum Sumfunctype sum_func () const=0;
  virtual void reset()=0;
  virtual void add()=0;
  virtual void reset_field()=0;
  virtual void update_field(int offset)=0;
  virtual bool keep_field_type(void) const { return 0; }
  virtual void fix_length_and_dec() { maybe_null=1; null_value=1; }
  virtual const char *func_name() const { return "?"; }
  table_map used_tables() const { return ~(table_map) 0; }
  void update_used_tables() { }
  void make_field(Send_field *field);
  void print(String *str);
};


class Item_sum_num :public Item_sum
{
public:
  Item_sum_num(Item *item_par) :Item_sum(item_par) {}
  bool fix_fields(THD *,struct st_table_list *);
  longlong val_int() { return (longlong) val(); } /* Real as default */
  String *str(String*str);
  void reset_field();
};


class Item_sum_int :public Item_sum_num
{
public:
  Item_sum_int(Item *item_par) :Item_sum_num(item_par) {}
  double val() { return (double) val_int(); }
  String *str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
};


class Item_sum_sum :public Item_sum_num
{
  double sum;
  void fix_length_and_dec() { maybe_null=null_value=0; }

  public:
  Item_sum_sum(Item *item_par) :Item_sum_num(item_par),sum(0.0) {}
  enum Sumfunctype sum_func () const {return SUM_FUNC;}
  void reset();
  void add();
  double val();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "sum"; }
};


class Item_sum_count :public Item_sum_int
{
  longlong count;
  table_map used_table_cache;
  void fix_length_and_dec()
    { decimals=0; max_length=21; maybe_null=null_value=0; }

  public:
  Item_sum_count(Item *item_par)
    :Item_sum_int(item_par),count(0),used_table_cache(~(table_map) 0)
  {}
  table_map used_tables() const { return used_table_cache; }
  enum Sumfunctype sum_func () const { return COUNT_FUNC; }
  void reset();
  void add();
  void make_const(longlong count_arg) { count=count_arg; used_table_cache=0; }
  longlong val_int();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "count"; }
};


class Item_sum_avg :public Item_sum_num
{
  void fix_length_and_dec() { decimals+=4; maybe_null=1; }

  double sum;
  ulonglong count;

  public:
  Item_sum_avg(Item *item_par) :Item_sum_num(item_par),count(0) {}
  enum Sumfunctype sum_func () const {return AVG_FUNC;}
  void reset();
  void add();
  double val();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "avg"; }
};


class Item_sum_std :public Item_sum_num
{
  double sum;
  double sum_sqr;
  ulonglong count;
  void fix_length_and_dec() { decimals+=4; maybe_null=1; }

  public:
  Item_sum_std(Item *item_par) :Item_sum_num(item_par),count(0) {}
  enum Sumfunctype sum_func () const { return STD_FUNC; }
  void reset();
  void add();
  double val();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "std"; }
};


// This class is a string or number function depending on num_func

class Item_sum_hybrid :public Item_sum
{
 protected:
  String value,tmp_value;
  double sum;
  Item_result hybrid_type;
  int cmp_sign;
  table_map used_table_cache;

  public:
  Item_sum_hybrid(Item *item_par,int sign) :Item_sum(item_par),cmp_sign(sign),
    used_table_cache(~(table_map) 0)
  {}
  bool fix_fields(THD *,struct st_table_list *);
  table_map used_tables() const { return used_table_cache; }

  void reset()
  {
    sum=0.0;
    value.length(0);
    null_value=1;
    add();
  }
  double val();
  longlong val_int() { return (longlong) val(); } /* Real as default */
  void reset_field();
  String *str(String *);
  void make_const() { used_table_cache=0; }
  bool keep_field_type(void) const { return 1; }
  enum Item_result result_type () const { return hybrid_type; }
  void update_field(int offset);
  void min_max_update_str_field(int offset);
  void min_max_update_real_field(int offset);
  void min_max_update_int_field(int offset);
};


class Item_sum_min :public Item_sum_hybrid
{
public:
  Item_sum_min(Item *item_par) :Item_sum_hybrid(item_par,1) {}
  enum Sumfunctype sum_func () const {return MIN_FUNC;}

  void add();
  const char *func_name() const { return "min"; }
};


class Item_sum_max :public Item_sum_hybrid
{
public:
  Item_sum_max(Item *item_par) :Item_sum_hybrid(item_par,-1) {}
  enum Sumfunctype sum_func () const {return MAX_FUNC;}

  void add();
  const char *func_name() const { return "max"; }
};


class Item_sum_bit :public Item_sum_int
{
  void fix_length_and_dec()
    { decimals=0; max_length=21; maybe_null=null_value=0; }

 protected:
  ulonglong bits,reset_bits;

  public:
  Item_sum_bit(Item *item_par,ulonglong reset_arg)
    :Item_sum_int(item_par),reset_bits(reset_arg),bits(reset_arg) {}
  enum Sumfunctype sum_func () const {return SUM_BIT_FUNC;}
  void reset();
  longlong val_int();
  void reset_field();
};


class Item_sum_or :public Item_sum_bit
{
  public:
  Item_sum_or(Item *item_par) :Item_sum_bit(item_par,LL(0)) {}
  void add();
  void update_field(int offset);
  const char *func_name() const { return "bit_or"; }
};


class Item_sum_and :public Item_sum_bit
{
  public:
  Item_sum_and(Item *item_par) :Item_sum_bit(item_par,(ulonglong) LL(~0)) {}
  void add();
  void update_field(int offset);
  const char *func_name() const { return "bit_and"; }
};


/* Item to get the value of a stored sum function */

class Item_avg_field :public Item_result_field
{
public:
  Field *field;
  Item_avg_field(Item_sum_avg *item);
  enum Type type() const { return FIELD_AVG_ITEM; }
  double val();
  longlong val_int() { return (longlong) val(); }
  String *str(String*);
  void make_field(Send_field *field);
};


class Item_std_field :public Item_result_field
{
public:
  Field *field;
  Item_std_field(Item_sum_std *item);
  enum Type type() const { return FIELD_STD_ITEM; }
  double val();
  longlong val_int() { return (longlong) val(); }
  String *str(String*);
  void make_field(Send_field *field);
};
