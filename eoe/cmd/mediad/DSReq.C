#include "DSReq.H"

#include <dslib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/file.h>

#include "Log.H"
#include "Task.H"

Task            DSReq::close_task(DSReq::close_proc, NULL);
DSReq::DspCache DSReq::dsp_cache[];
int             DSReq::n_cached;
int             DSReq::cache_size;

//  The constructor is not inline in case it has to change in a future
//  version.  A chance of maintaining binary compatibility...
//
//  The destructor exists for the same reason.

DSReq::DSReq(const SCSIAddress& addr)
: _addr(addr)
{ }

DSReq::~DSReq()
{ }

dsreq *
DSReq::dsptr()
{
    int	h_cached = n_cached;
    if (n_cached > CACHE_SIZE) 
	h_cached = CACHE_SIZE;

    /* we use h_cached to ensure that the counter doesn't run off
     * the end of the cache array. h_cached can be no larger than
     * the array size. BZ - #589379
     */
    for (DspCache *p = dsp_cache, *end = p + h_cached; p < end; p++)
	if (p->addr == _addr)
	    return p->dsp;

    char path[24];
    (void) sprintf(path, "/dev/scsi/sc%ud%ul%u",
		   _addr.ctlr(), _addr.id(), _addr.lun());
    dsreq *dsp = dsopen(path, O_RDONLY);
    if (dsp)
    {
	//  Put the new entry in the cache, flushing anything already there.

	DspCache *cachep = &dsp_cache[n_cached++ % CACHE_SIZE];
	if (cachep->dsp)
	    dsclose(cachep->dsp);

	cachep->addr = _addr;
	cachep->dsp = dsp;

	//  Remember to close this later.

	if (!close_task.scheduled())
	    close_task.schedule(Task::ASAP);
    }
    else if (errno != EBUSY)		// EBUSY happens all the time.
	Log::perror("Can't dsopen %s", path);

    return dsp;
}

int
DSReq::g0cmd(uc b0, uc b1, uc b2, uc b3, uc b4, uc b5)
{
    dsreq *dsp = dsptr();
    if (!dsp)
	return errno;
    fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), b0, b1, b2, b3, b4, b5);
    filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ | DSRQ_SENSE);
    return doscsireq(getfd(dsp), dsp);
}

int
DSReq::g1cmd(uc b0, uc b1, uc b2, uc b3, uc b4,
	     uc b5, uc b6, uc b7, uc b8, uc b9)
{
    dsreq *dsp = dsptr();
    if (!dsp)
	return errno;
    fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp),
	      b0, b1, b2, b3, b4, b5, b6, b7, b8, b9);
    filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ | DSRQ_SENSE);
    return doscsireq(getfd(dsp), dsp);
}

void
DSReq::close_all()
{
    for (int i = 0; i < n_cached && i < CACHE_SIZE; i++)
    {   dsclose(dsp_cache[i].dsp);
	dsp_cache[i].dsp = NULL;
    }
    n_cached = 0;
}

void
DSReq::close_proc(Task&, void *)
{
    close_all();
}
