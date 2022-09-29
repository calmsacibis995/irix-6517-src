/* Compability file ; This file only contains dummy functions */

#ifdef __GNUC__
#pragma interface
#endif

#include <queues.h>

class Item_func_unique_users :public Item_real_func
{
public:
  Item_func_unique_users(Item *name_arg,int start,int end,List<Item> &list)
    :Item_real_func(list) {}
  double val() { return 0.0; }
  void fix_length_and_dec() { decimals=0; max_length=6; }
};

class Item_sum_unique_users :public Item_sum_num
{
public:
  Item_sum_unique_users(Item *name_arg,int start,int end,Item *item_arg)
    :Item_sum_num(item_arg) {}
  double val() { return 0.0; }  
  enum Sumfunctype sum_func () const {return UNIQUE_USERS_FUNC;}
  void reset() {}
  void add() {}
  void reset_field() {}
  void update_field(int offset) {}
  bool fix_fields(THD *thd,struct st_table_list *tlist) { return 0;}
};
