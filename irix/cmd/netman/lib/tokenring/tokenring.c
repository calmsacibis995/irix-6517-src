#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "protoindex.h"
#include "tokenring.h"

#line 22 "tokenring.pi"

/*
 * Hand crafted reminder:
 *	- Search for "Protocol tokenring_proto" change from 20 to 34
 *	- Search for "(fid >= 19)" and change that line to
 *		      (fid >= 19 && fid < 22)
 *	- Search for "eaddr_to_name(tokenring->dst)" and change to
 *		     "ether_hostname((struct etheraddr*)tokenring->dst)".
 *		In 2 places.
 *	- Search for "if ((*tokenring).src) {" and change to
 *		     "if ((*tokenring).src[0] & 0x80) {"
 *		In 2 places.
 */
#line 22 "tokenring.c"

#define tokenringroutedesfid_rnum 20
#define tokenringroutedesfid_ibridge 21

static ProtoField tokenringroutedes_fields[] = {
	{"rnum",4,"Ring Number",tokenringroutedesfid_rnum,(void*)DS_ZERO_EXTEND,-12,0,EXOP_NUMBER,0},
	{"ibridge",7,"Individual Bridge",tokenringroutedesfid_ibridge,(void*)DS_ZERO_EXTEND,-4,12,EXOP_NUMBER,0},
};

struct tokenringroutedes {
	unsigned short rnum:12;
	unsigned char ibridge:4;
}; /* tokenringroutedes */

static ProtoStruct tokenringroutedes_struct = PSI("routedes",tokenringroutedes_fields);

#define tokenringfid_prior 1
#define tokenringfid_tk 2
#define tokenringfid_mon 3
#define tokenringfid_resv 4
#define tokenringfid_res 6
#define tokenringfid_ctrl 7
#define tokenringfid_srb 10
#define tokenringfid_arb 11
#define tokenringfid_nb 12
#define tokenringfid_len 13
#define tokenringfid_dir 14
#define tokenringfid_lf 15
#define tokenringfid_resb 16
#define tokenringfid_rd 17

static ProtoField tokenringrd1_element =
	{0,0,0,18,&tokenringroutedes_struct,2,0,EXOP_STRUCT,1};

static ProtoField tokenring_fields[] = {
	{"prior",5,"Priority Bits",tokenringfid_prior,(void*)DS_ZERO_EXTEND,-3,0,EXOP_NUMBER,1},
	{"tk",2,"Token Bit",tokenringfid_tk,(void*)DS_ZERO_EXTEND,-1,3,EXOP_NUMBER,1},
	{"mon",3,"Monitor Bit",tokenringfid_mon,(void*)DS_ZERO_EXTEND,-1,4,EXOP_NUMBER,1},
	{"resv",4,"Reservation Bits",tokenringfid_resv,(void*)DS_ZERO_EXTEND,-3,5,EXOP_NUMBER,2},
	{"ftype",5,"Frame Type Bits",tokenringfid_ftype,(void*)DS_ZERO_EXTEND,-2,8,EXOP_NUMBER,1},
	{"res",3,"Reserved Bits",tokenringfid_res,(void*)DS_ZERO_EXTEND,-2,10,EXOP_NUMBER,2},
	{"ctrl",4,"Control Bits",tokenringfid_ctrl,(void*)DS_ZERO_EXTEND,-4,12,EXOP_NUMBER,1},
	{"dst",3,"Destionation Address",tokenringfid_dst,(void*)DS_ZERO_EXTEND,6,2,EXOP_ADDRESS,0},
	{"src",3,"Source Address",tokenringfid_src,(void*)DS_ZERO_EXTEND,6,8,EXOP_ADDRESS,0},
	{"srb",3,"Single Route Broadcast",tokenringfid_srb,(void*)DS_ZERO_EXTEND,-3,112,EXOP_NUMBER,1},
	{"arb",3,"All Route Broadcast",tokenringfid_arb,(void*)DS_ZERO_EXTEND,-3,112,EXOP_NUMBER,1},
	{"nb",2,"Non Broadcast",tokenringfid_nb,(void*)DS_ZERO_EXTEND,-3,112,EXOP_NUMBER,1},
	{"len",3,"Length Bits",tokenringfid_len,(void*)DS_ZERO_EXTEND,-5,0,EXOP_NUMBER,1},
	{"dir",3,"Direction Bits",tokenringfid_dir,(void*)DS_ZERO_EXTEND,-1,5,EXOP_NUMBER,1},
	{"lf",2,"Largest Frame Bits",tokenringfid_lf,(void*)DS_ZERO_EXTEND,-3,6,EXOP_NUMBER,1},
	{"resb",4,"Reserved Bits",tokenringfid_resb,(void*)DS_ZERO_EXTEND,-4,9,EXOP_NUMBER,2},
	{"rd",2,"Route Designator Field",tokenringfid_rd,&tokenringrd1_element,0,1,EXOP_ARRAY,1},
};

