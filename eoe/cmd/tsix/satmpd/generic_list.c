#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <stdlib.h>
#include "generic_list.h"
#include "ref.h"

#define NEW(x) ((x *) emalloc(sizeof(x)))

/****************************************************************************/
/****************************************************************************/
/**                                                                        **/
/**                         Internal functions                             **/
/**                                                                        **/
/****************************************************************************/
/****************************************************************************/

static void *
emalloc (size_t n)
{
    void *ptr;

    ptr = (void *) malloc (n);
    if (ptr == NULL)
	exit (EXIT_FAILURE);
    return ptr;
}

/****************************************************************************/
/****************************************************************************/
/**                                                                        **/
/**                         External functions                             **/
/**                                                                        **/
/****************************************************************************/
/****************************************************************************/

void
initialize_list (Generic_list *list)
{
    list->info = NEW (Generic_list_info);

    list->info->pre_element.pointer = NULL;
    list->info->pre_element.previous = &list->info->pre_element;
    list->info->pre_element.next = &list->info->post_element;
    list->info->post_element.pointer = NULL;
    list->info->post_element.previous = &list->info->pre_element;
    list->info->post_element.next = &list->info->post_element;

    list->info->lt = NULL;
    list->info->num_of_elements = 0;
    (void) mrlock_initialize (&list->mr);
}

/****************************************************************************/

void
initialize_sorted_list (Generic_list *list, gl_lt_func lt)
{
    initialize_list (list);
    list->info->lt = lt;
}

/****************************************************************************/

void
destroy_list (Generic_list *list)
{
    remove_all (list);
    free ((void *) list->info);
    (void) mrlock_destroy (&list->mr);
}

/****************************************************************************/

void
add_to_beginning (Generic_list *list, void *pointer)
{
    Generic_list_element *element;

    if (!pointer)
	return;

    incref (pointer);
    element = NEW (Generic_list_element);
    (void) mrlock_wrlock (&list->mr);
    element->next = list->info->pre_element.next;
    element->previous = &list->info->pre_element;
    element->pointer = pointer;

    list->info->pre_element.next->previous = element;
    list->info->pre_element.next = element;

    list->info->num_of_elements++;
    mrlock_unlock (&list->mr);
}

/****************************************************************************/

void
add_to_end (Generic_list *list, void *pointer)
{
    Generic_list_element *element;

    if (!pointer)
	return;

    incref (pointer);
    element = NEW (Generic_list_element);
    (void) mrlock_wrlock (&list->mr);
    element->next = &list->info->post_element;
    element->previous = list->info->post_element.previous;
    element->pointer = pointer;

    list->info->post_element.previous->next = element;
    list->info->post_element.previous = element;

    list->info->num_of_elements++;
    mrlock_unlock (&list->mr);
}

/****************************************************************************/

void
add_to_list (Generic_list *list, void *pointer)
{
    Generic_list_element *element, *new_element;

    (void) mrlock_wrlock (&list->mr);
    if (list->info->lt)
    {
	if (!pointer)
	{
	    mrlock_unlock (&list->mr);
	    return;
	}

	incref (pointer);
	element = list->info->pre_element.next;
	while (element != &list->info->post_element &&
	       (*list->info->lt) (element->pointer, pointer))
	    element = element->next;

	new_element = NEW (Generic_list_element);
	new_element->next = element;
	new_element->previous = element->previous;
	new_element->pointer = pointer;

	element->previous->next = new_element;
	element->previous = new_element;

	list->info->num_of_elements++;
	mrlock_unlock (&list->mr);
    }
    else
    {
	mrlock_unlock (&list->mr);
	add_to_end (list, pointer);
    }
}

/****************************************************************************/

void *
remove_from_list (Generic_list *list, void *pointer)
{
    Generic_list_element *element;

    (void) mrlock_wrlock (&list->mr);
    element = list->info->post_element.previous;

    while (element != &list->info->pre_element && element->pointer != pointer)
	element = element->previous;

    if (element == &list->info->pre_element)
    {
	/* No such element was found. */
	mrlock_unlock (&list->mr);
	return NULL;
    }

    element->previous->next = element->next;
    element->next->previous = element->previous;

    free (element);
    list->info->num_of_elements--;

    mrlock_unlock (&list->mr);

    return pointer;
}

/****************************************************************************/

void *
remove_from_beginning (Generic_list *list)
{
    Generic_list_element *element;
    void *pointer;

    (void) mrlock_wrlock (&list->mr);
    if (list->info->num_of_elements == 0)
    {
	mrlock_unlock (&list->mr);
	return NULL;
    }

    element = list->info->pre_element.next;

    pointer = element->pointer;
    list->info->pre_element.next = element->next;
    element->next->previous = &list->info->pre_element;

    free (element);
    list->info->num_of_elements--;

    mrlock_unlock (&list->mr);

    return pointer;
}

