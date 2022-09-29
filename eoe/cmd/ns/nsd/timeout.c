#include <stdio.h>
#include <sys/time.h>
#include <malloc.h>
#include <ns_api.h>
#include <ns_daemon.h>

static nsd_times_t *tq;
static struct timeval tm;

/*
** This routine is called to set the timeout for the central select.
** It just maps an absolute time to an offset.
*/
struct timeval *
nsd_timeout_set(void)
{
	struct timeval now;

	if (tq) {
		gettimeofday(&now);
		tm.tv_sec = tq->t_timeout.tv_sec - now.tv_sec;
		tm.tv_usec = tq->t_timeout.tv_usec - now.tv_usec;
		if (tm.tv_usec >= 1000000) {
			tm.tv_sec++;
			tm.tv_usec -= 1000000;
		}
		if (tm.tv_usec < 0) {
			tm.tv_sec--;
			tm.tv_usec += 1000000;
		}

		/*
		** Select croaks on negative timeouts, so we just make
		** it very tiny.
		*/
		if (tm.tv_sec < 0) {
			tm.tv_sec = 0;
			tm.tv_usec = 1;
		}

		return &tm;
	}
	return (struct timeval *)0;
}

/*
** This function pops the next timeout record off the stack.
*/
nsd_times_t *
nsd_timeout_next(void)
{
	nsd_times_t *t;

	if (! tq) {
		return (nsd_times_t *)0;
	}

	t = tq;

	if (t) {
		tq = tq->t_next;
	}

	return t;
}

/*
** This routine will add a new timeout record into the timeout queue.
*/
nsd_times_t *
nsd_timeout_new(nsd_file_t *rq, long timeout, nsd_timeout_proc *proc,
    void *data)
{
	nsd_times_t *new, *current, *last;
	struct timeval now;

	/*
	** Get space for a new timeout structure.
	*/
	new = (nsd_times_t *)nsd_calloc(1, sizeof(nsd_times_t));
	if (! new) {
		nsd_logprintf(NSD_LOG_RESOURCE, "timeout_new: malloc failed");
		return (nsd_times_t *)0;
	}

	/*
	** Fill in our new structue.
	*/
	new->t_file = rq;
	new->t_proc = proc;
	new->t_clientdata = data;

	gettimeofday(&now);

	new->t_timeout.tv_sec = now.tv_sec + (timeout/1000);
	new->t_timeout.tv_usec = now.tv_usec + ((timeout % 1000) * 1000);
	if (new->t_timeout.tv_usec < now.tv_usec) {
		new->t_timeout.tv_sec++;
		new->t_timeout.tv_usec = now.tv_usec - new->t_timeout.tv_usec;
	}

	/*
	** Search through the list for a place to insert this timeout.
	*/
	for (current = tq, last = 0;
	    current && ((new->t_timeout.tv_sec > current->t_timeout.tv_sec) ||
	     ((new->t_timeout.tv_sec == current->t_timeout.tv_sec) &&
	     (new->t_timeout.tv_usec >= current->t_timeout.tv_usec)));
	    last = current, current = current->t_next);
	if (last) {
		new->t_next = current;
		last->t_next = new;
	} else {
		new->t_next = current;
		tq = new;
	}
#ifdef DEBUG
       nsd_logprintf(NSD_LOG_MAX, "Current timeout list:\n");
       for (current = tq; current; current = current->t_next) {
               nsd_logprintf(NSD_LOG_MAX, "\trq: %x tvsec: %d tvusec %d\n",
                   current->t_file, current->t_timeout.tv_sec,
                   current->t_timeout.tv_usec);
       }
#endif

	return new;
}

/*
** This routine will remove the first timeout record for a given
** request.
*/
int
nsd_timeout_remove(nsd_file_t *rq) {
	nsd_times_t *current, *last=(nsd_times_t *)0;

	for (current = tq; current && current->t_file != rq;
	    last = current, current = current->t_next);
	if (current) {
		if (last) {
			last->t_next = current->t_next;
		} else {
			tq = current->t_next;
		}
		free(current);
		return NSD_OK;
	}
	return NSD_ERROR;
}