static Enumeration tokenringfrmtype;
static Enumerator tokenringfrmtype_values[] = {
	{"MACFrame",8,0},
	{"LLCFrame",8,1},
	{"Undefine",8,2},
	{"Undefined",9,3},
};

static Enumeration tokenringdbit;
static Enumerator tokenringdbit_values[] = {
	{"LeftToRight",11,0},
	{"RightToLeft",11,1},
};

static Enumeration tokenringfrmbit;
static Enumerator tokenringfrmbit_values[] = {
	{"LargestIs516",12,0},
	{"LargestIs1500",13,1},
	{"LargestIs2052",13,2},
	{"LargestIs4472",13,3},
	{"LargestIs8144",13,4},
	{"LargestIs11407",14,5},
	{"LargestIs17800",14,6},
	{"AllRoutesBroadcast",18,7},
};

static struct tokenring tokenring_frame;

static ProtoIndex *tokenring_index;

static int	tokenring_init(void);
static void	tokenring_embed(Protocol *pr, long type, long qual);
static ExprType	tokenring_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc);
static int	tokenring_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex);
static int	tokenring_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex);
static void	tokenring_decode(DataStream *ds, ProtoStack *ps, PacketView *pv);

Protocol tokenring_proto = {
	"tokenring",9,"Token Ring Medium Access Control Header",PRID_TOKENRING,
	DS_BIG_ENDIAN,34,0,
	tokenring_init,pr_stub_setopt,tokenring_embed,pr_stub_resolve,
	tokenring_compile,tokenring_match,tokenring_fetch,tokenring_decode,
	0,0,0,0
};

static int
tokenring_init()
{
	if (!pr_register(&tokenring_proto, tokenring_fields, lengthof(tokenring_fields),
			 1	/* scopeload */
			 + lengthof(tokenringfrmtype_values)
			 + lengthof(tokenringdbit_values)
			 + lengthof(tokenringfrmbit_values))) {
		return 0;
	}
	en_init(&tokenringfrmtype, tokenringfrmtype_values, lengthof(tokenringfrmtype_values),
		&tokenring_proto);
	en_init(&tokenringdbit, tokenringdbit_values, lengthof(tokenringdbit_values),
		&tokenring_proto);
	en_init(&tokenringfrmbit, tokenringfrmbit_values, lengthof(tokenringfrmbit_values),
		&tokenring_proto);

	pin_create("tokenring", 0, 8, &tokenring_index);

	return 1;
}

static void
tokenring_embed(Protocol *pr, long type, long qual)
{
	if (pr)
		pin_enter(tokenring_index, type, qual, pr);
	else
		pin_remove(tokenring_index, type, qual);
} /* tokenring_embed */

static ExprType
tokenring_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
} /* tokenring_compile */

static int
tokenring_match(Expr *pex, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tokenring *tokenring = &tokenring_frame;
	ProtoField all;
	Protocol *pr;
	int matched;

	all.pf_id = 22;
	(void) tokenring_fetch(&all, ds, ps, rex);
	PS_PUSH(ps, &tokenring->_psf, &tokenring_proto);
	tokenring = (struct tokenring *)(ps->ps_top);
	pr = pin_match(tokenring_index, (*tokenring).ftype, 0);
	matched = (pr == pex->ex_prsym->sym_proto
		&& ex_match(pex, ds, ps, rex));
	PS_POP(ps);
	return matched;
} /* tokenring_match */

static int
tokenringroutedes_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tokenringroutedes *routedes = ps->ps_slink;
	int ok = 0, fid = pf->pf_id;

	if (!ds_bits(ds, &rex->ex_val, 12, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringroutedesfid_rnum) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	routedes->rnum = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringroutedesfid_ibridge) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	routedes->ibridge = rex->ex_val;
out:
	return ok;
} /* tokenringroutedes_fetch */