/****************************************************************************/

void *
remove_from_end (Generic_list *list)
{
    Generic_list_element *element;
    void *pointer;

    (void) mrlock_wrlock (&list->mr);
    if (list->info->num_of_elements == 0)
    {
	mrlock_unlock (&list->mr);
	return NULL;
    }

    element = list->info->post_element.previous;

    pointer = element->pointer;
    list->info->post_element.previous = element->previous;
    element->previous->next = &list->info->post_element;

    free (element);
    list->info->num_of_elements--;

    mrlock_unlock (&list->mr);

    return pointer;
}

/****************************************************************************/

void
remove_all (Generic_list *list)
{
    Generic_list_element *element;

    (void) mrlock_wrlock (&list->mr);
    element = list->info->pre_element.next;
    while (element != &list->info->post_element)
    {
	element = element->next;
	decref (element->previous->pointer);
	free (element->previous);
    }

    list->info->pre_element.next = &list->info->post_element;
    list->info->post_element.previous = &list->info->pre_element;
    list->info->num_of_elements = 0;
    mrlock_unlock (&list->mr);
}

/****************************************************************************/

void *
peek_at_beginning (Generic_list *list)
{
    void *ptr;

    (void) mrlock_rdlock (&list->mr);
    ptr = list->info->pre_element.next->pointer;
    mrlock_unlock (&list->mr);
    return ptr;
}

/****************************************************************************/

void *
peek_at_end (Generic_list *list)
{
    void *ptr;

    (void) mrlock_rdlock (&list->mr);
    ptr = list->info->post_element.previous->pointer;
    mrlock_unlock (&list->mr);
    return ptr;
}

/****************************************************************************/

void *
first_in_list (Generic_list *list)
{
    void *ptr;

    (void) mrlock_rdlock (&list->mr);
    incref (ptr = list->info->pre_element.next->next->previous->pointer);
    mrlock_unlock (&list->mr);
    return ptr;
}

/****************************************************************************/

void *
last_in_list (Generic_list *list)
{
    void *ptr;

    (void) mrlock_rdlock (&list->mr);
    incref (ptr = list->info->post_element.previous->previous->next->pointer);
    mrlock_unlock (&list->mr);
    return ptr;
}

/****************************************************************************/

int
num_of_objects (Generic_list *list)
{
    unsigned int num;

    (void) mrlock_rdlock (&list->mr);
    num = list->info->num_of_elements;
    mrlock_unlock (&list->mr);
    return num;
}

/****************************************************************************/

int
is_empty (Generic_list *list)
{
    int bool;

    (void) mrlock_rdlock (&list->mr);
    bool = (list->info->num_of_elements == 0);
    mrlock_unlock (&list->mr);
    return bool;
}

/****************************************************************************/

int
is_in_list (Generic_list *list, const void *pointer)
{
    Generic_list_element *element;
    int bool;

    (void) mrlock_rdlock (&list->mr);
    element = list->info->pre_element.next;

    while (element != &list->info->post_element && element->pointer != pointer)
	element = element->next;

    bool = (element != &list->info->post_element);
    mrlock_unlock (&list->mr);

    return bool;
}

/****************************************************************************/

void
perform_on_list (Generic_list *list, gl_do_func fn, void *args)
{
    Generic_list_element *element;

    (void) mrlock_rdlock (&list->mr);
    element = list->info->pre_element.next;
    while (element != &list->info->post_element)
    {
	incref (element->pointer);
	(*fn) (element->pointer, args);
	decref (element->pointer);
	element = element->next;
    }
    mrlock_unlock (&list->mr);
}

/****************************************************************************/

void *
first_that (Generic_list *list, gl_cmp_func fn, const void *args)
{
    Generic_list_element *element;
    void *ptr;

    (void) mrlock_rdlock (&list->mr);
    element = list->info->pre_element.next;
    while (element != &list->info->post_element)
    {
	incref (element->pointer);
	if ((*fn) (element->pointer, args))
	    break;
	decref (element->pointer);
	element = element->next;
    }

    ptr = element->pointer;
    mrlock_unlock (&list->mr);

    return ptr;
}

/****************************************************************************/

void *
last_that (Generic_list *list, gl_cmp_func fn, const void *args)
{
    Generic_list_element *element;
    void *ptr;

    (void) mrlock_rdlock (&list->mr);
    element = list->info->post_element.previous;
    while (element != &list->info->pre_element)
    {
	incref (element->pointer);
	if ((*fn) (element->pointer, args))
		break;
	decref (element->pointer);
	element = element->previous;
    }

    ptr = element->pointer;
    mrlock_unlock (&list->mr);

    return ptr;
}
