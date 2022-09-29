#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "protoindex.h"
#include "tcp.h"

#line 21 "rcp.pi"

/*
 * Handcraft Reminder:
 * 	Pidl is not fixed, the following has to be done to the end
 *	of rcp_decode for a variable to be decoded properly:
 *	    In rcp_decode add the line "int tmp = ds->ds_count;"
 *			before the line "_renew".
 *	    Change "MIN(ds->ds_count, ds->ds_count)" to "tmp");
 *	    Change all "ds->ds_count" to "tmp" on the "pv_showvarfield" line.
 *			Find in 2 places.
 *
 *	    The following is an example:
 *
 *              } else {
 *                    int tmp = ds->ds_count;
 *                    rcp->data = _renew(rcp->data, HOWMANY(ds->ds_count,
 *					1) * sizeof(char));
 *                    if (rcp->data == 0) goto out;
 *                    if (!ds_bytes(ds, rcp->data, tmp))
 *                              ds->ds_count = 0;
 *                    pv_showvarfield(pv, &rcp_fields[13], rcp->data, tmp,
 *                                      "%.*s", tmp, rcp->data);
 *              }
 *
 *
 *      Comment out 4 lines after "if (runix_continue(ds)) {"
 *      in rcp_decode:
 *              { int tell = ds_tellbit(ds);
 *              if (!ds_u_short(ds, &rcp->cookie))
 *                      return;
 *              ds_seekbit(ds, tell, DS_ABSOLUTE); }
 */

#line 94 "rcp.pi"

static int
runix_continue(DataStream *ds)
{
    return ds->ds_count > 0;
}
#line 51 "rcp.c"

#define rcpfid_clnwin 11
#define rcpfid_bpad1 12
#define rcpfid_resumef 13
#define rcpfid_rawmode 14
#define rcpfid_bpad2 15
#define rcpfid_discard 16
#define rcpfid_bpad3 17
#define rcpfid_cookie 18
#define rcpfid_fixval 19
#define rcpfid_nrow 20
#define rcpfid_nchar 21
#define rcpfid_npixx 22
#define rcpfid_npixy 23
#define rcpfid_data 24

static ProtoField rcpdata1_element =
	{0,0,0,25,(void*)DS_SIGN_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField rcpfixval1_element =
	{0,0,0,26,(void*)DS_SIGN_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField rcp_fields[] = {
	{"clnwin",6,"Send Current Window Size",rcpfid_clnwin,(void*)DS_ZERO_EXTEND,-1,0,EXOP_NUMBER,0},
	{"bpad1",5,"Bit Padding",rcpfid_bpad1,(void*)DS_ZERO_EXTEND,-1,1,EXOP_NUMBER,2},
	{"resumef",7,"Resume Interception and Flow Control",rcpfid_resumef,(void*)DS_ZERO_EXTEND,-1,2,EXOP_NUMBER,0},
	{"rawmode",7,"Switch to Raw Mode",rcpfid_rawmode,(void*)DS_ZERO_EXTEND,-1,3,EXOP_NUMBER,0},
	{"bpad2",5,"Bit Padding",rcpfid_bpad2,(void*)DS_ZERO_EXTEND,-2,4,EXOP_NUMBER,2},
	{"discard",7,"Discard All Buffered Data",rcpfid_discard,(void*)DS_ZERO_EXTEND,-1,6,EXOP_NUMBER,0},
	{"bpad3",5,"Bit Padding",rcpfid_bpad3,(void*)DS_ZERO_EXTEND,-1,7,EXOP_NUMBER,2},
	{"cookie",6,"Magic Cookie",rcpfid_cookie,(void*)DS_ZERO_EXTEND,2,0,EXOP_NUMBER,1},
	{"fixval",6,"2 Bytes of 's'",rcpfid_fixval,&rcpfixval1_element,2,2,EXOP_ARRAY,1},
	{"nrow",4,"Number of Char Rows",rcpfid_nrow,(void*)DS_ZERO_EXTEND,2,4,EXOP_NUMBER,0},
	{"nchar",5,"Number of Char Per Row",rcpfid_nchar,(void*)DS_ZERO_EXTEND,2,6,EXOP_NUMBER,0},
	{"npixx",5,"Number of Pixels in X direction",rcpfid_npixx,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"npixy",5,"Number of Pixels in Y direction",rcpfid_npixy,(void*)DS_ZERO_EXTEND,2,10,EXOP_NUMBER,0},
	{"data",4,"Data",rcpfid_data,&rcpdata1_element,0,0,EXOP_ARRAY,1},
};

static Enumeration rcpbitval;
static Enumerator rcpbitval_values[] = {
	{"False",5,0},
	{"True",4,1},
};

struct rcp {
	ProtoStackFrame _psf;
	unsigned char clnwin:1;
	unsigned char bpad1:1;
	unsigned char resumef:1;
	unsigned char rawmode:1;
	unsigned char bpad2:2;
	unsigned char discard:1;
	unsigned char bpad3:1;
	unsigned short cookie;
	char fixval[2];
	unsigned short nrow;
	unsigned short nchar;
	unsigned short npixx;
	unsigned short npixy;
	char *data;
}; /* rcp */

static struct rcp rcp_frame;

static int	rcp_init(void);
static ExprType	rcp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc);
static int	rcp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex);
static void	rcp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv);

