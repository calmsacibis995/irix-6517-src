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

/* Optimizing of many different type of queries with GROUP functions */

#include "mysql_priv.h"
#include "sql_select.h"

static bool find_range_key(Field* field,COND *cond,byte *key_buff);

/*****************************************************************************
** This function is only called for queries with sum functions and no
** GROUP BY part.
** This substitutes constants for some COUNT(), MIN() and MAX() functions.
** The function returns 1 if all items was resolved and -1 on impossible
** conditions
****************************************************************************/

int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds)
{
  List_iterator<Item> it(all_fields);
  int const_result=1;
  table_map removed_tables=0;
  Item *item;

  while ((item= it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM)
    {
      Item_sum *item_sum= (((Item_sum*) item));
      switch (item_sum->sum_func()) {
      case Item_sum::COUNT_FUNC:
	/*
	  If the expr in count(expr) can never be null we can change this
	  to the number of rows in the tables
	*/
	if (!conds && !((Item_sum_count*) item)->item->maybe_null)
	{
	  longlong count=1;
	  TABLE_LIST *table;
	  for (table=tables; table ; table=table->next)
	  {
	    ha_info(tables->table,0);
	    count*= table->table->keyfile_info.records;
	    if (table->on_expr)
	    {
	      const_result=0;			// Can't optimize left join
	      break;
	    }
	  }
	  if (!table)
	    ((Item_sum_count*) item)->make_const(count);
	}
	else
	  const_result=0;
	break;
      case Item_sum::MIN_FUNC:
      {
	/*
	  If MIN(expr) is the first part of a key or if all previous
	  parts of the key is found in the COND, then we can use
	  indexes to find the key.
	*/
	Item *expr=item_sum->item;
	if (expr->type() == Item::FIELD_ITEM)
	{
	  byte key_buff[MAX_KEY_LENGTH];

	  if (!find_range_key(((Item_field*) expr)->field,conds,key_buff))
	  {
	    const_result=0;
	    break;
	  }
	  TABLE *table=((Item_field*) expr)->field->table;
	  bool error;

	  if (!table->reginfo.ref_length)
	    error=ha_rfirst(table,table->record[0],
			    table->reginfo.ref_key) !=0;
	  else
	    error=ha_rkey(table,table->record[0],table->reginfo.ref_key,
			  key_buff,table->reginfo.ref_length,
			  HA_READ_KEY_OR_NEXT) ||
	      key_cmp(table,key_buff,table->reginfo.ref_key,
		      table->reginfo.ref_length);
	  if (table->key_read)
	  {
	    table->key_read=0;
	    ha_extra(table,HA_EXTRA_NO_KEYREAD);
	  }
	  if (error)
	    return -1;				// Impossible query
	  removed_tables|= table->map;
	}
	else if (!expr->const_item())		// This is VERY seldom false
	{
	  const_result=0;
	  break;
	}
	((Item_sum_min*) item_sum)->reset();
	((Item_sum_min*) item_sum)->make_const();
	break;
      }
      case Item_sum::MAX_FUNC:
      {
	/*
	  If MAX(expr) is the first part of a key or if all previous
	  parts of the key is found in the COND, then we can use
	  indexes to find the key.
	*/
	Item *expr=item_sum->item;
	if (expr->type() == Item::FIELD_ITEM)
	{
	  byte key_buff[MAX_KEY_LENGTH];

	  if (!find_range_key(((Item_field*) expr)->field,conds,key_buff))
	  {
	    const_result=0;
	    break;
	  }
	  TABLE *table=((Item_field*) expr)->field->table;
	  bool error;

	  if (!table->reginfo.ref_length)
	    error=ha_rlast(table,table->record[0],
			   table->reginfo.ref_key) !=0;
	  else
	  {
	    (void) ha_rkey(table,table->record[0],table->reginfo.ref_key,
			   key_buff,table->reginfo.ref_length,
			   HA_READ_AFTER_KEY);
	    error=ha_rprev(table,table->record[0],table->reginfo.ref_key) ||
	      key_cmp(table,key_buff,table->reginfo.ref_key,
		      table->reginfo.ref_length);
	  }
	  if (table->key_read)
	  {
	    table->key_read=0;
	    ha_extra(table,HA_EXTRA_NO_KEYREAD);
	  }
	  if (error)
	    return -1;				// Impossible query
	  removed_tables|= table->map;
	}
	else if (!expr->const_item())		// This is VERY seldom false
	{
	  const_result=0;
	  break;
	}
	((Item_sum_min*) item_sum)->reset();
	((Item_sum_min*) item_sum)->make_const();
	break;
      }
      default:
	const_result=0;
	break;
      }
    }
    else if (!item->const_item())
      const_result=0;
  }
  if (conds && (conds->used_tables() & ~ removed_tables))
    const_result=0;
  return const_result;
}

