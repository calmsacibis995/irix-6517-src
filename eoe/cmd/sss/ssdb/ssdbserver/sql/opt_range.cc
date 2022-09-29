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
#include <nisam.h>
#include "sql_select.h"


#ifndef EXTRA_DEBUG
#define test_rb_tree(A,B) {}
#define test_use_count(A) {}
#endif


static int sel_cmp(Field *f,char *a,char *b,uint8 a_flag,uint8 b_flag);

static const uint NO_MIN_RANGE=1,NO_MAX_RANGE=2,NEAR_MIN=4,NEAR_MAX=8;

class SEL_ARG :public Sql_alloc
{
public:
  uint8 min_flag,max_flag,maybe_flag;
  uint8 part;					// Which key part
  uint16 elements;				// Elements in tree
  ulong use_count;				// use of this sub_tree
  Field *field;
  char *min_value,*max_value;			// Pointer to range

  SEL_ARG *left,*right,*next,*prev,*parent,*next_key_part;
  enum leaf_color { BLACK,RED } color;
  enum Type { IMPOSSIBLE, MAYBE, MAYBE_KEY, KEY_RANGE } type;

  SEL_ARG() {}
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *,const char *,const char *);
  SEL_ARG(Field *field, uint8 part, char *min_value, char *max_value,
	  uint8 min_flag, uint8 max_flag, uint8 maybe_flag);
  SEL_ARG(enum Type type_arg)
    :use_count(1),type(type_arg),next_key_part(0),left(0),elements(1) {}
  inline bool is_same(SEL_ARG *arg)
  {
    if (type != arg->type)
      return 0;
    if (type != KEY_RANGE)
      return 1;
    return cmp_min_to_min(arg) == 0 && cmp_max_to_max(arg) == 0;
  }
  inline void merge_flags(SEL_ARG *arg) { maybe_flag|=arg->maybe_flag; }
  inline void maybe_smaller() { maybe_flag=1; }
  int cmp_min_to_min(SEL_ARG* arg)
  {
    return sel_cmp(field,min_value, arg->min_value, min_flag, arg->min_flag);
  }
  int cmp_min_to_max(SEL_ARG* arg)
  {
    return sel_cmp(field,min_value, arg->max_value, min_flag, arg->max_flag);
  }
  int cmp_max_to_max(SEL_ARG* arg)
  {
    return sel_cmp(field,max_value, arg->max_value, max_flag, arg->max_flag);
  }
  int cmp_max_to_min(SEL_ARG* arg)
  {
    return sel_cmp(field,max_value, arg->min_value, max_flag, arg->min_flag);
  }
  SEL_ARG *clone_and(SEL_ARG* arg)
  {						// Get overlapping range
    char *new_min,*new_max;
    uint8 flag_min,flag_max;
    if (cmp_min_to_min(arg) >= 0)
    {
      new_min=min_value; flag_min=min_flag;
    }
    else
    {
      new_min=arg->min_value; flag_min=arg->min_flag; /* purecov: deadcode */
    }
    if (cmp_max_to_max(arg) <= 0)
    {
      new_max=max_value; flag_max=max_flag;
    }
    else
    {
      new_max=arg->max_value; flag_max=arg->max_flag;
    }
    return new SEL_ARG(field, part, new_min, new_max, flag_min, flag_max,
		       test(maybe_flag && arg->maybe_flag));
  }
  SEL_ARG *clone_first(SEL_ARG *arg)
  {						// min <= X < arg->min
    return new SEL_ARG(field,part, min_value, arg->min_value,
		       min_flag, arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX,
		       maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone_last(SEL_ARG *arg)
  {						// min <= X <= key_max
    return new SEL_ARG(field, part, min_value, arg->max_value,
		       min_flag, arg->max_flag, maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone(SEL_ARG *new_parent,SEL_ARG **next);

  bool copy_min(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_min_to_min(arg) > 0)
    {
      min_value=arg->min_value; min_flag=arg->min_flag;
      if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
	  (NO_MAX_RANGE | NO_MIN_RANGE))
	return 1;				// Full range
    }
    maybe_flag|=arg->maybe_flag;
    return 0;
  }
  bool copy_max(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_max_to_max(arg) <= 0)
    {
      max_value=arg->max_value; max_flag=arg->max_flag;
      if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
	  (NO_MAX_RANGE | NO_MIN_RANGE))
	return 1;				// Full range
    }
    maybe_flag|=arg->maybe_flag;
    return 0;
  }

  void copy_min_to_min(SEL_ARG *arg)
  {
    min_value=arg->min_value; min_flag=arg->min_flag;
  }
  void copy_min_to_max(SEL_ARG *arg)
  {
    max_value=arg->min_value;
    max_flag=arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX;
  }
  void copy_max_to_min(SEL_ARG *arg)
  {
    min_value=arg->max_value;
    min_flag=arg->max_flag & NEAR_MAX ? 0 : NEAR_MIN;
  }
  void store(uint length,char **min_key,uint min_key_flag,
	     char **max_key, uint max_key_flag)
  {
    if (!(min_flag & NO_MIN_RANGE) &&
	!(min_key_flag & (NO_MIN_RANGE | NEAR_MIN)))
    {
      memcpy(*min_key,min_value,length);
      (*min_key)+= length;
    }
    if (!(max_flag & NO_MAX_RANGE) &&
	!(max_key_flag & (NO_MAX_RANGE | NEAR_MAX)))
    {
      memcpy(*max_key,max_value,length);
      (*max_key)+= length;
    }
  }
  SEL_ARG *insert(SEL_ARG *key);
  SEL_ARG *tree_delete(SEL_ARG *key);
  SEL_ARG *find_range(SEL_ARG *key);
  SEL_ARG *rb_insert(SEL_ARG *leaf);
  friend SEL_ARG *rb_delete_fixup(SEL_ARG *root,SEL_ARG *key, SEL_ARG *par);
#ifdef EXTRA_DEBUG
  friend int test_rb_tree(SEL_ARG *element,SEL_ARG *parent);
  void test_use_count(SEL_ARG *root);
#endif
  SEL_ARG *first();
  void make_root();
  inline bool simple_key()
  {
    return !next_key_part && elements == 1;
  }
  void increment_use_count(long count)
  {
    if (next_key_part)
    {
      next_key_part->use_count+=count;
      count*= (next_key_part->use_count-count);
      for (SEL_ARG *pos=next_key_part->first(); pos ; pos=pos->next)
	if (pos->next_key_part)
	  pos->increment_use_count(count);
    }
  }
  void free_tree()
  {
    for (SEL_ARG *pos=first(); pos ; pos=pos->next)
      if (pos->next_key_part)
      {
	pos->next_key_part->use_count--;
	pos->next_key_part->free_tree();
      }
  }

  inline SEL_ARG **parent_ptr()
  {
    return parent->left == this ? &parent->left : &parent->right;
  }
  SEL_ARG *clone_tree();
};


class SEL_TREE :public Sql_alloc
{
public:
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;
  SEL_TREE(enum Type type_arg) :type(type_arg) {}
  SEL_TREE() :type(KEY) { bzero((char*) keys,sizeof(keys));}
  SEL_ARG *keys[MAX_KEY];
};


typedef struct st_qsel_param {
  uint baseflag,current_tablenr,keys;
  table_map prev_tables,read_tables,current_table;
  TABLE *table;
  bool quick;				// Don't calulate possible keys
  KEY_PART key_parts[MAX_KEY*MAX_REF_PARTS],*key_parts_end,*key[MAX_KEY];
  uint real_keynr[MAX_KEY];
  char min_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH],
    max_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  MEM_ROOT alloc;
} PARAM;


static SEL_TREE * get_mm_parts(PARAM *param,Field *field,
			       Item_func::Functype type,Item *value,
			       Item_result cmp_type);
static SEL_ARG *get_mm_leaf(PARAM *param,Field *field,KEY_PART *key_part,
			    Item_func::Functype type,Item *value,
			    Item_result cmp_type);
static bool like_range(const char *ptr,uint length,char wild_prefix,
		       uint field_length, char *min_str,char *max_str,
		       char max_sort_char);
