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

/* mysql_select and join optimization */

#include "mysql_priv.h"
#include "sql_select.h"
#include <nisam.h>

my_string join_type_str[]={ "UNKNOWN","system","const","eq_ref","ref",
			    "MAYBE_REF","ALL","range","index" };

static bool make_join_statistics(JOIN *join,TABLE_LIST *tables,COND *conds,
				  DYNAMIC_ARRAY *keyuse);
static uint update_ref_and_keys(DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,
				uint tables,COND *conds,table_map table_map);
static int sort_keyuse(KEYUSE *a,KEYUSE *b);
static void set_position(JOIN *join,uint index,JOIN_TAB *table,KEYUSE *key);
static void find_best_combination(JOIN *join);
static void find_best(JOIN *join,table_map rest_tables,uint index,
		      double record_count,double read_time);
static uint cache_record_length(JOIN *join,uint index);
static double prev_record_reads(JOIN *join,table_map found_ref);
static void get_best_combination(JOIN *join);
static void make_simple_join(JOIN *join,TABLE *tmp_table);
static bool make_join_select(JOIN *join,SQL_SELECT *select,COND *item);
static void make_join_readinfo(JOIN *join,uint options);
static void join_free(JOIN *join);
static ORDER *remove_const(JOIN *join,ORDER *first_order,bool *simple_order);
static void return_zero_rows(select_result *res,TABLE_LIST *tables,
			     List<Item> &fields, bool send_row,
			     uint select_options, const char *info,
			     Item *having);
static COND *optimize_cond(COND *conds,Item::cond_result *cond_value);
static COND *remove_eq_conds(COND *cond,Item::cond_result *cond_value);
static TABLE * create_tmp_table(THD *thd,JOIN *join,List<Item> &fields,
				ORDER *group,
				bool distinct,bool save_sum_fields,
				bool no_distinct_limit);
static void free_tmp_table(THD *thd, TABLE *entry);
static int do_select(JOIN *join,List<Item> *fields,TABLE *tmp_table,
		     Procedure *proc);
static int sub_select_cache(JOIN *join,JOIN_TAB *join_tab,bool end_of_records);
static int sub_select(JOIN *join,JOIN_TAB *join_tab,bool end_of_records);
static int flush_cacheed_records(JOIN *join,JOIN_TAB *join_tab,bool skipp_last);
static int end_send(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static int end_send_group(JOIN *join, JOIN_TAB *join_tab,bool end_of_records);
static int end_write(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static int end_update(JOIN *join, JOIN_TAB *join_tab, bool end_of_records);
static int end_write_group(JOIN *join, JOIN_TAB *join_tab,
			   bool end_of_records);
static void copy_fields(JOIN *join);
static int test_if_group_changed(List<Item_buff> &list);
static int join_read_const_tables(JOIN *join);
static int join_read_system(JOIN_TAB *tab);
static int join_read_const(JOIN_TAB *tab);
static int join_read_key(JOIN_TAB *tab);
static int join_read_always_key(JOIN_TAB *tab);
static int join_no_more_records(READ_RECORD *info);
static int join_read_next(READ_RECORD *info);
static int join_init_quick_read_record(JOIN_TAB *tab);
static int test_if_quick_select(JOIN_TAB *tab);
static int join_init_read_record(JOIN_TAB *tab);
static int join_init_read_first_with_key(JOIN_TAB *tab);
static int join_init_read_next_with_key(READ_RECORD *info);
static int join_init_read_last_with_key(JOIN_TAB *tab);
static int join_init_read_prev_with_key(READ_RECORD *info);
static COND *make_cond_for_table(COND *cond,table_map table,
				 table_map used_table);
static REF_FIELD* part_of_refkey(TABLE *form,Field *field);
static uint find_shortest_key(TABLE *table, key_map usable_keys);
static int create_sort_index(JOIN_TAB *tab,ORDER *order,ha_rows select_limit);
static int remove_duplicates(TABLE *entry,List<Item> &fields);
static SORT_FIELD * make_unireg_sortorder(ORDER *order, uint *length);
static int join_init_cache(THD *thd,JOIN_TAB *tables,uint table_count);
static ulong used_blob_length(CACHE_FIELD **ptr);
static bool store_record_in_cache(JOIN_CACHE *cache);
static void reset_cache(JOIN_CACHE *cache);
static void read_cacheed_record(JOIN_TAB *tab);
static void cp_buffer_from_ref(byte *buffer,REF_FIELD *fields,uint length);
static int setup_order(THD *thd,TABLE_LIST *tables, List<Item> &fields,
		       List <Item> &all_fields, ORDER *order);
static int setup_group(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		       List<Item> &all_fields, ORDER *order, bool *hidden);
static bool setup_new_fields(THD *thd,TABLE_LIST *tables,List<Item> &fields,
			     List<Item> &all_fields,ORDER *new_order);
static ORDER *add_all_fields_to_order(JOIN *join,ORDER *order,
				      List<Item> &fields);
static void count_field_types(JOIN *join,List<Item> &fields);
static bool test_if_subpart(ORDER *a,ORDER *b);
static TABLE *get_sort_by_table(ORDER *a,ORDER *b,TABLE_LIST *tables);
static void calc_group_buffer(JOIN *join,ORDER *group);
static void alloc_group_fields(JOIN *join,ORDER *group);
static void setup_copy_fields(JOIN *join,List<Item> &fields);
static void make_sum_func_list(JOIN *join,List<Item> &fields);
static void change_to_use_tmp_fields(List<Item> &func);
static void change_refs_to_tmp_fields(List<Item> &func);
static void init_tmptable_sum_functions(Item_sum **func);
static void update_tmptable_sum_func(Item_sum **func,TABLE *tmp_table);
static void copy_funcs(Item_result_field **func_ptr);
static void copy_sum_funcs(Item_sum **func_ptr);
static void add_ref_to_table_cond(JOIN_TAB *join_tab);
static void init_sum_functions(Item_sum **func);
static void update_sum_func(Item_sum **func);
static void select_describe(JOIN *join);
static void describe_info(const char *info);

/*****************************************************************************
** check fields, find best join, do the select and output fields.
** mysql_select assumes that all tables are allready opened
*****************************************************************************/

int
mysql_select(THD *thd,TABLE_LIST *tables,List<Item> &fields,COND *conds,
	     ORDER *order, ORDER *group,Item *having,ORDER *proc_param,
	     uint select_options,select_result *result)
{
  TABLE		*tmp_table;
  int		error;
  bool		need_tmp,hidden_group_fields;
  bool		simple_order,simple_group;
  Item::cond_result cond_value;
  SQL_SELECT	*select;
  DYNAMIC_ARRAY keyuse;
  JOIN		join;
  Procedure	*procedure;
  List<Item>	all_fields(fields);
  bool		select_distinct;
  DBUG_ENTER("mysql_select");

  /* Check that all tables, fields, conds and order are ok */

  select_distinct=test(select_options & SELECT_DISTINCT);
  tmp_table=0;
  select=0;
  bzero((char*) &keyuse,sizeof(keyuse));
  thd->proc_info="init";

  if (setup_fields(thd,tables,fields,1,&all_fields) ||
      setup_conds(thd,tables,conds) ||
      setup_order(thd,tables,fields,all_fields,order) ||
      setup_group(thd,tables,fields,all_fields,group,&hidden_group_fields))
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (having)
  {
    thd->where="having clause";
    thd->allow_sum_func=1;
    if (having->fix_fields(thd,tables) || thd->fatal_error)
      DBUG_RETURN(-1);				/* purecov: inspected */
    if (having->with_sum_func)
      having->split_sum_func(all_fields);
  }
  /*
    Check if one one uses a not constant column with group functions
    and no GROUP BY.
    TODO:  Add check of calculation of GROUP functions and fields:
           SELECT COUNT(*)+table.col1 from table1;
    */
  join.table=0;
  join.tables=0;
  {
    if (!group)
    {
      uint flag=0;
      List_iterator<Item> it(fields);
      Item *item;
      while ((item= it++))
      {
	if (item->with_sum_func)
	  flag|=1;
	else if (!item->const_item())
	  flag|=2;
      }
      if (flag == 3)
      {
	my_error(ER_MIX_OF_GROUP_FUNC_AND_FIELDS,MYF(0));
	DBUG_RETURN(-1);
      }
    }
    TABLE_LIST *table;
    for (table=tables ; table ; table=table->next)
      join.tables++;
  }

  procedure=setup_procedure(thd,proc_param,result,&error);
  if (error)
    DBUG_RETURN(-1);				/* purecov: inspected */
  if (procedure)
  {
    if (setup_new_fields(thd,tables,fields,all_fields,procedure->param_fields))
    {						/* purecov: inspected */
      delete procedure;				/* purecov: inspected */
      DBUG_RETURN(-1);				/* purecov: inspected */
    }
    if (procedure->group)
    {
      if (!test_if_subpart(procedure->group,group))
      {						/* purecov: inspected */
	my_message(0,"Can't handle procedures with differents groups yet",
		   MYF(0));			/* purecov: inspected */
	delete procedure;			/* purecov: inspected */
	DBUG_RETURN(-1);			/* purecov: inspected */
      }
    }
#ifdef NOT_NEEDED
    else if (!group && procedure->flags & PROC_GROUP)
    {
      my_message(0,"Select must have a group with this procedure",MYF(0));
      delete procedure;
      DBUG_RETURN(-1);
    }
#endif
    if (order && (procedure->flags & PROC_NO_SORT))
    { /* purecov: inspected */
      my_message(0,"Can't use order with this procedure",MYF(0)); /* purecov: inspected */
      delete procedure; /* purecov: inspected */
      DBUG_RETURN(-1); /* purecov: inspected */
    }
  }

  /* Init join struct */
  join.thd=thd;
  join.lock=thd->lock;
  join.join_tab=0;
  join.copy_field=0;
  join.sum_funcs=0;
  join.send_records=0L;
  join.end_write_records= HA_POS_ERROR;
  join.first_record=join.sort_and_group=0;
  join.select_options=select_options;
  join.result=result;
  count_field_types(&join,all_fields);
  join.const_tables=0;
  join.having=0;
  join.group= group != 0;

#ifdef RESTRICTED_GROUP
  if (join.sum_func_count && !group && (join.func_count || join.field_count))
  {
    my_message(ER_WRONG_SUM_SELECT,ER(ER_WRONG_SUM_SELECT));
    delete procedure;
    DBUG_RETURN(-1);
  }
#endif
  if (result->prepare(fields))
  { /* purecov: inspected */
    delete procedure; /* purecov: inspected */
    DBUG_RETURN(-1); /* purecov: inspected */
  }

#ifdef HAVE_REF_TO_FIELDS			// Not done yet
  /* Add HAVING to WHERE if possible */
  if (having && !group && ! join.sum_func_count)
  {
    if (!conds)
    {
      conds=having;
      having=0;
    }
    else if ((conds=new Item_cond_and(conds,having)))
    {
      conds->fix_fields(thd,tables);
      conds->change_ref_to_fields(thd,tables);
      having=0;
    }
  }
#endif

  conds=optimize_cond(conds,&cond_value);
  if (cond_value == Item::COND_FALSE || !thd->select_limit)
  {					/* Impossible cond */
    return_zero_rows(result, tables, fields,join.sum_func_count != 0 && !group,
		     select_options,"Impossible WHERE",join.having);
    delete procedure;
    DBUG_RETURN(0);
  }

  /* Optimize count(*) */
  if (tables && join.sum_func_count && ! group)
  {
    int res;
    if ((res=opt_sum_query(tables, all_fields, conds)))
    {
      if (res < 0)
      {
	return_zero_rows(result, tables, fields, !group,
			 select_options,"No matching min/max row",join.having);
	delete procedure;
	DBUG_RETURN(0);
      }
      if (select_options & SELECT_DESCRIBE)
      {
	describe_info("Select tables optimized away");
	delete procedure;
	DBUG_RETURN(0);
      }
      tables=0;					// All tables resolved
    }
  }
  if (!tables)
  {						// Only test of functions
    if (select_options & SELECT_DESCRIBE)
      describe_info("No tables used");
    else
    {
      result->send_fields(fields,1);
      if (!having || having->val_int())
      {
	if (result->send_data(fields))
	  result->send_error(0,NullS);		/* purecov: inspected */
	else
	  result->send_eof();
      }
      else
	result->send_eof();
    }
    delete procedure;
    DBUG_RETURN(0);
  }

  error = -1;
  join.sort_by_table=get_sort_by_table(order,group,tables);

  /* Calculate how to do the join */
  thd->proc_info="statistics";
  if (make_join_statistics(&join,tables,conds,&keyuse))
    goto err;
  thd->proc_info="preparing";
  if (join_read_const_tables(&join) && ! (select_options & SELECT_DESCRIBE))
  {
    error=0;
    return_zero_rows(result,tables,fields,
		     join.sum_func_count != 0 && !group,0,"",join.having);
    goto err;
  }
  if (!(thd->options & OPTION_BIG_SELECTS) &&
      join.best_read > (double) max_join_size &&
      !(select_options & SELECT_DESCRIBE))
  { /* purecov: inspected */
    result->send_error(ER_TOO_BIG_SELECT,ER(ER_TOO_BIG_SELECT)); /* purecov: inspected */
    error= 1; /* purecov: inspected */
    goto err; /* purecov: inspected */
  }
  if (!thd->locked_tables)
    mysql_unlock_some_tables(join.table,join.const_tables);
  select=make_select(join.table,join.tables,join.const_tables,
		     join.const_tables,conds,&error);
  if (error)
  { /* purecov: inspected */
    error= -1; /* purecov: inspected */
    goto err; /* purecov: inspected */
  }
  if (make_join_select(&join,select,conds))
  {
    error=0;
    return_zero_rows(result,tables,fields,join.sum_func_count != 0 && !group,
		     select_options,
		     "Impossible WHERE noticed after reading const tables",
		     join.having);
    goto err;
  }

  error= -1;					/* if goto err */

  /* Optimize distinct away if possible */
  if (group || join.sum_func_count)
  {
    if (! hidden_group_fields)
      select_distinct=0;
  }
  else if (select_distinct && join.tables - join.const_tables == 1 &&
	   (order || thd->select_limit == HA_POS_ERROR))
  {
    if ((group=add_all_fields_to_order(&join,order,fields)))
    {
      select_distinct=0;
      order=0;
      join.group=1;				// For end_write_group
    }
  }
  order=remove_const(&join,order,&simple_order);
  group=remove_const(&join,group,&simple_group);

  calc_group_buffer(&join,group);
  join.send_group_parts=join.group_parts;	/* Save org parts */
  if (procedure && procedure->group)
  {
    group=procedure->group=remove_const(&join,procedure->group,&simple_group);
    calc_group_buffer(&join,group);
  }

  if (test_if_subpart(group,order) || (!group && join.sum_func_count))
    order=0;

  // Can't use sort on head table if using cache
  if (join.full_join)
  {
    if (group)
      simple_group=0;
    if (order)
      simple_order=0;
  }

  need_tmp= (join.const_tables != join.tables &&
	     ((select_distinct || !simple_order || !simple_group) ||
	      (group && order)));

  make_join_readinfo(&join,
		     (select_options & SELECT_DESCRIBE) | SELECT_USE_CACHE);
  DBUG_EXECUTE("info",TEST_join(&join););
  if (select_options & SELECT_DESCRIBE)
  {
    select_describe(&join);
    error=0;
    goto err;
  }
  /*
    Because filesort always does a full table scan or a quick range scan
    one must add the removed reference to the select for the table.
    We only need to do this when we have a simple_order or simple_group
    as in other cases the join is done before the sort.
    */
  if ((order || group) && join.join_tab[join.const_tables].type != JT_ALL &&
      (simple_order || simple_group))
  {
#ifndef SAFE
    add_ref_to_table_cond(&join.join_tab[join.const_tables]);
#else
    // Do first a temporary table that contains the few found records.
    need_tmp=1; simple_order=simple_group=0;	// Force tmp table without sort
#endif
  }

  if (select_options & SELECT_SMALL_RESULT && (group || select_distinct))
  {
    need_tmp=1; simple_order=simple_group=0;	// Force tmp table without sort
  }

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    DBUG_PRINT("info",("Creating tmp table"));
    thd->proc_info="Creating tmp table";

    if (!(tmp_table =
	  create_tmp_table(thd,&join,all_fields,
			   ((!simple_group && !procedure &&
			     !(test_flags & TEST_NO_KEY_GROUP)) ?
			    group : (ORDER*) 0),
			   group ? 0 : select_distinct,
			   group && simple_group,
			   order == 0)))
      goto err;					/* purecov: inspected */

    if (having && (join.sort_and_group || tmp_table->distinct && !group))
      join.having=having;

    /* if group or order on first table, sort first */
    if (group && simple_group)
    {
      DBUG_PRINT("info",("Sorting for group"));
      thd->proc_info="Sorting for group";
      if (create_sort_index(&join.join_tab[join.const_tables],group,
			    HA_POS_ERROR))
	goto err; /* purecov: inspected */
      make_sum_func_list(&join,all_fields);
      alloc_group_fields(&join,group);
      group=0;
    }
    else
    {
      make_sum_func_list(&join,all_fields);
      if (!group && ! tmp_table->distinct && order && simple_order)
      {
	DBUG_PRINT("info",("Sorting for order"));
	thd->proc_info="Sorting for order";
	if (create_sort_index(&join.join_tab[join.const_tables],order,
			      HA_POS_ERROR))
	  goto err;				/* purecov: inspected */
	order=0;
      }
    }
    thd->proc_info="Copying to tmp table";
    if (do_select(&join,(List<Item> *) 0,tmp_table,0))
      goto err;					/* purecov: inspected */
    if (join.having)
      join.having=having=0;			// Allready done

    /* Change sum_fields reference to calculated fields in tmp_table */
    if (join.sort_and_group || tmp_table->group)
    {
      change_to_use_tmp_fields(all_fields);
      join.field_count+=join.sum_func_count+join.func_count;
      join.sum_func_count=join.func_count=0;
    }
    else
    {
      change_refs_to_tmp_fields(all_fields);
      join.field_count+=join.func_count;
      join.func_count=0;
    }
    if (tmp_table->group)
    {						// Already grouped
      if (!order)
	order=group;				/* order by group */
      group=0;
    }

    /*
    ** If we have different sort & group then we must sort the data by group
    ** and copy it to another tmp table
    */

    if (group && (!test_if_subpart(group,order) || select_distinct))
    {					/* Must copy to another table */
      TABLE *tmp_table2;
      DBUG_PRINT("info",("Creating group table"));

      /* Free first data from old join */
      join_free(&join);
      make_simple_join(&join,tmp_table);
      calc_group_buffer(&join,group);
      count_field_types(&join,all_fields);

      /* group data to new table */
      if (!(tmp_table2 = create_tmp_table(thd,&join,all_fields,(ORDER*) 0,
					  0,1,0)))
	goto err;				/* purecov: inspected */
      if (group)
      {
	thd->proc_info="Creating sort index";
	if (create_sort_index(join.join_tab,group,HA_POS_ERROR))
	{
	  free_tmp_table(thd,tmp_table2); /* purecov: inspected */
	  goto err;				/* purecov: inspected */
	}
	alloc_group_fields(&join,group);
	group=0;
      }
      make_sum_func_list(&join,all_fields);	// Is this possible ?
      thd->proc_info="Copying to group table";
      if (do_select(&join,(List<Item> *) 0,tmp_table2,0))
	goto err;				/* purecov: inspected */

      free_tmp_table(thd,tmp_table);
      join.const_tables=join.tables;		// Mark free for join_free()
      tmp_table=tmp_table2;
      join.join_tab[0].table=0;			// Table is freed

      change_to_use_tmp_fields(all_fields);	// No sum funcs anymore
      join.field_count+=join.sum_func_count;
      join.sum_func_count=0;
    }

    if (tmp_table->distinct)
      select_distinct=0;			/* Each row is uniq */

    if (select_distinct && ! group)
    {
      thd->proc_info="Removing duplicates";
      if (remove_duplicates(tmp_table,fields))
	goto err; /* purecov: inspected */
      select_distinct=0;
    }
    tmp_table->reginfo.lock_type=TL_UNLOCK;
    join_free(&join);				/* Free quick selects */
    make_simple_join(&join,tmp_table);
    calc_group_buffer(&join,group);
    count_field_types(&join,all_fields);
  }
  if (procedure)
  {
    procedure->change_columns(fields);
    count_field_types(&join,all_fields);
  }
  if (group || join.sum_func_count ||
      (procedure && (procedure->flags & PROC_GROUP)))
  {
    alloc_group_fields(&join,group);
    setup_copy_fields(&join,all_fields);
    make_sum_func_list(&join,all_fields);
    if (thd->fatal_error)
      goto err; /* purecov: inspected */
  }
  if (group || order)
  {
    DBUG_PRINT("info",("Sorting for send_fields"));
    thd->proc_info="Sorting result";
    /* If we have already done the group, add HAVING to sorted table */
    if (having && ! group && ! join.sort_and_group)
    {
      having->update_used_tables();		// Tablenr may have changed
      table_map used_tables=(1L << join.const_tables+1)-1L;
      JOIN_TAB *table=&join.join_tab[join.const_tables];

      Item* sort_table_cond=make_cond_for_table(having,used_tables,used_tables);
      if (sort_table_cond)
      {
	if (!table->select)
	  if (!(table->select=new SQL_SELECT))
	    goto err;
	if (!table->select->cond)
	  table->select->cond=sort_table_cond;
	else					// This should never happen
	  if (!(table->select->cond=new Item_cond_and(table->select->cond,
						      sort_table_cond)))
	    goto err;
	DBUG_EXECUTE("where",print_where(table->select->cond,
					 "select and having"););
	having=make_cond_for_table(having,~0L,~used_tables);
	DBUG_EXECUTE("where",print_where(conds,"having after sort"););
      }
    }
    if (create_sort_index(&join.join_tab[join.const_tables],
			  group ? group : order,
			  (having || group ||
			   join.const_tables != join.tables - 1) ?
			  HA_POS_ERROR : thd->select_limit))
      goto err; /* purecov: inspected */
  }
  join.having=having;				// Actually a parameter
  thd->proc_info="Sending data";
  error=do_select(&join,&fields,NULL,procedure);

err:
  thd->proc_info="end";
  join_free(&join);
  thd->proc_info="end2";			// QQ
  if (tmp_table)
    free_tmp_table(thd,tmp_table);
  thd->proc_info="end3";			// QQ
  delete select;
  delete_dynamic(&keyuse);
  delete procedure;
  thd->proc_info="end4";			// QQ
  DBUG_RETURN(error);
}

/*****************************************************************************
**	Create JOIN_TABS, make a guess about the table types,
**	Approximate how many records will be used in each table
*****************************************************************************/

static inline Item *and_conds(Item *a,Item *b)
{
  if (!b) return a;
  if (!a) return b;
  Item *cond=new Item_cond_and(a,b);
  cond->update_used_tables();
  return cond;
}

/*****************************************************************************
*	Approximate how many records will be read from table
*****************************************************************************/

static ha_rows get_quick_record_count(SQL_SELECT *select,uint table,uint keys)
{
  int error;
  DBUG_ENTER("get_quick_record_count");
  if (select)
  {
    select->head=select->tables[table];
    if ((error=select->test_quick_select(keys,0L,HA_POS_ERROR)) == 1)
      DBUG_RETURN(select->quick->records);
    if (error == -1)
      DBUG_RETURN(0);
    DBUG_PRINT("warning",("Couldn't use record count on const keypart"));
  }
  DBUG_RETURN(HA_POS_ERROR);			/* This shouldn't happend */
}


static bool
make_join_statistics(JOIN *join,TABLE_LIST *tables,COND *conds,
		     DYNAMIC_ARRAY *keyuse_array)
{
  int error;
  uint i,table_count,const_count,found_ref,refs,key,const_ref,eq_part;
  table_map const_bits;
  TABLE **table_vector,*table;
  JOIN_TAB *stat,*stat_end,*s,**stat_ref;
  SQL_SELECT *select;
  KEYUSE *keyuse,*start_keyuse;
  table_map outer_join=0;
  JOIN_TAB *stat_vector[MAX_TABLES+1];
  DBUG_ENTER("make_join_statistics");

  table_count=join->tables;
  stat=(JOIN_TAB*) sql_calloc(sizeof(JOIN_TAB)*table_count);
  stat_ref=(JOIN_TAB**) sql_calloc(sizeof(JOIN_TAB*)*table_count);
  table_vector=(TABLE**) sql_alloc(sizeof(TABLE**)*(table_count*2));
  if (!stat || !stat_ref || !table_vector)
    DBUG_RETURN(1);				// Eom /* purecov: inspected */
  select=0;

  join->best_ref=stat_vector;

  stat_end=stat+table_count;
  const_bits=const_count=0;
  for (s=stat,i=0 ; tables ; s++,tables=tables->next,i++)
  {
    TABLE *table;

    stat_vector[i]=s;
    table_vector[i]=s->table=table=tables->table;
    ha_info(table,2);				// Get record count
    table->quick_keys=0;
    if ((s->on_expr=tables->on_expr))
    {
      table->maybe_null=table->outer_join=1;	// Mark for send fields
      s->key_dependent=s->dependent=
	s->on_expr->used_tables() & ~(table->map);
      outer_join|=table->map;
      continue;
    }
    if (tables->straight)			// We don't have to move this
      s->dependent= table_vector[i-1]->map;
    else
      s->dependent=0L;
    s->key_dependent=0L;
    if (table->system || table->keyfile_info.records <= 1L)
    {
      s->type=JT_SYSTEM;
      const_bits|=table->map;
      set_position(join,const_count++,s,(KEYUSE*) 0);
    }
  }
  stat_vector[i]=0;

  /*
  ** If outer join: Re-arrange tables in stat_vector so that outer join
  ** tables are after all tables it is dependent of.
  ** For example: SELECT * from A LEFT JOIN B ON B.c=C.c, C WHERE A.C=C.C
  ** Will shift table B after table C.
  */
  if (outer_join)
  {
    table_map used_tables=0L;
    for (i=0 ; i < join->tables-1 ; i++)
    {
      while (stat_vector[i]->dependent & ~used_tables)
      {
	table_map tmp_tables=used_tables;
	JOIN_TAB *save= stat_vector[i];
	uint j=i;
	do
	{
	  stat_vector[j]=stat_vector[j+1];
	  if ((stat_vector[j]->dependent & save->table->map) &&
	      (save->dependent & stat_vector[j]->table->map))
	  {
	    join->tables=0;			// Don't use join->table
	    my_error(ER_WRONG_OUTER_JOIN,MYF(0));
	    DBUG_RETURN(1);
	  }
	  tmp_tables|= stat_vector[j]->table->map;
	  j++;
	}
	while (save->dependent & ~tmp_tables);
	stat_vector[j]=save;
      }
      used_tables|= stat_vector[i]->table->map;
    }
  }

  if (conds || outer_join)
    VOID(update_ref_and_keys(keyuse_array,stat,join->tables,conds,
			     ~outer_join));

  /* loop until no more const tables are found */
  do
  {
    found_ref=0;
    for (JOIN_TAB **pos=stat_vector+const_count; s= *pos ; pos++)
    {
      if (s->on_expr)				// If outer join table
      {
	if (s->dependent & ~(const_bits))	// All dep. must be constants
	  continue;
	if (s->table->keyfile_info.records <= 1L)
	{					// system table
	  s->type=JT_SYSTEM;
	  const_bits|=s->table->map;
	  set_position(join,const_count++,s,(KEYUSE*) 0);
	  continue;
	}
      }
      /* check if table can be read by key or table only uses const refs */
      if ((keyuse=s->keyuse))
      {
	s->type= JT_REF;
	table=s->table;
	while (keyuse->table == table)
	{
	  start_keyuse=keyuse;
	  key=keyuse->key;
	  s->keys|= 1L << key;			// QQ: remove this ?

	  refs=const_ref=eq_part=0;
	  do
	  {
	    if (!keyuse->field ||
		const_bits & keyuse->field->table->map)
	      const_ref|= 1L << keyuse->keypart;
	    else
	      refs|=keyuse->field->table->map;
	    eq_part|= 1L << keyuse->keypart;
	    keyuse++;
	  } while (keyuse->table == table && keyuse->key == key);

	  if (eq_part == PREV_BITS(table->key_info[key].key_parts) &&
	      !table->key_info[key].dupp_key)
	  {
	    if (const_ref == eq_part)
	    {					// Found everything for ref.
	      s->type=JT_CONST;
	      const_bits|=table->map;
	      set_position(join,const_count++,s,start_keyuse);
	      break;
	    }
	    else
	      found_ref|= refs;			// Is const is theese are const
	  }
	}
      }
    }
  } while (const_bits & found_ref);

  /* Calc how many (possible) matched records in each table */

  for (s=stat ; s < stat_end ; s++)
  {
    if (s->type == JT_SYSTEM || s->type == JT_CONST)
    {
      s->found_records=s->records=s->read_time=1;	/* System or ref */
      continue;
    }
    /* Approxomite found rows and time to read them */
    s->found_records=s->records=s->table->keyfile_info.records;
    s->read_time=(ha_rows) ((s->table->keyfile_info.data_file_length)/IO_SIZE)+1;
    /* if (s->type == JT_EQ_REF)
      continue; */
    if (s->const_keys)
    {
      ha_rows records;
      if (!select)
	select=make_select(table_vector,table_count,s->table->tablenr,
			   0,and_conds(conds,s->on_expr),&error);
      records=get_quick_record_count(select,s->table->tablenr,
				     s->const_keys);
      s->quick=select->quick;
      s->needed_reg=select->needed_reg;
      select->quick=0;
      if (records != HA_POS_ERROR)
      {
	s->found_records=records;
        s->read_time= s->quick ? s->quick->read_time : 0;
      }
      else
	s->const_keys=0;			/* Don't use const key */
    }
  }
  delete select;

  /* Find best combination and return it */
  join->join_tab=stat;
  join->map2table=stat_ref;
  join->table= table_vector;
  join->const_tables=const_count;
  join->const_bits=const_bits;

  if (join->const_tables != join->tables)
    find_best_combination(join);
  else
  {
    memcpy((gptr) join->best_positions,(gptr) join->positions,
	   sizeof(POSITION)*join->const_tables);
    join->best_read=1.0;
  }
  get_best_combination(join);
  DBUG_RETURN(0);
}


/*****************************************************************************
**	check with keys are used and with tables references with tables
**	updates in stat:
**	  keys	     Bitmap of all used keys
**	  const_keys Bitmap of all keys with may be used with quick_select
**	  keyuse     Pointer to possible keys
*****************************************************************************/

typedef struct key_field_t {		// Used when finding key fields
  Field		*field;
  Item		*val;			// May be empty if diff constant
  uint		level,const_level;	// QQ: Remove const_level
  bool		eq_func;
} KEY_FIELD;


/* merge new key definitions to old ones, remove those not used in both */

static KEY_FIELD *
merge_key_fields(KEY_FIELD *start,KEY_FIELD *new_fields,KEY_FIELD *end,
		 uint and_level)
{
  if (start == new_fields)
    return start;				// Impossible or
  if (new_fields == end)
    return start;				// No new fields, skipp all

  KEY_FIELD *first_free=new_fields;

  /* Mark all found fields in old array */
  for (; new_fields != end ; new_fields++)
  {
    for (KEY_FIELD *old=start ; old != first_free ; old++)
    {
      if (old->field == new_fields->field)
      {
	if (new_fields->val->type() == Item::FIELD_ITEM)
	{
	  if (old->val->eq(new_fields->val))
	    old->level=old->const_level=and_level;
	}
	else if (old->val->eq(new_fields->val) && old->eq_func &&
		 new_fields->eq_func)
	{
	  old->level=old->const_level=and_level;
	}
	else					// Impossible; remove it
	{
	  if (old == --first_free)		// If last item
	    break;
	  *old= *first_free;			// Remove old value
	  old--;					// Retry this value
	}
      }
    }
  }
  /* Remove all not used items */
  for (KEY_FIELD *old=start ; old != first_free ;)
  {
    if (old->level != and_level && old->const_level != and_level)
    {						// Not used in all levels
      if (old == --first_free)
	break;
      *old= *first_free;			// Remove old value
      continue;
    }
    old++;
  }
  return first_free;
}


static void
add_key_field(JOIN_TAB *stat,KEY_FIELD **key_fields,uint and_level,
	      Field *field,bool eq_func,Item *value,table_map usable_tables)
{
  if (!(field->flags & PART_KEY_FLAG))
    return;					// Not a key. Skipp it
  // Mark as possible quick key
  uint tablenr=field->table->tablenr;
  if (!(usable_tables & (table_map) 1 << tablenr))
    return;					// outer join table
  stat+=tablenr;
  stat[0].keys|=field->key_start;		// Add possible keys

    /* Save the following cases:
       Field op constant
       Field LIKE constant where constant doesn't start with a wildcard
       field = field2 where field2 is in a different table
       */
  if (!value)
  {						// Probably BETWEEN or IN
    stat[0].const_keys |= field->key_start;
    return;					// Can't be used as eq key
  }
  if ((value->type() == Item::FIELD_ITEM))
  {
    if (((Item_field *) value)->field->table->tablenr == tablenr ||
	!eq_func)
      return;
    stat[0].key_dependent|=value->used_tables();
  }
  else if (!value->const_item())
    return;
  else
    stat[0].const_keys |= field->key_start;

  (*key_fields)->field=field;
  (*key_fields)->eq_func=eq_func;
  (*key_fields)->val=value;
  (*key_fields)->level=(*key_fields)->const_level=and_level;
  (*key_fields)++;
}


static void
add_key_fields(JOIN_TAB *stat,KEY_FIELD **key_fields,uint *and_level,
	       COND *cond, table_map usable_tables)
{
  if (cond->type() == Item_func::COND_ITEM)
  {
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    KEY_FIELD *org_key_fields= *key_fields;

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item *item;
      while ((item=li++))
	add_key_fields(stat,key_fields,and_level,item,usable_tables);
      for (; org_key_fields != *key_fields ; org_key_fields++)
      {
	if (org_key_fields->const_level == org_key_fields->level)
	  org_key_fields->const_level=org_key_fields->level= *and_level;
	else
	  org_key_fields->const_level= *and_level;
      }
    }
    else
    {
      (*and_level)++;
      add_key_fields(stat,key_fields,and_level,li++,usable_tables);
      Item *item;
      while ((item=li++))
      {
	KEY_FIELD *start_key_fields= *key_fields;
	(*and_level)++;
	add_key_fields(stat,key_fields,and_level,item,usable_tables);
	*key_fields=merge_key_fields(org_key_fields,start_key_fields,
				     *key_fields,++(*and_level));
      }
    }
    return;
  }
  /* If item is of type 'field op field/constant' add it to key_fields */

  if (cond->type() != Item::FUNC_ITEM)
    return;
  Item_func *cond_func= (Item_func*) cond;
  switch (cond_func->select_optimize()) {
  case Item_func::OPTIMIZE_NONE:
    break;
  case Item_func::OPTIMIZE_KEY:
    if (cond_func->key_item()->type() == Item::FIELD_ITEM)
      add_key_field(stat,key_fields,*and_level,
		    ((Item_field*) (cond_func->key_item()))->field,
		    0,(Item*) 0,usable_tables);
    break;
  case Item_func::OPTIMIZE_OP:
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      add_key_field(stat,key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[0]))->field,
		    cond_func->functype() == Item_func::EQ_FUNC,
		    (cond_func->arguments()[1]),usable_tables);
    }
    if (cond_func->arguments()[1]->type() == Item::FIELD_ITEM &&
	cond_func->functype() != Item_func::LIKE_FUNC)
    {
      add_key_field(stat,key_fields,*and_level,
		    ((Item_field*) (cond_func->arguments()[1]))->field,
		    cond_func->functype() == Item_func::EQ_FUNC,
		    (cond_func->arguments()[0]),usable_tables);
    }
    break;
  }
  return;
}

