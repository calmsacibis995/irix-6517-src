#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "protoindex.h"
#include "tcp.h"

#line 21 "rlogin.pi"

/*
 * Handcraft Reminder:
 * 	Pidl is not fixed, the following has to be done to the end
 *	of rlogin_decode for a variable to be decoded properly:
 *	    In rlogin_decode add the line "int tmp = ds->ds_count;"
 *			before the line "_renew".
 *	    Change "MIN(ds->ds_count, ds->ds_count)" to "tmp");
 *	    Change all "ds->ds_count" to "tmp" on the "pv_showvarfield" line.
 *			Find in 2 places.
 *
 *	    The following is an example:
 *
 *              } else {
 *                    int tmp = ds->ds_count;
 *                    rlogin->data = _renew(rlogin->data, HOWMANY(ds->ds_count,
 *					1) * sizeof(char));
 *                    if (rlogin->data == 0) goto out;
 *                    if (!ds_bytes(ds, rlogin->data, tmp))
 *                              ds->ds_count = 0;
 *                    pv_showvarfield(pv, &rlogin_fields[13], rlogin->data, tmp,
 *                                      "%.*s", tmp, rlogin->data);
 *              }
 *
 *
 *	Comment out 4 lines after "if (runix_continue(ds)) {"
 *	in rlogin_decode:
 *		{ int tell = ds_tellbit(ds);
 *		if (!ds_u_short(ds, &rlogin->cookie))
 *		        return;
 *		ds_seekbit(ds, tell, DS_ABSOLUTE); }
 */

#line 93 "rlogin.pi"

static int
runix_continue(DataStream *ds)
{
    return ds->ds_count > 0;
}
#line 51 "rlogin.c"

#define rloginfid_clnwin 11
#define rloginfid_bpad1 12
#define rloginfid_resumef 13
#define rloginfid_rawmode 14
#define rloginfid_bpad2 15
#define rloginfid_discard 16
#define rloginfid_bpad3 17
#define rloginfid_cookie 18
#define rloginfid_fixval 19
#define rloginfid_nrow 20
#define rloginfid_nchar 21
#define rloginfid_npixx 22
#define rloginfid_npixy 23
#define rloginfid_data 24

static ProtoField rlogindata1_element =
	{0,0,0,25,(void*)DS_SIGN_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField rloginfixval1_element =
	{0,0,0,26,(void*)DS_SIGN_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField rlogin_fields[] = {
	{"clnwin",6,"Send Current Window Size",rloginfid_clnwin,(void*)DS_ZERO_EXTEND,-1,0,EXOP_NUMBER,0},
	{"bpad1",5,"Bit Padding",rloginfid_bpad1,(void*)DS_ZERO_EXTEND,-1,1,EXOP_NUMBER,2},
	{"resumef",7,"Resume Interception and Flow Control",rloginfid_resumef,(void*)DS_ZERO_EXTEND,-1,2,EXOP_NUMBER,0},
	{"rawmode",7,"Switch to Raw Mode",rloginfid_rawmode,(void*)DS_ZERO_EXTEND,-1,3,EXOP_NUMBER,0},
	{"bpad2",5,"Bit Padding",rloginfid_bpad2,(void*)DS_ZERO_EXTEND,-2,4,EXOP_NUMBER,2},
	{"discard",7,"Discard All Buffered Data",rloginfid_discard,(void*)DS_ZERO_EXTEND,-1,6,EXOP_NUMBER,0},
	{"bpad3",5,"Bit Padding",rloginfid_bpad3,(void*)DS_ZERO_EXTEND,-1,7,EXOP_NUMBER,2},
	{"cookie",6,"Magic Cookie",rloginfid_cookie,(void*)DS_ZERO_EXTEND,2,0,EXOP_NUMBER,1},
	{"fixval",6,"2 Bytes of 's'",rloginfid_fixval,&rloginfixval1_element,2,2,EXOP_ARRAY,1},
	{"nrow",4,"Number of Char Rows",rloginfid_nrow,(void*)DS_ZERO_EXTEND,2,4,EXOP_NUMBER,0},
	{"nchar",5,"Number of Char Per Row",rloginfid_nchar,(void*)DS_ZERO_EXTEND,2,6,EXOP_NUMBER,0},
	{"npixx",5,"Number of Pixels in X direction",rloginfid_npixx,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"npixy",5,"Number of Pixels in Y direction",rloginfid_npixy,(void*)DS_ZERO_EXTEND,2,10,EXOP_NUMBER,0},
	{"data",4,"Data",rloginfid_data,&rlogindata1_element,0,0,EXOP_ARRAY,1},
};

static Enumeration rloginbitval;
static Enumerator rloginbitval_values[] = {
	{"False",5,0},
	{"True",4,1},
};

struct rlogin {
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
}; /* rlogin */

static struct rlogin rlogin_frame;

static int	rlogin_init(void);
static ExprType	rlogin_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc);
static int	rlogin_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex);
static void	rlogin_decode(DataStream *ds, ProtoStack *ps, PacketView *pv);