/* Count in how many times table is used (up to MAX_KEY_PARTS+1) */

uint count_table_entries(COND *cond,TABLE *table)
{
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_OR_FUNC)
      return (cond->used_tables() & table->map) ? MAX_REF_PARTS+1 : 0;

    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    uint count=0;
    while ((item=li++))
    {
      if ((count+=count_table_entries(item,table)) > MAX_REF_PARTS)
	return MAX_REF_PARTS+1;
    }
    return count;
  }
  if (cond->type() == Item::FUNC_ITEM &&
      ((Item_func*) cond)->functype() == Item_func::EQ_FUNC &&
      cond->used_tables() == table->map)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM)
    {
      if (!(((Item_field*) left_item)->field->flags & PART_KEY_FLAG) ||
	  !right_item->const_item())
	return MAX_REF_PARTS+1;
      return 1;
    }
    if (right_item->type() == Item::FIELD_ITEM)
    {
      if (!(((Item_field*) right_item)->field->flags & PART_KEY_FLAG) ||
	  !left_item->const_item())
	return MAX_REF_PARTS+1;
      return 1;
    }
  }
  return (cond->used_tables() & table->map) ? MAX_REF_PARTS+1 : 0;
}


/* check that the field is usable as key part */

bool part_of_cond(COND *cond,Field *field)
{
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_OR_FUNC)
      return 0;					// Already checked

    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
    {
      if (part_of_cond(item,field))
	return 1;
    }
    return 0;
  }
  if (cond->type() == Item::FUNC_ITEM &&
      ((Item_func*) cond)->functype() == Item_func::EQ_FUNC &&
      cond->used_tables() == field->table->map)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM)
    {
      if (((Item_field*) left_item)->field != field ||
	  !right_item->const_item())
	return 0;
    }
    else if (right_item->type() == Item::FIELD_ITEM)
    {
      if (((Item_field*) right_item)->field != field ||
	  !left_item->const_item())
	return 0;
      right_item=left_item;			// const item in right
    }
    store_val_in_field(field,right_item);
    return 1;
  }
  return 0;
}


/* Check if we can get value for field by using a key */

static bool find_range_key(Field* field,COND *cond,byte *key_buff)
{
  if (!(field->flags & PART_KEY_FLAG))
    return 0;					// Not part of a key. Skipp it

  TABLE *table=field->table;
  uint index=0;

  /* Check if some key has field as first key part */
  if (field->key_start && (! cond || ! (cond->used_tables() & table->map)))
  {
    for (key_map key=field->key_start ; !(key & 1) ; index++)
      key>>=1;
    table->reginfo.ref_length=0;
    table->reginfo.ref_key=index;
    if ((field->part_of_key && (table_map) 1 << index))
    {
      table->key_read=1;
      ha_extra(table,HA_EXTRA_KEYREAD);
    }
    return 1;					// Ok to use key
  }
  /*
  ** Check if WHERE consist of exactly the previous key parts for some key
  */
  if (!cond)
    return 0;
  uint table_entries= count_table_entries(cond,table);
  if (!table_entries || table_entries > MAX_REF_PARTS)
    return 0;

  KEY *keyinfo,*keyinfo_end;
  for (keyinfo=table->key_info, keyinfo_end=keyinfo+table->keys ;
       keyinfo != keyinfo_end;
       keyinfo++,index++)
  {
    if (table_entries < keyinfo->key_parts)
    {
      byte *key_ptr=key_buff;
      KEY_PART_INFO *part,*part_end;
      for (part=keyinfo->key_part, part_end=part+table_entries ;
	   part != part_end ;
	   part++)
      {
	if (!part_of_cond(cond,part->field))
	  break;
	part->field->get_image((char*) key_ptr,part->length);	// Save found constant
	key_ptr+=part->length;
      }
      if (part == part_end && part->field == field)
      {
	table->reginfo.ref_length= (uint) (key_ptr-key_buff);
	table->reginfo.ref_key=index;
	if ((field->part_of_key && (table_map) 1 << index))
	{
	  table->key_read=1;
	  ha_extra(table,HA_EXTRA_KEYREAD);
	}
	return 1;					// Ok to use key
      }
    }
  }
  return 0;					// No possible key
}
