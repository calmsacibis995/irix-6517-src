#include "Dlist.h"

void DlistTraverse(Dlist_t *head,Dlist_t *end,
		  void (*func)(void *,void *),void *Pkey)
{
  Dlist_t *listPtr;

  if(end && !head)		/* Silly check. */
    return;

  while(head != end)
    {
      /* 
       * Get our pointer first.. 
       * Don't rely on the pointer being around later.
       */
      listPtr=head->next;	
      (*func)((void *)head,Pkey);
      head = listPtr;
    }
  return;
}

/*
 * Callers responsiblility to free elem.
 */
void DlistFindRemove(Dlist_t **head,Dlist_t **tail,

		     int (*func_compare)(void *,void *),
		     void *Pcompare,

		     int (*func_beforeremove)(void *,void *),
		     void *Pbeforeremove,		     

		     int (*func_afterremove)(void *,void *),
		     void *Pafterremove)
{
  Dlist_t *ptrListTemp;

  if(!head)			/* Silly check. */
    return;

  ptrListTemp=*head;

  while(ptrListTemp)
    {
      ptrListTemp=DlistFind(*head,NULL,func_compare,Pcompare);

      if(func_beforeremove)
	(*func_beforeremove)(Pbeforeremove,ptrListTemp);

      DlistRemove(head,tail,ptrListTemp);

      if(func_afterremove)
	(*func_afterremove)(Pafterremove,ptrListTemp);

    }
}


Dlist_t *DlistFind(Dlist_t *head,Dlist_t *end,
		  int (*func_compare)(void *,void *),void *Pcompare)
{
  Dlist_t *listPtr;

  if(end && !head)		/* Silly check. */
    return NULL;

  while(head != end)
    {
      listPtr=head->next;	
      if((*func_compare)((void *)head,Pcompare))
	break;
      head = listPtr;
    }
  return head;
}

/*
 * Caller has to be aware elem becomes the new head. 
 */
void DlistAddAtHead(Dlist_t *head,Dlist_t *elem)
{
  if(!head)
    {
      elem->next=elem->before=NULL;
      return;
    }
  head->before=elem;

  elem->before=NULL;
  elem->next=head;
}

/*
 * Caller has to be aware elem becomes the new tail. 
 */
void DlistAddAtTail(Dlist_t *tail,Dlist_t *elem)
{
  if(!tail)
    {
      elem->next=elem->before=NULL;
      return;
    }
  tail->next=elem;

  elem->before=tail;
  elem->next=NULL;
}

/*
 * Callers responsiblility to free elem.
 */
void DlistRemove(Dlist_t **head,Dlist_t **tail,Dlist_t *elem)
{
  if(!elem)			/* Silly check */
    return;

  if(tail && elem == *tail)
    *tail = elem->before;

  if(head && elem == *head)
    *head = elem->next;

  if(elem->before)
    ((Dlist_t *)elem->before)->next=elem->next;

  if(elem->next)
    ((Dlist_t *)elem->next)->before=elem->before;
}
