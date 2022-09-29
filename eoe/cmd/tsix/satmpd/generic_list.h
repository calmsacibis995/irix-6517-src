#ifndef GENERIC_LIST_HEADER
#define GENERIC_LIST_HEADER

#ident "$Revision: 1.2 $"

#include "sem.h"

typedef int  (*gl_lt_func) (void *, void *);
typedef int  (*gl_cmp_func) (const void *, const void *);
typedef void (*gl_do_func) (void *, void *);

typedef struct GLE_struct {
	void *pointer;
	struct GLE_struct *previous;
	struct GLE_struct *next;
} Generic_list_element;

typedef struct {
	Generic_list_element pre_element, post_element;
	gl_lt_func lt;
	unsigned int num_of_elements;
} Generic_list_info;

typedef struct Generic_list {
	mrlock mr;
	Generic_list_info *info;
} Generic_list;

#define Generic_stack Generic_list
#define Generic_queue Generic_list

void initialize_list (Generic_list *);
void initialize_sorted_list (Generic_list *, gl_lt_func);
void destroy_list(Generic_list *);
void add_to_beginning(Generic_list *, void *);
void add_to_end(Generic_list *, void *);
void add_to_list(Generic_list *, void *);
void *remove_from_beginning(Generic_list *);
void *remove_from_end(Generic_list *);
void *remove_from_list(Generic_list *, void *);
void remove_all(Generic_list *);
void *peek_at_beginning(Generic_list *);
void *peek_at_end(Generic_list *);

void *first_in_list(Generic_list *);
void *last_in_list(Generic_list *);

int num_of_objects(Generic_list *);
int is_empty(Generic_list *);
int is_in_list(Generic_list *, const void *);

void perform_on_list (Generic_list *, gl_do_func, void *);
void *first_that (Generic_list *, gl_cmp_func, const void *);
void *last_that (Generic_list *, gl_cmp_func, const void *);


/****************************************************************************/

/* Stack operations */

#define initialize_stack initialize_list
#define destroy_stack destroy_list
#define push add_to_beginning
#define pop remove_from_beginning
#define peek_at_top peek_at_beginning
#define copy_stack copy_list

/* Queue operations */

#define initialize_queue initialize_list
#define destroy_queue destroy_list
#define enqueue add_to_end
#define dequeue remove_from_beginning
#define dequeue_all remove_all
#define peek_at_head peek_at_beginning
#define peek_at_tail peek_at_end
#define copy_queue copy_list

#endif /* GENERIC_LIST_HEADER */
