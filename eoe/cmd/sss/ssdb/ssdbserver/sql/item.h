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
#pragma interface			/* gcc class implementation */
#endif

struct st_table_list;
void item_init(void);				/* Init item functions */

class Item :public Sql_alloc {
  Item(const Item &);				/* Prevent use of theese */
  void operator=(Item &);
public:
  enum Type {FIELD_ITEM,FUNC_ITEM,SUM_FUNC_ITEM,STRING_ITEM,
	     INT_ITEM,REAL_ITEM,NULL_ITEM,VARBIN_ITEM,
	     COPY_STR_ITEM,FIELD_AVG_ITEM,
	     PROC_ITEM,COND_ITEM,REF_ITEM,FIELD_STD_ITEM, CONST_ITEM};
  enum cond_result { COND_UNDEF,COND_OK,COND_TRUE,COND_FALSE };

  my_bool maybe_null;			/* If item may be null */
  my_bool null_value;			/* if item is null */
  my_bool binary;
  my_bool with_sum_func;

  uint8 marker,decimals;
  uint32 max_length;
  my_string name;			/* Name from select */
  String str_value;			/* used to store value */
  Item *next;

  // alloc & destruct is done as start of select using sql_alloc
  Item();
  virtual ~Item() { name=0; }		/*lint -e1509 */
  void set_name(char* str,uint length=0);
  void init_make_field(Send_field *tmp_field,enum enum_field_types type);
  virtual bool fix_fields(THD *,struct st_table_list *);
  virtual bool save_in_field(Field *field);
  virtual void save_org_in_field(Field *field)
    { (void) save_in_field(field); }
  virtual bool send(String *str);
  virtual bool eq(const Item *) const;
  virtual Item_result result_type () const { return REAL_RESULT; }
  virtual enum Type type() const =0;
  virtual double val()=0;
  virtual longlong val_int()=0;
  virtual String *str(String*)=0;
  virtual void make_field(Send_field *field)=0;
  virtual Field *tmp_table_field() { return 0; }
  virtual const char *full_name() const { return name ? name : "???"; }
  virtual double val_result() { return val(); }
  virtual longlong val_int_result() { return val_int(); }
  virtual String *str_result(String* tmp) { return str(tmp); }
  virtual table_map used_tables() const { return (table_map) 0L; }
  virtual bool basic_const_item() const { return 0; }
  virtual Item *new_item() { return 0; }	/* Only for const items */
  virtual cond_result eq_cmp_result() const { return COND_OK; }
  inline uint float_length(uint decimals_par) const {return (13+decimals_par);}
  inline bool const_item() const { return used_tables() == 0; }
  virtual void print(String *str) { str->append(full_name()); }
  virtual void update_used_tables() {}
  virtual void split_sum_func(List<Item> &fields) { return; }
};


class Item_ident :public Item
{
public:
  const char *db_name;
  const char *table_name;
  const char *field_name;
  Item_ident(const char *db_name_par,const char *table_name_par,
	     const char *field_name_par)
    :db_name(db_name_par),table_name(table_name_par),field_name(field_name_par)
    { name = (char*) field_name_par; }
  const char *full_name() const;
};

class Item_field :public Item_ident
{
  void set_field(Field *field);
public:
  Field *field,*result_field;
  // Item_field() {}

  Item_field(const char *db_par,const char *table_name_par,
	     const char *field_name_par)
    :field(0),result_field(0),
    Item_ident(db_par,table_name_par,field_name_par) {}
  Item_field(Field *field);
  enum Type type() const { return FIELD_ITEM; }
  bool eq(const Item *item) const;
  double val();
  longlong val_int();
  String *str(String*);
  double val_result();
  longlong val_int_result();
  String *str_result(String* tmp);
  bool send(String *str) { return result_field->send(str); }
  void make_field(Send_field *field);
  bool fix_fields(THD *,struct st_table_list *);
  bool save_in_field(Field *field);
  void save_org_in_field(Field *field);
  table_map used_tables() const;
  enum Item_result result_type () const
  {
    return field->result_type();
  }
  Field *tmp_table_field() { return result_field; }
};