Protocol rcp_proto = {
	"rcp",3,"Remote Login/Shell Protocol",PRID_RCP,
	DS_BIG_ENDIAN,-1,0,
	rcp_init,pr_stub_setopt,pr_stub_embed,pr_stub_resolve,
	rcp_compile,pr_stub_match,rcp_fetch,rcp_decode,
	0,0,0,0
};

static int
rcp_init()
{
	if (!pr_register(&rcp_proto, rcp_fields, lengthof(rcp_fields),
			 1	/* scopeload */
			 + lengthof(rcpbitval_values))) {
		return 0;
	}
	if (!(pr_nest(&rcp_proto, PRID_TCP, 514, 0, 0))) {
		return 0;
	}
	en_init(&rcpbitval, rcpbitval_values, lengthof(rcpbitval_values),
		&rcp_proto);

	return 1;
}

static ExprType
rcp_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
} /* rcp_compile */

static int
rcp_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct rcp *rcp = &rcp_frame;
	struct tcp *tcp;
	int ok = 0, fid = pf->pf_id;

	PS_PUSH(ps, &rcp->_psf, &rcp_proto);
	if (fid >= 25) {
		switch (fid) {
		  case 25:
			if (!ds_seek(ds, 0 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			rcp->data = _renew(rcp->data, HOWMANY(ds->ds_count, 1) * sizeof(char));
			if (rcp->data == 0) goto out;
			if (!ds_char(ds, &rcp->data[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = rcp->data[pf->pf_cookie];
			ok = 1; goto out;
		  case 26:
			if (!ds_seek(ds, 2 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_char(ds, &rcp->fixval[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = rcp->fixval[pf->pf_cookie];
			ok = 1; goto out;
		}
	}
	tcp = (struct tcp *)(ps->ps_top->psf_down);
	if ((*tcp).urp) {
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_clnwin) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->clnwin = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_bpad1) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->bpad1 = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_resumef) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->resumef = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_rawmode) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->rawmode = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 2, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_bpad2) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->bpad2 = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_discard) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->discard = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rcpfid_bpad3) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rcp->bpad3 = rex->ex_val;
	}
	if (runix_continue(ds)) {
		{ int tell = ds_tellbit(ds);
		if (!ds_u_short(ds, &rcp->cookie))
			goto out;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		if ((*rcp).cookie==65535) {
			if (!ds_u_short(ds, &rcp->cookie))
				goto out;
			if (fid == rcpfid_cookie) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rcp->cookie;
				ok = 1; goto out;
			}
			if (fid == rcpfid_fixval) {
				rex->ex_op = EXOP_STRING;
				rex->ex_str.s_ptr = (char *) ds->ds_next;
				rex->ex_str.s_len = MIN(2, ds->ds_count);
				ok = 1; goto out;
			}
			if (!ds_seek(ds, 1 * 2, DS_RELATIVE)) goto out;
			if (!ds_u_short(ds, &rcp->nrow))
				goto out;
			if (fid == rcpfid_nrow) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rcp->nrow;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &rcp->nchar))
				goto out;
			if (fid == rcpfid_nchar) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rcp->nchar;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &rcp->npixx))
				goto out;
			if (fid == rcpfid_npixx) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rcp->npixx;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &rcp->npixy))
				goto out;
			if (fid == rcpfid_npixy) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rcp->npixy;
				ok = 1; goto out;
			}
		} else {
			if (fid == rcpfid_data) {
				rex->ex_op = EXOP_STRING;
				rex->ex_str.s_ptr = (char *) ds->ds_next;
				rex->ex_str.s_len = ds->ds_count;
				ok = 1; goto out;
			}
			if (!ds_seek(ds, 1 * ds->ds_count, DS_RELATIVE)) goto out;
		}
	}