static SEL_TREE *get_mm_tree(PARAM *param,COND *cond);
static ha_rows check_quick_select(PARAM *param,uint index,SEL_ARG *key_tree);
static ha_rows check_quick_keys(PARAM *param,uint index,SEL_ARG *key_tree,
				char *min_key,uint min_key_flag,
				char *max_key, uint max_key_flag);

static QUICK_SELECT *get_quick_select(PARAM *param,uint index,
				      SEL_ARG *key_tree);
#ifndef DBUG_OFF
static void print_quick(QUICK_SELECT *quick,table_map needed_reg);
#endif
static SEL_TREE *tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_TREE *tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_ARG *sel_add(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_or(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag);
static bool get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1);
static bool get_quick_keys(PARAM *param,QUICK_SELECT *quick,uint index,
			   SEL_ARG *key_tree,char *min_key,uint min_key_flag,
			   char *max_key,uint max_key_flag);
static bool eq_tree(SEL_ARG* a,SEL_ARG *b);

static SEL_ARG null_element(SEL_ARG::IMPOSSIBLE);


/***************************************************************************
** Basic functions for SQL_SELECT and QUICK_SELECT
***************************************************************************/

SQL_SELECT::SQL_SELECT() :quick(0),cond(0),free_cond(0)
{
  quick_keys=needed_reg=0;
  my_b_clear(&file);
}


SQL_SELECT::~SQL_SELECT()
{
  delete quick;
  if (free_cond)
    delete cond;
  close_cacheed_file(&file);
}

#undef index					// Fix or Unixware 7

QUICK_SELECT::QUICK_SELECT(TABLE *table,uint key_nr,bool no_alloc)
  :next(0),head(table),index(key_nr),it(ranges)
{
  if (!no_alloc)
  {
    init_sql_alloc(&alloc,1024);		// Allocates everything here
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);
  }
  else
    bzero((char*) &alloc,sizeof(alloc));
  next=0;
}

QUICK_SELECT::~QUICK_SELECT()
{
  free_root(&alloc);
}


QUICK_RANGE::QUICK_RANGE()
  :min_key(0),max_key(0),min_length(0),max_length(0),
   flag(NO_MIN_RANGE | NO_MAX_RANGE)
{}


SEL_ARG::SEL_ARG(SEL_ARG &arg) :Sql_alloc()
{
  type=arg.type;
  min_flag=arg.min_flag;
  max_flag=arg.max_flag;
  maybe_flag=arg.maybe_flag;
  part=arg.part;
  field=arg.field;
  min_value=arg.min_value;
  max_value=arg.max_value;
  next_key_part=arg.next_key_part;
  use_count=1; elements=1;
}


inline void SEL_ARG::make_root()
{
  left=right= &null_element;
  color=BLACK;
  next=prev=0;
  use_count=0; elements=1;
}

SEL_ARG::SEL_ARG(Field *f,const char *min_value_arg,const char *max_value_arg)
  :min_flag(0), max_flag(0), maybe_flag(0), elements(1), use_count(1),
   field(f), min_value((char*) min_value_arg), max_value((char*) max_value_arg),
   type(KEY_RANGE),color(BLACK),next(0),prev(0),next_key_part(0)
{
  left=right= &null_element;
}

SEL_ARG::SEL_ARG(Field *field_,uint8 part_,char *min_value_,char *max_value_,
		 uint8 min_flag_,uint8 max_flag_,uint8 maybe_flag_)
  :min_flag(min_flag_),max_flag(max_flag_),maybe_flag(maybe_flag_),
   part(part_),elements(1),use_count(1),field(field_),
   min_value(min_value_), max_value(max_value_),
   type(KEY_RANGE),color(BLACK),next(0),prev(0),next_key_part(0)
{
  left=right= &null_element;
}

SEL_ARG *SEL_ARG::clone(SEL_ARG *new_parent,SEL_ARG **next_arg)
{
  SEL_ARG *tmp;
  if (type != KEY_RANGE)
  {
    tmp=new SEL_ARG(type);
    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;
  }
  else
  {
    tmp=new SEL_ARG(field,part, min_value,max_value,
		    min_flag, max_flag, maybe_flag);
    tmp->parent=new_parent;
    tmp->next_key_part=next_key_part;
    if (left != &null_element)
      tmp->left=left->clone(tmp,next_arg);

    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;

    if (right != &null_element)
      tmp->right=right->clone(tmp,next_arg);
  }
  increment_use_count(1);
  return tmp;
}

SEL_ARG *SEL_ARG::first()
{
  SEL_ARG *next_arg=this;
  if (!next_arg->left)
    return 0;					// MAYBE_KEY
  while (next_arg->left != &null_element)
    next_arg=next_arg->left;
  return next_arg;
}


/*
  Check if a compare is ok, when one takes ranges in account
  Returns -2 or 2 if the ranges where 'joined' like  < 2 and >= 2
 */

