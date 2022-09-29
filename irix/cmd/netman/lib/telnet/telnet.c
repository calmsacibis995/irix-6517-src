#include "enum.h"
#include "protodefs.h"
#include "protoindex.h"
#include "tcp.h"

#line 119 "telnet.pi"

static int
telnet_continue(DataStream *ds)
{
	return ds->ds_count > 0;
}
#line 14 "telnet.c"

#define telnetfid_data 11
#define telnetfid_iac 12
#define telnetfid_data255 13
#define telnetfid_cmd 14
#define telnetfid_option 15
#define telnetfid_optdata 16

static ProtoField telnet_fields[] = {
	{"data",4,"Data",telnetfid_data,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,2},
	{"iac",3,"Interpret As Command",telnetfid_iac,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,2},
	{"data255",7,"Data",telnetfid_data255,(void*)DS_ZERO_EXTEND,1,1,EXOP_NUMBER,2},
	{"cmd",3,"Command",telnetfid_cmd,(void*)DS_ZERO_EXTEND,1,1,EXOP_NUMBER,0},
	{"option",6,"Option",telnetfid_option,(void*)DS_ZERO_EXTEND,1,2,EXOP_NUMBER,0},
	{"optdata",7,"Option Data",telnetfid_optdata,(void*)DS_ZERO_EXTEND,1,3,EXOP_NUMBER,2},
};

static Enumeration telnettelnetCommand;
static Enumerator telnettelnetCommand_values[] = {
	{"EndOfFile",9,236},
	{"Suspend",7,237},
	{"Abort",5,238},
	{"EndOfRecord",11,239},
	{"EndSubNegotiation",17,240},
	{"NoOperation",11,241},
	{"DataMark",8,242},
	{"Break",5,243},
	{"InterruptProcess",16,244},
	{"AbortOutput",11,245},
	{"AreYouThere",11,246},
	{"EraseChar",9,247},
	{"EraseLine",9,248},
	{"GoAhead",7,249},
	{"StartSubNegotiation",19,250},
	{"Will",4,251},
	{"Wont",4,252},
	{"Do",2,253},
	{"Dont",4,254},
	{"InterpreteAsCommand",19,255},
};

static Enumeration telnettelnetOption;
static Enumerator telnettelnetOption_values[] = {
	{"TransmitBinary",14,0},
	{"Echo",4,1},
	{"Reconnect",9,2},
	{"SuppressGoAhead",15,3},
	{"MessageSize",11,4},
	{"Status",6,5},
	{"TimingMark",10,6},
	{"RemoteControl",13,7},
	{"NegotiateLineWidth",18,8},
	{"NegotiatePageSize",17,9},
	{"NegotiateCR",11,10},
	{"NegotiateHTabStops",18,11},
	{"NegotiateHTabs",14,12},
	{"NegotiateFormFeed",17,13},
	{"NegotiateVTabStops",18,14},
	{"NegotiateVTabs",14,15},
	{"NegotiateLineFeed",17,16},
	{"ExtendedASCII",13,17},
	{"Logout",6,18},
	{"ByteMacro",9,19},
	{"DataEntryTerminal",17,20},
	{"SupDup",6,21},
	{"SupDupOutput",12,22},
	{"SendLocation",12,23},
	{"TerminalType",12,24},
	{"EndOfRecordOpt",14,25},
	{"Tuid",4,26},
	{"OutMark",7,27},
	{"TtyLocation",11,28},
	{"IBM3270Regime",13,29},
	{"X3Pad",5,30},
	{"NegotiateWindowSize",19,31},
	{"TerminalSpeed",13,32},
	{"FlowControl",11,33},
	{"LineMode",8,34},
	{"XDisplay",8,35},
	{"Environment",11,36},
	{"Authentication",14,45},
	{"ExtendedOptionsList",19,255},
};

struct telnet {
	ProtoStackFrame _psf;
	unsigned char data;
	unsigned char iac;
	unsigned char data255;
	unsigned char cmd;
	unsigned char option;
	unsigned char optdata;
}; /* telnet */

static struct telnet telnet_frame;

static int	telnet_init(void);
static ExprType	telnet_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc);
static int	telnet_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex);
static void	telnet_decode(DataStream *ds, ProtoStack *ps, PacketView *pv);

Protocol telnet_proto = {
	"telnet",6,"TELNET Protocol",PRID_TELNET,
	DS_BIG_ENDIAN,1,0,
	telnet_init,pr_stub_setopt,pr_stub_embed,pr_stub_resolve,
	telnet_compile,pr_stub_match,telnet_fetch,telnet_decode,
	0,0,0,0
};

static int
telnet_init()
{
	if (!pr_register(&telnet_proto, telnet_fields, lengthof(telnet_fields),
			 1	/* scopeload */
			 + lengthof(telnettelnetCommand_values)
			 + lengthof(telnettelnetOption_values))) {
		return 0;
	}
	if (!(pr_nest(&telnet_proto, PRID_TCP, 23, 0, 0))) {
		return 0;
	}
	en_init(&telnettelnetCommand, telnettelnetCommand_values, lengthof(telnettelnetCommand_values),
		&telnet_proto);
	en_init(&telnettelnetOption, telnettelnetOption_values, lengthof(telnettelnetOption_values),
		&telnet_proto);

	return 1;
}

static ExprType
telnet_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
} /* telnet_compile */

static int
telnet_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct telnet *telnet = &telnet_frame;
	int ok = 0, fid = pf->pf_id;

	PS_PUSH(ps, &telnet->_psf, &telnet_proto);
