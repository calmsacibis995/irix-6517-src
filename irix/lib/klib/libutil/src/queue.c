#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <klib/queue.h>

/* 
 * enqueue() -- Add a new element to the tail of doubly linked list.
 */
void
enqueue(element_t **list, element_t *new)
{
	element_t *head;

	/* 
	 * If there aren't any elements on the list, then make new element the 
	 * head of the list and make it point to itself (next and prev).
	 */
	if (!(head = *list)) {
		new->next = new;
		new->prev = new;
		*list = new;
	}
	else {
		head->prev->next = new;
		new->prev = head->prev;
		new->next = head;
		head->prev = new;
	}
}

/* 
 * dequeue() -- Remove an element from the head of doubly linked list.
 */
element_t *
dequeue(element_t **list)
{
	element_t *head;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return((element_t *)NULL);
	}

	head = *list;

	/* If there is only one element on list, just remove it 
	 */
	if (head->next == head) {
		*list = (element_t *)NULL;
	}
	else {
		head->next->prev = head->prev;
		head->prev->next = head->next;
		*list = head->next;
	}
	head->next = 0;
	return(head);
}

/*
 * findqueue()
 */
int
findqueue(element_t **list, element_t *item)
{
	element_t *e;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return(0);
	}

	e = *list;

	/* Check to see if there is only one element on the list. 
	 */
	if (e->next == e) {
		if (e != item) {
			return(0);
		}
	}
	else {
		/* Now walk linked list looking for item
		 */
		while(1) {
			if (e == item) {
				break;
			}
			else if (e->next == *list) {
				return(0);
			}
			e = e->next;
		}
	}
	return(1);
}

/*
 * findlist_queue()
 */
int
findlist_queue(list_of_ptrs_t **list,  list_of_ptrs_t *item, 
		  int (*compare)(void *,void *))
{
	list_of_ptrs_t *e;

	/* If there's nothing queued up, just return 
	 */
	if (!*list) {
		return(0);
	}

	e = *list;

	/* Check to see if there is only one element on the list. 
	 */
	if (((element_t *)e)->next == (element_t *)e) {
		if (compare(e,item)) {
			return(0);
		}
	}
	else {
		/* Now walk linked list looking for item
		 */
		while(1) {
			if (!compare(e,item)) {
				break;
			}
			else if (((element_t *)e)->next == (element_t *)*list) {
				return(0);
			}
			e = (list_of_ptrs_t *)((element_t *)e)->next;
		}
	}
	return(1);
}

/* 
 * remqueue() -- Remove specified element from doubly linked list.
 */
void
remqueue(element_t **list, element_t *item)
{
	/* Check to see if item is first on the list
	 */
	if (*list == item) {
		if (item->next == item) {
			*list = (element_t *)NULL;
			return;
		}
		else {
			*list = item->next;
		}
	}

	/* Remove item from list
	 */
	item->next->prev = item->prev;
	item->prev->next = item->next;
}