/*
**	Add all keys with uses 'field' for some keypart
**	If field->and_level != and_level then only mark key_part as const_part
*/

static uint
max_part_bit(table_map bits)
{
  uint found;
  for (found=0; bits & 1 ; found++,bits>>=1) ;
  return found;
}


static void
add_key_part(DYNAMIC_ARRAY *keyuse_array,KEY_FIELD *key_field)
{
  Field *field=key_field->field,*cmp_field;
  TABLE *form= field->table;
  KEYUSE keyuse;
  cmp_field= ((key_field->val->type() == Item::FIELD_ITEM) ?
	      ((Item_field*) key_field->val)->field : 0);

  for (uint key=0 ; key < form->keys ; key++)
  {
    bool test_eq=  (key_field->eq_func);
    if (test_eq || (1L << key) && field->key_start)
    {
      uint key_parts= test_eq ? (uint) form->key_info[key].key_parts : 1;
      for (uint part=0 ; part <  key_parts ; part++)
      {
	/*
	  QQ: Fix this to remove the pack_length() test.
	  One should be able to use different packed fields
	  */
	if (field->eq(form->key_info[key].key_part[part].field) &&
	    (!cmp_field ||
	     (cmp_field->pack_length() == field->pack_length() &&
	      cmp_field->type() == field->type())))
	{
	  if (test_eq)
	  {
	    keyuse.table= field->table;
	    keyuse.field= cmp_field;
	    keyuse.val =  key_field->val;
	    keyuse.key =  key;
	    keyuse.keypart=part;
	    VOID(insert_dynamic(keyuse_array,(gptr) &keyuse));
	  }
	}
      }
    }
  }
}


static int
sort_keyuse(KEYUSE *a,KEYUSE *b)
{
  if (a->table->tablenr != b->table->tablenr)
    return (int) (a->table->tablenr - b->table->tablenr);
  if (a->key != b->key)
    return (int) (a->key - b->key);
  if (a->keypart != b->keypart)
    return (int) (a->keypart - b->keypart);
  return test(a->field) - test(b->field);	// Place const first
}


/*
** Update keyuse array with all possible keys we can use to fetch rows
** join_tab is a array in tablenr_order
** stat is a reference array in 'prefered' order.
*/

