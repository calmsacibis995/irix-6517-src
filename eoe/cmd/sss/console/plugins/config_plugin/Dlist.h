#ifndef _DLIST_H_
#define _DLIST_H_
#include <stdio.h>

typedef struct Dlist_s {
    struct Dlist_s *next;
    struct Dlist_s *before;
    cm_hndl_t cmp;
    int children_present_in_dlist;
    int grandchildren_present_in_dlist;
} Dlist_t;


#define DELETE_DLIST(H,CMP)             DlistDelete_by_cm_hndl(H,CMP)
#define INSERT_DLIST(H,TAIL,ELEM)       DlistInsert_by_cm_hndl(H,TAIL,ELEM)
#define NEXT_DLIST(H,CMP,pCMP)          DlistNext_by_cm_hndl(H,CMP,pCMP)
#define CHILDREN_INSERTED(H,CMP,T_OR_F) DlistSetChildren(H,CMP,T_OR_F)
#define GRANDCHILDREN_INSERTED(H,CMP,T_OR_F) \
                                        DlistSetGrandChildren(H,CMP,T_OR_F)

#endif /* _DLIST_H_ */