Protocol rlogin_proto = {
	"rlogin",6,"Remote Login/Shell Protocol",PRID_RLOGIN,
	DS_BIG_ENDIAN,-1,0,
	rlogin_init,pr_stub_setopt,pr_stub_embed,pr_stub_resolve,
	rlogin_compile,pr_stub_match,rlogin_fetch,rlogin_decode,
	0,0,0,0
};

static int
rlogin_init()
{
	if (!pr_register(&rlogin_proto, rlogin_fields, lengthof(rlogin_fields),
			 1	/* scopeload */
			 + lengthof(rloginbitval_values))) {
		return 0;
	}
	if (!(pr_nest(&rlogin_proto, PRID_TCP, 513, 0, 0))) {
		return 0;
	}
	en_init(&rloginbitval, rloginbitval_values, lengthof(rloginbitval_values),
		&rlogin_proto);

	return 1;
}

static ExprType
rlogin_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
} /* rlogin_compile */

static int
rlogin_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct rlogin *rlogin = &rlogin_frame;
	struct tcp *tcp;
	int ok = 0, fid = pf->pf_id;

	PS_PUSH(ps, &rlogin->_psf, &rlogin_proto);
	if (fid >= 25) {
		switch (fid) {
		  case 25:
			if (!ds_seek(ds, 0 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			rlogin->data = _renew(rlogin->data, HOWMANY(ds->ds_count, 1) * sizeof(char));
			if (rlogin->data == 0) goto out;
			if (!ds_char(ds, &rlogin->data[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = rlogin->data[pf->pf_cookie];
			ok = 1; goto out;
		  case 26:
			if (!ds_seek(ds, 2 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_char(ds, &rlogin->fixval[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = rlogin->fixval[pf->pf_cookie];
			ok = 1; goto out;
		}
	}
	tcp = (struct tcp *)(ps->ps_top->psf_down);
	if ((*tcp).urp) {
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_clnwin) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->clnwin = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_bpad1) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->bpad1 = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_resumef) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->resumef = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_rawmode) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->rawmode = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 2, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_bpad2) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->bpad2 = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_discard) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->discard = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == rloginfid_bpad3) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		rlogin->bpad3 = rex->ex_val;
	}
	if (runix_continue(ds)) {
		{ int tell = ds_tellbit(ds);
		if (!ds_u_short(ds, &rlogin->cookie))
			goto out;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		if ((*rlogin).cookie==65535) {
			if (!ds_u_short(ds, &rlogin->cookie))
				goto out;
			if (fid == rloginfid_cookie) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rlogin->cookie;
				ok = 1; goto out;
			}
			if (fid == rloginfid_fixval) {
				rex->ex_op = EXOP_STRING;
				rex->ex_str.s_ptr = (char *) ds->ds_next;
				rex->ex_str.s_len = MIN(2, ds->ds_count);
				ok = 1; goto out;
			}
			if (!ds_seek(ds, 1 * 2, DS_RELATIVE)) goto out;
			if (!ds_u_short(ds, &rlogin->nrow))
				goto out;
			if (fid == rloginfid_nrow) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rlogin->nrow;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &rlogin->nchar))
				goto out;
			if (fid == rloginfid_nchar) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rlogin->nchar;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &rlogin->npixx))
				goto out;
			if (fid == rloginfid_npixx) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rlogin->npixx;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &rlogin->npixy))
				goto out;
			if (fid == rloginfid_npixy) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = rlogin->npixy;
				ok = 1; goto out;
			}
		} else {
			if (fid == rloginfid_data) {
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
} /* rlogin_fetch */

static void
rlogin_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct rlogin *rlogin = &rlogin_frame;
	struct tcp *tcp;
	Protocol *pr;
	long val;

	PS_PUSH(ps, &rlogin->_psf, &rlogin_proto);
	tcp = (struct tcp *)(ps->ps_top->psf_down);
	if ((*tcp).urp) {
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->clnwin = val;
		pv_showfield(pv, &rlogin_fields[0], &val,
			     "%-5s", en_name(&rloginbitval, rlogin->clnwin));
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->bpad1 = val;
		pv_showfield(pv, &rlogin_fields[1], &val,
			     "%-3u", rlogin->bpad1);
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->resumef = val;
		pv_showfield(pv, &rlogin_fields[2], &val,
			     "%-5s", en_name(&rloginbitval, rlogin->resumef));
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->rawmode = val;
		pv_showfield(pv, &rlogin_fields[3], &val,
			     "%-5s", en_name(&rloginbitval, rlogin->rawmode));
		if (!ds_bits(ds, &val, 2, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->bpad2 = val;
		pv_showfield(pv, &rlogin_fields[4], &val,
			     "%-3u", rlogin->bpad2);
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->discard = val;
		pv_showfield(pv, &rlogin_fields[5], &val,
			     "%-5s", en_name(&rloginbitval, rlogin->discard));
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		rlogin->bpad3 = val;
		pv_showfield(pv, &rlogin_fields[6], &val,
			     "%-3u", rlogin->bpad3);
	}
	if (runix_continue(ds)) {
		/*
		{ int tell = ds_tellbit(ds);
		if (!ds_u_short(ds, &rlogin->cookie))
			return;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		*/
		if ((*rlogin).cookie==65535) {
			if (!ds_u_short(ds, &rlogin->cookie))
				ds->ds_count = 0;
			pv_showfield(pv, &rlogin_fields[7], &rlogin->cookie,
				     "%-5u [%u]", rlogin->cookie, 65535);
			if (!ds_bytes(ds, rlogin->fixval, MIN(ds->ds_count, 2)))
				ds->ds_count = 0;
			pv_showfield(pv, &rlogin_fields[8], rlogin->fixval,
				        "%.*s", 2, rlogin->fixval);
			if (!ds_u_short(ds, &rlogin->nrow))
				ds->ds_count = 0;
			pv_showfield(pv, &rlogin_fields[9], &rlogin->nrow,
				     "%-5u", rlogin->nrow);
			if (!ds_u_short(ds, &rlogin->nchar))
				ds->ds_count = 0;
			pv_showfield(pv, &rlogin_fields[10], &rlogin->nchar,
				     "%-5u", rlogin->nchar);
			if (!ds_u_short(ds, &rlogin->npixx))
				ds->ds_count = 0;
			pv_showfield(pv, &rlogin_fields[11], &rlogin->npixx,
				     "%-5u", rlogin->npixx);
			if (!ds_u_short(ds, &rlogin->npixy))
				ds->ds_count = 0;
			pv_showfield(pv, &rlogin_fields[12], &rlogin->npixy,
				     "%-5u", rlogin->npixy);
		} else {
			int tmp = ds->ds_count;
			rlogin->data = _renew(rlogin->data, HOWMANY(ds->ds_count, 1) * sizeof(char));
			if (rlogin->data == 0) goto out;
			if (!ds_bytes(ds, rlogin->data, tmp))
				ds->ds_count = 0;
			pv_showvarfield(pv, &rlogin_fields[13], rlogin->data, tmp,
				        "%.*s", tmp, rlogin->data);
		}
	}
	pr = 0;
	pv_decodeframe(pv, pr, ds, ps);
out:
	PS_POP(ps);
} /* rlogin_decode */