out:
	PS_POP(ps);
	return ok;
} /* rcp_fetch */

static void
rcp_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct rcp *rcp = &rcp_frame;
	struct tcp *tcp;
	Protocol *pr;
	long val;

	PS_PUSH(ps, &rcp->_psf, &rcp_proto);
	tcp = (struct tcp *)(ps->ps_top->psf_down);
	if ((*tcp).urp) {
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->clnwin = val;
		pv_showfield(pv, &rcp_fields[0], &val,
			     "%-5s", en_name(&rcpbitval, rcp->clnwin));
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->bpad1 = val;
		pv_showfield(pv, &rcp_fields[1], &val,
			     "%-3u", rcp->bpad1);
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->resumef = val;
		pv_showfield(pv, &rcp_fields[2], &val,
			     "%-5s", en_name(&rcpbitval, rcp->resumef));
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->rawmode = val;
		pv_showfield(pv, &rcp_fields[3], &val,
			     "%-5s", en_name(&rcpbitval, rcp->rawmode));
		if (!ds_bits(ds, &val, 2, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->bpad2 = val;
		pv_showfield(pv, &rcp_fields[4], &val,
			     "%-3u", rcp->bpad2);
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->discard = val;
		pv_showfield(pv, &rcp_fields[5], &val,
			     "%-5s", en_name(&rcpbitval, rcp->discard));
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rcp->bpad3 = val;
		pv_showfield(pv, &rcp_fields[6], &val,
			     "%-3u", rcp->bpad3);
	}
	if (runix_continue(ds)) {
		/*
		{ int tell = ds_tellbit(ds);
		if (!ds_u_short(ds, &rcp->cookie))
			return;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		*/
		if ((*rcp).cookie==65535) {
			if (!ds_u_short(ds, &rcp->cookie))
				ds->ds_count = 0;
			pv_showfield(pv, &rcp_fields[7], &rcp->cookie,
				     "%-5u [%u]", rcp->cookie, 65535);
			if (!ds_bytes(ds, rcp->fixval, MIN(ds->ds_count, 2)))
				ds->ds_count = 0;
			pv_showfield(pv, &rcp_fields[8], rcp->fixval,
				        "%.*s", 2, rcp->fixval);
			if (!ds_u_short(ds, &rcp->nrow))
				ds->ds_count = 0;
			pv_showfield(pv, &rcp_fields[9], &rcp->nrow,
				     "%-5u", rcp->nrow);
			if (!ds_u_short(ds, &rcp->nchar))
				ds->ds_count = 0;
			pv_showfield(pv, &rcp_fields[10], &rcp->nchar,
				     "%-5u", rcp->nchar);
			if (!ds_u_short(ds, &rcp->npixx))
				ds->ds_count = 0;
			pv_showfield(pv, &rcp_fields[11], &rcp->npixx,
				     "%-5u", rcp->npixx);
			if (!ds_u_short(ds, &rcp->npixy))
				ds->ds_count = 0;
			pv_showfield(pv, &rcp_fields[12], &rcp->npixy,
				     "%-5u", rcp->npixy);
		} else {
			int tmp = ds->ds_count;
			rcp->data = _renew(rcp->data, HOWMANY(ds->ds_count, 1) * sizeof(char));
			if (rcp->data == 0) goto out;
			if (!ds_bytes(ds, rcp->data, tmp))
				ds->ds_count = 0;
			pv_showvarfield(pv, &rcp_fields[13], rcp->data, tmp,
				        "%.*s", tmp, rcp->data);
		}
	}
	pr = 0;
	pv_decodeframe(pv, pr, ds, ps);
out:
	PS_POP(ps);
} /* rcp_decode */