/*XXX Handcrafting: Change the if to a while */
	while (telnet_continue(ds)) {
		{ int tell = ds_tellbit(ds);
		if (!ds_u_char(ds, &telnet->data))
			goto out;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		if ((*telnet).data!=255) {
			if (!ds_u_char(ds, &telnet->data))
				goto out;
			if (fid == telnetfid_data) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = telnet->data;
				ok = 1; goto out;
			}
		} else {
			if (!ds_u_char(ds, &telnet->iac))
				goto out;
			if (fid == telnetfid_iac) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = telnet->iac;
				ok = 1; goto out;
			}
			{ int tell = ds_tellbit(ds);
			if (!ds_u_char(ds, &telnet->cmd))
				goto out;
			ds_seekbit(ds, tell, DS_ABSOLUTE); }
			if ((*telnet).cmd==255) {
				if (!ds_u_char(ds, &telnet->data255))
					goto out;
				if (fid == telnetfid_data255) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = telnet->data255;
					ok = 1; goto out;
				}
			} else {
				if (!ds_u_char(ds, &telnet->cmd))
					goto out;
				if (fid == telnetfid_cmd) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = telnet->cmd;
					ok = 1; goto out;
				}
				if ((*telnet).cmd>249) {
					if (!ds_u_char(ds, &telnet->option))
						goto out;
					if (fid == telnetfid_option) {
						rex->ex_op = EXOP_NUMBER;
						rex->ex_val = telnet->option;
						ok = 1; goto out;
					}
/* XXX guarded backref: cmd */
					if ((*telnet).cmd==250) {
/* XXX guarded backref: option */
						switch ((*telnet).option) {
						  default:
							{ int tell = ds_tellbit(ds);
							if (!ds_u_char(ds, &telnet->optdata))
								goto out;
							ds_seekbit(ds, tell, DS_ABSOLUTE); }
/*XXX Handcrafting: Change the if to a while */
							while ((*telnet).optdata!=255&&telnet_continue(ds)) {
								if (!ds_u_char(ds, &telnet->optdata))
									goto out;
								if (fid == telnetfid_optdata) {
									rex->ex_op = EXOP_NUMBER;
									rex->ex_val = telnet->optdata;
									ok = 1; goto out;
								}
/*XXX Handcrafting Start: Add lookahead at bottom of loop */
								{ int tell = ds_tellbit(ds);
								if (!ds_u_char(ds, &telnet->optdata))
									goto out;
								ds_seekbit(ds, tell, DS_ABSOLUTE); }
/*XXX Handcrafting End */
							}
							break;
						}
					}
				}
			}
		}
	}
out:
	PS_POP(ps);
	return ok;
} /* telnet_fetch */

static void
telnet_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct telnet *telnet = &telnet_frame;
	Protocol *pr;

	PS_PUSH(ps, &telnet->_psf, &telnet_proto);
/*XXX Handcrafting: Change the if to a while */
	while (telnet_continue(ds)) {
		{ int tell = ds_tellbit(ds);
		if (!ds_u_char(ds, &telnet->data))
			return;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		if ((*telnet).data!=255) {
			if (!ds_u_char(ds, &telnet->data))
				ds->ds_count = 0;
			pv_showfield(pv, &telnet_fields[0], &telnet->data,
				     "%-3u", telnet->data);
		} else {
			if (!ds_u_char(ds, &telnet->iac))
				ds->ds_count = 0;
			pv_showfield(pv, &telnet_fields[1], &telnet->iac,
				     "%-3u", telnet->iac);
			{ int tell = ds_tellbit(ds);
			if (!ds_u_char(ds, &telnet->cmd))
				return;
			ds_seekbit(ds, tell, DS_ABSOLUTE); }
			if ((*telnet).cmd==255) {
				if (!ds_u_char(ds, &telnet->data255))
					ds->ds_count = 0;
				pv_showfield(pv, &telnet_fields[2], &telnet->data255,
					     "%-3u", telnet->data255);
			} else {
				if (!ds_u_char(ds, &telnet->cmd))
					ds->ds_count = 0;
				pv_showfield(pv, &telnet_fields[3], &telnet->cmd,
					     "%-19s", en_name(&telnettelnetCommand, telnet->cmd));
				if ((*telnet).cmd>249) {
					if (!ds_u_char(ds, &telnet->option))
						ds->ds_count = 0;
					pv_showfield(pv, &telnet_fields[4], &telnet->option,
						     "%-19s", en_name(&telnettelnetOption, telnet->option));
/* XXX guarded backref: cmd */
					if ((*telnet).cmd==250) {
/* XXX guarded backref: option */
						switch ((*telnet).option) {
						  default:
							{ int tell = ds_tellbit(ds);
							if (!ds_u_char(ds, &telnet->optdata))
								return;
							ds_seekbit(ds, tell, DS_ABSOLUTE); }
/*XXX Handcrafting: Change the if to a while */
							while ((*telnet).optdata!=255&&telnet_continue(ds)) {
								if (!ds_u_char(ds, &telnet->optdata))
									ds->ds_count = 0;
								pv_showfield(pv, &telnet_fields[5], &telnet->optdata,
									     "%-3u", telnet->optdata);
/*XXX Handcrafting Start: Add lookahead at bottom of loop */
								{ int tell = ds_tellbit(ds);
								if (!ds_u_char(ds, &telnet->optdata))
									return;
								ds_seekbit(ds, tell, DS_ABSOLUTE); }
/*XXX Handcrafting End */
							}
							break;
						}
					}
				}
			}
		}
	}
	pr = 0;
	pv_decodeframe(pv, pr, ds, ps);
out:
	PS_POP(ps);
} /* telnet_decode */