static uint
update_ref_and_keys(DYNAMIC_ARRAY *keyuse,JOIN_TAB *join_tab,uint tables,
		    COND *cond, table_map normal_tables)
{
  uint	and_level,i,tablenr,found_eq_constant;
  KEY_FIELD *key_fields,*end;

  if (!(key_fields=(KEY_FIELD*)
	my_malloc(sizeof(key_fields[0])*
		  (current_thd->cond_count+1)*2,MYF(0))))
    return 0; /* purecov: inspected */
  and_level=0; end=key_fields;
  if (cond)
    add_key_fields(join_tab,&end,&and_level,cond,normal_tables);
  for (i=0 ; i < tables ; i++)
  {
    if (join_tab[i].on_expr)
    {
      add_key_fields(join_tab,&end,&and_level,join_tab[i].on_expr,
		     join_tab[i].table->map);
    }
  }
  VOID(init_dynamic_array(keyuse,sizeof(KEYUSE),20,20));

  /* fill keyuse with found key parts */
  for (KEY_FIELD *field=key_fields ; field != end ; field++)
    add_key_part(keyuse,field);
  my_free((gptr) key_fields,MYF(0));

  /*
  ** remove ref if there is a keypart which is a ref and a const.
  ** remove keyparts without previous keyparts.
  */
  if (keyuse->elements)
  {
    KEYUSE end,*prev,*save_pos,*use;

    qsort(keyuse->buffer,keyuse->elements,sizeof(KEYUSE),
	  (qsort_cmp) sort_keyuse);

    bzero((char*) &end,sizeof(end));		/* Add for easy testing */
    VOID(insert_dynamic(keyuse,(gptr) &end));

    use=save_pos=dynamic_element(keyuse,0,KEYUSE*);
    prev=&end;
    found_eq_constant=0;
    for (i=0 ; i < keyuse->elements-1 ; i++,use++)
    {
      if (use->key == prev->key && use->table == prev->table)
      {
	if (prev->keypart+1 < use->keypart ||
	    prev->keypart == use->keypart && found_eq_constant)
	  continue;				/* remove */
      }
      else if (use->keypart != 0)		// First found must be 0
	continue;

      *save_pos= *use;
      prev=use;
      found_eq_constant= !use->field;
      tablenr=use->table->tablenr;
      if (!join_tab[tablenr].keyuse)
	join_tab[tablenr].keyuse= save_pos;	/* Save ptr to first use */
      save_pos++;
    }
    i=(uint) (save_pos-(KEYUSE*) keyuse->buffer);
    VOID(set_dynamic(keyuse,(gptr) &end,i));
    keyuse->elements=i;
  }
  return and_level;
}


/*****************************************************************************
**	Go through all combinations of not marked tables and find the one
**	which uses least records
*****************************************************************************/

/* Save const tables first as used tables */

static void
set_position(JOIN *join,uint index,JOIN_TAB *table,KEYUSE *key)
{
  join->positions[index].table= table;
  join->positions[index].key=key;
  join->positions[index].records_read=1.0;	/* This is a const table */
  join->index[table->table->tablenr]= (table_map) 1L << index;

  /* Move the const table as down as possible in best_ref */
  JOIN_TAB **pos=join->best_ref+index+1;
  JOIN_TAB *next=join->best_ref[index];
  for ( ;next != table ; pos++)
  {
    JOIN_TAB *tmp=pos[0];
    pos[0]=next;
    next=tmp;
  }
  join->best_ref[index]=table;
}


static void
find_best_combination(JOIN *join)
{
  DBUG_ENTER("find_best_combination");
  join->best_read=DBL_MAX;
  find_best(join,PREV_BITS(join->tables) & ~join->const_bits,
	    join->const_tables,1.0,0.0);
  DBUG_VOID_RETURN;
}


static void
find_best(JOIN *join,table_map rest_tables,uint index,double record_count,
	  double read_time)
{
  uint keypart;
  ulong rec;
  double tmp;

  if (!rest_tables)
  {
    DBUG_PRINT("best",("read_time: %g  record_count: %g",read_time,
		       record_count));

    read_time+=record_count/(double) TIME_FOR_COMPARE;
    if (join->sort_by_table &&
	join->sort_by_table != join->positions[join->const_tables].table->table)
      read_time+=record_count;			// We have to make a temp table
    if (read_time < join->best_read)
    {
      memcpy((gptr) join->best_positions,(gptr) join->positions,
	     sizeof(POSITION)*index);
      join->best_read=read_time;
    }
    return;
  }
  if (read_time+record_count/10.0 >= join->best_read)
    return;					/* Found better before */

  JOIN_TAB *s;
  double best_record_count=DBL_MAX,best_read_time=DBL_MAX;
  for (JOIN_TAB **pos=join->best_ref+index ; (s=*pos) ; pos++)
  {
    table_map real_table_bit=s->table->map;
    if (rest_tables & real_table_bit && !(rest_tables & s->dependent))
    {
      double best,records;
      best=records=DBL_MAX;
      KEYUSE *best_key=0;

      if (s->keyuse)
      {						/* Use key if possible */
	TABLE *table=s->table;
	KEYUSE *keyuse,*start_key=0;
	double best_records=DBL_MAX;

	/* Test how we can use keys */
	rec= s->records/10;			/* Assume 10 records/key */
	for (keyuse=s->keyuse ; keyuse->table == table ;)
	{
	  key_map found_part=0;
	  table_map found_ref=0;
	  uint key=keyuse->key;
	  KEY *keyinfo=table->key_info+key;

	  start_key=keyuse;
	  do
	  {
	    keypart=keyuse->keypart;
	    do
	    {
	      if (!keyuse->field ||
		  !(rest_tables & keyuse->field->table->map))
	      {
		found_part|=1L << keypart;
		if (keyuse->field)
		  found_ref|= join->index[keyuse->field->table->tablenr];
	      }
	      /*
	      ** If we find a ref, assume this table matches a proportional
	      ** part of this table.
	      ** For example 100 records matching this table with 5000 records
	      ** gives 5000/100 = 50 records per key
	      */
	      if (keyuse->field &&
		  rec > keyuse->field->table->keyfile_info.records)
		rec=keyuse->field->table->keyfile_info.records;
	      keyuse++;
	    } while (keyuse->table == table && keyuse->key == key &&
		     keyuse->keypart == keypart);
	  } while (keyuse->table == table && keyuse->key == key);

	  /*
	  ** Assume that that each key matches a proportional part of table.
	  */
	  if (!found_part)
	    continue;				// Nothing usable found
	  if (rec == 0)
	    rec=1L;				// Fix for small tables
	  /*
	  ** Check if we found full key
	  */
	  if (found_part == PREV_BITS(keyinfo->key_parts))
	  {				/* use eq key */
	    if (!keyinfo->dupp_key)
	    {
	      tmp=prev_record_reads(join,found_ref);
	      records=1.0;
	    }
	    else
	    {
	      if (!found_ref)
	      {
		if (table->quick_keys & ((key_map) 1 << key))
		  records= (double) table->quick_rows[key];
		else
		  records= (double) s->records;	// quick_range couldn't use key!
	      }
	      else
	      {
		if (!table->keyfile_info.rec_per_key ||
		    !(records=table->keyfile_info.rec_per_key[key]))
		{				// Prefere longer keys
		  records=
		    ((double) s->records / (double) rec *
		     (1.0 +
		      ((double) (table->max_key_length-keyinfo->key_length) /
		       (double) table->max_key_length)));
		  // Fix ranges for small tables */
		  if (records > (double) s->records/5.0)
		    records= (double) s->records/5.0;
		  if (records < 2.0)
		    records=2.0;		// Can't be as good as a unique
		}
	      }
	      if (table->used_keys & ((key_map) 1 << key))
	      {
		/* we can use only index tree */
		uint keys_per_block= table->keyfile_info.block_size/2/
		  table->key_info[key].key_length+1;
		tmp=record_count*(records+keys_per_block-1)/keys_per_block;
	      }
	      else
		tmp=record_count*records;
	    }
	  }
	  else
	  {
	    /*
	    ** Use as much key-parts as possible and a uniq key is better
	    ** than a not unique key
	    ** Set tmp to (previous record count) * (records / combination)
	    */
	    if (found_part & 1)
	    {
	      /*
	      ** Assume that the first key part matches 1% of the file
	      ** and that the hole key matches 10 (dupplicates) or 1
	      ** (unique) records.
	      ** Assume also that more key matches proportionally more
	      ** records
	      ** This gives the formula:
	      ** records= (x * (b-a) + a*c-b)/(c-1)
	      **
	      ** b = records matched by whole key
	      ** a = records matched by first key part (10% of all records?)
	      ** c = number of key parts in key
	      ** x = used key parts (1 <= x <= c)
	      */
	      double rec_per_key;
	      if (keyinfo->dupp_key)
		rec_per_key=1;
	      if (!table->keyfile_info.rec_per_key ||
		  !(rec_per_key=(double) table->keyfile_info.rec_per_key[key]))
		rec_per_key=(double) s->records/rec+1;

	      if (rec_per_key/(double) s->records >= 0.01)
		tmp=rec_per_key;
	      else
	      {
		double a=s->records*0.01;
		tmp=(max_part_bit(found_part) * (rec_per_key - a) +
		     a*keyinfo->key_parts - rec_per_key)/(keyinfo->key_parts-1);
		set_if_bigger(tmp,1.0);
	      }
	      records=(ulong) tmp;
	      if (table->used_keys & ((key_map) 1 << key))
	      {
		/* we can use only index tree */
		uint keys_per_block= table->keyfile_info.block_size/2/
		  table->key_info[key].key_length+1;
		tmp=(tmp+keys_per_block-1)/keys_per_block;
	      }
	      tmp*=record_count;
	    }
	    else
	      tmp=best;				// Do nothing
	  }
	  if (tmp < best)
	  {
	    best=tmp;
	    best_records=records;
	    best_key=start_key;
	  }
	}
	records=best_records;
      }
      if (records >= s->found_records || best > s->read_time)
      {						// Check full join
	if (s->on_expr)
	{
	  tmp=s->found_records;			// Can't use read cache
	}
	else
	{
	  tmp=(double) s->read_time;
	  /* Calculate time to read trough cache */
	  tmp*=(1.0+floor((double) cache_record_length(join,index)*
			  record_count/(double) join_buff_size));
	}
	if (best == DBL_MAX ||
	    (tmp  + record_count/10.0*s->found_records <
	     best + record_count/10.0*records))
	{
	  best=tmp;
	  records=s->found_records;
	  best_key=0;
	}
      }
      join->positions[index].records_read=(double) records;
      join->positions[index].key=best_key;
      join->positions[index].table= s;
      join->index[s->table->tablenr]= (table_map) 1L << index;
      if (!best_key && index == join->const_tables &&
	  s->table == join->sort_by_table)
	join->sort_by_table= (TABLE*) 1;	// Must use temporary table

     /*
	Go to the next level only if there hasn't been a better key on
	this level! This will cut down the search for a lot simple cases!
       */
      double current_record_count=record_count*records;
      double current_read_time=read_time+best;
      if (best_record_count > current_record_count ||
	  best_read_time > current_read_time ||
	  index == join->const_tables && s->table == join->sort_by_table)
      {
	if (best_record_count >= current_record_count &&
	    best_read_time >= current_read_time &&
	    (!(s->key_dependent & rest_tables) || records < 2.0))
	{
	  best_record_count=current_record_count;
	  best_read_time=current_read_time;
	}
	swap(JOIN_TAB*,join->best_ref[index],*pos);
	find_best(join,rest_tables & ~real_table_bit,index+1,
		  current_record_count,current_read_time);
	swap(JOIN_TAB*,join->best_ref[index],*pos);
      }
      if (join->select_options & SELECT_STRAIGHT_JOIN)
	break;				// Don't test all combinations
    }
  }
}


/*
** Find how much space the prevous read not const tables takes in cache
*/

static uint
cache_record_length(JOIN *join,uint index)
{
  uint length;
  JOIN_TAB **pos,**end;
  THD *thd=current_thd;

  length=0;
  for (pos=join->best_ref+join->const_tables,end=join->best_ref+index ;
       pos != end ;
       pos++)
  {
    JOIN_TAB *join_tab= *pos;
    if (!join_tab->used_fieldlength)
    {					/* Not calced yet */
      uint null_fields,blobs,fields,rec_length;
      null_fields=blobs=fields=rec_length=0;

      Field **f_ptr,*field;
      for (f_ptr=join_tab->table->field ; (field= *f_ptr) ; f_ptr++)
      {
	if (field->query_id == thd->query_id)
	{
	  uint flags=field->flags;
	  fields++;
	  rec_length+=field->pack_length();
	  if (flags & BLOB_FLAG)
	    blobs++;
	  if (!(flags & NOT_NULL_FLAG))
	    null_fields++;
	}
      }
      if (null_fields)
	rec_length+=(join_tab->table->null_fields+7)/8;
      if (join_tab->table->maybe_null)
	rec_length+=sizeof(my_bool);
      if (blobs)
      {
	uint blob_length=(uint) (join_tab->table->
				 keyfile_info.mean_rec_length-
				 (join_tab->table->reclength-
				  rec_length));
	rec_length+=(uint) max(4,blob_length);
      }
      join_tab->used_fields=fields;
      join_tab->used_fieldlength=rec_length;
      join_tab->used_blobs=blobs;
    }
    length+=join_tab->used_fieldlength;
  }
  return length;
}


static double
prev_record_reads(JOIN *join,table_map found_ref)
{
  double found=1.0;

  for (POSITION *pos=join->positions ; found_ref ; pos++,found_ref>>=1)
  {
    if ((found_ref & 1) || !pos->key)
      found*=pos->records_read;
  }
  return found;
}


/*****************************************************************************
**	Set up join struct according to best position.
**	Change tablenr to new order
*****************************************************************************/

static void
get_best_combination(JOIN *join)
{
  uint i,ref_count,key,length,tablenr;
  table_map used_tables;
  TABLE *table;
  JOIN_TAB *join_tab,*j;
  KEYUSE *keyuse;
  KEY *keyinfo;
  uint table_count;

  table_count=join->tables;
  join->join_tab=join_tab=(JOIN_TAB*) sql_alloc(sizeof(JOIN_TAB)*table_count);
  join->const_tables=0;				/* for checking */
  join->full_join=0;

  used_tables=0;
  for (j=join_tab, tablenr=0 ; tablenr < table_count ; tablenr++,j++)
  {
    TABLE *form;
    *j= *join->best_positions[tablenr].table;
    used_tables|= j->table->map;
    form=join->table[tablenr]=j->table;
    form->reginfo.ref_fields=0;
    form->reginfo.ref_key= -1;
    j->info=0;					// For describe

    if (j->type == JT_SYSTEM)
    {
      j->table->const_table=1;
      if (join->const_tables == tablenr)
	join->const_tables++;
      continue;
    }
    if (!j->keys || !(keyuse= join->best_positions[tablenr].key))
    {
      j->type=JT_ALL;
      if (tablenr != join->const_tables)
	join->full_join=1;
    }
    else
    {
      uint keyparts;
      /*
      ** Use best key from find_best
      */
      table=j->table;
      key=keyuse->key;

      keyparts=length=0;
      keyinfo=table->key_info+key;
      do
      {
	if ((!keyuse->field ||
	     (used_tables & keyuse->field->table->map)))
	{
	  if (keyparts == keyuse->keypart)
	  {
	    keyparts++;
	    length+=keyinfo->key_part[keyuse->keypart].length;
	  }
	}
	keyuse++;
      } while (keyuse->table == table && keyuse->key == key);


      /* set up fieldref */
      keyinfo=table->key_info+key;
      form->reginfo.ref_fields=keyparts;
      form->reginfo.ref_length=length;
      form->reginfo.ref_key=(int) key;
      form->reginfo.key_buff= (byte*) sql_calloc(length);
      ref_count=0;
      keyuse=join->best_positions[tablenr].key;
      REF_FIELD *ref_field=form->reginfo.ref_field;
      for (i=0 ; i < keyparts ; keyuse++,ref_field++,i++)
      {
	while (keyuse->keypart != i ||
	       (keyuse->field &&
		!(used_tables & keyuse->field->table->map)))
	  keyuse++;				/* Skipp other parts */
	if (!(ref_field->field=keyuse->field))
	{					// Compare against constant
	  Field *field=keyinfo->key_part[i].field;
	  store_val_in_field(field,keyuse->val);
	  ref_field->length=field->pack_length();
	  ref_field->ptr=(char*) sql_alloc(ref_field->length);
	  field->get_image(ref_field->ptr,ref_field->length);
	  ref_field->field=field->new_field(0); // Make a new field for cmp()
	  ref_field->field->move_field(ref_field->ptr);
	  /* Make a nice field name for 'explain' */
	  ref_field->field->table_name=0;
	  ref_field->field->field_name= (char*) keyuse->val->full_name();
	}
	else
	{
	  ref_count++;
	  ref_field->ptr=keyuse->field->ptr;
	  ref_field->length=form->key_info[key].key_part[i].length;
	}
	/* Fix last key part if last key is only a keypart */
	ref_field->length=min(ref_field->length,length);
	length-=ref_field->length;
      }
      if (j->type == JT_CONST)
      {
	j->table->const_table=1;
	if (join->const_tables == tablenr)
	  join->const_tables++;
      }
      else if (keyinfo->dupp_key || keyparts != keyinfo->key_parts)
	j->type=JT_REF;				/* Must read with repeat */
      else if (!ref_count)
      {						/* Should never be reached */
	j->type=JT_CONST;			/* purecov: deadcode */
	if (join->const_tables == tablenr)
	  join->const_tables++;			/* purecov: deadcode */
      }
      else
	j->type=JT_EQ_REF;
    }
  }

  for (i=0 ; i < table_count ; i++)
  {
    join->table[i]->tablenr=i;
    join->table[i]->map = (table_map) 1 << i;
    join->map2table[i]=join->join_tab+i;	// For MySQL 3.23
  }
  return;
}

/*
** This function is only called for const items on fields with are keys
** (NOT NULL fields*)
** returns 1 if there was some conversion made when the field was stored.
*/

bool
store_val_in_field(Field *field,Item *val)
{
  if (val->null_value)
  {
    field->reset();
    return 0;
  }
  THD *thd=current_thd;
  ulong cuted_fields=thd->cuted_fields;
  thd->count_cuted_fields=1;
  if (val->result_type() == STRING_RESULT)
  {
    String *result=val->str((String*) 0);	// Safe because it's a const
    field->store(result->ptr(),result->length());
  }
  else if (val->result_type() == INT_RESULT)
    field->store(val->val_int());
  else
    field->store(val->val());
  current_thd->count_cuted_fields=0;
  return cuted_fields != thd->cuted_fields;
}