static int sel_cmp(Field *field, char *a,char *b,uint8 a_flag,uint8 b_flag)
{
  /* First check if there was a compare to a min or max element */
  if (a_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
  {
    if ((a_flag & (NO_MIN_RANGE | NO_MAX_RANGE)) ==
	(b_flag & (NO_MIN_RANGE | NO_MAX_RANGE)))
      return 0;
    return (a_flag & NO_MIN_RANGE) ? -1 : 1;
  }
  if (b_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
    return (b_flag & NO_MIN_RANGE) ? 1 : -1;

  int cmp=field->cmp(a,b);
  if (cmp) return cmp < 0 ? -1 : 1;		// The values differed

  /* Check if the compared equal arguments was defined with open/closed range */
  if (a_flag & (NEAR_MIN | NEAR_MAX))
  {
    if ((a_flag & (NEAR_MIN | NEAR_MAX)) == (b_flag & (NEAR_MIN | NEAR_MAX)))
      return 0;
    if (!(b_flag & (NEAR_MIN | NEAR_MAX)))
      return (a_flag & NEAR_MIN) ? 2 : -2;
    return (a_flag & NEAR_MIN) ? 1 : -1;
  }
  if (b_flag & (NEAR_MIN | NEAR_MAX))
    return (b_flag & NEAR_MIN) ? -2 : 2;
  return 0;					// The elements where equal
}


SEL_ARG *SEL_ARG::clone_tree()
{
  SEL_ARG link,*next_arg,*root;
  next_arg= &link;
  root=clone((SEL_ARG *) 0, &next_arg);
  next_arg->next=0;				// Fix last link
  link.next->prev=0;				// Fix first link
  root->use_count=0;
  return root;
}

/*****************************************************************************
**	Test if a key can be used in different ranges
**	Returns:
**	-1 if impossible select
**	0 if can't use quick_select
**	1 if found usably range
**	Updates the folling in the select parameter:
**	needed_reg ; Bits for keys with may be used if all prev regs are read
**	quick	   ; Parameter to use when reading records.
**      In the table struct the following information is updated:
**	quick_keys ; Which keys can be used
**	quick_rows ; How many rows the key matches
*****************************************************************************/

int SQL_SELECT::test_quick_select(key_map keys_to_use,table_map prev_tables,
				  ha_rows limit)
{
  PARAM param;
  uint basflag;
  DBUG_ENTER("test_quick_select");

  delete quick;
  quick=0;
  needed_reg=quick_keys=0;
  if (!cond || (specialflag & SPECIAL_SAFE_MODE) || !limit)
    DBUG_RETURN(0); /* purecov: inspected */
  if (!((basflag= head->db_capabilities) & HA_KEYPOS_TO_RNDPOS) &&
      keys_to_use == (uint) ~0 || !keys_to_use)
    DBUG_RETURN(0);				/* Not smart database */
  records=head->keyfile_info.records;
  if (!records)
    records++;					/* purecov: inspected */
  read_time=(ha_rows) (((head->keyfile_info.data_file_length)/IO_SIZE)+1+
		       records / TIME_FOR_COMPARE);
  if (limit < records)
    read_time=records+1;			// Force to use index
  else if (read_time < 2)
    DBUG_RETURN(0);				/* No nead for quick select */

  DBUG_PRINT("info",("Time to scan table: %ld",(long) read_time));

  /* set up parameter that is passed to all functions */

  param.baseflag=basflag;
  param.prev_tables=prev_tables;
  param.read_tables=read_tables;
  param.current_table= head->map;
  param.current_tablenr= head->tablenr;
  param.table=head;
  current_thd->no_errors=1;			// Don't warn about NULL

  KEY_PART *key_parts=param.key_parts;
  param.keys=0;
  for (uint index=0 ; index < head->keys ; index++)
  {
    if (!(keys_to_use & ((key_map) 1L << index)))
      continue;
    KEY *key_info= &head->key_info[index];
    param.key[param.keys]=key_parts;
    for (uint part=0 ; part < key_info->key_parts ; part++,key_parts++)
    {
      key_parts->key=param.keys;
      key_parts->part=part;
      key_parts->length= key_info->key_part[part].length;
      key_parts->field=  key_info->key_part[part].field;
    }
    param.real_keynr[param.keys++]=index;
  }

  if (key_parts != param.key_parts)		// If there is some usable key
  {
    param.key_parts_end=key_parts;
    SEL_TREE *tree;
    MEM_ROOT *old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
    my_pthread_setspecific_ptr(THR_MALLOC,&param.alloc);
    init_sql_alloc(&param.alloc,1024);

    if ((tree=get_mm_tree(&param,cond)))
    {
      if (tree->type == SEL_TREE::IMPOSSIBLE)
      {
	records=0L;				// Return -1 from this function
	read_time=HA_POS_ERROR;
      }
      else if (tree->type == SEL_TREE::KEY)
      {
	SEL_ARG **key,**end,**best_key=0;
	uint index;

	for (index=0,key=tree->keys, end=key+param.keys ;
	     key != end ;
	     key++,index++)
	{
	  ha_rows found_records,found_read_time;

	  if (*key)
	  {
	    if ((*key)->type == SEL_ARG::MAYBE_KEY ||
		(*key)->maybe_flag)
	      needed_reg|= (table_map) 1 << param.real_keynr[index];

	    found_records=check_quick_select(&param,index, *key);
	    if (found_records != HA_POS_ERROR &&
		head->used_keys & ((table_map) 1 << param.real_keynr[index]))
	    {
	      /*
	      ** We can resolve this by only reading trough this key
	      ** Assume that we will read trough the whole key range
	      ** and that all key blocks are half full (normally things are
	      ** much better)
	      */
	      uint keys_per_block= head->keyfile_info.block_size/2/head->key_info[param.real_keynr[index]].key_length+1;
	      found_read_time=(found_records+keys_per_block-1)/keys_per_block+1;
	    }
	    else
	      found_read_time=found_records;
	    if (read_time > found_read_time)
	    {
	      read_time=found_read_time;
	      records=found_records;
	      best_key=key;
	    }
	  }
	}
	if (best_key && records)
	{
	  if ((quick=get_quick_select(&param,(uint) (best_key-tree->keys),
				      *best_key)))
	  {
	    quick_keys|= 1L << param.real_keynr[(uint) (best_key-tree->keys)];
	    quick->records=records;
	    quick->read_time=read_time;
	  }
	}
      }
    }
    free_root(&param.alloc);			// Return memory & allocator
    my_pthread_setspecific_ptr(THR_MALLOC,old_root);
  }
  current_thd->no_errors=0;
  DBUG_EXECUTE("info",print_quick(quick,needed_reg););
  DBUG_RETURN(records ? test(quick) : -1);
}

	/* make a select tree of all keys in condition */

static SEL_TREE *get_mm_tree(PARAM *param,COND *cond)
{
  SEL_TREE *tree=0;
  DBUG_ENTER("get_mm_tree");

  if (cond->type() == Item::COND_ITEM)
  {
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      tree=0;
      Item *item;
      while ((item=li++))
      {
	SEL_TREE *new_tree=get_mm_tree(param,item);
	tree=tree_and(param,tree,new_tree);
	if (tree && tree->type == SEL_TREE::IMPOSSIBLE)
	  break;
      }
    }
    else
    {						// COND OR
      tree=get_mm_tree(param,li++);
      if (tree)
      {
	Item *item;
	while ((item=li++))
	{
	  SEL_TREE *new_tree=get_mm_tree(param,item);
	  if (!new_tree)
	    DBUG_RETURN(0);
	  tree=tree_or(param,tree,new_tree);
	  if (!tree || tree->type == SEL_TREE::ALWAYS)
	    break;
	}
      }
    }
    DBUG_RETURN(tree);
  }
  /* Here when simple cond */
  if (cond->const_item())
  {
    if (cond->val_int())
      DBUG_RETURN(new SEL_TREE(SEL_TREE::ALWAYS));
    DBUG_RETURN(new SEL_TREE(SEL_TREE::IMPOSSIBLE));
  }
  table_map ref_tables=cond->used_tables();
  if (ref_tables & ~(param->prev_tables | param->read_tables |
		     param->current_table))
    DBUG_RETURN(0);				// Can't be calculated yet
  if (cond->type() != Item::FUNC_ITEM)
  {						// Should be a field
    if (ref_tables & param->current_table)
      DBUG_RETURN(0);
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE));
  }
  if (!(ref_tables & param->current_table))
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE)); // This may be false or true
  Item_func *cond_func= (Item_func*) cond;
  if (!cond_func->select_optimize())
    DBUG_RETURN(0);				// Can't be calculated

  if (cond_func->functype() == Item_func::BETWEEN)
  {
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (cond_func->arguments()[0]))->field;
      Item_result cmp_type=field->cmp_type();
      tree= get_mm_parts(param,field,Item_func::GE_FUNC,
			 cond_func->arguments()[1],cmp_type);
      DBUG_RETURN(tree_and(param,tree,
			   get_mm_parts(param, field,
					Item_func::LE_FUNC,
					cond_func->arguments()[2],cmp_type)));
    }
    DBUG_RETURN(0);
  }
  if (cond_func->functype() == Item_func::IN_FUNC)
  {						// COND OR
    Item_func_in *func=(Item_func_in*) cond_func;
    if (func->key_item()->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (func->key_item()))->field;
      Item_result cmp_type=field->cmp_type();
      tree= get_mm_parts(param,field,Item_func::EQ_FUNC,
			 func->arguments()[0],cmp_type);
      if (!tree)
	DBUG_RETURN(tree);			// Not key field
      for (uint i=1 ; i < func->argument_count(); i++)
      {
	SEL_TREE *new_tree=get_mm_parts(param,field,Item_func::EQ_FUNC,
					func->arguments()[i],cmp_type);
	tree=tree_or(param,tree,new_tree);
      }
      DBUG_RETURN(tree);
    }
    DBUG_RETURN(0);				// Can't optimize this IN
  }


  /* check field op const */
  if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
  {
    tree= get_mm_parts(param,
		       ((Item_field*) (cond_func->arguments()[0]))->field,
		       cond_func->functype(),
		       cond_func->arguments()[1],
		       ((Item_field*) (cond_func->arguments()[0]))->field->
		       cmp_type());
  }
  /* check const op field */
  if (!tree &&
      cond_func->arguments()[1]->type() == Item::FIELD_ITEM &&
      cond_func->functype() != Item_func::LIKE_FUNC)
  {
    DBUG_RETURN(get_mm_parts(param,
			     ((Item_field*)
			      (cond_func->arguments()[1]))->field,
			     ((Item_bool_func2*) cond_func)->rev_functype(),
			     cond_func->arguments()[0],
			     ((Item_field*)
			      (cond_func->arguments()[1]))->field->cmp_type()
			     ));
  }
  DBUG_RETURN(tree);
}


static SEL_TREE *
get_mm_parts(PARAM *param,Field *field, Item_func::Functype type,Item *value,
	     Item_result cmp_type)
{
  DBUG_ENTER("get_mm_parts");
  if (field->table->tablenr != param->current_tablenr)
    DBUG_RETURN(0);

  KEY_PART *key_part = param->key_parts,*end=param->key_parts_end;
  SEL_TREE *tree=0;
  if (value->used_tables() & ~(param->prev_tables | param->read_tables))
    DBUG_RETURN(0);
  for ( ; key_part != end ; key_part++)
  {
    if (field->eq(key_part->field))
    {
      SEL_ARG *sel_arg=0;
      if (!tree)
	tree=new SEL_TREE();
      if (!(value->used_tables() & ~param->read_tables))
      {
	sel_arg=get_mm_leaf(param,key_part->field,key_part,type,value,
			    cmp_type);
	if (!sel_arg)
	  continue;
	if (sel_arg->type == SEL_ARG::IMPOSSIBLE)
	{
	  tree->type=SEL_TREE::IMPOSSIBLE;
	  DBUG_RETURN(tree);
	}
      }
      else
	sel_arg=new SEL_ARG(SEL_ARG::MAYBE_KEY);// This key may be used later
      sel_arg->part=(uchar) key_part->part;
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
    }
  }
  DBUG_RETURN(tree);
}


