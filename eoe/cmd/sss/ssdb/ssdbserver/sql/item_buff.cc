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

/* Buffers to save and compare item values */

#include "mysql_priv.h"

/*
** Create right type of item_buffer for an item
*/

Item_buff *new_Item_buff(Item *item)
{
  if (item->type() == Item::FIELD_ITEM &&
      !(((Item_field *) item)->field->flags & BLOB_FLAG))
    return new Item_field_buff((Item_field *) item);
  if (item->result_type() == STRING_RESULT)
    return new Item_str_buff((Item_field *) item);
  if (item->result_type() == INT_RESULT)
    return new Item_int_buff((Item_field *) item);
  return new Item_real_buff(item);
}

Item_buff::~Item_buff() {}

/*
** Compare with old value and replace value with new value
** Return true if values have changed
*/

bool Item_str_buff::cmp(void)
{
  String *res;
  bool tmp;

  res=item->str(&tmp_value);
  if (null_value != item->null_value)
  {
    if ((null_value= item->null_value))
      return TRUE;				// New value was null
    tmp=TRUE;
  }
  else if (null_value)
    return 0;					// new and old value was null
  else if (!item->binary)
    tmp= sortcmp(&value,res) != 0;
  else
    tmp= stringcmp(&value,res) != 0;
  if (tmp)
    value.copy(*res);				// Remember for next cmp
  return tmp;
}

Item_str_buff::~Item_str_buff()
{
  item=0;					// Safety
}

bool Item_real_buff::cmp(void)
{
  double nr=item->val();
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    return TRUE;
  }
  return FALSE;
}

bool Item_int_buff::cmp(void)
{
  longlong nr=item->val_int();
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    return TRUE;
  }
  return FALSE;
}


bool Item_field_buff::cmp(void)
{
  bool tmp= field->cmp(buff) != 0;
  if (tmp)
    field->get_image(buff,length);
  if (null_value != field->is_null())
  {
    null_value= !null_value;
    tmp=TRUE;
  }
  return tmp;
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<Item_buff>;
template class List_iterator<Item_buff>;
#endif
