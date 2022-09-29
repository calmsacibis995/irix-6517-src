/* When using sql procedures */

#ifdef __GNUC__
#pragma interface				/* For gcc */
#endif

#define PROC_NO_SORT 1				/* Bits in flags */
#define PROC_GROUP   2				/* proc must have group */

/* Procedure items used by procedures to store values for send_fields */

class Item_proc :public Item
{
  double value;
public:
  Item_proc(char *name_arg) { this->name=name_arg; }
  enum Type type() const { return Item::PROC_ITEM; }
  enum Item_result result_type () const { return REAL_RESULT; }
  void set(double nr) { value=nr; }
  double val() { return value; }
  longlong val_int() { return (longlong) value; }
  String *str(String *s) { s->set(value,decimals); return s; }
};

class Procedure {
protected:
  List<Item> *fields;
  select_result *result;
public:
  const uint flags;
  ORDER *group,*param_fields;  
  Procedure(select_result *res,uint flags_par) :result(res),flags(flags_par) {}
  virtual ~Procedure() {group=param_fields=0; fields=0; }
  virtual void add(void)=0;
  virtual void end_group(void)=0;
  virtual int send_row(List<Item> &fields)=0;
  virtual void change_columns(List<Item> &fields)=0;
};

Procedure *setup_procedure(THD *thd,ORDER *proc_param,select_result *result,
			   int *error);
