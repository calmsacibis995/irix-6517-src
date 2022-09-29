/*
 * Character queue data type
 */

#define CQ_SIZE		1024		/* Power of 2 is most efficient */

typedef struct {
    u_char		buf[CQ_SIZE];
    int			ipos, opos;
} cq_t;

#define CQ_NEXT(p)	(((p) + 1) % CQ_SIZE)

#define cq_init(q)	bzero((q), sizeof (*(q)))
#define cq_empty(q)	((q)->ipos == (q)->opos)
#define cq_full(q)	(CQ_NEXT((q)->ipos) == (q)->opos)
#define cq_used(q)	((q)->opos <= (q)->ipos ?		\
			 (q)->ipos - (q)->opos :		\
			 CQ_SIZE + (q)->ipos - (q)->opos)
#define cq_room(q)	((q)->opos <= (q)->ipos ?		\
			 CQ_SIZE - 1 + (q)->opos - (q)->ipos :	\
			 (q)->opos - (q)->ipos - 1)
#define cq_add(q, c)	((q)->buf[(q)->ipos] = (u_char) (c),	\
			 (q)->ipos = CQ_NEXT((q)->ipos))
#define cq_rem(q, c)	((c) = (q)->buf[(q)->opos],		\
			 (q)->opos = CQ_NEXT((q)->opos))