static int
tokenring_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tokenring *tokenring = &tokenring_frame;
	int ok = 0, fid = pf->pf_id;

	PS_PUSH(ps, &tokenring->_psf, &tokenring_proto);
	switch (fid) {
	  case 18:
		if (!ds_seek(ds, 1 + 2 * pf->pf_cookie, DS_RELATIVE))
			goto out;
		tokenring->rd = renew(tokenring->rd, (*tokenring).len/2-1, struct tokenringroutedes);
		if (tokenring->rd == 0) goto out;
		rex->ex_op = EXOP_FIELD;
		rex->ex_field = pf;
		/* XXX inlineable struct element .rd1 */;
		ps->ps_slink = &tokenring->rd[pf->pf_cookie];
		ok = 1; goto out;
	}
	if (fid >= 19 && fid < 22) {
		ok = tokenringroutedes_fetch(pf, ds, ps, rex); goto out;
	}
	if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_prior) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->prior = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_tk) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->tk = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_mon) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->mon = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_resv) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->resv = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 2, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_ftype) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->ftype = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 2, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_res) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->res = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenringfid_ctrl) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenring->ctrl = rex->ex_val;
	if (!ds_bytes(ds, tokenring->dst, 6))
		goto out;
	if (fid == tokenringfid_dst) {
		rex->ex_op = EXOP_ADDRESS;
		bcopy(tokenring->dst, A_BASE(&rex->ex_addr, 6), 6);
		ok = 1; goto out;
	}
	if (!ds_bytes(ds, tokenring->src, 6))
		goto out;
	if (fid == tokenringfid_src) {
		rex->ex_op = EXOP_ADDRESS;
		bcopy(tokenring->src, A_BASE(&rex->ex_addr, 6), 6);
		ok = 1; goto out;
	}
	if ((*tokenring).src[0] & 0x80) {
		{ int tell = ds_tellbit(ds);
		if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
			goto out;
		tokenring->srb = rex->ex_val;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		switch ((*tokenring).srb&6) {
		  case 6:
			if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
				goto out;
			if (fid == tokenringfid_srb) {
				rex->ex_op = EXOP_NUMBER;
				ok = 1; goto out;
			}
			tokenring->srb = rex->ex_val;
			break;
		  case 4:
			if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
				goto out;
			if (fid == tokenringfid_arb) {
				rex->ex_op = EXOP_NUMBER;
				ok = 1; goto out;
			}
			tokenring->arb = rex->ex_val;
			break;
		  default:
			if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
				goto out;
			if (fid == tokenringfid_nb) {
				rex->ex_op = EXOP_NUMBER;
				ok = 1; goto out;
			}
			tokenring->nb = rex->ex_val;
			break;
		}
		if (!ds_bits(ds, &rex->ex_val, 5, DS_ZERO_EXTEND))
			goto out;
		if (fid == tokenringfid_len) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		tokenring->len = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
			goto out;
		if (fid == tokenringfid_dir) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		tokenring->dir = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 3, DS_ZERO_EXTEND))
			goto out;
		if (fid == tokenringfid_lf) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		tokenring->lf = rex->ex_val;
		if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
			goto out;
		if (fid == tokenringfid_resb) {
			rex->ex_op = EXOP_NUMBER;
			ok = 1; goto out;
		}
		tokenring->resb = rex->ex_val;
		if (fid == tokenringfid_rd) {
			rex->ex_op = EXOP_FIELD;
			rex->ex_field = &tokenringrd1_element;
			ok = 1; goto out;
		}
		if (!ds_seek(ds, 2 * (*tokenring).len/2-1, DS_RELATIVE)) goto out;
	}
out:
	PS_POP(ps);
	return ok;
} /* tokenring_fetch */

