#ifndef _COLLATE_H_
#define _COLLATE_H_

#include <limits.h>
#include "tables.h"

/* Handling of the order statement */
typedef enum { e_undef		= 0,
	       e_forward	= (1<<0),
	       e_backward	= (1<<1),
	       e_position	= (1<<2)
} collorder_t;

extern collorder_t	collorder [ COLL_WEIGHTS_MAX ];
extern int		collorder_len;

/* Storage of the collation order */
void coll_add_collsym ( const char * symbol );
void coll_add_collid ( const char * id, encs_t * weights[], int nb_weights );

#endif
