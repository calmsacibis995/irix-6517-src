#ifndef TIMER_HEADER
#define TIMER_HEADER

#ident "$Revision: 1.2 $"

/* The following definitions support timers with microsecond resolution */
#define USECS_PER_SEC		1000000

#define MANY_USECS		~0

#define SEC(x)			((x) / USECS_PER_SEC)
#define USEC(x)			((x) % USECS_PER_SEC)

/* timer queue definition */
typedef void (*timer_ast) (void *);

typedef struct timer_queue
{
	satmp_esi_t esi;		/* identifier for this event */
	time_t timeout;			/* timeout period for this event */
	time_t otimeout;		/* saved copy of original timeout */
	unsigned int ntries;		/* number of retries */
	void *client;			/* pointer to client data */
	timer_ast notify;		/* what to do on timeout */
	timer_ast destroy;		/* destroy client */
	timer_ast retry;		/* function to retry client */
	struct timer_queue *next;	/* next entry in the queue */
} tqueue;

int timer_initialize (void);
int timer_threads_start (void);
int timer_set (time_t, unsigned int, satmp_esi_t, void *,
	       timer_ast, timer_ast, timer_ast);
void *timer_cancel (satmp_esi_t);

#endif /* TIMER_HEADER */
