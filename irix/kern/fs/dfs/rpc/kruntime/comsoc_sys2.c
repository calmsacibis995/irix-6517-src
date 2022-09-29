

#include <sys/types.h>
#include <sys/pcb.h>                                               
#include <sys/cmn_err.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/vsocket.h>

/*
	#include <commonp.h>
	#include <com.h>
	#include <comprot.h>
	#include <comnaf.h>
	#include <comp.h>
*/


typedef int rpc_socket_error_t;

typedef struct socket *rpc_socket_t;       /* a kernel socket handle */

typedef struct {
    rpc_socket_t    so;
    short           events;         /* POLL{NORM,OUT,PRI} */
} rpc_socket_sel_t, *rpc_socket_sel_p_t;


#include <sys/errno.h>
#include <sys/kmem.h>

#include <sys/param.h>
#ifdef PRE64
#include <sys/user.h>
#endif
#include <sys/proc.h>
#include <sys/file.h>


/*
 * Includes and declarations for in-kernel select implementation.
 */
#include <sys/time.h>               /* for timeval */
#include <sys/systm.h>              /* for selwait */

extern int polladd(struct pollhead *php,
        short events,
        ulong_t gen,
        void *arg,
        struct polldat *pdp);

extern void polldel(struct polldat *pdp);

extern void polltime(uthread_t *ut);



/*
 * Masks for p_pollrotor.  We keep track of how many wakeups have been done
 * on the process while it was asleep.  If we have only had one, then the
 * dopoll() code will just pickup the rotor fd status and return.  This helps
 * processes like the X server that poll on a large # of fds with only one or
 * two ever active from instant to instant.
 */
#define ROTORFDMASK	0x3FFF
#define ROTORONCE	0x4000
#define ROTORMULTI	0xC000




int rpc__soo_select(
rpc_socket_sel_t *ssel,       /* checking for events in this */
rpc_socket_sel_t *fssel,
int anyyet,
struct pollhead **phpp,
unsigned int *genp,
int error)
{
        rpc_socket_t    so = ssel->so;
        short           events = ssel->events;
        short           *revents = &fssel->events;

	/* follows uipc_poll() in bsd/socket/uipc_vnodeops.c  .. */

	NETSPL_DECL(s1)

	error = 0;

#define sbselqueue(sb) (sb)->sb_flags |= SB_SEL

        /* we can set phpp outside lock, saves two arg restores */
        if (!anyyet) {
                *phpp = &so->so_ph;
                /*
                 * Snapshotting the pollhead's generation number here before
                 * we grab the socket locks is fine since taking it early
                 * simply provides a more conservative estimate of what
                 * ``generation'' of the pollhead our event state report
                 * indicates.
                 */
                *genp = POLLGEN(&so->so_ph);
        }

	*revents = events;
	SOCKET_LOCK(so);	  /* this is s=splnet() */
	SOCKET_QLOCK(so, s1);
        fssel->so = so;
	
	/* set the buffer selected flag on this bufs */
	/* not sure why this is done ..  */

        if ((*revents & (POLLIN|POLLRDNORM)) && !soreadable(so)) {
                *revents &= ~(POLLIN|POLLRDNORM);
                sbselqueue(&so->so_rcv);
        }
        if ((*revents & (POLLPRI|POLLRDBAND))
            && !(so->so_oobmark | (so->so_state & SS_RCVATMARK))) {
                *revents &= ~(POLLPRI|POLLRDBAND);
                sbselqueue(&so->so_rcv);
        }
        if ((*revents & POLLCONN) && (so->so_qlen == 0)) {
                *revents &= ~POLLCONN;
                sbselqueue(&so->so_rcv);
        }
        if ((*revents & POLLOUT) && !sowriteable(so)) {
                *revents &= ~POLLOUT;
                sbselqueue(&so->so_snd);
        }
        SOCKET_QUNLOCK(so, s1);		/* splx(s)  */
        SOCKET_UNLOCK(so);		/* splx(s)  */

        return (*revents);
}


/*
 * R P C _ _ S O C K E T _ S E L E C T
 * 
 *
 * Perform a 'select' operation on the list of sockets defined by the
 * soc array.  Return the active sockets in the "found" fsoc[] array
 * and the found count as a return value.
 *
 * This select stuff should look very similar to standard BSD processing.
 * 
 * For SGI ->
 *     don't have to follow select in kern/sgi/select as krpc sockets
 *  do not have a file & vnode entry associated to them. The rest
 *  of the stuff is similar to sgi's dopoll
 */