static SEL_ARG *
get_mm_leaf(PARAM *param,Field *field,KEY_PART *key_part,
	    Item_func::Functype type,Item *value,Item_result cmp_type)
{
  uint field_length=field->pack_length();
  SEL_ARG *tree;
  DBUG_ENTER("get_mm_leaf");

  if (type == Item_func::LIKE_FUNC)
  {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH],*min_str,*max_str;
    String tmp(buff1,sizeof(buff1)),*res;

    if (!(res= value->str(&tmp)))
      DBUG_RETURN(&null_element);

    // Check if this was a function. This should have be optimized away
    // in the sql_select.cc
    if (res != &tmp)
    {
      tmp.copy(*res);				// Get own copy
      res= &tmp;
    }
    if (field->cmp_type() != STRING_RESULT)
      DBUG_RETURN(0);				// Can only optimize strings

    if (!(min_str=sql_alloc(field_length*2)))
      DBUG_RETURN(0);
    max_str=min_str+field_length;

    if (field->binary())
      like_error=like_range(res->ptr(),res->length(),wild_prefix,field_length,
			    min_str,max_str,(char) 255);
    else
#ifndef USE_STRCOLL
      like_error=like_range(res->ptr(),res->length(),wild_prefix,field_length,
			    min_str,max_str,max_sort_char);
#else
    like_error= my_like_range(res->ptr(),res->length(),wild_prefix,
			      field_length, min_str, max_str);
#endif
    if (like_error)				// Can't optimize with LIKE
      DBUG_RETURN(0);
    DBUG_RETURN(new SEL_ARG(field,min_str,max_str));
  }

  if (!field->optimize_range() && type != Item_func::EQ_FUNC)
    DBUG_RETURN(0);				// Can't optimize this

  /* We can't always use indexes when comparing a string index to a number */
  /* cmp_type() is checked to allow compare of dates to numbers */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() != STRING_RESULT &&
      field->cmp_type() != value->result_type())
    DBUG_RETURN(0);

  if (value->save_in_field(field))
    DBUG_RETURN(&null_element);			// NULL is never true

  char *str=sql_alloc(key_part->length);	// Get local copy of key
  if (!str)
    DBUG_RETURN(0);
  field->get_image(str,key_part->length);
  if (!(tree=new SEL_ARG(field,str,str)))
    DBUG_RETURN(0);

  switch (type) {
  case Item_func::LT_FUNC:
    tree->max_flag=NEAR_MAX;
    /* fall through */
  case Item_func::LE_FUNC:
    tree->min_flag=NO_MIN_RANGE;		/* From start */
    break;
  case Item_func::GT_FUNC:
    tree->min_flag=NEAR_MIN;
    /* fall through */
  case Item_func::GE_FUNC:
    tree->max_flag=NO_MAX_RANGE;
    break;
  default:
    break;
  }
  DBUG_RETURN(tree);
}


/*
** Calculate min_str and max_str that ranges a LIKE string.
** Arguments:
** ptr		Pointer to LIKE string.
** ptr_length	Length of LIKE string.
** escape	Escape character in LIKE.  (Normally '\').
**		All escape characters should be removed from min_str and max_str
** res_length	Length of min_str and max_str.
** min_str	Smallest case sensitive string that ranges LIKE.
**		Should be space padded to res_length.
** max_str	Largest case sensitive string that ranges LIKE.
**		Normally padded with the biggest character sort value.
**
** The function should return 0 if ok and 1 if the LIKE string can't be
** optimized !
*/

static bool like_range(const char *ptr,uint ptr_length,char escape,
		       uint res_length, char *min_str,char *max_str,
		       char max_sort_char)
{
  const char *end=ptr+ptr_length;
  char *min_end=min_str+res_length;

  for (; ptr != end && min_str != min_end ; ptr++)
  {
    if (*ptr == escape && ptr+1 != end)
    {
      ptr++;					// Skipp escape
      *min_str++= *max_str++ = *ptr;
      continue;
    }
    if (*ptr == wild_one)			// '_' in SQL
    {
      *min_str++='\0';				// This should be min char
      *max_str++=max_sort_char;
      continue;
    }
    if (*ptr == wild_many)			// '%' in SQL
    {
      do {
	*min_str++ = ' ';			// Because if key compression
	*max_str++ = max_sort_char;
      } while (min_str != min_end);
      return 0;
    }
    *min_str++= *max_str++ = *ptr;
  }
  while (min_str != min_end)
    *min_str++ = *max_str++ = ' ';		// Because if key compression
  return 0;
}


/******************************************************************************
** Tree manipulation functions
** If tree is 0 it means that the condition can't be tested. It refers
** to a non existent table or to a field in current table with isn't a key.
** The different tree flags:
** IMPOSSIBLE:	 Condition is never true
** ALWAYS:	 Condition is always true
** MAYBE:	 Condition may exists when tables are read
** MAYBE_KEY:	 Condition refers to a key that may be used in join loop
** KEY_RANGE:	 Condition uses a key
******************************************************************************/

/*
** Add a new key test to a key when scanning through all keys
** This will never be called for same key parts.
*/

static SEL_ARG *
sel_add(SEL_ARG *key1,SEL_ARG *key2)
{
  SEL_ARG *root,**link;

  if (!key1)
    return key2;
  if (!key2)
    return key1;

  link= &root;
  while (key1 && key2)
  {
    if (key1->part < key2->part)
    {
      *link= key1;
      link= &key1->next_key_part;
      key1=key1->next_key_part;
    }
    else
    {
      *link= key2;
      link= &key2->next_key_part;
      key2=key2->next_key_part;
    }
  }
  *link=key1 ? key1 : key2;
  return root;
}

#define CLONE_KEY1_MAYBE 1
#define CLONE_KEY2_MAYBE 2
#define swap_clone_flag(A) ((A & 1) << 1) | ((A & 2) >> 1)


static SEL_TREE *
tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  if (!tree1)
    return tree2;
  if (!tree2)
    return tree1;
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    return tree1;
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    return tree2;
  if (tree1->type == SEL_TREE::MAYBE)
  {
    if (tree2->type == SEL_TREE::KEY)
      tree2->type=SEL_TREE::KEY_SMALLER;
    return tree2;
  }
  if (tree2->type == SEL_TREE::MAYBE)
  {
    tree1->type=SEL_TREE::KEY_SMALLER;
    return tree1;
  }

  /* Join the trees key per key */
  SEL_ARG **key1,**key2,**end;
  for (key1= tree1->keys,key2= tree2->keys,end=key1+param->keys ;
       key1 != end ; key1++,key2++)
  {
    uint flag=0;
    if (*key1 || *key2)
    {
      if (*key1 && !(*key1)->simple_key())
	flag|=CLONE_KEY1_MAYBE;
      if (*key2 && !(*key2)->simple_key())
	flag|=CLONE_KEY2_MAYBE;
      *key1=key_and(*key1,*key2,flag);
      if ((*key1)->type == SEL_ARG::IMPOSSIBLE)
      {
	tree1->type= SEL_TREE::IMPOSSIBLE;
	break;
      }
#ifdef EXTRA_DEBUG
      (*key1)->test_use_count(*key1);
#endif
    }
  }
  return tree1;
}



static SEL_TREE *
tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  if (!tree1 || !tree2)
    return 0;
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    return tree2;
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    return tree1;
  if (tree1->type == SEL_TREE::MAYBE)
    return tree1;				// Can't use this
  if (tree2->type == SEL_TREE::MAYBE)
    return tree2;

  /* Join the trees key per key */
  SEL_ARG **key1,**key2,**end;
  SEL_TREE *result=0;
  for (key1= tree1->keys,key2= tree2->keys,end=key1+param->keys ;
       key1 != end ; key1++,key2++)
  {
    *key1=key_or(*key1,*key2);
    if (*key1)
    {
      result=tree1;				// Added to tree1
#ifdef EXTRA_DEBUG
      (*key1)->test_use_count(*key1);
#endif
    }
  }
  return result;
}