static void
make_simple_join(JOIN *join,TABLE *tmp_table)
{
  TABLE **tableptr;
  JOIN_TAB *join_tab;

  tableptr=(TABLE**) sql_alloc(sizeof(TABLE*));
  join_tab=(JOIN_TAB*) sql_alloc(sizeof(JOIN_TAB));
  join->join_tab=join_tab;
  join->table=tableptr; tableptr[0]=tmp_table;
  join->tables=1;
  join->const_tables=0;
  join->copy_field_count=join->field_count=join->sum_func_count=
    join->func_count=0;
  join->first_record=join->sort_and_group=0;
  join->sum_funcs=0;
  join->send_records=0L;
  join->copy_field=0;

  join_tab->cache.buff=0;			/* No cacheing */
  join_tab->table=tmp_table;
  join_tab->select=0;
  join_tab->quick=0;
  join_tab->type= JT_ALL;			/* Map through all records */
  join_tab->keys= (uint) ~0;			/* test everything in quick */
  join_tab->info=0;
  join_tab->on_expr=0;
  tmp_table->status=0;
  tmp_table->null_row=0;
  join_tab->read_first_record= join_init_read_record;
}


static bool
make_join_select(JOIN *join,SQL_SELECT *select,COND *cond)
{
  DBUG_ENTER("make_join_select");
  if (select)
  {
    if (join->tables > 1)
      cond->update_used_tables();		// Tablenr may have changed
    {						// Check const tables
      COND *const_cond=
	make_cond_for_table(cond,((table_map) 1 << join->const_tables)-1L,0L);
      DBUG_EXECUTE("where",print_where(const_cond,"constants"););
      if (const_cond && !const_cond->val_int())
	DBUG_RETURN(1);				// Impossible const condition
    }
    select->const_tables=((table_map) 1L << join->const_tables)-1L;
    for (uint i=join->const_tables ; i < join->tables ; i++)
    {
      JOIN_TAB *tab=join->join_tab+i;
      COND *tmp=make_cond_for_table(cond,((table_map) 1L << (i+1))-1L,
				    (table_map) 1L << i);
      if (tmp)
      {
	DBUG_EXECUTE("where",print_where(tmp,tab->table->table_name););
	SQL_SELECT *sel=tab->select=(SQL_SELECT*)
	  sql_memdup((gptr) select, sizeof(SQL_SELECT));
	sel->cond=tmp;
	sel->head=sel->tables[i];
	if (tab->quick && tab->needed_reg == 0)
	{
	  sel->quick=tab->quick;		// Use value from get_quick_...
	  sel->quick_keys=sel->needed_reg=0;
	  tab->quick=0;
	}
	else
	{
	  delete tab->quick;
	  tab->quick=0;
	}
	uint ref_key=(uint) select->head->reginfo.ref_key+1;
	if (i == join->const_tables && ref_key)
	{
	  if (!sel->quick &&
	      sel->test_quick_select(ref_key,0L,join->thd->select_limit) < 0)
	    DBUG_RETURN(1);				// Impossible range
	}
	else if (tab->type == JT_ALL)
	{
	  if (!sel->quick &&
	      sel->test_quick_select(tab->keys, ((table_map) 1L << i) -1L,
				     join->thd->select_limit) < 0)
	    DBUG_RETURN(1);				// Impossible range
	  if (sel->quick_keys | sel->needed_reg)
	  {
	    tab->keys=sel->quick_keys | sel->needed_reg;
	    tab->use_quick= (sel->needed_reg &&
			     (!select->quick_keys ||
			      (select->quick &&
			       (select->quick->records >= 100L)))) ?
	      2 : 1;
	    sel->read_tables=PREV_BITS(i);
	  }
	  if (i != join->const_tables && tab->use_quick != 2)
	  {					/* Read with cache */
	    if ((tmp=make_cond_for_table(cond,
					 (PREV_BITS(join->const_tables) |
					  ((table_map) 1L << i)),
					 (table_map) 1L << i)))
	    {
	      DBUG_EXECUTE("where",print_where(tmp,"cache"););
	      tab->cache.select=(SQL_SELECT*) sql_memdup((gptr) sel,
						    sizeof(SQL_SELECT));
	      tab->cache.select->cond=tmp;
	      tab->cache.select->read_tables=PREV_BITS(join->const_tables);
	    }
	  }
	}
      }
    }
  }
  DBUG_RETURN(0);
}


static void
make_join_readinfo(JOIN *join,uint options)
{
  uint i;

  for (i=join->const_tables ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *table=tab->table;
    tab->read_record.form= table;
    tab->next_select=sub_select;		/* normal select */
    switch (tab->type) {
    case JT_SYSTEM:				// Only happens with left join
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_read_system;
      tab->read_record.read_record= join_no_more_records;
      break;
    case JT_CONST:				// Only happens with left join
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_read_const;
      tab->read_record.read_record= join_no_more_records;
      break;
    case JT_EQ_REF:
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_read_key;
      tab->read_record.read_record= join_no_more_records;
      if (table->used_keys & ((key_map) 1 << table->reginfo.ref_key))
      {
	table->key_read=1;
	ha_extra(table,HA_EXTRA_KEYREAD);
      }
      break;
    case JT_REF:
      table->status=STATUS_NO_RECORD;
      tab->read_first_record= join_read_always_key;
      tab->read_record.read_record= join_read_next;
      if (table->used_keys & ((key_map) 1 << table->reginfo.ref_key))
      {
	table->key_read=1;
	ha_extra(table,HA_EXTRA_KEYREAD);
      }
      break;
    case JT_ALL:
      /*
      ** if previous table use cache
      */
      table->status=STATUS_NO_RECORD;
      if (i != join->const_tables && (options & SELECT_USE_CACHE) &&
	  tab->use_quick != 2 && !tab->on_expr)
      {
	if ((options & SELECT_DESCRIBE) ||
	    !join_init_cache(join->thd,join->join_tab+join->const_tables,
			     i-join->const_tables))
	{
	  tab[-1].next_select=sub_select_cache; /* Patch previous */
	}
      }
      /* These init changes read_record */
      if (tab->use_quick == 2)
	tab->read_first_record= join_init_quick_read_record;
      else
      {
	tab->read_first_record= join_init_read_record;
	if (tab->select && tab->select->quick &&
	    table->used_keys & ((key_map) 1 << tab->select->quick->index))
	{
	  table->key_read=1;
	  ha_extra(table,HA_EXTRA_KEYREAD);
	}
	else if (table->used_keys)
	{					// Only read index tree
	  tab->index=find_shortest_key(table, table->used_keys);
	  tab->read_first_record= join_init_read_first_with_key;
	  tab->type=JT_NEXT;		// Read with ha_rfirst(), ha_rnext()
	}
      }
      break;
    default:
      DBUG_PRINT("error",("Table type %d found",tab->type)); /* purecov: deadcode */
      break; /* purecov: deadcode */
    case JT_UNKNOWN:
    case JT_MAYBE_REF:
      abort(); /* purecov: deadcode */
    }
  }
  join->join_tab[join->tables-1].next_select=0; /* Set by do_select */
}


static void
join_free(JOIN *join)
{
  JOIN_TAB *tab,*end;

  if (join->table)
  {
    /* only sorted table is cached */
    if (join->tables > join->const_tables)
      free_io_cache(join->table[join->const_tables]);
    for (tab=join->join_tab,end=tab+join->tables ; tab != end ; tab++)
    {
      delete tab->select;
      delete tab->quick;
      x_free(tab->cache.buff);
      end_read_record(&tab->read_record);
      if (tab->table && tab->table->key_read)
      {
	tab->table->key_read=0;
	ha_extra(tab->table,HA_EXTRA_NO_KEYREAD);
      }
    }
  }
  // We are not using tables anymore
  // Unlock all tables. We may be in a an INSERT .... SELECT statement.
  if (join->lock)
  {
    mysql_unlock_read_tables(join->lock);	// Don't free join->lock
    join->lock=0;
  }
  join->group_fields.delete_elements();
  join->copy_funcs.delete_elements();
  delete [] join->copy_field;
  join->copy_field=0;
}


/*****************************************************************************
** Remove the following expressions from ORDER BY and GROUP BY:
** Constant expressions
** Expression that only uses tables that are of type EQ_REF and the reference
** is in the ORDER list or if all referee tables are of the above types.
*****************************************************************************/

static bool
eq_ref_table(JOIN *join, ORDER *start_order, JOIN_TAB *tab)
{
  if (tab->cached_eq_ref_table)			// If cached
    return tab->eq_ref_table;
  tab->cached_eq_ref_table=1;
  for (;;)
  {
    if (tab->type == JT_CONST)
      return (tab->eq_ref_table=1);		// We can skip this /* purecov: inspected */
    if (tab->type != JT_EQ_REF)
      return (tab->eq_ref_table=0);		// We must use this
    REF_FIELD *ref_field=tab->table->reginfo.ref_field;
    REF_FIELD *end=ref_field+tab->table->reginfo.ref_fields;
    for (; ref_field != end ; ref_field++)
    {
      if (ref_field->field->table)
      {						// Not a const ref
	ORDER *order;
	for (order=start_order ; order ; order=order->next)
	{
	  if (order->item[0]->type() == Item::FIELD_ITEM &&
	      ((Item_field*) order->item[0])->field == ref_field->field)
	    break;
	}
	if (order)
	  break;				// Used in ORDER BY
	if (!eq_ref_table(join,start_order,
			  join->map2table[ref_field->field->table->tablenr]))
	  return (tab->eq_ref_table=0);
      }
    }
    return tab->eq_ref_table=1;
  }
}


static bool
only_eq_ref_tables(JOIN *join,ORDER *order,table_map tables)
{
  if (specialflag &  SPECIAL_SAFE_MODE)
    return 0;					// skip this optimize /* purecov: inspected */
  for (JOIN_TAB **tab=join->map2table ; tables ; tab++, tables>>=1)
  {
    if (tables & 1 && !eq_ref_table(join, order, *tab))
      return 0;
  }
  return 1;
}


/*
**  simple_order is set to 1 if sort_order only uses fields from head table
**  and the head table is not a LEFT JOIN table
*/

static ORDER *
remove_const(JOIN *join,ORDER *first_order,bool *simple_order)
{
  ORDER *order,**prev_ptr;
  table_map first_table= (table_map) 1L << join->const_tables;
  table_map not_const_tables= ~(first_table-1L);
  table_map ref;
  prev_ptr= &first_order;
  *simple_order= (join->tables != join->const_tables &&
		  join->join_tab[join->const_tables].on_expr ? 0 : 1);

  /* NOTE: A variable of not_const_tables ^ first_table; breaks gcc 2.7 */
  DBUG_ENTER("remove_const");

  for (order=first_order; order ; order=order->next)
  {
    order->item[0]->update_used_tables();
    if (order->item[0]->with_sum_func)
      *simple_order=0;				// Must do a temp table to sort
    else if (!(order->item[0]->used_tables() & not_const_tables))
      continue;					// skipp const item
    else if ((ref=order->item[0]->used_tables() &
	      (not_const_tables ^ first_table)))
    {
      if (only_eq_ref_tables(join,first_order,ref))
	continue;
      *simple_order=0;				// Must do a temp table to sort
    }
    *prev_ptr= order;				// use this entry
    prev_ptr= &order->next;
  }
  *prev_ptr=0;
  DBUG_PRINT("exit",("simple_order: %d",(int) *simple_order));
  DBUG_RETURN(first_order);
}

static void
return_zero_rows(select_result *result,TABLE_LIST *tables,List<Item> &fields,
		 bool send_row, uint select_options,const char *info,
		 Item *having)
{
  DBUG_ENTER("return_zero_rows");

  if (select_options & SELECT_DESCRIBE)
  {
    describe_info(info);
    DBUG_VOID_RETURN;
  }
  else if (send_row)
  {
    for (TABLE_LIST *table=tables; table ; table=table->next)
      table->table->null_row=1;			// All fields are NULL
    if (having && having->val_int() == 0.0)
      send_row=0;
  }
  if (!tables || !(result->send_fields(fields,1)))
  {
    if (send_row)
      result->send_data(fields);
    if (tables)					// Not from do_select()
      result->send_eof();
  }
  DBUG_VOID_RETURN;
}


static void clear_tables(JOIN *join)
{
  for (uint i=0 ; i < join->tables ; i++)
    join->table[i]->null_row=1;		// All fields are NULL
}

/*****************************************************************************
** Make som simple condition optimization:
** If there is a test 'field = const' change all refs to 'field' to 'const'
** Remove all dummy tests 'item = item', 'const op const'.
** Remove all 'item is NULL', when item can never be null!
** item->marker should be 0 for all items on entry
** Return in cond_value FALSE if condition is impossible (1 = 2)
*****************************************************************************/

class COND_CMP :public Sql_alloc {
public:
  Item *and_level;
  Item_func *cmp_func;
  COND_CMP(Item *a,Item_func *b) :and_level(a),cmp_func(b) {}
};

#ifdef __GNUC__
template class List<COND_CMP>;
template class List_iterator<COND_CMP>;
#endif

/*
** change field = field to field = const for each found field = const in the
** and_level
*/

static void
change_cond_ref_to_const(List<COND_CMP> *save_list,Item *and_father,
			 Item *cond, Item *field, Item *value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
      change_cond_ref_to_const(save_list,and_level ? cond : item, item,
			       field, value);
    return;
  }
  if (cond->eq_cmp_result() == Item::COND_OK)
    return;					// Not a boolean function

  Item_bool_func2 *func=  (Item_bool_func2*) cond;
  Item *left_item=  func->arguments()[0];
  Item *right_item= func->arguments()[1];
  Item_func::Functype functype=  func->functype();

  if (right_item->eq(field) && left_item != value)
  {
#ifdef DELETE_ITEMS
    delete right_item;
#endif
    func->arguments()[1] = value->new_item();	// QQ; Add test
    func->update_used_tables();
    if (functype == Item_func::EQ_FUNC && and_father != cond &&
	!left_item->const_item())
    {
      cond->marker=1;
      save_list->push_back(new COND_CMP(and_father,func));
    }
    func->cmp_type=item_cmp_type(func->arguments()[0]->result_type(),
				 func->arguments()[1]->result_type());
  }
  else if (left_item->eq(field) && right_item != value)
  {
#ifdef DELETE_ITEMS
    delete left_item;
#endif
    func->arguments()[0] = value = value->new_item(); // QQ; Add test
    func->update_used_tables();
    if (functype == Item_func::EQ_FUNC && and_father != cond &&
	!right_item->const_item())
    {
      func->arguments()[0] = func->arguments()[1]; // For easy check
      func->arguments()[1] = value;
      cond->marker=1;
      save_list->push_back(new COND_CMP(and_father,func));
    }
    func->cmp_type=item_cmp_type(func->arguments()[0]->result_type(),
				 func->arguments()[1]->result_type());
  }
}


static void
propagate_cond_constants(List<COND_CMP> *save_list,COND *and_level,COND *cond)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype() ==
      Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    List<COND_CMP> save;
    while ((item=li++))
    {
      propagate_cond_constants(&save,and_level ? cond : item, item);
    }
    if (and_level)
    {						// Handle other found items
      List_iterator<COND_CMP> cond_itr(save);
      COND_CMP *cond_cmp;
      while ((cond_cmp=cond_itr++))
	if (!cond_cmp->cmp_func->arguments()[0]->const_item())
	  change_cond_ref_to_const(&save,cond_cmp->and_level,
				   cond_cmp->and_level,
				   cond_cmp->cmp_func->arguments()[0],
				   cond_cmp->cmp_func->arguments()[1]);
    }
  }
  else if (and_level != cond && !cond->marker)		// In a AND group
  {
    if (cond->type() == Item::FUNC_ITEM &&
	((Item_func*) cond)->functype() == Item_func::EQ_FUNC)
    {
      Item_func_eq *func=(Item_func_eq*) cond;
      bool left_const= func->arguments()[0]->const_item();
      bool right_const=func->arguments()[1]->const_item();
      if (!(left_const && right_const))
      {
	if (right_const)
	{
	  func->arguments()[1]=resolve_const_item(func->arguments()[1],
						  func->arguments()[0]);
	  func->update_used_tables();
	  change_cond_ref_to_const(save_list,and_level,and_level,
				   func->arguments()[0],
				   func->arguments()[1]);
	}
	else if (left_const)
	{
	  func->arguments()[0]=resolve_const_item(func->arguments()[0],
						  func->arguments()[1]);
	  func->update_used_tables();
	  change_cond_ref_to_const(save_list,and_level,and_level,
				   func->arguments()[1],
				   func->arguments()[0]);
	}
      }
    }
  }
}


static COND *
optimize_cond(COND *conds,Item::cond_result *cond_value)
{
  if (!conds)
  {
    *cond_value= Item::COND_TRUE;
    return conds;
  }
  /* change field = field to field = const for each found field = const */
  DBUG_EXECUTE("where",print_where(conds,"original"););
  propagate_cond_constants((List<COND_CMP> *) 0,conds,conds);
  /*
  ** Remove all instances of item == item
  ** Remove all and-levels where CONST item != CONST item
  */
  DBUG_EXECUTE("where",print_where(conds,"after const change"););
  conds=remove_eq_conds(conds,cond_value) ;
  DBUG_EXECUTE("info",print_where(conds,"after remove"););
  return conds;
}


/*
** remove const and eq items. Return new item, or NULL if no condition
** cond_value is set to according:
** COND_OK    query is possible (field = constant)
** COND_TRUE  always true	( 1 = 1 )
** COND_FALSE always false	( 1 = 2 )
*/

static COND *
remove_eq_conds(COND *cond,Item::cond_result *cond_value)
{
  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= ((Item_cond*) cond)->functype()
      == Item_func::COND_AND_FUNC;
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
    Item::cond_result tmp_cond_value;

    *cond_value=Item::COND_UNDEF;
    Item *item;
    while ((item=li++))
    {
      Item *new_item=remove_eq_conds(item,&tmp_cond_value);
      if (!new_item)
      {
#ifdef DELETE_ITEMS
	delete item;				// This may be shared
#endif
	li.remove();
      }
      else if (item != new_item)
      {
#ifdef DELETE_ITEMS
	delete item;				// This may be shared
#endif
	VOID(li.replace(new_item));
      }
      if (*cond_value == Item::COND_UNDEF)
	*cond_value=tmp_cond_value;
      switch (tmp_cond_value) {
      case Item::COND_OK:			// Not TRUE or FALSE
	if (and_level || *cond_value == Item::COND_FALSE)
	  *cond_value=tmp_cond_value;
	break;
      case Item::COND_FALSE:
	if (and_level)
	{
	  *cond_value=tmp_cond_value;
	  return (COND*) 0;			// Always false
	}
	break;
      case Item::COND_TRUE:
	if (!and_level)
	{
	  *cond_value= tmp_cond_value;
	  return (COND*) 0;			// Always true
	}
	break;
      case Item::COND_UNDEF:			// Impossible
	break; /* purecov: deadcode */
      }
    }
    if (!((Item_cond*) cond)->argument_list()->elements ||
	*cond_value != Item::COND_OK)
      return (COND*) 0;
    if (((Item_cond*) cond)->argument_list()->elements == 1)
    {						// Remove list
      Item *item= ((Item_cond*) cond)->argument_list()->head();
      ((Item_cond*) cond)->argument_list()->empty();
      return item;
    }
  }
  else if (cond->type() == Item::FUNC_ITEM &&
	   ((Item_func*) cond)->functype() == Item_func::ISNULL_FUNC)
  {
    /*
    ** Handles this special case for some ODBC applications:
    ** The are requesting the row that was just updated with a auto_increment
    ** value with this construct:
    **
    ** SELECT * from table_name where auto_increment_column IS NULL
    ** This will be changed to:
    ** SELECT * from table_name where auto_increment_column = LAST_INSERT_ID
    */

    Item_func_isnull *func=(Item_func_isnull*) cond;
    Item **args= func->arguments();
    if (args[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) args[0])->field;
      if (field->flags & AUTO_INCREMENT_FLAG && !field->table->maybe_null)
      {
	COND *new_cond;
	if ((new_cond= new Item_func_eq(args[0],
					new Item_int("last_insert_id()",
						     current_thd->insert_id(),
						     21))))
	{
	  cond=new_cond;
	  cond->fix_fields(current_thd,0);
	}
      }
    }
  }
  else if (cond->const_item())
  {
    *cond_value= eval_const_cond(cond) ? Item::COND_TRUE : Item::COND_FALSE;
    return (COND*) 0;
  }
  else if ((*cond_value= cond->eq_cmp_result()) != Item::COND_OK)
  {						// boolan compare function
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->eq(right_item))
      return (COND*) 0;			// Compare of identical items
  }
  *cond_value=Item::COND_OK;
  return cond;				/* Point at next and level */
}


