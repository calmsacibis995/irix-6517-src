/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.
*/

#include "mysys_priv.h"
#include <queues.h>


	/* The actuall code for handling queues */

int init_queue(QUEUE *queue, uint max_elements, uint offset_to_key, 
	       pbool max_at_top, int (*compare) (byte *, byte *))
{
  DBUG_ENTER("init_queue");
  if ((queue->root= (byte **) my_malloc((max_elements+1)*sizeof(void*),
					 MYF(MY_WME))) == 0)
    DBUG_RETURN(1);
  queue->elements=0;
  queue->compare=compare;
  queue->max_elements=max_elements;
  queue->offset_to_key=offset_to_key;
  queue->max_at_top= max_at_top ? (-1 ^ 1) : 0;
  DBUG_RETURN(0);
}

void delete_queue(QUEUE *queue)
{
  DBUG_ENTER("delete_queue");
  if (queue->root)
  {
    my_free((gptr) queue->root,MYF(0));
    queue->root=0;
  }
  DBUG_VOID_RETURN;
}


	/* Code for insert, search and delete of elements */

void queue_insert(register QUEUE *queue, byte *element)
{
  reg2 uint index,next;

#ifndef DBUG_OFF
  if (queue->elements < queue->max_elements)
#endif
  {
    queue->root[0]=element;
    index= ++queue->elements;

    while ((queue->compare(element+queue->offset_to_key,
			  queue->root[(next=index >> 1)]+queue->offset_to_key)
	   ^ queue->max_at_top) < 0)
    {
      queue->root[index]=queue->root[next];
      index=next;
    }
    queue->root[index]=element;
  }
}

	/* Remove item from queue */
	/* Returns pointer to removed element */

byte *queue_remove(register QUEUE *queue, uint index)
{
#ifndef DBUG_OFF
  if (index >= queue->max_elements)
    return 0;
#endif
  {
    byte *element=queue->root[++index];		/* Intern index starts from 1 */
    queue->root[index]=queue->root[queue->elements--];
    _downheap(queue,index);
    return element;
  }
}


	/* Fix when element on top has been replaced */

#ifndef queue_replaced
void queue_replaced(queue)
QUEUE *queue;
{
  _downheap(queue,1);
}
#endif

	/* Fix heap when index have changed */

void _downheap(register QUEUE *queue, uint index)
{
  byte *element;
  uint elements,half_queue,next_index,offset_to_key;
  int cmp;

  offset_to_key=queue->offset_to_key;
  element=queue->root[index];
  half_queue=(elements=queue->elements) >> 1;

  while (index <= half_queue)
  {
    next_index=index+index;
    if (next_index < elements &&
	(queue->compare(queue->root[next_index]+offset_to_key,
			queue->root[next_index+1]+offset_to_key) ^
	 queue->max_at_top) > 0)
      next_index++;
    if ((cmp=queue->compare(queue->root[next_index]+offset_to_key,
			     element+offset_to_key)) == 0 ||
	(cmp ^ queue->max_at_top) > 0)
      break;
    queue->root[index]=queue->root[next_index];
    index=next_index;
  }
  queue->root[index]=element;
}