static SEL_ARG *
and_all_keys(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag)
{
  SEL_ARG *next;
  ulong use_count=key1->use_count;

  if (key1->elements != 1)
  {
    key2->use_count+=key1->elements-1;
    key2->increment_use_count((int) key1->elements-1);
  }
  if (key1->type == SEL_ARG::MAYBE_KEY)
  {
    key1->left= &null_element; key1->next=0;
  }
  for (next=key1->first(); next ; next=next->next)
  {
    if (next->next_key_part)
    {
      SEL_ARG *tmp=key_and(next->next_key_part,key2,clone_flag);
      if (tmp && tmp->type == SEL_ARG::IMPOSSIBLE)
      {
	key1=key1->tree_delete(next);
	continue;
      }
      next->next_key_part=tmp;
      if (use_count)
	next->increment_use_count(use_count);
    }
    else
      next->next_key_part=key2;
  }
  if (!key1)
    return &null_element;			// Impossible ranges
  key1->use_count++;
  return key1;
}



static SEL_ARG *
key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag)
{
  if (!key1)
    return key2;
  if (!key2)
    return key1;
  if (key1->part != key2->part)
  {
    if (key1->part > key2->part)
    {
      swap(SEL_ARG *,key1,key2);
      clone_flag=swap_clone_flag(clone_flag);
    }
    // key1->part < key2->part
    key1->use_count--;
    if (!(clone_flag & CLONE_KEY2_MAYBE) && key1->use_count > 0)
      key1=key1->clone_tree();
    return and_all_keys(key1,key2,clone_flag);
  }

  if (((clone_flag & CLONE_KEY2_MAYBE) &&
       !(clone_flag & CLONE_KEY1_MAYBE)) ||
      key1->type == SEL_ARG::MAYBE_KEY)
  {						// Put simple key in key2
    swap(SEL_ARG *,key1,key2);
    clone_flag=swap_clone_flag(clone_flag);
  }

  // If one of the key is MAYBE_KEY then the found region may be smaller
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    if (!(clone_flag & CLONE_KEY2_MAYBE) && key1->use_count > 1)
    {
      key1->use_count--;
      key1=key1->clone_tree();
      key1->use_count++;
    }
    if (key1->type == SEL_ARG::MAYBE_KEY)
    {						// Both are maybe key
      key1->next_key_part=key_and(key1->next_key_part,key2->next_key_part,
				 clone_flag);
      if (key1->next_key_part &&
	  key1->next_key_part->type == SEL_ARG::IMPOSSIBLE)
	return key1;
    }
    else
    {
      key1->maybe_smaller();
      if (key2->next_key_part)
	return and_all_keys(key1,key2,clone_flag);
      key2->use_count--;			// Key2 doesn't have a tree
    }
    return key1;
  }

  key1->use_count--;
  key2->use_count--;
  SEL_ARG *e1=key1->first(), *e2=key2->first(), *new_tree=0;

  while (e1 && e2)
  {
    int cmp=e1->cmp_min_to_min(e2);
    if (cmp < 0)
    {
      if (get_range(&e1,&e2,key1))
	continue;
    }
    else if (get_range(&e2,&e1,key2))
      continue;
    SEL_ARG *next=key_and(e1->next_key_part,e2->next_key_part,clone_flag);
    e1->increment_use_count(1);
    e2->increment_use_count(1);
    if (!next || next->type != SEL_ARG::IMPOSSIBLE)
    {
      SEL_ARG *new_arg= e1->clone_and(e2);
      new_arg->next_key_part=next;
      if (!new_tree)
      {
	new_tree=new_arg;
      }
      else
	new_tree=new_tree->insert(new_arg);
    }
    if (e1->cmp_max_to_max(e2) < 0)
      e1=e1->next;				// e1 can't overlapp next e2
    else
      e2=e2->next;
  }
  key1->free_tree();
  key2->free_tree();
  if (!new_tree)
    return &null_element;			// Impossible range
  return new_tree;
}


static bool
get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1)
{
  (*e1)=root1->find_range(*e2);			// first e1->min < e2->min
  if ((*e1)->cmp_max_to_min(*e2) < 0)
  {
    if (!((*e1)=(*e1)->next))
      return 1;
    if ((*e1)->cmp_min_to_max(*e2) > 0)
    {
      (*e2)=(*e2)->next;
      return 1;
    }
  }
  return 0;
}


static SEL_ARG *
key_or(SEL_ARG *key1,SEL_ARG *key2)
{
  if (!key1)
  {
    if (key2)
    {
      key2->use_count--;
      key2->free_tree();
    }
    return 0;
  }
  else if (!key2)
  {
    key1->use_count--;
    key1->free_tree();
    return 0;
  }
  key1->use_count--;
  key2->use_count--;

  if (key1->part != key2->part)
  {
    key1->free_tree();
    key2->free_tree();
    return 0;					// Can't optimize this
  }

  // If one of the key is MAYBE_KEY then the found region may be bigger
  if (key1->type == SEL_ARG::MAYBE_KEY)
  {
    key2->free_tree();
    key1->use_count++;
    return key1;
  }
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    key1->free_tree();
    key2->use_count++;
    return key2;
  }

  if (key1->use_count > 0)
  {
    if (key2->use_count == 0 || key1->elements > key2->elements)
    {
      swap(SEL_ARG *,key1,key2);
    }
    else
      key1=key1->clone_tree();
  }

  // Add tree at key2 to tree at key1
  bool key2_shared=key2->use_count != 0;
  key1->maybe_flag|=key2->maybe_flag;

  for (key2=key2->first(); key2; )
  {
    SEL_ARG *tmp=key1->find_range(key2);	// Find key1.min <= key2.min
    int cmp;

    if (!tmp)
    {
      tmp=key1->first();			// tmp.min > key2.min
      cmp= -1;
    }
    else if ((cmp=tmp->cmp_max_to_min(key2)) < 0)
    {						// Found tmp.max < key2.min
      SEL_ARG *next=tmp->next;
      if (cmp == -2 && eq_tree(tmp->next_key_part,key2->next_key_part))
      {
	// Join near ranges like tmp.max < 0 and key2.min >= 0
	SEL_ARG *key2_next=key2->next;
	if (key2_shared)
	{
	  key2=new SEL_ARG(*key2);
	  key2->increment_use_count(key1->use_count+1);
	  key2->next=key2_next;			// New copy of key2
	}
	key2->copy_min(tmp);
	if (!(key1=key1->tree_delete(tmp)))
	{					// Only one key in tree
	  key1=key2;
	  key1->make_root();
	  key2=key2_next;
	  break;
	}
      }
      if (!(tmp=next))				// tmp.min > key2.min
	break;					// Copy rest of key2
    }
    if (cmp < 0)
    {						// tmp.min > key2.min
      int tmp_cmp;
      if ((tmp_cmp=tmp->cmp_min_to_max(key2)) > 0) // if tmp.min > key2.max
      {
	if (tmp_cmp == 2 && eq_tree(tmp->next_key_part,key2->next_key_part))
	{					// ranges are connected
	  tmp->copy_min_to_min(key2);
	  key1->merge_flags(key2);
	  if (tmp->min_flag & NO_MIN_RANGE &&
	      tmp->max_flag & NO_MAX_RANGE)
	  {
	    if (key1->maybe_flag)
	      return new SEL_ARG(SEL_ARG::MAYBE_KEY);
	    return 0;
	  }
	  key2->increment_use_count(-1);	// Free not used tree
	  key2=key2->next;
	  continue;
	}
	else
	{
	  SEL_ARG *next=key2->next;		// Keys are not overlapping
	  if (key2_shared)
	  {
	    key1=key1->insert(new SEL_ARG(*key2)); // Must make copy
	    key2->increment_use_count(key1->use_count+1);
	  }
	  else
	    key1=key1->insert(key2);		// Will destroy key2_root
	  key2=next;
	  continue;
	}
      }
    }

    // tmp.max >= key2.min && tmp.min <= key.max  (overlapping ranges)
    if (eq_tree(tmp->next_key_part,key2->next_key_part))
    {
      if (tmp->is_same(key2))
      {
	tmp->merge_flags(key2);			// Copy maybe flags
	key2->increment_use_count(-1);		// Free not used tree
      }
      else
      {
	SEL_ARG *last=tmp;
	while (last->next && last->next->cmp_min_to_max(key2) <= 0 &&
	       eq_tree(last->next->next_key_part,key2->next_key_part))
	{
	  SEL_ARG *save=last;
	  last=last->next;
	  key1=key1->tree_delete(save);
	}
	if (last->copy_min(key2) || last->copy_max(key2))
	{					// Full range
	  key1->free_tree();
	  for (; key2 ; key2=key2->next)
	    key2->increment_use_count(-1);	// Free not used tree
	  if (key1->maybe_flag)
	    return new SEL_ARG(SEL_ARG::MAYBE_KEY);
	  return 0;
	}
      }
      key2=key2->next;
      continue;
    }

    if (cmp >= 0 && tmp->cmp_min_to_min(key2) < 0)
    {						// tmp.min <= x < key2.min
      SEL_ARG *new_arg=tmp->clone_first(key2);
      if (new_arg->next_key_part= key1->next_key_part)
	new_arg->increment_use_count(key1->use_count+1);
      tmp->copy_min_to_min(key2);
      key1=key1->insert(new_arg);
    }

    // tmp.min >= key2.min && tmp.min <= key2.max
    SEL_ARG key(*key2);				// Get copy we can modify
    for (;;)
    {
      if (tmp->cmp_min_to_min(&key) > 0)
      {						// key.min <= x < tmp.min
	SEL_ARG *new_arg=key.clone_first(tmp);
	if ((new_arg->next_key_part=key.next_key_part))
	  new_arg->increment_use_count(key1->use_count+1);
	key1=key1->insert(new_arg);
      }
      if ((cmp=tmp->cmp_max_to_max(&key)) <= 0)
      {						// tmp.min. <= x <= tmp.max
	tmp->maybe_flag|= key.maybe_flag;
	key.increment_use_count(key1->use_count+1);
	tmp->next_key_part=key_or(tmp->next_key_part,key.next_key_part);
	if (!cmp)				// Key2 is ready
	  break;
	key.copy_max_to_min(tmp);
	if (!(tmp=tmp->next))
	{
	  key1=key1->insert(new SEL_ARG(key));
	  key2=key2->next;
	  goto end;
	}
	if (tmp->cmp_min_to_max(&key) > 0)
	{
	  key1=key1->insert(new SEL_ARG(key));
	  break;
	}
      }
      else
      {
	SEL_ARG *new_arg=tmp->clone_last(&key); // tmp.min <= x <= key.max
	tmp->copy_max_to_min(&key);
	tmp->increment_use_count(key1->use_count+1);
	new_arg->next_key_part=key_or(tmp->next_key_part,key.next_key_part);
	key1=key1->insert(new_arg);
	break;
      }
    }
    key2=key2->next;
  }