/****************************************************************************
**	Create a temp table according to a field list
**	set distinct if duplicates could be removed
**	given fields field pointers are changed to point at tmp_table
**	for send_fields
****************************************************************************/


static TABLE *
create_tmp_table(THD *thd,JOIN *join,List<Item> &fields,ORDER *group,
		 bool distinct,bool save_sum_fields,bool allow_distinct_limit)
{
  int error;
  TABLE *table;
  uint	i,field_count,reclength,old_reclength,null_count,blob_count;
  char	*tmpname,path[FN_REFLEN];
  bool	save_new_field=0;
  byte	*pos;
  uchar *null_flags;
  Field **reg_field,**from_field;
  Copy_field *copy=0;
  TABLE *form;
  N_RECINFO *recinfo,*start_recinfo;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  Item_result_field **copy_func;
  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",("distinct: %d  save_sum_fields: %d  allow_distinct_limit: %d  group: %d",
		      (int) distinct, (int) save_sum_fields,
		      (int) allow_distinct_limit,test(group)));

  created_tmp_tables++;
  sprintf(path,"%s/SQL%lx_%x",mysql_tmpdir,thd->query_id,thd->tmp_table++);
  tmpname=sql_strdup(path);
  if (group)
  {
    if (join->group_parts > ha_max_key_parts[DB_TYPE_ISAM] ||
	join->group_length > ha_max_key_length[DB_TYPE_ISAM] ||
	!join->quick_group)
      group=0;					// Key is too big
    else for (ORDER *tmp=group ; tmp ; tmp=tmp->next)
      (*tmp->item)->marker=4;			// Store null in key
  }

  field_count=join->field_count+join->func_count+join->sum_func_count;
  if (!my_multi_malloc(MYF(MY_WME),
		       &table,sizeof(TABLE),
		       &reg_field,sizeof(Field*)*(field_count+1),
		       &from_field,sizeof(Field*)*field_count,
		       &copy_func,sizeof(Item**)*(join->func_count+1),
		       &keyinfo,sizeof(KEY),
		       &key_part_info,sizeof(KEY_PART_INFO),
		       &start_recinfo,sizeof(*recinfo)*(field_count*2+3),
		       NullS))
  {
    DBUG_RETURN(NULL); /* purecov: inspected */
  }
  if (!(join->copy_field=copy=new Copy_field[field_count]))
  {
    my_free((gptr) table,MYF(0)); /* purecov: inspected */
    DBUG_RETURN(NULL); /* purecov: inspected */
  }
  join->funcs=copy_func;
  table->field=reg_field;
  table->tmp_table = 1;				/* Mark as tmp table */
  table->tablenr=0;				/* Always head table */
  table->map=1;
  table->group=0;
  table->io_cache=0;
  table->distinct=0;
  table->db_stat=0;
  table->record[0]=0;
  table->io_cache=0;
  table->used_keys=0;

  /* make nisam database according to fields */

  form= table;
  bzero((char*) form,sizeof(*form));
  bzero((char*) reg_field,sizeof(Field*)*(field_count+1));
  bzero((char*) from_field,sizeof(Field*)*field_count);
  form->field=reg_field;
  form->real_name=tmpname;
  table->table_name=base_name(tmpname);
  form->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  form->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  form->reginfo.ref_key= -1;		/* Not using key read */

  /* Calculate with type of fields we will need in heap table */

  reclength=blob_count=null_count=field_count=0;

  List_iterator<Item> li(fields);
  Item *item;
  while ((item=li++))
  {
    Item::Type type;
    Item *org_field=item;
    Field *new_field=0;
    bool  maybe_null=0;

    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
      continue;
    type=item->type();
    if (type == Item::SUM_FUNC_ITEM)
    {
      if (item->const_item())
	type=Item::CONST_ITEM;
      else if (!group && !save_sum_fields)

      {						/* Can't calc group yet */
	save_new_field=1;			// Save new field later
	item=((Item_sum*) item)->item;
	type=item->type();
      }
    }
    switch (type) {
    case Item::SUM_FUNC_ITEM:
    {
      Item_sum *item_sum=(Item_sum*) item;
      maybe_null=item_sum->maybe_null;
      switch (item_sum->sum_func()) {
      case Item_sum::AVG_FUNC:			/* Place for sum & count */
	if (group)
	  new_field= new Field_string(sizeof(double)+sizeof(longlong),
				      maybe_null, item->name,form,1);
	else
	  new_field=new Field_double(item_sum->max_length,maybe_null,
				     item->name, form, item_sum->decimals);
	break;
      case Item_sum::STD_FUNC:			/* Place for sum & count */
	if (group)
	  new_field= new Field_string(sizeof(double)*2+sizeof(longlong),
				      maybe_null, item->name,form,1);
	else
	  new_field=new Field_double(item_sum->max_length, maybe_null,
				     item->name,form,item_sum->decimals);
	break;
      case Item_sum::UNIQUE_USERS_FUNC:
	new_field=new Field_long(9,maybe_null,item->name,form,1);
	break;
      default:
	switch (item_sum->result_type()) {
	case REAL_RESULT:
	  new_field=new Field_double(item_sum->max_length,maybe_null,
				     item->name,form,item_sum->decimals);
	  break;
	case INT_RESULT:
	  new_field=new Field_longlong(item_sum->max_length,maybe_null,
				       item->name,form);
	  break;
	case STRING_RESULT:
	  new_field= new Field_string(item_sum->max_length,maybe_null,
				      item->name,form,item->binary);
	  break;
	}
	break;
      }
      item_sum->result_field=new_field;		/* Save result of func here */
      break;
    }
    case Item::FIELD_ITEM:
      {
	Field *org_field=((Item_field*) item)->field;
	from_field[field_count]=org_field;
	new_field= org_field->new_field(form);
	((Item_field*) item)->result_field= new_field;
	if (org_field->flags & BLOB_FLAG)
	  blob_count++;
	if ((maybe_null= org_field->maybe_null()))
	  new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
      }
    break;
    case Item::FUNC_ITEM:
    case Item::COND_ITEM:
    case Item::FIELD_AVG_ITEM:
    case Item::FIELD_STD_ITEM:
      maybe_null=item->maybe_null;
      switch (item->result_type()) {
      case REAL_RESULT:
	new_field=new Field_double(item->max_length,maybe_null,
				   item->name,form,item->decimals);
	break;
      case INT_RESULT:
	new_field=new Field_longlong(item->max_length,maybe_null,
				     item->name,form);
	break;
      case STRING_RESULT:
	new_field= new Field_string(item->max_length,maybe_null,
				    item->name,form,item->binary);
	break;
      }
      *(copy_func++) = (Item_result_field*) item; // Save for copy_funcs
      ((Item_result_field*) item)->result_field=new_field;
      break;
    default:					// Dosen't have to be stored
      if (org_field->type() == Item::SUM_FUNC_ITEM)
	((Item_sum *) org_field)->result_field=0;
      continue;
    }
    if (save_new_field)
    {
      save_new_field=0;
      ((Item_sum *) org_field)->result_field= new_field;
    }
    if (!new_field)
      goto err;					// Of OOM
    reclength+=new_field->pack_length();
    if (org_field->marker == 4 && maybe_null)
    {
      reclength++;				// group maybe_null item
      new_field->flags|= GROUP_FLAG;
    }
    *(reg_field++) =new_field;
    field_count++;
    if (maybe_null)
      null_count++;
  }

  /* If result table is small; use a heap */
  if (blob_count ||
      (join->select_options & (OPTION_BIG_TABLES | SELECT_SMALL_RESULT)) ==
      OPTION_BIG_TABLES)
    form->db_type=DB_TYPE_ISAM;
  else
    form->db_type=DB_TYPE_HEAP;
  form->blob_fields=blob_count;
  if (form->db_type == DB_TYPE_ISAM && blob_count == 0)
    reclength++;				/* For delete link */
  old_reclength=reclength;
  reclength+=(null_count+7)/8;
  if (!reclength)
    reclength=1;				// Dummy select

  form->fields=field_count;
  form->reclength=reclength;
  if (!(form->record[0]= (byte *) my_malloc(reclength*3, MYF(MY_WME))))
    goto err;
  form->record[1]= form->record[0]+reclength;
  form->record[2]= form->record[1]+reclength;
  null_flags=(uchar*) form->record[0]+old_reclength;
  copy_func[0]=0;				// End marker

  recinfo=start_recinfo;
  pos=form->record[0];
  if (form->db_type == DB_TYPE_ISAM && blob_count == 0)
  {						// Room for delete link
    form->record[0][0]=(byte) 254;
    pos++;
    recinfo->base.type=FIELD_NORMAL;
    recinfo->base.length=1;
    recinfo++;
  }

  null_count=0;
  for (i=0,reg_field=form->field; i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG)
      {
	*pos++=0;				// Null is stored here
	recinfo->base.length=1;
	recinfo->base.type=FIELD_NORMAL;
	recinfo++;
      }
      field->move_field((char*) pos,null_flags+null_count/8,
			1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field((char*) pos,(uchar*) 0,0);
    field->reset();
    if (from_field[i])
    {						/* Not a formula Item */
      copy->set(field,from_field[i],save_sum_fields);
      copy++;
    }
    uint length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->base.length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->base.type= (int) FIELD_BLOB;
    else if (!field->zero_pack() &&
	     (field->type() == FIELD_TYPE_STRING ||
	      field->type() == FIELD_TYPE_VAR_STRING) &&
	     length >= 10 && blob_count)
      recinfo->base.type=FIELD_SKIPP_ENDSPACE;
    else
      recinfo->base.type=FIELD_NORMAL;
  }
  if (null_count)
  {
    recinfo->base.type=FIELD_NORMAL;
    recinfo->base.length=(null_count+7)/8;
    recinfo++;
  }
  recinfo->base.type=(int) FIELD_LAST;

  join->copy_field_count=(uint) (copy - join->copy_field);
  bfill(pos,(null_count+7)/8,255);		// Set null fields
  store_record(form,2);				// Make empty default record

  form->max_records=tmp_table_size/form->reclength;
  set_if_bigger(form->max_records,1);		// For dummy start options
  if (group)
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    byte *group_buff;
    table->group=group;				/* Table is grouped by key */
    join->group_buff=(byte*) sql_alloc(join->group_length);
    key_part_info=(KEY_PART_INFO*) sql_alloc(sizeof(KEY_PART_INFO)*
					     join->group_parts);
    if (!key_part_info)
      goto err; /* purecov: inspected */
    form->keys=1;
    form->key_info=keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->dupp_key=0;
    keyinfo->usable_key_parts=keyinfo->key_parts= join->group_parts;
    keyinfo->key_length=0;
    group_buff=join->group_buff;
    for (; group ; group=group->next,key_part_info++)
    {
      Field *field=(*group->item)->tmp_table_field();
      bool maybe_null;
      key_part_info->field=  field;
      key_part_info->offset= field->offset();
      key_part_info->length= field->pack_length();
      group->buff=(char*) group_buff;
      if (!(group->field=field->new_field(form)))
	goto err; /* purecov: inspected */
      if ((maybe_null=(*group->item)->maybe_null))
      {						// Here is the null marker
	*group_buff= 0;				// Init null byte
	key_part_info->offset--;
	key_part_info->length++;
      }
      group->field->move_field((char*) group_buff + (maybe_null ? 1 : 0),
			       (uchar*) 0,0);
      key_part_info->key_type=
	(field->key_type() != HA_KEYTYPE_TEXT ? FIELDFLAG_BINARY : 0);
      keyinfo->key_length+=  key_part_info->length;
      group_buff+=	     key_part_info->length;
    }
  }
  if (distinct && !group &&
      reclength < ha_max_key_length[form->db_type] &&
      (reclength < 256 || (join->select_options & SELECT_SMALL_RESULT) ||
      allow_distinct_limit && thd->select_limit < form->max_records))
  {
    /* This should be fixed to have a key part per field */
    if (distinct && allow_distinct_limit)
    {
      set_if_smaller(form->max_records,thd->select_limit);
      join->end_write_records=thd->select_limit;
    }
    else
      join->end_write_records= HA_POS_ERROR;
    table->distinct=distinct;
    form->keys=1;
    form->key_info=keyinfo;
    keyinfo->key_parts=1;
    keyinfo->dupp_key=0;
    keyinfo->key_length=(uint16) reclength;
    keyinfo->key_part=key_part_info;
    keyinfo->name="tmp";
    key_part_info->field= *table->field;
    key_part_info->offset= 0;
    key_part_info->length= reclength;
    key_part_info->key_type=FIELDFLAG_BINARY;
  }
  if (thd->fatal_error)
   goto err;					// End of memory /* purecov: inspected */
  if (form->db_type == DB_TYPE_ISAM)
  {
    N_KEYDEF keydef;
    if (form->keys)
    {						// Get keys for ni_create
      keydef.base.flag=HA_NOSAME;
      keydef.base.keysegs=  keyinfo->key_parts;
      for (i=0; i < keydef.base.keysegs ; i++)
      {
	keydef.seg[i].base.flag=0;		// No packing
	keydef.seg[i].base.type=
	  ((keyinfo->key_part[i].key_type & FIELDFLAG_BINARY) ?
	   HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT);
	keydef.seg[i].base.length=keyinfo->key_part[i].length;
	keydef.seg[i].base.start=keyinfo->key_part[i].offset;
      }
      keydef.seg[i].base.type=HA_KEYTYPE_END;
    }
    if ((error=ni_create(tmpname,form->keys,&keydef,start_recinfo,0L,0L,0,0,
			 0L)))
    {
      ha_error(form,error,MYF(0)); /* purecov: inspected */
      form->db_stat=0;
      goto err;
    }
    form->db_record_offset=form->reclength;
  }
  else
    form->db_record_offset=1;

  if ((error=ha_open(form,tmpname,O_RDWR,HA_OPEN_WAIT_IF_LOCKED)))
  {
    ha_error(form,error,MYF(0)); /* purecov: inspected */
    form->db_stat=0;
    goto err;
  }
  VOID(ha_lock(form,F_WRLCK));		/* Single thread table */
  VOID(ha_extra(form,HA_EXTRA_NO_READCHECK)); /* Not needed */
  DBUG_RETURN(table);

 err:
  free_tmp_table(thd,table); /* purecov: inspected */
  DBUG_RETURN(NULL); /* purecov: inspected */
}


static void
free_tmp_table(THD *thd, TABLE *entry)
{
  char *save_proc_info;
  DBUG_ENTER("free_tmp_table");
  DBUG_PRINT("enter",("table: %s",entry->table_name));

  save_proc_info=thd->proc_info;
  thd->proc_info="removing tmp table";
  if (entry->db_stat)
    VOID(ha_close(entry));
  if (!(test_flags & TEST_KEEP_TMP_TABLES) || entry->db_type == DB_TYPE_HEAP)
    VOID(ha_fdelete(entry->db_type,entry->real_name));
  my_free((gptr) entry->record[0],MYF(0));
  free_io_cache(entry);
  my_free((gptr) entry,MYF(0));
  thd->proc_info=save_proc_info;

  DBUG_VOID_RETURN;
}


/*****************************************************************************
**	Make a join of all tables and write it on socket or to table
*****************************************************************************/

static int
do_select(JOIN *join,List<Item> *fields,TABLE *table,Procedure *procedure)
{
  int error;
  JOIN_TAB *join_tab;
  int (*end_select)(struct st_join *,struct st_join_table *,bool);
  DBUG_ENTER("do_select");

  join->procedure=procedure;
  /*
  ** Tell the client how many fields there are in a row
  */
  if (!table)
    join->result->send_fields(*fields,1);
  else
  {
    VOID(ha_extra(table,HA_EXTRA_WRITE_CACHE));
    restore_record(table,2);			/* Make empty */
  }
  join->tmp_table=table;			/* Save for easy recursion */
  join->fields= fields;

  /* Set up select_end */
  if (table)
  {
    if (table->group)
    {
      DBUG_PRINT("info",("Using end_update"));
      end_select=end_update;
    }
    else if (join->sort_and_group)
    {
      DBUG_PRINT("info",("Using end_write_group"));
      end_select=end_write_group;
    }
    else
    {
      DBUG_PRINT("info",("Using end_write"));
      end_select=end_write;
    }
  }
  else
  {
    if (join->sort_and_group || (join->procedure &&
				 join->procedure->flags & PROC_GROUP))
      end_select=end_send_group;
    else
      end_select=end_send;
  }
  join->join_tab[join->tables-1].next_select=end_select;

  join_tab=join->join_tab+join->const_tables;
  join->send_records=0;
  if (join->tables == join->const_tables)
  {
    if (!(error=(*end_select)(join,join_tab,0)) || error == -3)
      error=(*end_select)(join,join_tab,1);
  }
  else
  {
    error=sub_select(join,join_tab,0);
    if (error >= 0)
      error=sub_select(join,join_tab,1);
    if (error == -3)
      error=0;					/* select_limit used */
  }
  if (!table)
  {
    if (error < 0)
      join->result->send_error(0,NullS); /* purecov: inspected */
    else
      join->result->send_eof();
  }
  if (error >= 0)
  {
    DBUG_PRINT("info",("%ld records output",join->send_records));
  }
  if (table)
    VOID(ha_extra(table,HA_EXTRA_NO_CACHE));
  DBUG_RETURN(error < 0);
}


static int
sub_select_cache(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{
  int error;

  if (end_of_records)
  {
    if ((error=flush_cacheed_records(join,join_tab,FALSE)) < 0)
      return error; /* purecov: inspected */
    return sub_select(join,join_tab,end_of_records);
  }
  if (join_tab->use_quick != 2 || test_if_quick_select(join_tab) <= 0)
  {
    if (join->thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0)); /* purecov: inspected */
      return -2;				// Aborted by user /* purecov: inspected */
    }
    if (!store_record_in_cache(&join_tab->cache))
      return 0;					// There is more room in cache
    return flush_cacheed_records(join,join_tab,FALSE);
  }
  if ((error=flush_cacheed_records(join,join_tab,TRUE)) < 0)
    return error; /* purecov: inspected */
  return sub_select(join,join_tab,end_of_records); /* Use ordinary select */
}