class Item_null :public Item
{
public:
  Item_null(char *name_par=0)
    { maybe_null=null_value=TRUE; name= name_par ? name_par : (char*) "NULL";}
  enum Type type() const { return NULL_ITEM; }
  bool eq(const Item *item) const;
  double val();
  longlong val_int();
  String *str(String *str);
  void make_field(Send_field *field);
  bool save_in_field(Field *field);
  enum Item_result result_type () const
  { return STRING_RESULT; }
  bool send(String *str);
  bool basic_const_item() const { return 1; }
  Item *new_item() { return new Item_null(name); }
};


class Item_int :public Item
{
public:
  const longlong value;
  Item_int(int32 i,uint length=11) :value((longlong) i)
    { max_length=length;}
#ifdef HAVE_LONG_LONG
  Item_int(longlong i,uint length=21) :value(i)
    { max_length=length;}
#endif
  Item_int(char *str,longlong i,uint length) :value(i)
    { max_length=length; name=str;}
  Item_int(const char *str) :
    value(str[0] == '-' ? strtoll(str,NULL,10) :
	  (longlong) strtoull(str,NULL,10))
    { max_length=strlen(str); name=(char*) str;}
  enum Type type() const { return INT_ITEM; }
  virtual enum Item_result result_type () const { return INT_RESULT; }
  longlong val_int() { return value; }
  double val() { return (double) value; }
  String *str(String*);
  void make_field(Send_field *field);
  bool save_in_field(Field *field);
  bool basic_const_item() const { return 1; }
  Item *new_item() { return new Item_int(name,value,max_length); }
  void print(String *str);
};


class Item_real :public Item
{
public:
  const double value;
  // Item_real() :value(0) {}
  Item_real(const char *str) :value(atof(str))
  {
    name=(char*) str;
    this->decimals=nr_of_decimals(str);
    max_length=strlen(str);
  }
  Item_real(const char *str,double val,uint decimal_par,uint length) :value(val)
  {
    name=(char*) str;
    decimals=decimal_par;
    max_length=length;
  }
  Item_real(double value_par) :value(value_par) {}
  bool save_in_field(Field *field);
  enum Type type() const { return REAL_ITEM; }
  double val() { return value; }
  longlong val_int() { return (longlong) (value+0.5);}
  String *str(String*);
  void make_field(Send_field *field);
  bool basic_const_item() const { return 1; }
  Item *new_item() { return new Item_real(name,value,decimals,max_length); }
};


class Item_string :public Item
{
public:
  Item_string(const char *str,uint length)
  {
    str_value.set(str,length);
    max_length=length;
    name=(char*) str_value.ptr();
  }
  Item_string(char *name_par,const char *str,uint length)
  {
    str_value.set(str,length);
    max_length=length;
    name=name_par;
  }
  ~Item_string() {}
  enum Type type() const { return STRING_ITEM; }
  double val() { return atof(str_value.ptr()); }
  longlong val_int() { return strtoll(str_value.ptr(),NULL,10); }
  String *str(String*) { return (String*) &str_value; }
  bool save_in_field(Field *field);
  void make_field(Send_field *field);
  enum Item_result result_type () const { return STRING_RESULT; }
  bool basic_const_item() const { return 1; }
  Item *new_item() { return new Item_string(name,str_value.ptr(),max_length); }
  String *const_string() { return &str_value; }
  inline void append(char *str,uint length) { str_value.append(str,length); }
  void print(String *str);
};


class Item_empty_string :public Item_string
{
public:
  Item_empty_string(char *header,uint length) :Item_string("",0)
    { name=header; max_length=length;}
};

class Item_varbinary :public Item
{
public:
  Item_varbinary(const char *str,uint str_length);
  ~Item_varbinary() {}
  enum Type type() const { return VARBIN_ITEM; }
  double val() { return (double) Item_varbinary::val_int(); }
  longlong val_int();
  String *str(String*) { return &str_value; }
  bool save_in_field(Field *field);
  void make_field(Send_field *field);
  enum Item_result result_type () const { return STRING_RESULT; }
};