rpc_socket_error_t rpc__socket_select
(
    int nsoc,
    rpc_socket_sel_t *tsoc,
    int *nfsoc,
    rpc_socket_sel_t *fsoc,
    struct timeval *tmo,
    uthread_t *ut	/* Address of dummy uthread structure from 
					local_lstate structure. */
)
{ 
    int error = 0;
    int s, i, rotor, rwonce, vcnt, rv;
    int dsize, size;
    struct timeval atv;
    toid_t tid;
    unsigned selticks=0;
    rpc_socket_sel_t  *soc;
    rpc_socket_sel_t  *soc_out;
    struct polldat *curdat=NULL, *darray;
    unsigned int gen;
    toid_t (*callback)(void (*)(), void *, long, ...);


    if (tmo) 
    {
        bcopy(tmo, &atv, sizeof (struct timeval));
    } 

    /* assuming that krpc does not need fastimer resolution */
    if(tmo) 
	if(timerisset(&atv))	
		callback = timeout_nothrd;
		selticks = hzto(&atv);

    if(selticks == 0)
	selticks = 1;

    /*
     * Create and initialize the file ptr and polldat data structures.
     */
    if (nsoc > NPOLLFILE) {
            dsize = nsoc * (int)sizeof(struct polldat);
            size = dsize;   /* for kmem_free below */
            darray = (struct polldat *) kmem_zalloc(size, KM_SLEEP);
            if (darray == NULL) {
                    *nfsoc = 0;
                    return EAGAIN;
            }
    } else {
            dsize = NPOLLFILE * sizeof(struct polldat);
            size = 0;               /* no kmem_free below */
            if (ut->ut_polldat == NULL)
                    ut->ut_polldat = (struct polldat *)
                            kmem_zalloc(dsize, KM_SLEEP);
            darray = ut->ut_polldat;
    }

	
    /*
     * Retry scan of fds until an event is found or until the
     * timeout is reached.
     */
    vcnt = 0;
    ut->ut_pollrotor = 0;
retry:
    curdat = darray;

    /*
     * We use a rotor here to cut down on the amount of time spent
     * re-adding & deleteing the poll entries on wakeups.  It is
     * likely that the rotor points to an FD with revents for us.
     * Note that when we wakeup, rotor equals 0 if no pollwakeup
     * has happened, or non-zero with a pollp index plus 1.
     */
    s = mutex_spinlock(&ut->ut_pollrotorlock);
    rotor = ut->ut_pollrotor & ROTORFDMASK;
    ASSERT(rotor <= nsoc);
    rwonce = ut->ut_pollrotor & ROTORMULTI;
    ut->ut_pollrotor = 0;
    mutex_spinunlock(&ut->ut_pollrotorlock, s);

    if (rotor)
            rotor -= 1;
    soc = &tsoc[rotor];
    soc_out = &fsoc[rotor];
    for( rv = 0, i=0; i < nsoc; i++) {
	struct pollhead *php;

quicktry:
	php = NULL;
        rpc__soo_select(soc, soc_out, vcnt, &php, &gen, error);

         if (soc_out->events) {
                    vcnt++;
                    ++soc_out;
                    rv |= soc_out->events & POLLNVAL;
                    if (rwonce == ROTORONCE)
                            goto pollout;
         } else if (php == NULL) {
                    /* if no pollhead, disable ROTORONCE opt */
                    rwonce = -1;
         } else if (vcnt == 0) {
                    curdat->pd_rotorhint = rotor;
                    if (polladd(php, soc->events, gen,
                            (void *)ut, curdat) != 0) {

                            /*
                             * a pollwakeup was done after the VOP_POLL.
                             * we need to retry to avoid the race between
                             * pollwakeup and polladd.
                             */
                            goto quicktry;
                    } else
                            curdat++;

         }
         if (++rotor >= nsoc) {
		 soc = tsoc;
		 soc_out = fsoc;
                 rotor = 0;
         } else {
                 ++soc;
         }
    }
        if (vcnt)
                goto pollout;

        s = mutex_spinlock(&ut->ut_pollrotorlock);
        /*
         * If going to sleep and rwonce is negative, must disable
         * ROTORONCE wakeup optimizations.  We have a least one fd
         * without a pollwakeup handle.
         */
        if (rwonce < 0)
                ut->ut_pollrotor |= ROTORMULTI;

        /*
         * Since we hold the lock, anybody checking to see if
         * we are on the poll semaphore will wait until we get there
         */
        if ((ut->ut_pollrotor & ROTORFDMASK) != 0) {
                /* A pollwakeup we care about has happened */
                mutex_spinunlock(&ut->ut_pollrotorlock, s);

                /* get rid of accumulated polldats */
                while (--curdat >= darray)
                        polldel(curdat);
                goto retry;
        }
        /*
         * If you get here, the poll of fds was unsuccessful.
         * First make sure your timeout hasn't been reached.
         * If not then sleep and wait until some fd becomes
         * readable, writeable, or gets an exception.
         */
        if (tmo)  {
                if (selticks == 0) {
                        mutex_spinunlock(&ut->ut_pollrotorlock, s);
                        goto pollout;
                } else
                        tid = callback(polltime, ut, selticks);
        }

        /*
         * Put ourselves on the sync variable.
         */
        rv = sv_wait_sig(&UT_TO_KT(ut)->k_timewait,
                (PZERO+1)|TIMER_SHIFT(AS_SELECT_WAIT),
                &ut->ut_pollrotorlock, s);

        /*
         * Delete the timeout if we posted one.  If it went off
         * while we were asleep, we get back a selticks value of
         * zero.  In that case, just exit dopoll().  Otherwise,
         * save the time tick value returned for the next loop.
         */
        if (tmo) {
                ASSERT(selticks);
                if ((selticks = untimeout(tid)) == 0)
                        goto pollout;
        }

        if (rv == -1)
                error = EINTR;
        else {
                /* get rid of accumulated polldats */
                while (--curdat >= darray)
                        polldel(curdat);
                goto retry;
        }

pollout:

        /*
         * Poll cleanup code.
         */
        ut->ut_pollrotor = -1;

        /* get rid of accumulated polldats */
        while (--curdat >= darray)
                polldel(curdat);

        if (size)
                kmem_free((caddr_t)darray, size);

        *nfsoc = vcnt;

        return error;
}