static int
sub_select(JOIN *join,JOIN_TAB *join_tab,bool end_of_records)
{
  int error;
  READ_RECORD *info;

  join_tab->table->null_row=0;
  if (end_of_records)
    return (*join_tab->next_select)(join,join_tab+1,end_of_records);

  bool found=0;
  if ((error=(*join_tab->read_first_record)(join_tab)) >= 0)
  {
    info= &join_tab->read_record;
    do
    {
      if (join->thd->killed)			// Aborted by user
      {
	my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
	return -2;				/* purecov: inspected */
      }
      if (!error && (!join_tab->on_expr || join_tab->on_expr->val_int()))
      {
	found=1;
	if (!(join_tab->select && join_tab->select->skipp_record()))
	{
	  if ((error=(*join_tab->next_select)(join,join_tab+1,0)) < 0)
	    return error;
	}
      }
    } while ((error=info->read_record(info)) <= 0);
    if (error != HA_ERR_END_OF_FILE)
    {
      ha_error(info->form,error,MYF(0));	/* purecov: inspected */
      return -1;				/* purecov: inspected */
    }
  }
  if (!found && join_tab->on_expr)
  {						// OUTER JOIN
    join_tab->table->null_row=1;		// For group by without error
    restore_record(join_tab->table,2);		// Make empty record
    if (!(join_tab->select && join_tab->select->skipp_record()))
    {
      if ((error=(*join_tab->next_select)(join,join_tab+1,0)) < 0)
	return error; /* purecov: inspected */
    }
  }
  return 0;
}


static int
flush_cacheed_records(JOIN *join,JOIN_TAB *join_tab,bool skipp_last)
{
  int error;
  READ_RECORD *info;

  if (!join_tab->cache.records)
    return 0;				/* Nothing to do */
  if (skipp_last)
    (void) store_record_in_cache(&join_tab->cache); // Must save this for later
  if (join_tab->use_quick == 2)
  {
    if (join_tab->select->quick)
    {					/* Used quick select last. reset it */
      delete join_tab->select->quick;
      join_tab->select->quick=0;
    }
  }
 /* read through all records */
  if ((error=join_init_read_record(join_tab)) < 0)
    return 1;				/* No records (not fatal) */

  for (JOIN_TAB *tmp=join->join_tab; tmp != join_tab ; tmp++)
  {
    tmp->status=tmp->table->status;
    tmp->table->status=0;
  }

  info= &join_tab->read_record;
  do
  {
    if (join->thd->killed)
    {
      my_error(ER_SERVER_SHUTDOWN,MYF(0)); /* purecov: inspected */
      return -2;				// Aborted by user /* purecov: inspected */
    }
    SQL_SELECT *select=join_tab->select;
    if (!error && (!join_tab->cache.select ||
		   !join_tab->cache.select->skipp_record()))
    {
      uint i;
      reset_cache(&join_tab->cache);
      for (i=(join_tab->cache.records- (skipp_last ? 1 : 0)) ; i-- > 0 ;)
      {
	read_cacheed_record(join_tab);
	if (!select || !select->skipp_record())
	  if ((error=(join_tab->next_select)(join,join_tab+1,0)) < 0)
	    return error; /* purecov: inspected */
      }
    }
  } while ((error=info->read_record(info)) <= 0);
  if (skipp_last)
    read_cacheed_record(join_tab);	/* Restore current record */
  reset_cache(&join_tab->cache);
  join_tab->cache.records=0; join_tab->cache.ptr_record= (uint) ~0;
  if (error != HA_ERR_END_OF_FILE)
  {
    ha_error(info->form,error,MYF(0)); /* purecov: inspected */
    return -1; /* purecov: inspected */
  }
  for (JOIN_TAB *tmp2=join->join_tab; tmp2 != join_tab ; tmp2++)
    tmp2->table->status=tmp2->status;
  return 0;
}


/*****************************************************************************
**	The different ways to read a record
*****************************************************************************/

static int
join_read_const_tables(JOIN *join)
{
  uint i;

  DBUG_ENTER("join_read_const_tables");
  for (i=0 ; i < join->const_tables ; i++)
  {
    TABLE *form=join->table[i];
    form->null_row=0;
    form->status=STATUS_NO_RECORD;

    if (join->join_tab[i].type == JT_SYSTEM)
    {
      if (join_read_system(join->join_tab+i))
      {						// Info for DESCRIBE
	join->join_tab[i].info="const row not found";
	join->best_positions[i].records_read=0.0;
	if (!form->outer_join)
	  DBUG_RETURN(1);
      }
    }
    else
    {
      if (join_read_const(join->join_tab+i))
      {
	join->join_tab[i].info="unique row not found";
	join->best_positions[i].records_read=0.0;
	if (!form->outer_join)
	  DBUG_RETURN(1);
      }
    }
    if (join->join_tab[i].on_expr && !form->null_row)
    {
      if ((form->null_row= test(join->join_tab[i].on_expr->val_int() == 0)))
	restore_record(form,2);
    }
    if (!form->null_row)
      form->maybe_null=0;
  }
  DBUG_RETURN(0);
}


static int
join_read_system(JOIN_TAB *tab)
{
  TABLE *form= tab->table;
  if (form->status & STATUS_GARBAGE)		// If first read
  {
    if (ha_readfirst(form,form->record[0]))
    {
      form->null_row=1;				// This is ok.
      restore_record(form,2);			// Make empty record
      return -1;
    }
    store_record(form,1);
  }
  else if (!form->status)			// Only happens with left join
    restore_record(form,1);			// restore old record
  form->null_row=0;
  return form->status ? -1 : 0;
}


static int
join_read_const(JOIN_TAB *tab)
{
  int error;
  TABLE *form= tab->table;
  if (form->status & STATUS_GARBAGE)		// If first read
  {
    cp_buffer_from_ref(form->reginfo.key_buff,
		       form->reginfo.ref_field,
		       form->reginfo.ref_length);
    if ((error=ha_rkey(form,form->record[0],form->reginfo.ref_key,
		       form->reginfo.key_buff,
		       form->reginfo.ref_length,HA_READ_KEY_EXACT)))
    {
#ifdef EXTRA_DEBUG
      if (error != HA_ERR_KEY_NOT_FOUND)
	sql_print_error("read_const: Got error %d when reading table",error);
#endif
      form->null_row=1;
      restore_record(form,2);			// Make empty record
      return -1;
    }
    store_record(form,1);
  }
  else if (!form->status)			// Only happens with left join
    restore_record(form,1);			// restore old record
  form->null_row=0;
  return form->status ? -1 : 0;
}

static int
join_read_key(JOIN_TAB *tab)
{
  int error;
  TABLE *form= tab->table;

  if (cmp_buffer_with_ref(form->reginfo.key_buff,
			  form->reginfo.ref_field,
			  form->reginfo.ref_length) ||
      (form->status & (STATUS_GARBAGE | STATUS_NO_PARENT)))
  {
    error=ha_rkey(form,form->record[0],form->reginfo.ref_key,
		  form->reginfo.key_buff,
		  form->reginfo.ref_length,HA_READ_KEY_EXACT);
#ifdef EXTRA_DEBUG
    if (error && error != HA_ERR_KEY_NOT_FOUND)
      sql_print_error("read_key: Got error %d when reading table",error);
#endif
  }
  return form->status ? -1 : 0;
}


static int
join_read_always_key(JOIN_TAB *tab)
{
  int error;
  TABLE *form= tab->table;

  cp_buffer_from_ref(form->reginfo.key_buff,
		     form->reginfo.ref_field,
		     form->reginfo.ref_length);
  if ((error=ha_rkey(form,form->record[0],form->reginfo.ref_key,
		     form->reginfo.key_buff,
		     form->reginfo.ref_length,HA_READ_KEY_EXACT)))
  {
#ifdef EXTRA_DEBUG
    if (error != HA_ERR_KEY_NOT_FOUND)
      sql_print_error("read_const: Got error %d when reading table",error);
#endif
    return -1; /* purecov: inspected */
  }
  return 0;
}


	/* ARGSUSED */
static int
join_no_more_records(READ_RECORD *info __attribute__((unused)))
{
  return HA_ERR_END_OF_FILE;
}


static int
join_read_next(READ_RECORD *info)
{
  int error;
  TABLE *form= info->form;

  if ((error=ha_rnext(form,form->record[0],form->reginfo.ref_key)) ||
      key_cmp(form,form->reginfo.key_buff,(uint) form->reginfo.ref_key,
	      form->reginfo.ref_length))
  {
#ifdef EXTRA_DEBUG
    if (error && error != HA_ERR_END_OF_FILE)
      sql_print_error("read_next: Got error %d when reading table",error);
#endif
    form->status= STATUS_GARBAGE;
    return HA_ERR_END_OF_FILE;
  }
  return 0;
}


static int
join_init_quick_read_record(JOIN_TAB *tab)
{
  if (test_if_quick_select(tab) == -1)
    return -1;					/* No possible records */
  return join_init_read_record(tab);
}


static int
test_if_quick_select(JOIN_TAB *tab)
{
  delete tab->select->quick;
  tab->select->quick=0;
  return tab->select->test_quick_select(tab->keys,0L,HA_POS_ERROR);
}


static int
join_init_read_record(JOIN_TAB *tab)
{
  int result;
  if (tab->select && tab->select->quick)
    tab->select->quick->reset();
  init_read_record(&tab->read_record, tab->table, tab->select);
  result=(*tab->read_record.read_record)(&tab->read_record);
  return result == HA_ERR_END_OF_FILE ? -1 : (result ? 1 : 0);
}

static int
join_init_read_first_with_key(JOIN_TAB *tab)
{
  int error;
  TABLE *table=tab->table;
  if (!table->key_read && (table->used_keys & ((key_map) 1 << tab->index)))
  {
    table->key_read=1;
    ha_extra(table,HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_init_read_next_with_key;
  tab->read_record.form=table;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  error=ha_rfirst(tab->table,tab->table->record[0],tab->index);
#ifdef EXTRA_DEBUG
  if (error && error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
    sql_print_error("read_first_with_key: Got error %d when reading table",error);
#endif
    return error;
}

static int
join_init_read_next_with_key(READ_RECORD *info)
{
  int error=ha_rnext(info->form,info->record,info->index);
#ifdef EXTRA_DEBUG
  if (error && error != HA_ERR_END_OF_FILE)
    sql_print_error("read_next_with_key: Got error %d when reading table",error);
#endif
  return error;
}

static int
join_init_read_last_with_key(JOIN_TAB *tab)
{
  TABLE *table=tab->table;
  int error;
  if (!table->key_read && (table->used_keys & ((key_map) 1 << tab->index)))
  {
    table->key_read=1;
    ha_extra(table,HA_EXTRA_KEYREAD);
  }
  tab->table->status=0;
  tab->read_record.read_record=join_init_read_prev_with_key;
  tab->read_record.form=table;
  tab->read_record.index=tab->index;
  tab->read_record.record=table->record[0];
  error=ha_rlast(tab->table,tab->table->record[0],tab->index);
#ifdef EXTRA_DEBUG
  if (error && error != HA_ERR_END_OF_FILE)
    sql_print_error("read_first_with_key: Got error %d when reading table",error);
#endif
  return error;
}

static int
join_init_read_prev_with_key(READ_RECORD *info)
{
  int error=ha_rprev(info->form,info->record,info->index);
#ifdef EXTRA_DEBUG
  if (error && error != HA_ERR_END_OF_FILE)
    sql_print_error("read_prev_with_key: Got error %d when reading table",error);
#endif
  return error;
}


/*****************************************************************************
**  The different end of select functions
*****************************************************************************/

	/* ARGSUSED */
static int
end_send(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	 bool end_of_records)
{
  if (!end_of_records)
  {
    int error;
    if (join->procedure)
      error=join->procedure->send_row(*join->fields);
    else
    {
      if (join->having && join->having->val_int() == 0.0)
	return 0;				// Didn't match having
      error=join->result->send_data(*join->fields);
    }
    if (error)
      return -1; /* purecov: inspected */
    if (++join->send_records >= join->thd->select_limit)
      return -3;				// Abort nicely
  }
  return 0;
}


	/* ARGSUSED */
static int
end_send_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	       bool end_of_records)
{
  int index= -1;

  if (!join->first_record || end_of_records ||
      (index=test_if_group_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || (end_of_records && !join->group))
    {
      if (join->procedure)
	join->procedure->end_group();
      if (index < (int) join->send_group_parts)
      {
	int error;
	if (join->procedure)
	  error=join->procedure->send_row(*join->fields) ? 1 : 0;
	else
	{
	  if (!join->first_record)
	    clear_tables(join);
	  if (join->having && join->having->val_int() == 0.0)
	    error= -1;				// Didn't satisfy having
	  else
	    error=join->result->send_data(*join->fields) ? 1 : 0;
	}
	if (error > 0)
	  return -1; /* purecov: inspected */
	if (end_of_records)
	  return 0;
	if (!error && ++join->send_records >= join->thd->select_limit)
	  return -3;				/* Abort nicely */
      }
    }
    else
    {
      if (end_of_records)
	return 0;
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (index < (int) join->send_group_parts)
    {
      copy_fields(join);
      init_sum_functions(join->sum_funcs);
      if (join->procedure)
	join->procedure->add();
      return(0);
    }
  }
  update_sum_func(join->sum_funcs);
  if (join->procedure)
    join->procedure->add();
  return(0);
}


	/* ARGSUSED */
static int
end_write(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	  bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int error;
  if (join->thd->killed)			// Aborted by user
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    return -2;					/* purecov: inspected */
  }
  if (!end_of_records)
  {
    copy_fields(join);
    copy_funcs(join->funcs);
    if (!join->having || join->having->val_int())
    {
      if ((error=ha_write(table,table->record[0])))
      {
	if (error != HA_ERR_FOUND_DUPP_KEY || !table->distinct)
	{
	  ha_error(table,error,MYF(0));		/* purecov: inspected */
	  return -1;				/* purecov: inspected */
	}
      }
      else
      {
	if (++join->send_records >= join->end_write_records)
	  return -3;
      }
    }
  }
  return 0;
}

	/* Group by searching after group record and updating it if possible */
	/* ARGSUSED */

static int
end_update(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
	   bool end_of_records)
{
  TABLE *table=join->tmp_table;
  ORDER   *group;
  int	  error;

  if (end_of_records)
    return 0;
  if (join->thd->killed)			// Aborted by user
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    return -2;					/* purecov: inspected */
  }

  copy_fields(join);				// Groups are copied twice.
  /* Make a key of group index */
  for (group=table->group ; group ; group=group->next)
  {
    Item *item= *group->item;
    item->save_org_in_field(group->field);
    if (item->maybe_null)
      group->buff[0]=item->null_value ? 0: 1;	// Save reversed value
  }
  if (!ha_rkey(table,table->record[1],0,join->group_buff,0,
	       HA_READ_KEY_EXACT))
  {						/* Update old record */
    restore_record(table,1);
    update_tmptable_sum_func(join->sum_funcs,table);
    if ((error=ha_update(table,table->record[1],
			 table->record[0])))
    {
      ha_error(table,error,MYF(0));		/* purecov: inspected */
      return -1;				/* purecov: inspected */
    }
    return 0;
  }

  /* The null bits are already set */
  KEY_PART_INFO *key_part;
  for (group=table->group,key_part=table->key_info[0].key_part;
       group ;
       group=group->next,key_part++)
    memcpy(table->record[0]+key_part->offset, group->buff, key_part->length);

  init_tmptable_sum_functions(join->sum_funcs);
  copy_funcs(join->funcs);
  if ((error=ha_write(table,table->record[0])))
  {
    ha_error(table,error,MYF(0));		/* purecov: inspected */
    return -1;					/* purecov: inspected */
  }
  join->send_records++;
  return 0;
}


	/* ARGSUSED */
static int
end_write_group(JOIN *join, JOIN_TAB *join_tab __attribute__((unused)),
		bool end_of_records)
{
  TABLE *table=join->tmp_table;
  int	  error;
  int	  index= -1;

  if (join->thd->killed)
  {						// Aborted by user
    my_error(ER_SERVER_SHUTDOWN,MYF(0));	/* purecov: inspected */
    return -2;					/* purecov: inspected */
  }
  if (!join->first_record || end_of_records ||
      (index=test_if_group_changed(join->group_fields)) >= 0)
  {
    if (join->first_record || (end_of_records && !join->group))
    {
      if (join->procedure)
	join->procedure->end_group();
      if (index < (int) join->send_group_parts)
      {
	if (!join->first_record)
	  clear_tables(join);
	copy_sum_funcs(join->sum_funcs);
	if (!join->having || join->having->val_int())
	{
	  if ((error=ha_write(table,table->record[0])))
	  {
	    ha_error(table,error,MYF(0));	/* purecov: inspected */
	    return -1;				/* purecov: inspected */
	  }
	  else
	    join->send_records++;
	}
	if (end_of_records)
	  return 0;
      }
    }
    else
    {
      join->first_record=1;
      VOID(test_if_group_changed(join->group_fields));
    }
    if (index < (int) join->send_group_parts)
    {
      copy_fields(join);
      copy_funcs(join->funcs);
      init_sum_functions(join->sum_funcs);
      if (join->procedure)
	join->procedure->add();
      return(0);
    }
  }
  update_sum_func(join->sum_funcs);
  if (join->procedure)
    join->procedure->add();
  return(0);
}


/*****************************************************************************
** Remove calculation with tables that aren't yet read. Remove also tests
** against fields that are read through key.
** We can't remove tests that are made against columns which are stored
** in sorted order.
*****************************************************************************/

static bool test_if_ref(Item_field *left_item,Item *right_item)
{
  Field *field=left_item->field;
  REF_FIELD *ref_field=part_of_refkey(field->table,field);

  if (ref_field)
  {						// FIELD = FIELD
    if (right_item->type() == Item::FIELD_ITEM &&
	((Item_field *) right_item)->field == ref_field->field)
    {						// may be removed
      return ((Item_field *) right_item)->field->pack_length() ==
	ref_field->length;
    }
    else if (right_item->const_item() && !ref_field->field->table_name)
    {						// FIELD = constant
      if (!field->table->const_table)		// Don't change const table
      {
	// We can remove binary fields and numerical fields except float,
	// as float comparison isn't 100 % secure
	if (field->binary() &&
	    (field->type() != FIELD_TYPE_FLOAT ||
	     ((Field_float*) field)->decimals == 0))
	{
	  if (!store_val_in_field(field,right_item) &&
	      !field->cmp(ref_field->ptr))
	    return 1;
	}
      }
    }
  }
  return 0;					// keep it
}


static COND *
make_cond_for_table(COND *cond,table_map tables,table_map used_table)
{
  if (used_table && !(cond->used_tables() & used_table))
    return (COND*) 0;				// Already checked
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (COND*) 0;			// OOM /* purecov: inspected */
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_table(item,tables,used_table);
	if (fix)
	  new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (COND*) 0;			// Always true
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->used_tables_cache=((Item_cond*) cond)->used_tables_cache &
	  tables;
	return new_cond;
      }
    }
    else
    {						// Or list
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (COND*) 0;			// OOM /* purecov: inspected */
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix=make_cond_for_table(item,tables,0L);
	if (!fix)
	  return (COND*) 0;			// Always true
	new_cond->argument_list()->push_back(fix);
      }
      new_cond->used_tables_cache=((Item_cond_or*) cond)->used_tables_cache;
      return new_cond;
    }
  }

  /*
  ** Because the following test takes a while and it can be done
  ** table_count times, we mark each item that we have examined with the result
  ** of the test
  */

  if (cond->marker == 3 || (cond->used_tables() & ~tables))
    return (COND*) 0;				// Can't check this yet
  if (cond->marker == 2 || cond->eq_cmp_result() == Item::COND_OK)
    return cond;				// Not boolean op

  if (((Item_func*) cond)->functype() == Item_func::EQ_FUNC)
  {
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (left_item->type() == Item::FIELD_ITEM &&
	test_if_ref((Item_field*) left_item,right_item))
    {
      cond->marker=3;			// Checked when read
      return (COND*) 0;
    }
    if (right_item->type() == Item::FIELD_ITEM &&
	test_if_ref((Item_field*) right_item,left_item))
    {
      cond->marker=3;			// Checked when read
      return (COND*) 0;
    }
  }
  cond->marker=2;
  return cond;
}