class Item_result_field :public Item	/* Item with result field */
{
public:
  Field *result_field;			/* Save result of function here */
  Item_result_field() :result_field(0) {}
  Field *tmp_table_field() { return result_field; }
};


class Item_ref :public Item_ident
{
  Item **ref;
public:
  Item_ref(char *db_par,char *table_name_par,char *field_name_par)
    :ref(0), Item_ident(db_par,table_name_par,field_name_par) {}
  Item_ref(Item **item, char *table_name_par,char *field_name_par)
    :ref(item), Item_ident(NullS,table_name_par,field_name_par) {}
  enum Type type() const		{ return REF_ITEM; }
  bool eq(const Item *item) const	{ return (*ref)->eq(item); }
  ~Item_ref() { if (ref) delete *ref; }
  double val()
  {
    double tmp=(*ref)->val_result();
    null_value=(*ref)->null_value;
    return tmp;
  }
  longlong val_int()
  {
    longlong tmp=(*ref)->val_int_result();
    null_value=(*ref)->null_value;
    return tmp;
  }
  String *str(String* tmp)
  {
    tmp=(*ref)->str_result(tmp);
    null_value=(*ref)->null_value;
    return tmp;
  }
  bool send(String *tmp)		{ return (*ref)->send(tmp); }
  void make_field(Send_field *field)	{ (*ref)->make_field(field); }
  bool fix_fields(THD *,struct st_table_list *);
  bool save_in_field(Field *field)	{ return (*ref)->save_in_field(field); }
  void save_org_in_field(Field *field)	{ (*ref)->save_org_in_field(field); }
  enum Item_result result_type () const { return (*ref)->result_type(); }
  table_map used_tables() const		{ return (*ref)->used_tables(); }
};


#include "item_sum.h"
#include "item_func.h"
#include "item_cmpfunc.h"
#include "item_strfunc.h"
#include "item_timefunc.h"
#include "item_uniq.h"

class Item_copy_string :public Item
{
public:
  Item *item;
  Item_copy_string(Item *i) :item(i)
  {
    null_value=maybe_null=item->maybe_null;
    decimals=item->decimals;
    max_length=item->max_length;
    name=item->name;
  }
  ~Item_copy_string() { delete item; }
  enum Type type() const { return COPY_STR_ITEM; }
  enum Item_result result_type () const { return STRING_RESULT; }
  double val()	   { return 0.0; }		// Never done
  longlong val_int() { return 0; }		// Never done
  String *str(String*);
  void make_field(Send_field *field) { item->make_field(field); }
  void copy();
  table_map used_tables() const { return (table_map) 1L; }
};


class Item_buff :public Sql_alloc {
public:
  my_bool null_value;
  Item_buff() :null_value(0) {}
  virtual bool cmp(void)=0;
  virtual ~Item_buff(); /*line -e1509 */
};

class Item_str_buff :public Item_buff
{
  Item *item;
  String value,tmp_value;
public:
  Item_str_buff(Item *arg) :item(arg),value(arg->max_length) {}
  bool cmp(void);
  ~Item_str_buff();				// Deallocate String:s
};


class Item_real_buff :public Item_buff
{
  Item *item;
  double value;
public:
  Item_real_buff(Item *item_par) :item(item_par),value(0.0) {}
  bool cmp(void);
};

class Item_int_buff :public Item_buff
{
  Item *item;
  longlong value;
public:
  Item_int_buff(Item *item_par) :item(item_par),value(0) {}
  bool cmp(void);
};


class Item_field_buff :public Item_buff
{
  char *buff;
  Field *field;
  uint length;

public:
  Item_field_buff(Item_field *item)
  {
    field=item->field;
    buff= (char*) sql_calloc(length=field->pack_length());
  }
  bool cmp(void);
};


extern Item_buff *new_Item_buff(Item *item);
extern Item_result item_cmp_type(Item_result a,Item_result b);
extern Item *resolve_const_item(Item *item,Item *cmp_item);
