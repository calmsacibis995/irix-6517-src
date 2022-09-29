#ident "$Revision: 1.4 $"

#define _KERNEL 1
#include <sys/types.h>
#undef _KERNEL

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include "dir_stack.h"
#include "err_protos.h"

/*
 * a directory stack for holding directories while
 * we traverse filesystem hierarchy subtrees.
 * names are kind of misleading as this is really
 * implemented as an inode stack.  so sue me...
 */

static dir_stack_t	dirstack_freelist;
static int		dirstack_init = 0;

void
dir_stack_init(dir_stack_t *stack)
{
	stack->cnt = 0;
	stack->head = NULL;

	if (dirstack_init == 0)  {
		dirstack_init = 1;
		dir_stack_init(&dirstack_freelist);
	}

	stack->cnt = 0;
	stack->head = NULL;

	return;
}

static void
dir_stack_push(dir_stack_t *stack, dir_stack_elem_t *elem)
{
	assert(stack->cnt > 0 || stack->cnt == 0 && stack->head == NULL);

	elem->next = stack->head;
	stack->head = elem;
	stack->cnt++;

	return;
}

static dir_stack_elem_t *
dir_stack_pop(dir_stack_t *stack)
{
	dir_stack_elem_t *elem;

	if (stack->cnt == 0)  {
		assert(stack->head == NULL);
		return(NULL);
	}

	elem = stack->head;

	assert(elem != NULL);

	stack->head = elem->next;
	elem->next = NULL;
	stack->cnt--;

	return(elem);
}

void
push_dir(dir_stack_t *stack, xfs_ino_t ino)
{
	dir_stack_elem_t *elem;

	if (dirstack_freelist.cnt == 0)  {
		if ((elem = malloc(sizeof(dir_stack_elem_t))) == NULL)  {
			do_error(
			"couldn't malloc dir stack element, try more swap\n");
			exit(1);
		}
	} else  {
		elem = dir_stack_pop(&dirstack_freelist);
	}

	elem->ino = ino;

	dir_stack_push(stack, elem);

	return;
}

xfs_ino_t
pop_dir(dir_stack_t *stack)
{
	dir_stack_elem_t *elem;
	xfs_ino_t ino;

	elem = dir_stack_pop(stack);

	if (elem == NULL)
		return(NULLFSINO);

	ino = elem->ino;
	elem->ino = NULLFSINO;

	dir_stack_push(&dirstack_freelist, elem);

	return(ino);
}