static REF_FIELD *
part_of_refkey(TABLE *table,Field *field)
{
  uint key,keypart;

  key=(uint) table->reginfo.ref_key;
  for (keypart=0 ; keypart < table->reginfo.ref_fields ; keypart++)
    if (field->eq(table->key_info[key].key_part[keypart].field))
      return &table->reginfo.ref_field[keypart];
  return (REF_FIELD*) 0;
}


/*****************************************************************************
** Test if one can use the key to resolve ORDER BY
** Returns: 1 if key is ok.
**          0 if key can't be used
**          -1 if reverse key can be used
*****************************************************************************/

static int test_if_order_by_key(ORDER *order, TABLE *table, uint index)
{
  KEY_PART_INFO *key_part,*key_part_end;
  key_part=table->key_info[index].key_part;
  key_part_end=key_part+table->key_info[index].key_parts;
  int reverse=0;

  for (; order ; order=order->next)
  {
    Field *field=((Item_field*) (*order->item))->field;
    int flag;
    if (key_part == key_part_end || key_part->field != field)
      return 0;

    flag=(order->asc == test(key_part->key_part_flag == 0)) ? 1 : -1;
    if (reverse && flag != reverse)
      return 1;
    reverse=flag;
    key_part++;
  }
  return reverse;
}

static uint find_shortest_key(TABLE *table, key_map usable_keys)
{
  uint min_length= (uint) ~0;
  uint best= MAX_KEY;
  for (uint nr=0; usable_keys ; usable_keys>>=1, nr++)
  {
    if (usable_keys & 1)
    {
      if (table->key_info[nr].key_length < min_length)
      {
	min_length=table->key_info[nr].key_length;
	best=nr;
      }
    }
  }
  return best;
}


/*****************************************************************************
** If not selecting by given key, create a index how records should be read
** return: 0  ok
**	  -1 some fatal error
**	   1  no records
*****************************************************************************/

static int
create_sort_index(JOIN_TAB *tab,ORDER *order,ha_rows select_limit)
{
  SORT_FIELD *sortorder;
  int ref_key;
  uint length;
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  key_map usable_keys;
  DBUG_ENTER("create_sort_index");

  /* Check which keys can be used to resolve ORDER BY */
  usable_keys= ~(key_map) 0;
  for (ORDER *tmp_order=order; tmp_order ; tmp_order=tmp_order->next)
  {
    if ((*tmp_order->item)->type() != Item::FIELD_ITEM)
    {
      usable_keys=0;
      break;
    }
    usable_keys&=((Item_field*) (*tmp_order->item))->field->part_of_key;
  }

  ref_key= -1;
  if (table->reginfo.ref_key >= 0)		// Constant range in WHERE
    ref_key=table->reginfo.ref_key;
  else if (select && select->quick)		// Range found by opt_range
    ref_key=select->quick->index;

  if (ref_key >= 0)
  {
    /* Check if we get the rows in requested sorted order by using the key */
    if ((usable_keys & ((key_map) 1 << ref_key)) &&
	test_if_order_by_key(order,table,ref_key) == 1)
      DBUG_RETURN(0);			/* No need to sort */
  }
  else
  {
    /* check if we can use a key to resolve the group */
    /* Tables using JT_NEXT are handled here */
    uint nr;
    key_map keys=usable_keys;

    /*
      If not used with LIMIT, only use keys if the whole query can be
      resolved with a key;  This is because filesort() is usually faster than
      retrieving all rows through an index.
    */
    if (select_limit >= table->keyfile_info.records)
      keys&= table->used_keys;
    
    for (nr=0; keys ; keys>>=1, nr++)
    {
      if (keys & 1)
      {
	int flag;
	if ((flag=test_if_order_by_key(order,table,nr)))
	{
	  tab->index=nr;
	  tab->read_first_record=  (flag > 0 ? join_init_read_first_with_key:
				    join_init_read_last_with_key);
	  tab->type=JT_NEXT;		// Read with ha_rfirst(), ha_rnext()
	  DBUG_RETURN(0);
	}
      }
    }
    /* Check if we can use only indexes to resolve the group by */
    if (!select && usable_keys)
    {
      uint index=find_shortest_key(table,usable_keys);
      /* Make a quick range over the key to force filesort to use it */
      QUICK_RANGE *tmp=new QUICK_RANGE();
      if (!tmp || !(select= new SQL_SELECT) ||
	  !(select->quick=new QUICK_SELECT(table,index,1)))
	DBUG_RETURN(-1);
      select->quick->ranges.push_front(tmp);
      table->key_read=1;
      ha_extra(table,HA_EXTRA_KEYREAD);
    }
  }
  if (!(sortorder=make_unireg_sortorder(order,&length)))
    DBUG_RETURN(-1);				/* purecov: inspected */
  table->io_cache=(IO_CACHE*) my_malloc(sizeof(IO_CACHE),
					MYF(MY_FAE | MY_ZEROFILL));
  table->status=0;				// May be wrong if quick_select

  // If table has a range, move it to select
  if (select && !select->quick && table->reginfo.ref_key >= 0 && tab->quick)
  {
    select->quick=tab->quick;
    tab->quick=0;
  }
  table->found_records=filesort(&table,sortorder,length,
				select, 0L, select_limit);
  delete select;				// filesort did select
  tab->select=0;
  tab->type=JT_ALL;				// Read with normal read_record
  tab->read_first_record= join_init_read_record;
  if (table->key_read)				// Restore if we used indexes
  {
    table->key_read=0;
    ha_extra(table,HA_EXTRA_NO_KEYREAD);
  }
  DBUG_RETURN(table->found_records == HA_POS_ERROR);
}


/*****************************************************************************
** Remove duplicates from tmp table
** Table is a locked single thread table
** fields is the number of fields to check (from the end)
*****************************************************************************/

static int
remove_duplicates(TABLE *entry,List<Item> &fields)
{
  READ_RECORD info;
  int error;
  uint reclength,offset,field_count;
  char *org_record,*new_record;
  DBUG_ENTER("remove_duplicates");

  entry->reginfo.lock_type=TL_WRITE;
  VOID(ha_extra(entry,HA_EXTRA_NO_READCHECK));

  /* Calculate how many saved fields there is in list */
  field_count=0;
  List_iterator<Item> it(fields);
  Item *item;
  while ((item=it++))
    if (item->tmp_table_field())
      field_count++;

  if (!field_count)
  {						// only const items
    ha_reset(entry);				// Remove all execpt first
    if (!(error=ha_r_rnd(entry,entry->record[0],(byte*) 0)))
    {
      while ((error=ha_r_rnd(entry,entry->record[0],(byte*) 0)) <= 0)
	(void) ha_delete(entry,entry->record[0]);
    }
    DBUG_RETURN(0);
  }

  offset=entry->field[entry->fields - field_count]->offset();
  reclength=entry->reclength-offset;
  org_record=(char*) entry->record[0]+offset;
  new_record=(char*) entry->record[1]+offset;

  free_io_cache(entry);				// Safety
  init_read_record(&info,entry,(SQL_SELECT *) 0);
  while ((error=info.read_record(&info)) <= 0)
  {
    if (error == 0)
    {
      ha_info(entry,1);
      memcpy(new_record,org_record,reclength);
      while ((error=info.read_record(&info)) <= 0)
      {
	if (error == 0)
	{
	  if (memcmp(org_record,new_record,reclength) == 0)
	    if ((error=ha_delete(entry,entry->record[0])))
	    {
	      ha_error(entry,error,MYF(0)); /* purecov: inspected */
	      error= -1; /* purecov: inspected */
	      goto end; /* purecov: inspected */
	    }
	}
      }
      if (ha_r_rnd(entry,entry->record[0],entry->keyfile_info.ref.refpos))
      {
	error= -1; /* purecov: inspected */
	goto end; /* purecov: inspected */
      }
      VOID(ha_extra(entry,HA_EXTRA_REINIT_CACHE));
    }
  }
  if (error != HA_ERR_END_OF_FILE)
  {
    ha_error(entry,error,MYF(0));		/* purecov: inspected */
    error= -1;					/* purecov: inspected */
    goto end;					/* purecov: inspected */
  }
  error=0;
end:
  end_read_record(&info);
  DBUG_RETURN(error);
}


static SORT_FIELD *
make_unireg_sortorder(ORDER *order, uint *length)
{
  uint count;
  SORT_FIELD *sort,*pos;
  DBUG_ENTER("make_unireg_sortorder");

  count=0;
  for (ORDER *tmp = order; tmp; tmp=tmp->next)
    count++;
  pos=sort=(SORT_FIELD*) sql_alloc(sizeof(SORT_FIELD)*(count+1));

  for (;order;order=order->next,pos++)
  {
    pos->field=0; pos->item=0;
    if (order->item[0]->type() == Item::FIELD_ITEM)
      pos->field= ((Item_field*) (*order->item))->field;
    else if (order->item[0]->type() == Item::SUM_FUNC_ITEM &&
	     !order->item[0]->const_item())
      pos->field= ((Item_sum*) order->item[0])->tmp_table_field();
    else if (order->item[0]->type() == Item::COPY_STR_ITEM)
    {						// Blob patch
      pos->item= ((Item_copy_string*) (*order->item))->item;
    }
    else
      pos->item= *order->item;
    pos->reverse=! order->asc;
  }
  *length=count;
  DBUG_RETURN(sort);
}


/*****************************************************************************
**	Fill join cache with packed records
**	Records are stored in tab->cache.buffer and last record in
**	last record is stored with pointers to blobs to support very big
**	records
******************************************************************************/

static int
join_init_cache(THD *thd,JOIN_TAB *tables,uint table_count)
{
  reg1 uint i;
  uint length,blobs,size;
  CACHE_FIELD *copy,**blob_ptr;
  JOIN_CACHE  *cache;
  DBUG_ENTER("join_init_cache");

  cache= &tables[table_count].cache;
  cache->fields=blobs=0;

  for (i=0 ; i < table_count ; i++)
  {
    cache->fields+=tables[i].used_fields;
    blobs+=tables[i].used_blobs;
  }
  if (!(cache->field=(CACHE_FIELD*)
	sql_alloc(sizeof(CACHE_FIELD)*(cache->fields+table_count*2)+(blobs+1)*
		  sizeof(CACHE_FIELD*))))
  {
    my_free((gptr) cache->buff,MYF(0));		/* purecov: inspected */
    cache->buff=0;				/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  copy=cache->field;
  blob_ptr=cache->blob_ptr=(CACHE_FIELD**)
    (cache->field+cache->fields+table_count*2);

  length=0;
  for (i=0 ; i < table_count ; i++)
  {
    uint null_fields=0,used_fields;

    Field **f_ptr,*field;
    for (f_ptr=tables[i].table->field,used_fields=tables[i].used_fields ;
	 used_fields ;
	 f_ptr++)
    {
      field= *f_ptr;
      if (field->query_id == thd->query_id)
      {
	used_fields--;
	length+=field->fill_cache_field(copy);
	if (copy->blob_field)
	  (*blob_ptr++)=copy;
	if (field->maybe_null())
	  null_fields++;
	copy++;
      }
    }
    /* Copy null bits from table */
    if (null_fields && tables[i].table->null_fields)
    {						/* must copy null bits */
      copy->str=(char*) tables[i].table->null_flags;
      copy->length=(tables[i].table->null_fields+7)/8;
      copy->strip=0;
      copy->blob_field=0;
      length+=copy->length;
      copy++;
      cache->fields++;
    }
    /* If outer join table, copy null_row flag */
    if (tables[i].table->maybe_null)
    {
      copy->str= (char*) &tables[i].table->null_row;
      copy->length=sizeof(tables[i].table->null_row);
      copy->strip=0;
      copy->blob_field=0;
      length+=copy->length;
      copy++;
      cache->fields++;
    }
  }

  cache->records=0; cache->ptr_record= (uint) ~0;
  cache->length=length+blobs*sizeof(char*);
  cache->blobs=blobs;
  *blob_ptr=0;					/* End sequentel */
  size=max(join_buff_size,cache->length);
  if (!(cache->buff=(uchar*) my_malloc(size,MYF(0))))
    DBUG_RETURN(1);				/* Don't use cache */ /* purecov: inspected */
  cache->end=cache->buff+size;
  reset_cache(cache);
  DBUG_RETURN(0);
}


static ulong
used_blob_length(CACHE_FIELD **ptr)
{
  uint length,blob_length;
  for (length=0 ; *ptr ; ptr++)
  {
    (*ptr)->blob_length=blob_length=(*ptr)->blob_field->get_length();
    length+=blob_length;
    (*ptr)->blob_field->get_ptr(&(*ptr)->str);
  }
  return length;
}


static bool
store_record_in_cache(JOIN_CACHE *cache)
{
  ulong length;
  uchar *pos;
  CACHE_FIELD *copy,*end_field;
  bool last_record;

  pos=cache->pos;
  end_field=cache->field+cache->fields;

  length=cache->length;
  if (cache->blobs)
    length+=used_blob_length(cache->blob_ptr);
  if ((last_record=(length > (uint) (cache->end - pos))))
    cache->ptr_record=cache->records;

  /*
  ** There is room in cache. Put record there
  */
  cache->records++;
  for (copy=cache->field ; copy < end_field; copy++)
  {
    if (copy->blob_field)
    {
      if (last_record)
      {
	copy->blob_field->get_image((char*) pos,copy->length+sizeof(char*));
	pos+=copy->length+sizeof(char*);
      }
      else
      {
	copy->blob_field->get_image((char*) pos,copy->length); // blob length
	memcpy(pos+copy->length,copy->str,copy->blob_length);  // Blob data
	pos+=copy->length+copy->blob_length;
      }
    }
    else
    {
      if (copy->strip)
      {
	char *str,*end;
	for (str=copy->str,end= str+copy->length;
	     end > str && end[-1] == ' ' ;
	     end--) ;
	length=(uint) (end-str);
	memcpy(pos+1,str,length);
	*pos=(uchar) length;
	pos+=length+1;
      }
      else
      {
	memcpy(pos,copy->str,copy->length);
	pos+=copy->length;
      }
    }
  }
  cache->pos=pos;
  return last_record || (uint) (cache->end -pos) < cache->length;
}


static void
reset_cache(JOIN_CACHE *cache)
{
  cache->record_nr=0;
  cache->pos=cache->buff;
}


static void
read_cacheed_record(JOIN_TAB *tab)
{
  uchar *pos;
  uint length;
  bool last_record;
  CACHE_FIELD *copy,*end_field;

  last_record=tab->cache.record_nr++ == tab->cache.ptr_record;
  pos=tab->cache.pos;

  for (copy=tab->cache.field,end_field=copy+tab->cache.fields ;
       copy < end_field;
       copy++)
  {
    if (copy->blob_field)
    {
      if (last_record)
      {
	copy->blob_field->set_image((char*) pos,copy->length+sizeof(char*));
	pos+=copy->length+sizeof(char*);
      }
      else
      {
	copy->blob_field->set_ptr((char*) pos,(char*) pos+copy->length);
	pos+=copy->length+copy->blob_field->get_length();
      }
    }
    else
    {
      if (copy->strip)
      {
	memcpy(copy->str,pos+1,length=(uint) *pos);
	memset(copy->str+length,' ',copy->length-length);
	pos+=1+length;
      }
      else
      {
	memcpy(copy->str,pos,copy->length);
	pos+=copy->length;
      }
    }
  }
  tab->cache.pos=pos;
  return;
}


bool
cmp_buffer_with_ref(byte *buffer,REF_FIELD *fields,uint length)
{
  reg3 bool flag;
  reg2 uint tmp_length;

  for (flag=0 ; length ; fields++, buffer+=tmp_length, length-=tmp_length)
  {
    tmp_length=fields->length;
#ifdef NOT_NEEDED
    if (tmp_length > length)
      tmp_length=length; /* purecov: deadcode */
#endif
    if (flag || fields->field->cmp_image((char*) buffer,(size_t) tmp_length))
    {
      flag=1;
      fields->field->get_image((char*) buffer,(size_t) tmp_length);
    }
  }
  return flag;
}


static void
cp_buffer_from_ref(byte *buffer,REF_FIELD *fields,uint length)
{
  reg3 uint tmp_length;

  for (; length ; fields++, buffer+=tmp_length, length-=tmp_length)
  {
    tmp_length=fields->length;
    fields->field->get_image((char*) buffer,(size_t) tmp_length);
  }
}


/*****************************************************************************
** Group and order functions
*****************************************************************************/

/*
** Find order/group item in requested columns and change the item to point at
** it. If item doesn't exists, add it first in the field list
** Return 0 if ok.
*/

static int
find_order_in_list(THD *thd,TABLE_LIST *tables,ORDER *order,List<Item> &fields,
		   List<Item> &all_fields)
{
  if ((*order->item)->type() == Item::INT_ITEM)
  {						/* Order by position */
    Item *item=0;
    List_iterator<Item> li(fields);

    for (uint count= (uint) ((Item_int*) (*order->item))->value ;
	 count-- && (item=li++) ;) ;
    if (!item)
    {
      my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),
		      MYF(0),(*order->item)->full_name(),
	       thd->where);
      return 1;
    }
    order->item=li.ref();
    return 0;
  }
  char *save_where=thd->where;
  thd->where=0;					// No error if not found
  Item **item=find_item_in_list(*order->item,fields);
  thd->where=save_where;
  if (item)
  {
    order->item=item;				// use it
    return 0;
  }
  if ((*order->item)->fix_fields(thd,tables) || thd->fatal_error)
    return 1;					// Wrong field
  all_fields.push_front(*order->item);		// Add new field to field list
  order->item=(Item**) all_fields.head_ref();
  return 0;
}


/*
** Change order to point at item in select list. If item isn't a number
** and doesn't exits in the select list, add it the the field list.
*/