end:
  while (key2)
  {
    SEL_ARG *next=key2->next;
    if (key2_shared)
    {
      key2->increment_use_count(key1->use_count+1);
      key1=key1->insert(new SEL_ARG(*key2));	// Must make copy
    }
    else
      key1=key1->insert(key2);			// Will destroy key2_root
    key2=next;
  }
  key1->use_count++;
  return key1;
}


/* Compare if two trees are equal */

static bool eq_tree(SEL_ARG* a,SEL_ARG *b)
{
  if (a == b)
    return 1;
  if (!a || !b || !a->is_same(b))
    return 0;
  if (a->left != &null_element && b->left != &null_element)
  {
    if (!eq_tree(a->left,b->left))
      return 0;
  }
  else if (a->left != &null_element || b->left != &null_element)
    return 0;
  if (a->right != &null_element && b->right != &null_element)
  {
    if (!eq_tree(a->right,b->right))
      return 0;
  }
  else if (a->right != &null_element || b->right != &null_element)
    return 0;
  if (a->next_key_part != b->next_key_part)
  {						// Sub range
    if (!a->next_key_part != !b->next_key_part ||
	!eq_tree(a->next_key_part, b->next_key_part))
      return 0;
  }
  return 1;
}


SEL_ARG *
SEL_ARG::insert(SEL_ARG *key)
{
  SEL_ARG *element,**par,*last_element;

  LINT_INIT(par); LINT_INIT(last_element);
  for (element= this; element != &null_element ; )
  {
    last_element=element;
    if (key->cmp_min_to_min(element) > 0)
    {
      par= &element->right; element= element->right;
    }
    else
    {
      par = &element->left; element= element->left;
    }
  }
  *par=key;
  key->parent=last_element;
	/* Link in list */
  if (par == &last_element->left)
  {
    key->next=last_element;
    if (key->prev=last_element->prev)
      key->prev->next=key;
    last_element->prev=key;
  }
  else
  {
    if ((key->next=last_element->next))
      key->next->prev=key;
    key->prev=last_element;
    last_element->next=key;
  }
  key->left=key->right= &null_element;
  SEL_ARG *root=rb_insert(key);			// rebalance tree
  root->use_count=this->use_count;		// copy root info
  root->elements= this->elements+1;
  root->maybe_flag=this->maybe_flag;
  return root;
}


/*
** Find best key with min <= given key
** Because the call context this should never return 0 to get_range
*/

SEL_ARG *
SEL_ARG::find_range(SEL_ARG *key)
{
  SEL_ARG *element=this,*found=0;

  for (;;)
  {
    if (element == &null_element)
      return found;
    int cmp=element->cmp_min_to_min(key);
    if (cmp == 0)
      return element;
    if (cmp < 0)
    {
      found=element;
      element=element->right;
    }
    else
      element=element->left;
  }
}


/*
** Remove a element from the tree
** This also frees all sub trees that is used by the element
*/

SEL_ARG *
SEL_ARG::tree_delete(SEL_ARG *key)
{
  enum leaf_color remove_color;
  SEL_ARG *root,*nod,**par,*fix_par;
  root=this; this->parent= 0;

  /* Unlink from list */
  if (key->prev)
    key->prev->next=key->next;
  if (key->next)
    key->next->prev=key->prev;
  key->increment_use_count(-1);
  if (!key->parent)
    par= &root;
  else
    par=key->parent_ptr();

  if (key->left == &null_element)
  {
    *par=nod=key->right;
    fix_par=key->parent;
    if (nod != &null_element)
      nod->parent=fix_par;
    remove_color= key->color;
  }
  else if (key->right == &null_element)
  {
    *par= nod=key->left;
    nod->parent=fix_par=key->parent;
    remove_color= key->color;
  }
  else
  {
    SEL_ARG *tmp=key->next;			// next bigger key (exist!)
    nod= *tmp->parent_ptr()= tmp->right;	// unlink tmp from tree
    fix_par=tmp->parent;
    if (nod != &null_element)
      nod->parent=fix_par;
    remove_color= tmp->color;

    tmp->parent=key->parent;			// Move node in place of key
    (tmp->left=key->left)->parent=tmp;
    if ((tmp->right=key->right) != &null_element)
      tmp->right->parent=tmp;
    tmp->color=key->color;
    *par=tmp;
    if (fix_par == key)				// key->right == key->next
      fix_par=tmp;				// new parent of nod
  }

  if (root == &null_element)
    return 0;					// Maybe root later
  if (remove_color == BLACK)
    root=rb_delete_fixup(root,nod,fix_par);
  test_rb_tree(root,root->parent);

  root->use_count=this->use_count;		// Fix root counters
  root->elements=this->elements-1;
  root->maybe_flag=this->maybe_flag;
  return root;
}


	/* Functions to fix up the tree after insert and delete */

static void left_rotate(SEL_ARG **root,SEL_ARG *leaf)
{
  SEL_ARG *y=leaf->right;
  leaf->right=y->left;
  if (y->left != &null_element)
    y->left->parent=leaf;
  if (!(y->parent=leaf->parent))
    *root=y;
  else
    *leaf->parent_ptr()=y;
  y->left=leaf;
  leaf->parent=y;
}

