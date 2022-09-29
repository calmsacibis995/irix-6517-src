#include "heap.h"
#include "protodefs.h"
#include "protoindex.h"
#include "tcp.h"

#line 22 "ftp.pi"

/*
 * Handcraft Reminder:
 *      if pidl is not fixed, the following has to be done to the end
 *      of ftp_decode for a variable to be decoded properly:
 *          Add the line "int tmp = ds->ds_count;" before the line "_renew".
 *          Change "MIN(ds->ds_count, ds->ds_count)" to "tmp");
 *          Change all "ds->ds_count" to "tmp" on the "pv_showvarfield" line.
 *                      Find in 2 places.
 *
 *          The following is an example:
 *
 *              } else {
 *                      int tmp = ds->ds_count;
 *                      runix->data = _renew(runix->data, HOWMANY(ds->ds_count,
 *                                      1) * sizeof(char));
 *                      if (runix->data == 0) goto out;
 *                      if (!ds_bytes(ds, runix->data, tmp))
 *                              ds->ds_count = 0;
 *                      pv_showvarfield(pv, &runix_fields[13], runix->data, tmp, *                                      "%.*s", tmp, runix->data);
 *              }
 */
#line 30 "ftp.c"

#define ftpfid_cmdreply 11
#define ftpfid_data 12

static ProtoField ftpdata1_element =
	{0,0,0,13,(void*)DS_SIGN_EXTEND,1,0,EXOP_NUMBER,2};

static ProtoField ftpcmdreply1_element =
	{0,0,0,14,(void*)DS_SIGN_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField ftp_fields[] = {
	{"cmdreply",8,"FTP command or reply",ftpfid_cmdreply,&ftpcmdreply1_element,0,0,EXOP_ARRAY,0},
	{"data",4,"FTP data",ftpfid_data,&ftpdata1_element,0,0,EXOP_ARRAY,2},
};

struct ftp {
	ProtoStackFrame _psf;
	char *cmdreply;
	char *data;
}; /* ftp */

static struct ftp ftp_frame;

static int	ftp_init(void);
static ExprType	ftp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc);
static int	ftp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex);
static void	ftp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv);

Protocol ftp_proto = {
	"ftp",3,"File Transfer Protocol",PRID_FTP,
	DS_BIG_ENDIAN,0,0,
	ftp_init,pr_stub_setopt,pr_stub_embed,pr_stub_resolve,
	ftp_compile,pr_stub_match,ftp_fetch,ftp_decode,
	0,0,0,0
};

static int
ftp_init()
{
	if (!pr_register(&ftp_proto, ftp_fields, lengthof(ftp_fields),
			 1	/* scopeload */)) {
		return 0;
	}
	if (!(pr_nest(&ftp_proto, PRID_TCP, 20, 0, 0) &
	      pr_nest(&ftp_proto, PRID_TCP, 21, 0, 0))) {
		return 0;
	}

	return 1;
}

static ExprType
ftp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
} /* ftp_compile */

static int
ftp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct ftp *ftp = &ftp_frame;
	struct tcp *tcp;
	int ok = 0, fid = pf->pf_id;

	PS_PUSH(ps, &ftp->_psf, &ftp_proto);
	if (fid >= 13) {
		switch (fid) {
		  case 13:
			if (!ds_seek(ds, 0 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			ftp->data = _renew(ftp->data, HOWMANY(ds->ds_count, 1) * sizeof(char));
			if (ftp->data == 0) goto out;
			if (!ds_char(ds, &ftp->data[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = ftp->data[pf->pf_cookie];
			ok = 1; goto out;
		  case 14:
			if (!ds_seek(ds, 0 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			ftp->cmdreply = _renew(ftp->cmdreply, HOWMANY(ds->ds_count, 1) * sizeof(char));
			if (ftp->cmdreply == 0) goto out;
			if (!ds_char(ds, &ftp->cmdreply[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = ftp->cmdreply[pf->pf_cookie];
			ok = 1; goto out;
		}
	}
	tcp = (struct tcp *)(ps->ps_top->psf_down);
	if ((*tcp).sport==21||(*tcp).dport==21) {
		if (fid == ftpfid_cmdreply) {
			rex->ex_op = EXOP_STRING;
			rex->ex_str.s_ptr = (char *) ds->ds_next;
			rex->ex_str.s_len = ds->ds_count;
			ok = 1; goto out;
		}
		if (!ds_seek(ds, 1 * ds->ds_count, DS_RELATIVE)) goto out;
	} else
	if ((*tcp).sport==20||(*tcp).dport==20) {
		if (fid == ftpfid_data) {
			rex->ex_op = EXOP_STRING;
			rex->ex_str.s_ptr = (char *) ds->ds_next;
			rex->ex_str.s_len = ds->ds_count;
			ok = 1; goto out;
		}
		if (!ds_seek(ds, 1 * ds->ds_count, DS_RELATIVE)) goto out;
	}
out:
	PS_POP(ps);
	return ok;
} /* ftp_fetch */

static void
ftp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct ftp *ftp = &ftp_frame;
	struct tcp *tcp;
	Protocol *pr;
	int tmp;

	PS_PUSH(ps, &ftp->_psf, &ftp_proto);
	tcp = (struct tcp *)(ps->ps_top->psf_down);
	if ((*tcp).sport==21||(*tcp).dport==21) {
		tmp = ds->ds_count;
		ftp->cmdreply = _renew(ftp->cmdreply, HOWMANY(ds->ds_count, 1) * sizeof(char));
		if (ftp->cmdreply == 0) goto out;
		if (!ds_bytes(ds, ftp->cmdreply, tmp))
			ds->ds_count = 0;
		pv_showvarfield(pv, &ftp_fields[0], ftp->cmdreply, tmp,
			        "%.*s", tmp, ftp->cmdreply);
	} else
	if ((*tcp).sport==20||(*tcp).dport==20) {
		tmp = ds->ds_count;
		ftp->data = _renew(ftp->data, HOWMANY(ds->ds_count, 1) * sizeof(char));
		if (ftp->data == 0) goto out;
		if (!ds_bytes(ds, ftp->data, tmp))
			ds->ds_count = 0;
		pv_showvarfield(pv, &ftp_fields[1], ftp->data, tmp,
			        "%.*s", tmp, ftp->data);
	}
	pr = 0;
	pv_decodeframe(pv, pr, ds, ps);
out:
	PS_POP(ps);
} /* ftp_decode */