static int
setup_order(THD *thd,TABLE_LIST *tables,List<Item> &fields,
	     List<Item> &all_fields, ORDER *order)
{
  thd->where="order clause";
  for (; order; order=order->next)
  {
    if (find_order_in_list(thd,tables,order,fields,all_fields))
      return 1;
  }
  return 0;
}


static int
setup_group(THD *thd,TABLE_LIST *tables,List<Item> &fields,
	    List<Item> &all_fields, ORDER *order, bool *hidden_group_fields)
{
  *hidden_group_fields=0;
  if (!order)
    return 0;				/* Everything is ok */

#ifdef RESTRICTED_GROUP
  Item *item;
  List_iterator<Item> li(fields);
  while ((item=li++))
    item->marker=0;			/* Marker that field is not used */
#endif
  uint org_fields=all_fields.elements;

  thd->where="group statement";
  for ( ; order; order=order->next)
  {
    if (find_order_in_list(thd,tables,order,fields,all_fields))
      return 1;
    (*order->item)->marker=1;		/* Mark found */
    if ((*order->item)->with_sum_func)
    {
      my_printf_error(ER_WRONG_GROUP_FIELD, ER(ER_WRONG_GROUP_FIELD),MYF(0),
		      (*order->item)->full_name());
      return 1;
    }
  }
#ifdef RESTRICTED_GROUP
  li.rewind();
  while ((item=li++))
  {
    if (item->type() != Item::SUM_FUNC_ITEM && !item->marker)
    {
      my_printf_error(ER_WRONG_FIELD_WITH_GROUP,ER(ER_WRONG_FIELD_WITH_GROUP),
		      MYF(0),item->full_name());
      return 1;
    }
  }
#endif
  if (org_fields != all_fields.elements)
    *hidden_group_fields=1;			// group fields is not used
  return 0;
}

/*
** Add fields with aren't used at start of field list. Return FALSE if ok
*/

static bool
setup_new_fields(THD *thd,TABLE_LIST *tables,List<Item> &fields,
		 List<Item> &all_fields, ORDER *new_field)
{
  Item	  **item;
  DBUG_ENTER("setup_new_fields");

  thd->set_query_id=1;				// Not really needed, but...
  thd->where=0;					// Don't give error
  for ( ; new_field ; new_field=new_field->next)
  {
    if ((item=find_item_in_list(*new_field->item,fields)))
      new_field->item=item;			/* Change to shared Item */
    else
    {
      thd->where="procedure list";
      if ((*new_field->item)->fix_fields(thd,tables))
	DBUG_RETURN(1); /* purecov: inspected */
      new_field->free_me=TRUE;
      all_fields.push_front(*new_field->item);
    }
  }
  DBUG_RETURN(0);
}

/*
** Add all fields non const fields with dosen't exist in order to order
** If all fields are const return 0
*/

static ORDER *
add_all_fields_to_order(JOIN *join,ORDER *order_list,List<Item> &fields)
{
  List_iterator<Item> li(fields);
  Item *item;
  ORDER *order,**prev;
  bool found_item=0;

  while ((item=li++))
    item->marker=0;			/* Marker that field is not used */

  prev= &order_list;
  for (order=order_list ; order; order=order->next)
  {
    (*order->item)->marker=1;
    prev= &order->next;
  }

  li.rewind();
  while ((item=li++))
  {
    if (item->const_item() || item->with_sum_func)
      continue;
    found_item=1;
    if (!item->marker)
    {
      ORDER *ord=(ORDER*) sql_alloc(sizeof(ORDER));
      ord->item=li.ref();
      ord->asc=1;
      ord->free_me=0;
      *prev=ord;
      prev= &ord->next;
    }
  }
  *prev=0;
  return found_item ? order_list : 0;
}


/*****************************************************************************
** Update join with count of the different type of fields
*****************************************************************************/

static void
count_field_types(JOIN *join,List<Item> &fields)
{
  List_iterator<Item> li(fields);
  Item *field;

  join->field_count=join->sum_func_count=join->func_count=0;
  join->quick_group=1;
  while ((field=li++))
  {
    if (field->type() == Item::FIELD_ITEM)
      join->field_count++;
    else if (field->type() == Item::FUNC_ITEM ||
	     field->type() == Item::COND_ITEM ||
	     field->type() == Item::FIELD_AVG_ITEM ||
	     field->type() == Item::FIELD_STD_ITEM)
      join->func_count++;
    else if (field->type() == Item::SUM_FUNC_ITEM && ! field->const_item())
    {
      Item_sum *sum_item=(Item_sum*) field;
      if (!sum_item->quick_group)
	join->quick_group=0;
      join->sum_func_count++;
      if (sum_item->item->type() == Item::FUNC_ITEM)
	join->func_count++;
    }
  }
}


/*
  Return 1 if second is a subpart of first argument
  If first parts has different direction, change it to second part
  (group is sorted like order)
*/

static bool
test_if_subpart(ORDER *a,ORDER *b)
{
  for (; a && b; a=a->next,b=b->next)
  {
    if (a->item == b->item)
      a->asc=b->asc;
    else
      return 0;
  }
  return test(!b);
}

/*
  Return table number if there is only one table in sort order
  and group and order is compatible
  else return ~0;
*/

static TABLE *
get_sort_by_table(ORDER *a,ORDER *b,TABLE_LIST *tables)
{
  table_map map=0L;
  DBUG_ENTER("get_sort_by_table");

  if (!a)
    a=b;					// Only one need to be given
  else if (!b)
    b=a;

  for (; a && b; a=a->next,b=b->next)
  {
    if (a->item != b->item)
      DBUG_RETURN(0);
    map|=a->item[0]->used_tables();
  }
  if (!map)
    DBUG_RETURN(0);

  for ( ; !(map & 1) ; map>>=1,tables=tables->next) ;
  if (map != (table_map) 1)
    DBUG_RETURN(0);				// More than one table
  DBUG_PRINT("exit",("sort by table: %d",tables->table->tablenr));
  DBUG_RETURN(tables->table);
}


	/* calc how big buffer we need for comparing group entries */

static void
calc_group_buffer(JOIN *join,ORDER *group)
{
  uint key_length=0,parts=0;
  for (; group ; group=group->next)
  {
    Field *field=(*group->item)->tmp_table_field();
    if (field)
      key_length+=field->pack_length();
    else if ((*group->item)->result_type() == REAL_RESULT)
      key_length+=sizeof(double);
    else if ((*group->item)->result_type() == INT_RESULT)
      key_length+=sizeof(longlong);
    else
      key_length+=(*group->item)->max_length;
    parts++;
    if ((*group->item)->maybe_null)
      key_length++;
  }
  join->group_length=key_length;
  join->group_parts=parts;
}


/*
** Get a list of buffers for saveing last group
** Groups are saved in reverse order for easyer check loop
*/

static void
alloc_group_fields(JOIN *join,ORDER *group)
{
  if (group)
  {
    for (; group ; group=group->next)
      join->group_fields.push_front(new_Item_buff(*group->item));
  }
  join->sort_and_group=1;			/* Mark for do_select */
}


static int
test_if_group_changed(List<Item_buff> &list)
{
  List_iterator<Item_buff> li(list);
  int index= -1,i;
  Item_buff *buff;

  for (i=(int) list.elements-1 ; (buff=li++) ; i--)
  {
    if (buff->cmp())
      index=i;
  }
  return index;
}



/*
** Setup copy_fields to save fields at start of new group
** Only FIELD_ITEM:s and FUNC_ITEM:s needs to be saved between groups.
** Change old item_field to use a new field with points at saved fieldvalue
** This function is only called before use of send_fields
*/

static void
setup_copy_fields(JOIN *join,List<Item> &fields)
{
  Item *pos;
  uint field_count;
  List_iterator<Item> li(fields);
  Copy_field *copy;

  field_count=join->field_count;
  copy=join->copy_field= new Copy_field[field_count];
  join->copy_funcs.empty();
  while ((pos=li++))
  {
    if (pos->type() == Item::FIELD_ITEM)
    {
      Item_field *item=(Item_field*) pos;
      if (item->field->flags & BLOB_FLAG)
      {
	pos=new Item_copy_string(pos);
	VOID(li.replace(pos));
	join->copy_funcs.push_back(pos);
	continue;
      }

      /* set up save buffer and change result_field to point at saved value */
      Field *field= item->field;
      item->result_field=field->new_field(field->table);
      copy->set((char*) sql_alloc(field->pack_length()+1),
		item->result_field);
      item->result_field->move_field(copy->to_ptr,copy->to_null_ptr,1);
      copy++;
    }
    else if ((pos->type() == Item::FUNC_ITEM ||
	      pos->type() == Item::COND_ITEM) &&
	     !pos->with_sum_func)
    {						// Save for send fields
      pos=new Item_copy_string(pos);
      VOID(li.replace(pos));
      join->copy_funcs.push_back(pos);
    }
  }
  join->copy_field_count=(uint) (copy - join->copy_field);
}


/*
** Copy fields and null values between two tables
*/

static void
copy_fields(JOIN *join)
{
  Copy_field *ptr=join->copy_field,*end=ptr+join->copy_field_count;

  for ( ; ptr != end; ptr++)
    (*ptr->do_copy)(ptr);

  List_iterator<Item> it(join->copy_funcs);
  Item_copy_string *item;
  while ((item = (Item_copy_string*) it++))
  {
    item->copy();
  }
}


/*****************************************************************************
** Make an array of pointer to sum_functions to speed up sum_func calculation
*****************************************************************************/

static void
make_sum_func_list(JOIN *join,List<Item> &fields)
{
  Item_sum **func = (Item_sum**) sql_alloc(sizeof(Item_sum*)*
					   (join->sum_func_count+1));
  List_iterator<Item> it(fields);
  join->sum_funcs=func;

  Item *field;
  while ((field=it++))
  {
    if (field->type() == Item::SUM_FUNC_ITEM && !field->const_item())
    {
      *func++=(Item_sum*) field;
    }
  }
  *func=0;					// End marker
}


/*
** Change all funcs and sum_funcs to fields in tmp table
*/

static void
change_to_use_tmp_fields(List<Item> &items)
{
  List_iterator<Item> it(items);
  Item *item_field,*item;

  while ((item=it++))
  {
    Field *field;
    if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
      continue;
    if (item->type() == Item::FIELD_ITEM)
    {
      ((Item_field*) item)->field=
	((Item_field*) item)->result_field;
    }
    else if ((field=item->tmp_table_field()))
    {
      if (item->type() == Item::SUM_FUNC_ITEM && field->table->group)
      {
	if (((Item_sum*) item)->sum_func() == Item_sum::AVG_FUNC)
	  item_field=(Item*) new Item_avg_field((Item_sum_avg*) item);
	else if (((Item_sum*) item)->sum_func() == Item_sum::STD_FUNC)
	  item_field=(Item*) new Item_std_field((Item_sum_std*) item);
	else
	  item_field=(Item*) new Item_field(field);
      }
      else
	item_field=(Item*) new Item_field(field);
      item_field->name=item->name;		/*lint -e613 */
#ifdef DELETE_ITEMS
      delete it.replace(item_field);		/*lint -e613 */
#else
      (void) it.replace(item_field);		/*lint -e613 */
#endif
    }
  }
}


/*
** Change all sum_func refs to fields to point at fields in tmp table
** Change all funcs to be fields in tmp table
*/

static void
change_refs_to_tmp_fields(List<Item> &items)
{
  List_iterator<Item> it(items);
  Item *item;

  while ((item= it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM)
    {
      if (!item->const_item())
      {
	Item_sum *sum_item= (Item_sum*) item;
	if (sum_item->result_field)		// If not a const sum func
	{
#ifdef DELETE_ITEMS
	  delete sum_item->item;			// This is a nop
#endif
	  sum_item->item= new Item_field(sum_item->result_field);
	}
      }
    }
    else if (item->with_sum_func)
      continue;
    else if (item->type() == Item::FUNC_ITEM || item->type() == Item::COND_ITEM)
    {						/* All funcs are stored */
#ifdef DELETE_ITEMS
      delete it.replace(new Item_field(((Item_func*) item)->result_field));
#else
      (void) it.replace(new Item_field(((Item_func*) item)->result_field));
#endif
    }
    else if (item->type() == Item::FIELD_ITEM)	/* Change refs */
    {
      ((Item_field*)item)->field=((Item_field*) item)->result_field;
    }
  }
}



/******************************************************************************
** code for calculating functions
******************************************************************************/

static void
init_tmptable_sum_functions(Item_sum **func_ptr)
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->reset_field();
}


	/* Update record 0 in tmp_table from record 1 */

static void
update_tmptable_sum_func(Item_sum **func_ptr,
			 TABLE *tmp_table __attribute__((unused)))
{
  Item_sum *func;
  while ((func= *(func_ptr++)))
    func->update_field(0);
}


	/* Copy result of sum functions to record in tmp_table */

static void
copy_sum_funcs(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func = *func_ptr) ; func_ptr++)
    (void) func->save_in_field(func->result_field);
  return;
}


static void
init_sum_functions(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= (Item_sum*) *func_ptr) ; func_ptr++)
    func->reset();
}


static void
update_sum_func(Item_sum **func_ptr)
{
  Item_sum *func;
  for (; (func= (Item_sum*) *func_ptr) ; func_ptr++)
    func->add();
}

	/* Copy result of functions to record in tmp_table */

static void
copy_funcs(Item_result_field **func_ptr)
{
  Item_result_field *func;
  for (; (func = *func_ptr) ; func_ptr++)
    (void) func->save_in_field(func->result_field);
  return;
}


/*****************************************************************************
** Create a condition for a const reference and add this to the
** currenct select for the table
*****************************************************************************/

static void add_ref_to_table_cond(JOIN_TAB *join_tab)
{
  Item_cond_and *cond=new Item_cond_and();
  TABLE *table=join_tab->table;
  int error;
  DBUG_ENTER("add_ref_to_table_cond");

  for (uint i=0 ; i < table->reginfo.ref_fields ; i++)
  {
    Item *value;
    LINT_INIT(value);
    Field *field=table->key_info[table->reginfo.ref_key].key_part[i].field;
    field->set_image(table->reginfo.ref_field[i].ptr,field->pack_length());

    switch (field->cmp_type()) {
    case REAL_RESULT:
      value=new Item_real(field->val_real());
      break;
    case INT_RESULT:
      value=new Item_int(field->val_int());
      break;
    case STRING_RESULT:
      {
	char buff[MAX_FIELD_WIDTH];
	String str(buff,sizeof(buff));
	field->val_str(&str,&str);
	value=new Item_string(sql_strmake(str.ptr(),str.length()),
			      str.length());
	break;
      }
    }
    cond->add(new Item_func_eq(new Item_field(field),value));
  }
  cond->fix_fields((THD *) 0,(TABLE_LIST *) 0);
  if (join_tab->select)
  {
    cond->add(join_tab->select->cond);
    join_tab->select->cond=cond;
  }
  else
    join_tab->select=make_select(&join_tab->table,1,0,0,cond,&error);
  DBUG_VOID_RETURN;
}

/****************************************************************************
** Send a description about what how the select will be done to stdout
****************************************************************************/

static void select_describe(JOIN *join)
{
  DBUG_ENTER("select_describe");

  List<Item> field_list;
  Item *item;

  field_list.push_back(new Item_empty_string("table",NAME_LEN));
  field_list.push_back(new Item_empty_string("type",10));
  field_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_LEN*MAX_KEY));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key",NAME_LEN));
  item->maybe_null=1;
  field_list.push_back(item=new Item_int("key_len",0,3));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("ref",
						  NAME_LEN*MAX_REF_PARTS));
  item->maybe_null=1;
  field_list.push_back(new Item_real("rows",0.0,0,10));
  field_list.push_back(new Item_empty_string("Extra",80));
  if (send_fields(join->thd,field_list,1))
    return; /* purecov: inspected */

  char buff[141];
  String tmp(buff,sizeof(buff)),*packet= &join->thd->packet;
  for (uint i=0 ; i < join->tables ; i++)
  {
    JOIN_TAB *tab=join->join_tab+i;
    TABLE *table=tab->table;

    if (tab->type == JT_ALL && tab->select && tab->select->quick)
      tab->type= JT_RANGE;
    packet->length(0);
    net_store_data(packet,table->table_name);
    net_store_data(packet,join_type_str[tab->type]);
    tmp.length(0);
    key_map bits;
    uint j;
    for (j=0,bits=tab->keys ; bits ; j++,bits>>=1)
    {
      if (bits & 1)
      {
	if (tmp.length())
	  tmp.append(',');
	tmp.append(table->key_info[j].name);
      }
    }
    if (tmp.length())
      net_store_data(packet,tmp.ptr(),tmp.length());
    else
      net_store_null(packet);
    if (table->reginfo.ref_fields)
    {
      net_store_data(packet,table->key_info[table->reginfo.ref_key].name);
      net_store_data(packet,(long) table->reginfo.ref_length);
      tmp.length(0);
      for (uint ref=0 ; ref < table->reginfo.ref_fields ; ref++)
      {
	Field *field=table->reginfo.ref_field[ref].field;
	if (tmp.length())
	  tmp.append(',');
	if (field->table_name)
	{
	  tmp.append(field->table_name);
	  tmp.append('.');
	}
	tmp.append(field->field_name);
      }
      net_store_data(packet,tmp.ptr(),tmp.length());
    }
    else if (tab->type == JT_NEXT)
    {
      net_store_data(packet,table->key_info[tab->index].name);
      net_store_data(packet,(long) table->key_info[tab->index].key_length);
      net_store_null(packet);
    }
    else if (tab->select && tab->select->quick)
    {
      net_store_data(packet,table->key_info[tab->select->quick->index].name);;
      net_store_null(packet);
      net_store_null(packet);
    }
    else
    {
      net_store_null(packet);
      net_store_null(packet);
      net_store_null(packet);
    }
    sprintf(buff,"%.0f",join->best_positions[i].records_read);
    net_store_data(packet,buff);
    my_bool key_read=table->key_read;
    if (tab->type == JT_NEXT &&
	((table->used_keys & ((key_map) 1 << tab->index))))
      key_read=1;

    if (tab->info)
      net_store_data(packet,tab->info);
    else if (tab->select)
    {
      buff[0]=0;
      if (tab->use_quick == 2)
	sprintf(buff,"range checked for each record (index map: %lu)",
		tab->keys);
      else if (!tab->select->quick)
	strmov(buff,"where used");
      if (key_read)
	strmov(strend(buff),"; Using index");
      net_store_data(packet,buff);
    }
    else if (key_read)
      net_store_data(packet,"Using index");
    else
      net_store_data(packet,"",0);
    if (my_net_write(&join->thd->net,(char*) packet->ptr(),packet->length()))
      DBUG_VOID_RETURN;				/* purecov: inspected */
  }
  send_eof(&join->thd->net);
  DBUG_VOID_RETURN;
}


static void describe_info(const char *info)
{
  List<Item> field_list;
  THD *thd=current_thd;
  String *packet= &thd->packet;

  field_list.push_back(new Item_empty_string("Comment",80));
  if (send_fields(thd,field_list,1))
    return; /* purecov: inspected */
  packet->length(0);
  net_store_data(packet,info);
  if (!my_net_write(&thd->net,(char*) packet->ptr(),packet->length()))
    send_eof(&thd->net);
}
