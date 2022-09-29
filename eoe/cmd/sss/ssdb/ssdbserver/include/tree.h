/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

#ifndef _tree_h
#define _tree_h
#ifdef	__cplusplus
extern "C" {
#endif

#define MAX_TREE_HIGHT	40	/* = max 1048576 leafs in tree */
#define ELEMENT_KEY(tree,element)\
(tree->offset_to_key ? (void*)((byte*) element+tree->offset_to_key) :\
			*((void**) (element+1)))

#define tree_set_pointer(element,ptr) *((byte **) (element+1))=((byte*) (ptr))

typedef enum { left_root_right, right_root_left } TREE_WALK;
typedef uint32 element_count;
typedef int (*tree_walk_action)(void *,element_count,void *);

#ifdef MSDOS
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  unsigned long count;
  uchar    colour;			/* black is marked as 1 */
} TREE_ELEMENT;
#else
typedef struct st_tree_element {
  struct st_tree_element *left,*right;
  uint32 count:31,
	 colour:1;			/* black is marked as 1 */
} TREE_ELEMENT;
#endif /* MSDOS */

typedef struct st_tree {
  TREE_ELEMENT *root,null_element;
  TREE_ELEMENT **parents[MAX_TREE_HIGHT];
  uint offset_to_key,elements_in_tree,size_of_element;
  qsort_cmp compare;
  void (*free)(void *);
} TREE;

	/* Functions on hole tree */
void init_tree(TREE *tree,int size,qsort_cmp compare,
	       void (*free_element)(void*));
void delete_tree(TREE*);

	/* Functions on leafs */
TREE_ELEMENT *tree_insert(TREE *tree,void *key,uint key_size);
void *tree_search(TREE *tree,void *key);
int tree_walk(TREE *tree,tree_walk_action action,
	      void *argument, TREE_WALK visit);
int tree_delete(TREE *tree,void *key);

#ifdef	__cplusplus
}
#endif
#endif