static void
tokenringroutedes_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct tokenringroutedes *routedes = ps->ps_slink;
	long val;

	pv_push(pv, ps->ps_top->psf_proto,
		tokenringroutedes_struct.pst_parent->pf_name,
		tokenringroutedes_struct.pst_parent->pf_namlen,
		tokenringroutedes_struct.pst_parent->pf_title);
	if (!ds_bits(ds, &val, 12, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	routedes->rnum = val;
	pv_showfield(pv, &tokenringroutedes_fields[0], &val,
		     "%-5u", routedes->rnum);
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	routedes->ibridge = val;
	pv_showfield(pv, &tokenringroutedes_fields[1], &val,
		     "%-3u", routedes->ibridge);
out:	pv_pop(pv);
} /* tokenringroutedes_decode */

static void
tokenring_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct tokenring *tokenring = &tokenring_frame;
	Protocol *pr;
	long val;

	PS_PUSH(ps, &tokenring->_psf, &tokenring_proto);
	if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->prior = val;
	pv_showfield(pv, &tokenring_fields[0], &val,
		     "%-3u", tokenring->prior);
	if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->tk = val;
	pv_showfield(pv, &tokenring_fields[1], &val,
		     "%-3u", tokenring->tk);
	if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->mon = val;
	pv_showfield(pv, &tokenring_fields[2], &val,
		     "%-3u", tokenring->mon);
	if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->resv = val;
	pv_showfield(pv, &tokenring_fields[3], &val,
		     "%-3u", tokenring->resv);
	if (!ds_bits(ds, &val, 2, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->ftype = val;
	pv_showfield(pv, &tokenring_fields[4], &val,
		     "%-9s", en_name(&tokenringfrmtype, tokenring->ftype));
	if (!ds_bits(ds, &val, 2, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->res = val;
	pv_showfield(pv, &tokenring_fields[5], &val,
		     "%-3u", tokenring->res);
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenring->ctrl = val;
	pv_showfield(pv, &tokenring_fields[6], &val,
		     "%-3u", tokenring->ctrl);
	if (!ds_bytes(ds, tokenring->dst, 6))
		ds->ds_count = 0;
	pv_showfield(pv, &tokenring_fields[7], tokenring->dst,
		     "%s", ether_hostname((struct etheraddr*)tokenring->dst));
	if (!ds_bytes(ds, tokenring->src, 6))
		ds->ds_count = 0;
	pv_showfield(pv, &tokenring_fields[8], tokenring->src,
		     "%s", ether_hostname((struct etheraddr*)tokenring->src));
	if ((*tokenring).src[0] & 0x80) {
		{ int tell = ds_tellbit(ds);
		if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
			return;
		tokenring->srb = val;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		switch ((*tokenring).srb&6) {
		  case 6:
			if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
				ds->ds_count = 0;
			tokenring->srb = val;
			pv_showfield(pv, &tokenring_fields[9], &val,
				     "%-3u", tokenring->srb);
			break;
		  case 4:
			if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
				ds->ds_count = 0;
			tokenring->arb = val;
			pv_showfield(pv, &tokenring_fields[10], &val,
				     "%-3u", tokenring->arb);
			break;
		  default:
			if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
				ds->ds_count = 0;
			tokenring->nb = val;
			pv_showfield(pv, &tokenring_fields[11], &val,
				     "%-3u", tokenring->nb);
			break;
		}
		if (!ds_bits(ds, &val, 5, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		tokenring->len = val;
		pv_showfield(pv, &tokenring_fields[12], &val,
			     "%-3u", tokenring->len);
		if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		tokenring->dir = val;
		pv_showfield(pv, &tokenring_fields[13], &val,
			     "%-11s", en_name(&tokenringdbit, tokenring->dir));
		if (!ds_bits(ds, &val, 3, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		tokenring->lf = val;
		pv_showfield(pv, &tokenring_fields[14], &val,
			     "%-18s", en_name(&tokenringfrmbit, tokenring->lf));
		if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
			ds->ds_count = 0;
		tokenring->resb = val;
		pv_showfield(pv, &tokenring_fields[15], &val,
			     "%-3u", tokenring->resb);
		{ int i0;
		tokenring->rd = renew(tokenring->rd, (*tokenring).len/2-1, struct tokenringroutedes);
		if (tokenring->rd == 0) goto out;
		for (i0 = 0; i0 < (*tokenring).len/2-1; i0++) {
			char name[16], title[36];
			tokenringrd1_element.pf_namlen = nsprintf(name, sizeof name, "rd[%d]", i0);
			nsprintf(title, sizeof title, "Route Designator Field[%d]", i0);
			tokenringrd1_element.pf_name = name, tokenringrd1_element.pf_title = title;
			ps->ps_slink = &tokenring->rd[i0];
			tokenringroutedes_struct.pst_parent = &tokenringrd1_element;
			tokenringroutedes_decode(ds, ps, pv);
		} /* for i0 */
		}
	}
	tokenring = (struct tokenring *)(ps->ps_top);
	pr = pin_match(tokenring_index, (*tokenring).ftype, 0);
	pv_decodeframe(pv, pr, ds, ps);
out:
	PS_POP(ps);
} /* tokenring_decode */