static void right_rotate(SEL_ARG **root,SEL_ARG *leaf)
{
  SEL_ARG *y=leaf->left;
  leaf->left=y->right;
  if (y->right != &null_element)
    y->right->parent=leaf;
  if (!(y->parent=leaf->parent))
    *root=y;
  else
    *leaf->parent_ptr()=y;
  y->right=leaf;
  leaf->parent=y;
}


SEL_ARG *
SEL_ARG::rb_insert(SEL_ARG *leaf)
{
  SEL_ARG *y,*par,*par2,*root;
  root= this; root->parent= 0;

  leaf->color=RED;
  while (leaf != root && (par= leaf->parent)->color == RED)
  {					// This can't be root or 1 level under
    if (par == (par2= leaf->parent->parent)->left)
    {
      y= par2->right;
      if (y->color == RED)
      {
	par->color=BLACK;
	y->color=BLACK;
	leaf=par2;
	leaf->color=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->right)
	{
	  left_rotate(&root,leaf->parent);
	  par=leaf;			/* leaf is now parent to old leaf */
	}
	par->color=BLACK;
	par2->color=RED;
	right_rotate(&root,par2);
	break;
      }
    }
    else
    {
      y= par2->left;
      if (y->color == RED)
      {
	par->color=BLACK;
	y->color=BLACK;
	leaf=par2;
	leaf->color=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->left)
	{
	  right_rotate(&root,par);
	  par=leaf;
	}
	par->color=BLACK;
	par2->color=RED;
	left_rotate(&root,par2);
	break;
      }
    }
  }
  root->color=BLACK;
  test_rb_tree(root,root->parent);
  return root;
}


SEL_ARG *rb_delete_fixup(SEL_ARG *root,SEL_ARG *key,SEL_ARG *par)
{
  SEL_ARG *x,*w;
  root->parent=0;

  x= key;
  while (x != root && x->color == SEL_ARG::BLACK)
  {
    if (x == par->left)
    {
      w=par->right;
      if (w->color == SEL_ARG::RED)
      {
	w->color=SEL_ARG::BLACK;
	par->color=SEL_ARG::RED;
	left_rotate(&root,par);
	w=par->right;
      }
      if (w->left->color == SEL_ARG::BLACK && w->right->color == SEL_ARG::BLACK)
      {
	w->color=SEL_ARG::RED;
	x=par;
      }
      else
      {
	if (w->right->color == SEL_ARG::BLACK)
	{
	  w->left->color=SEL_ARG::BLACK;
	  w->color=SEL_ARG::RED;
	  right_rotate(&root,w);
	  w=par->right;
	}
	w->color=par->color;
	par->color=SEL_ARG::BLACK;
	w->right->color=SEL_ARG::BLACK;
	left_rotate(&root,par);
	x=root;
	break;
      }
    }
    else
    {
      w=par->left;
      if (w->color == SEL_ARG::RED)
      {
	w->color=SEL_ARG::BLACK;
	par->color=SEL_ARG::RED;
	right_rotate(&root,par);
	w=par->left;
      }
      if (w->right->color == SEL_ARG::BLACK && w->left->color == SEL_ARG::BLACK)
      {
	w->color=SEL_ARG::RED;
	x=par;
      }
      else
      {
	if (w->left->color == SEL_ARG::BLACK)
	{
	  w->right->color=SEL_ARG::BLACK;
	  w->color=SEL_ARG::RED;
	  left_rotate(&root,w);
	  w=par->left;
	}
	w->color=par->color;
	par->color=SEL_ARG::BLACK;
	w->left->color=SEL_ARG::BLACK;
	right_rotate(&root,par);
	x=root;
	break;
      }
    }
    par=x->parent;
  }
  x->color=SEL_ARG::BLACK;
  return root;
}


	/* Test that the proporties for a red-black tree holds */

#ifdef EXTRA_DEBUG
int test_rb_tree(SEL_ARG *element,SEL_ARG *parent)
{
  int count_l,count_r;

  if (element == &null_element)
    return 0;					// Found end of tree
  if (element->parent != parent)
  {
    sql_print_error("Wrong tree: Parent doesn't point at parent");
    return -1;
  }
  if (element->color == SEL_ARG::RED &&
      (element->left->color == SEL_ARG::RED ||
       element->right->color == SEL_ARG::RED))
  {
    sql_print_error("Wrong tree: Found two red in a row");
    return -1;
  }
  if (element->left == element->right && element->left != &null_element)
  {						// Dummy test
    sql_print_error("Wrong tree: Found right == left");
    return -1;
  }
  count_l=test_rb_tree(element->left,element);
  count_r=test_rb_tree(element->right,element);
  if (count_l >= 0 && count_r >= 0)
  {
    if (count_l == count_r)
      return count_l+(element->color == SEL_ARG::BLACK);
    sql_print_error("Wrong tree: Incorrect black-count: %d - %d",
	    count_l,count_r);
  }
  return -1;					// Error, no more warnings
}

static ulong count_key_part_usage(SEL_ARG *root, SEL_ARG *key)
{
  ulong count= 0;
  for (root=root->first(); root ; root=root->next)
  {
    if (root->next_key_part)
    {
      if (root->next_key_part == key)
	count++;
      if (root->next_key_part->part < key->part)
	count+=count_key_part_usage(root->next_key_part,key);
    }
  }
  return count;
}


void SEL_ARG::test_use_count(SEL_ARG *root)
{
  if (this == root && use_count != 1)
  {
    sql_print_error("Use_count: Wrong count %lu for root",use_count);
    return;
  }
  if (this->type != SEL_ARG::KEY_RANGE)
    return;
  uint e_count=0;
  for (SEL_ARG *pos=first(); pos ; pos=pos->next)
  {
    e_count++;
    if (pos->next_key_part)
    {
      ulong count=count_key_part_usage(root,pos->next_key_part);
      if (count > pos->next_key_part->use_count)
      {
	sql_print_error("Use_count: Wrong count for key at %lx, %lu should be %lu",
			pos,pos->next_key_part->use_count,count);
	return;
      }
      pos->next_key_part->test_use_count(root);
    }
  }
  if (e_count != elements)
    sql_print_error("Wrong use count: %u for tree at %lx", e_count,
		    (gptr) this);
}

#endif



/*****************************************************************************
** Check how many records we will find by using the found tree
*****************************************************************************/

static ha_rows
check_quick_select(PARAM *param,uint index,SEL_ARG *tree)
{
  ha_rows records;
  DBUG_ENTER("check_quick_select");

  if (!tree)
    DBUG_RETURN(HA_POS_ERROR);			// Can't use it
  if (tree->type == SEL_ARG::IMPOSSIBLE)
    DBUG_RETURN(0L);				// Impossible select. return
  if (tree->type != SEL_ARG::KEY_RANGE || tree->part != 0)
    DBUG_RETURN(HA_POS_ERROR);				// Don't use tree
  records=check_quick_keys(param,index,tree,param->min_key,0,param->max_key,0);
  if (records != HA_POS_ERROR)
  {
    uint key=param->real_keynr[index];
    param->table->quick_keys|= (key_map) 1L << key;
    param->table->quick_rows[key]=records;
  }
  DBUG_RETURN(records);
}


