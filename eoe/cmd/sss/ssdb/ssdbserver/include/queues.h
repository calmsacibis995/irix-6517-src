/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/*
  Code for generell handling of priority Queues.
  Implemention of queues from "Algoritms in C" by Robert Sedgewick.
  Copyright Monty Program KB.
  By monty.
*/

#ifndef _queues_h
#define _queues_h
#ifdef	__cplusplus
extern "C" {
#endif

typedef struct st_queue {
  byte **root;
  uint elements;
  uint max_elements;
  uint offset_to_key;			/* compare is done on element+offset */
  int max_at_top;			/* Set if queue_top gives max */
  int  (*compare)(byte *,byte *);
} QUEUE;

#define queue_top(queue) ((queue)->root[1])
#define queue_element(queue,index) ((queue)->root[index+1])
#define queue_end(queue) ((queue)->root[(queue)->elements])
#define queue_replaced(queue) _downheap(queue,1)

int init_queue(QUEUE *queue,uint max_elements,uint offset_to_key,
	       pbool max_at_top, int (*compare)(byte *,byte *));
void delete_queue(QUEUE *queue);
void queue_insert(QUEUE *queue,byte *element);
byte *queue_remove(QUEUE *queue,uint index);
void _downheap(QUEUE *queue,uint index);

#ifdef	__cplusplus
}
#endif
#endif
