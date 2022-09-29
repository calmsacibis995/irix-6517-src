#ifndef _DLIST_H_
#define _DLIST_H_
#include <stdio.h>

typedef struct Dlist_s {
  void *next;
  void *before;
} Dlist_t;

extern void DlistTraverse(Dlist_t *head,Dlist_t *end,
			  void (*func)(void *,void *),void *Pkey);
extern void DlistFindRemove(Dlist_t **head,Dlist_t **tail,

			    int (*func_compare)(void *,void *),
			    void *Pcompare,
			    
			    int (*func_beforeremove)(void *,void *),
			    void *Pbeforeremove,
			    
			    int (*func_afterremove)(void *,void *),
			    void *Pafterremove);
extern Dlist_t *DlistFind(Dlist_t *head,Dlist_t *end,
		       int (*func_compare)(void *,void *),void *Pcompare);
extern void DlistAddAtHead(Dlist_t *head,Dlist_t *elem);
extern void DlistAddAtTail(Dlist_t *tail,Dlist_t *elem);
extern void DlistRemove(Dlist_t **head,Dlist_t **tail,Dlist_t *elem);
  

#define DELETE_DLIST(HEAD,TAIL,ELEM)               \
{                                                  \
  DlistRemove((Dlist_t **)HEAD,(Dlist_t **)TAIL,   \
	      (Dlist_t *)ELEM);                    \
}

#define ADD_DLIST_AT_TAIL(HEAD,TAIL,ELEM)          \
{                                                  \
  DlistAddAtTail((Dlist_t *)TAIL,(Dlist_t *)ELEM); \
  TAIL=ELEM;                                       \
  if (HEAD == NULL)                                \
     HEAD = TAIL;                                  \
}

#define ADD_DLIST_AT_HEAD(HEAD,TAIL,ELEM)          \
{                                                  \
  DlistAddAtHEAD((Dlist_t *)HEAD,(Dlist_t *)ELEM); \
  HEAD=ELEM;                                       \
  if (TAIL == NULL)                                \
     TAIL = HEAD;                                  \
}

#define TRAVERSE_DLIST(HEAD,END,FUNC,PKEY) \
{                                          \
  DlistTraverse((Dlist_t *)HEAD,           \
		(Dlist_t *)END,            \
    (void (*)(void *,void *))FUNC,         \
    (void *)PKEY);                         \
}

#define FIND_DLIST(HEAD,END,FUNC,PKEY)     \
  DlistFind((Dlist_t *)HEAD,               \
	    (Dlist_t *)END,                \
	    (int (*)(void *,void *))FUNC,  \
	    (void *)PKEY)


#endif /* _DLIST_H_ */