static ha_rows
check_quick_keys(PARAM *param,uint index,SEL_ARG *key_tree,
		 char *min_key,uint min_key_flag, char *max_key,
		 uint max_key_flag)
{
  ha_rows records=0,tmp;
  if (key_tree->left != &null_element)
  {
    records=check_quick_keys(param,index,key_tree->left,min_key,min_key_flag,
			     max_key,max_key_flag);
    if (records == HA_POS_ERROR)			// Impossible
      return records;
  }
  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      !key_tree->min_flag &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {						// const key as prefix
    char *tmp_min_key=min_key,*tmp_max_key=max_key;
    key_tree->store(param->key[index][key_tree->part].length,
		    &tmp_min_key,min_key_flag,&tmp_max_key,max_key_flag);
    tmp=check_quick_keys(param,index,key_tree->next_key_part,
			 tmp_min_key, min_key_flag | key_tree->min_flag,
			 tmp_max_key, max_key_flag | key_tree->max_flag);
  }
  else
  {
    char *tmp_min_key=min_key,*tmp_max_key=max_key;
    key_tree->store(param->key[index][key_tree->part].length,
		    &tmp_min_key,min_key_flag, &tmp_max_key,max_key_flag);

    uint keynr=param->real_keynr[index];
    uint min_key_length= (uint) (tmp_min_key- param->min_key);
    uint max_key_length= (uint) (tmp_max_key- param->max_key);
    if (!key_tree->min_flag && ! key_tree->max_flag &&
	! min_key_flag && ! max_key_flag &&
	(uint) key_tree->part+1 == param->table->key_info[keynr].key_parts &&
	!param->table->key_info[keynr].dupp_key &&
	min_key_length == (uint) (tmp_max_key- param->max_key) &&
	!memcmp(param->min_key,param->max_key,min_key_length))
      tmp=1;					// Max one record
    else
      tmp=ni_records_in_range((N_INFO*) param->table->file,
			      (int) keynr,
			      (byte*) (!min_key_length ? NullS :
				       param->min_key),
			      min_key_length,
			      ((key_tree->min_flag | min_key_flag) & NEAR_MIN ?
			       HA_READ_AFTER_KEY : HA_READ_KEY_EXACT),
			      (byte*) (!max_key_length ? NullS :
				       param->max_key),
			      max_key_length,
			      ((key_tree->max_flag | max_key_flag) & NEAR_MAX ?
			       HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY));
  }
  if (tmp == HA_POS_ERROR)			// Impossible
    return tmp;
  records+=tmp;
  if (key_tree->right != &null_element)
  {
    tmp=check_quick_keys(param,index,key_tree->right,min_key,min_key_flag,
			 max_key,max_key_flag);
    if (tmp == HA_POS_ERROR)
      return tmp;
    records+=tmp;
  }
  return records;
}


/****************************************************************************
** change a tree to a structure to be used by quick_select
** This uses it's own malloc tree
****************************************************************************/

static QUICK_SELECT *
get_quick_select(PARAM *param,uint index,SEL_ARG *key_tree)
{
  QUICK_SELECT *quick;
  DBUG_ENTER("get_quick_select");
  if ((quick=new QUICK_SELECT(param->table,param->real_keynr[index])))
  {
    if (get_quick_keys(param,quick,index,key_tree,param->min_key,0,
		       param->max_key,0))
    {
      delete quick;
      quick=0;
    }
    else
    {
      quick->key_parts=(KEY_PART*)
	sql_memdup(param->key[index],
		   sizeof(KEY_PART)*
		   param->table->key_info[param->real_keynr[index]].key_parts);
    }
  }
  DBUG_RETURN(quick);
}


/*
** Fix this to get all possible sub_ranges
*/

static bool
get_quick_keys(PARAM *param,QUICK_SELECT *quick,uint index,
	       SEL_ARG *key_tree,char *min_key,uint min_key_flag,
	       char *max_key, uint max_key_flag)
{
  if (key_tree->left != &null_element)
  {
    if (get_quick_keys(param,quick,index,key_tree->left,
		       min_key,min_key_flag, max_key, max_key_flag))
      return 1;
  }
  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      !key_tree->min_flag &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {						  // const key as prefix
    char *tmp_min_key=min_key,*tmp_max_key=max_key;
    key_tree->store(param->key[index][key_tree->part].length,
		    &tmp_min_key,min_key_flag,&tmp_max_key,max_key_flag);
    if (get_quick_keys(param,quick,index,key_tree->next_key_part,
		       tmp_min_key,min_key_flag | key_tree->min_flag,
			 tmp_max_key, max_key_flag | key_tree->max_flag))
      return 1;
  }
  else
  {
    char *tmp_min_key=min_key,*tmp_max_key=max_key;
    key_tree->store(param->key[index][key_tree->part].length,
		    &tmp_min_key,min_key_flag, &tmp_max_key, max_key_flag);
    uint flag=key_tree->min_flag | key_tree->max_flag;
    if (tmp_min_key != param->min_key)
      flag&= ~NO_MIN_RANGE;
    else
      flag|= NO_MIN_RANGE;
    if (tmp_max_key != param->max_key)
      flag&= ~NO_MAX_RANGE;
    else
      flag|= NO_MAX_RANGE;
    QUICK_RANGE *range= new QUICK_RANGE(param->min_key,
					(uint) (tmp_min_key - param->min_key),
					param->max_key,
					(uint) (tmp_max_key - param->max_key),
					flag);
    if (!range)					// Not enough memory
      return 1;
    quick->ranges.push_back(range);
  }
  if (key_tree->right != &null_element)
    return get_quick_keys(param,quick,index,key_tree->right,
			  min_key,min_key_flag,
			  max_key,max_key_flag);
  return 0;
}


	/* get next possible record using quick-struct */

int QUICK_SELECT::get_next()
{
  QUICK_RANGE *range;
  DBUG_ENTER("get_next");

  for (;;)
  {
    if (next == 1)
    {						// Already read through key
      if (ha_rnext(head,head->record[0],index) == 0 &&
	  cmp_next(*it.ref()) == 0)
	DBUG_RETURN(0);
    }
    if (!(range=it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used
    next=1;
    if (range->flag & NO_MIN_RANGE)	// Read first record
    {
      int error;
      if ((error=ha_rfirst(head,head->record[0],index)))
	DBUG_RETURN(error);			// Empty table
      if (cmp_next(range) == 0)
	DBUG_RETURN(0);				// No matching records
      next=0;					// To next range
      continue;
    }
    if (ha_rkey(head,head->record[0],index,(byte*) range->min_key,
		range->min_length,
		(!(range->flag & NEAR_MIN) ? HA_READ_KEY_OR_NEXT :
		 HA_READ_AFTER_KEY)))
      continue;
    if (cmp_next(range) == 0)
      DBUG_RETURN(0);				// Found key is in range
    next=0;					// To next range
  }
}

	/* compare if found key is over max-value */
	/* Returns 0 if key <= range->max_key */

int QUICK_SELECT::cmp_next(QUICK_RANGE *range)
{
  if (range->flag & NO_MAX_RANGE)
    return (0);					/* key can't be to large */

  KEY_PART *key_part=key_parts;
  for (char *key=range->max_key, *end=key+range->max_length;
       key < end;
       key+= key_part++->length)
  {
    int cmp;
    if ((cmp=key_part->field->cmp(key)) < 0)
      return 0;
    if (cmp > 0)
      return 1;
  }
  return (range->flag & NEAR_MAX) ? 1 : 0;		// Exact match
}


/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

#ifndef DBUG_OFF

static void
print_key(KEY_PART *key_part,const char *key,uint used_length)
{
  char buff[1024];
  String tmp(buff,sizeof(buff));

  for (uint length=0; length < used_length ; length+=key_part++->length)
  {
    Field *field=key_part->field;
    field->set_image((char*) key,key_part->length);
    field->val_str(&tmp,&tmp);
    if (length != 0)
      fputc('/',DBUG_FILE);
    fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE);
    key+=key_part->length;
  }
}

extern "C" {
extern pthread_mutex_t THR_LOCK_dbug;
}

static void print_quick(QUICK_SELECT *quick,table_map needed_reg)
{
  QUICK_RANGE *range;
  DBUG_ENTER("print_param");
  if (! _db_on_ || !quick)
    DBUG_VOID_RETURN;

  List_iterator<QUICK_RANGE> li(quick->ranges);
  pthread_mutex_lock(&THR_LOCK_dbug);
  fprintf(DBUG_FILE,"Used quck_range on key: %d (other_keys: %lu):\n",
	  quick->index, needed_reg);
  while ((range=li++))
  {
    if (!(range->flag & NO_MIN_RANGE))
    {
      print_key(quick->key_parts,range->min_key,range->min_length);
      if (range->flag & NEAR_MIN)
	fputs(" < ",DBUG_FILE);
      else
	fputs(" <= ",DBUG_FILE);
    }
    fputs("X",DBUG_FILE);

    if (!(range->flag & NO_MAX_RANGE))
    {
      if (range->flag & NEAR_MAX)
	fputs(" < ",DBUG_FILE);
      else
	fputs(" <= ",DBUG_FILE);
      print_key(quick->key_parts,range->max_key,range->max_length);
    }
    fputs("\n",DBUG_FILE);
  }
  pthread_mutex_unlock(&THR_LOCK_dbug);
  DBUG_VOID_RETURN;
}

#endif

/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<QUICK_RANGE>;
template class List_iterator<QUICK_RANGE>;
#endif
