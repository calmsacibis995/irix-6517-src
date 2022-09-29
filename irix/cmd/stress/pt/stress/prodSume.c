/*
 *	Pthreads producer-consumer demo program.
 *
 * A spectacularly useless form of cat(1).
 *
 * A set of producer threads remove buffers from the free queue,
 * read into them from stdin and then place them on the input queue.
 * A set of consumer threads remove buffers from the input queue
 * write them to stdout and then place them on the free queue.
 *
 * The numbers of threads and buffers is tunable.
 *
 * The output data may be reordered due to scheduling.
 *
 * A peculiarity in the code is the termination sequence:
 *	When a producer finds no more input it recycles the buffer,
 *	enqueues a NULL buffer to the producer queue and exits.
 *	The NULL buffer causes the producers count for the queue to
 *	be decremented and when it reaches zero any waiting consumers
 *	will be woken up.  Consumers who wake up and find no producers
 *	conclude that they should exit too.
 */

#include	<pthread.h>
#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<getopt.h>

#define	FALSE	0
#define	TRUE	1

#define	ARGS	"p:c:f:s"
#define	USAGE	"Usage: %s [-p <producers][-c <consumers>][-f <bufs>][-s]\n"

#define	DATABUFSIZE	1

typedef struct dataElt {
	struct dataElt	*next;
	struct dataElt	*prev;
	char		data[DATABUFSIZE];
	int		dataLen;
} dataElt_t;

typedef struct dataQueue {
	pthread_mutex_t	qLock;
	pthread_cond_t	qCondVar;
	dataElt_t	*qHead;
	dataElt_t	*qTail;
	int		qElts;
	int		qProducers;
	int		starve;
} dataQueue_t;

static void	*producerThread();
static void	*consumerThread();
static void	initQueue();
static void	enQueue();
static dataElt_t *deQueue();
static void	dumpStats();

static dataQueue_t	freeQueue;
static dataQueue_t	inputQueue;


int
main(int argc, char *argv[])
{
	pthread_t	id;
	int		c, errflg = 0;
	int		producers = 1, consumers = 1;
	int		freeBufs = 1;
	int		doStats = FALSE;

	optarg = NULL;
	while (!errflg && (c = getopt(argc, argv, ARGS)) != EOF)
		switch (c) {
		case 'p'	:
			producers = atoi(optarg);
			break;
		case 'c'	:
			consumers = atoi(optarg);
			break;
		case 'f'	:
			freeBufs = atoi(optarg);
			break;
		case 's'	:
			doStats = TRUE;
			break;
		default :
			errflg++;
		}
	if (errflg || optind < argc) {
		fprintf(stderr, USAGE, argv[0]);
		exit(0);
	}

	initQueue(&freeQueue, freeBufs, -1);
	initQueue(&inputQueue, 0, producers);

	if (doStats)
		atexit(dumpStats);

	while (producers-- > 0)
		pthread_create(&id, 0, producerThread, &inputQueue);
	while (--consumers > 0)
		pthread_create(&id, 0, consumerThread, &inputQueue);
	consumerThread(&inputQueue);
}


static void *
producerThread(dataQueue_t *produceQ)
{
	dataElt_t	*buf;

	for (;;) {
		buf = deQueue(&freeQueue);
		if ((buf->dataLen = read(0, buf->data, DATABUFSIZE)) == 0) {
			enQueue(&freeQueue, buf);
			enQueue(produceQ, NULL);
			pthread_exit(0);
		}
		enQueue(produceQ, buf);
	}
}


static void *
consumerThread(dataQueue_t *consumeQ)
{
	dataElt_t	*buf;

	for (;;) {
		buf = deQueue(consumeQ);
		if (buf == NULL)
			pthread_exit(0);
		write(1, buf->data, buf->dataLen);
		enQueue(&freeQueue, buf);
	}
}


static void
initQueue(dataQueue_t *queue, int seed, int producers)
{
	dataElt_t	*elt;

	pthread_cond_init(&queue->qCondVar, 0);
	pthread_mutex_init(&queue->qLock, 0);
	queue->qHead = NULL;
	queue->qTail = NULL;
	queue->qElts = 0;
	queue->qProducers = producers;
	queue->starve = 0;
	while (seed-- > 0) {
		elt = (dataElt_t *)malloc(sizeof(dataElt_t));
		elt->next = elt->prev = NULL;
		enQueue(queue, elt);
	}
}


static void
enQueue(dataQueue_t *queue, dataElt_t *element)
{
	pthread_mutex_lock(&queue->qLock);
	if (element == NULL) {
		if (--queue->qProducers == 0)
			pthread_cond_broadcast(&queue->qCondVar);
		pthread_mutex_unlock(&queue->qLock);
		return;
	}
	element->prev = NULL;
	element->next = queue->qHead;
	if (++queue->qElts == 1)
		queue->qHead = queue->qTail = element;
	else {
		queue->qHead->prev = element;
		queue->qHead = element;
	}
	pthread_mutex_unlock(&queue->qLock);
	pthread_cond_signal(&queue->qCondVar);
}


static dataElt_t *
deQueue(dataQueue_t *queue)
{
	dataElt_t	*element;

	pthread_mutex_lock(&queue->qLock);
	while (queue->qElts <= 0) {
		if (queue->qProducers == 0) {
			pthread_mutex_unlock(&queue->qLock);
			return (NULL);
		}
		queue->starve++;
		pthread_cond_wait(&queue->qCondVar, &queue->qLock);
	}
	element = queue->qTail;
	if (--queue->qElts == 0)
		queue->qHead = queue->qTail = NULL;
	else {
		queue->qTail = queue->qTail->prev;
		queue->qTail->next = NULL;
	}
	pthread_mutex_unlock(&queue->qLock);
	return (element);
}


static void
dumpStats()
{
	fprintf(stderr, "freeQueue elts %d starve %d\n",
			freeQueue.qElts, freeQueue.starve);
	fprintf(stderr, "inputQueue elts %d starve %d\n",
			inputQueue.qElts, inputQueue.starve);
}
