#ifndef DSReq_included
#define DSReq_included

#include <limits.h>

#include "SCSIAddress.H"

struct dsreq;
class Task;

//  A DSReq is a funny kind of automatically opening /dev/scsi
//  channel.  You can use it like a struct dsreq (as in <sys/dsreq.h>)
//  but you don't have to explicitly open it or close it.  It opens
//  and closes itself automatically, and also prevents duplicate
//  opens.
//
//  For example,
//
//	SCSIAddress addr(0, 3);		// SCSI ctlr 0, ID 3 (LUN 0)
//	DSReq dsr(addr);		// no side effects yet.
//  #ifdef EXPLICIT
//	dsreq *dsp = dsr.dsptr();	// side effect: dsopen
//	inquiry12(dsp, ...);		// inquiry12 from <dslib.h>
//	status = dsp->ds_status;
//  #else
//	inquiry12(dsr, ...);		// automatic conversion
//	status = dsr->ds_status;	// here, too.
//  #endif
//					// dsclose happens later.
//
//  DSReq also has a few convenience functions defined.  It's
//  nominally easier, faster and smaller code to say
//
//	status = dsp.g0cmd(a, b, c, d, e, f);
//
//  than
//
//	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), a, b, c, d, e, f);
//	filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ | DSRQ_SENSE);
//	status = doscsireq(getfd(dsp), dsp);

class DSReq {

    typedef unsigned char uc;

public:

    DSReq(const SCSIAddress&);
    ~DSReq();

    //  Access the dsreq pointer.  There are three ways to do it.

    dsreq *dsptr();
    operator dsreq * ()			{ return dsptr(); }
    dsreq *operator -> ()		{ return dsptr(); }

    //  Convenience functions.

    int g0cmd(uc, uc, uc, uc, uc, uc);
    int g1cmd(uc, uc, uc, uc, uc, uc, uc, uc, uc, uc);

    //  Force all cached dsp's closed.

    static void close_all();

private:

    enum { CACHE_SIZE = 10 };

    struct DspCache {
	SCSIAddress addr;
	dsreq *dsp;
    };

    //  Instance Variables

    SCSIAddress _addr;

    //  Class Variables

    static Task close_task;
    static DspCache dsp_cache[CACHE_SIZE];
    static int n_cached, cache_size;

    // Class Method

    static void close_proc(Task&, void *);

};

#endif /* !DSReq_included */
