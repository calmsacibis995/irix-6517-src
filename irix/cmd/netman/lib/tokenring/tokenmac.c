#include "enum.h"
#include "heap.h"
#include "protodefs.h"
#include "protoindex.h"
#include "tokenring.h"

#line 22 "tokenmac.pi"

/*
 * Hand crafted reminder:
 *	- Search for "if (mac_do_subvector(ds))" and change
 *	  'if' to 'while'. They are in 2 different places.
 */

#line 360 "tokenmac.pi"

static int
mac_do_subvector (DataStream *ds)
{
    return ds->ds_count > 0;
    /* Get the position (value can be passed) of 'll' and get
       the current position. If current minus ll = value of ll
       return false else return true, ie more to decode. */
    /* see appletalk end. ds->ds_count>0 afp */
}
#line 27 "tokenmac.c"

#define tokenmacsvidfrmtfid_cs 88
#define tokenmacsvidfrmtfid_ro 89
#define tokenmacsvidfrmtfid_codept 90

static ProtoField tokenmacsvidfrmt_fields[] = {
	{"cs",2,"Common/Specific Indicator",tokenmacsvidfrmtfid_cs,(void*)DS_ZERO_EXTEND,-1,0,EXOP_NUMBER,0},
	{"ro",2,"Required/Optional Indicator",tokenmacsvidfrmtfid_ro,(void*)DS_ZERO_EXTEND,-1,1,EXOP_NUMBER,0},
	{"codept",6,"Code Point",tokenmacsvidfrmtfid_codept,(void*)DS_ZERO_EXTEND,-6,2,EXOP_NUMBER,0},
};

struct tokenmacsvidfrmt {
	unsigned char cs:1;
	unsigned char ro:1;
	unsigned char codept:6;
}; /* tokenmacsvidfrmt */

static ProtoStruct tokenmacsvidfrmt_struct = PSI("svidfrmt",tokenmacsvidfrmt_fields);

#define tokenmacfid_ll 23
#define tokenmacfid_dcl 24
#define tokenmacfid_scl 25
#define tokenmacfid_mvid 26
#define tokenmacfid_len 27
#define tokenmacfid_svid 28
#define tokenmacfid_beacon 29
#define tokenmacfid_naun 30
#define tokenmacfid_lrn 31
#define tokenmacfid_apl 32
#define tokenmacfid_stv 33
#define tokenmacfid_efc 34
#define tokenmacfid_pad 35
#define tokenmacfid_aap 36
#define tokenmacfid_cor 37
#define tokenmacfid_lnna 38
#define tokenmacfid_pl 39
#define tokenmacfid_rc 40
#define tokenmacfid_rdcl 41
#define tokenmacfid_rscl 42
#define tokenmacfid_rmvid 43
#define tokenmacfid_res 44
#define tokenmacfid_pcl 45
#define tokenmacfid_fty 46
#define tokenmacfid_mty 47
#define tokenmacfid_mnum 48
#define tokenmacfid_snum 49
#define tokenmacfid_seqn 50
#define tokenmacfid_rsml 51
#define tokenmacfid_wd 52
#define tokenmacfid_macf 53
#define tokenmacfid_sid 54
#define tokenmacfid_rsss 55
#define tokenmacfid_tsc 56
#define tokenmacfid_ga 57
#define tokenmacfid_fa 58
#define tokenmacfid_lerr 59
#define tokenmacfid_ierr 60
#define tokenmacfid_berr 61
#define tokenmacfid_acerr 62
#define tokenmacfid_adt 63
#define tokenmacfid_resv 64
#define tokenmacfid_lferr 65
#define tokenmacfid_rj 66
#define tokenmacfid_fcerr 67
#define tokenmacfid_ferr 68
#define tokenmacfid_terr 69
#define tokenmacfid_ec 70
#define tokenmacfid_llong 71
#define tokenmacfid_lsvid 72
#define tokenmacfid_llen 73

static ProtoField tokenmacfa1_element =
	{0,0,0,74,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmacga1_element =
	{0,0,0,75,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmacrsss1_element =
	{0,0,0,76,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmacsid1_element =
	{0,0,0,77,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmacmacf1_element =
	{0,0,0,78,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,2};

static ProtoField tokenmacwd1_element =
	{0,0,0,79,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,2};

static ProtoField tokenmacrsml1_element =
	{0,0,0,80,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmacseqn1_element =
	{0,0,0,81,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField tokenmacsnum1_element =
	{0,0,0,82,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField tokenmacmnum1_element =
	{0,0,0,83,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField tokenmacmty1_element =
	{0,0,0,84,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,1};

static ProtoField tokenmaclnna1_element =
	{0,0,0,85,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmacnaun1_element =
	{0,0,0,86,(void*)DS_ZERO_EXTEND,1,0,EXOP_NUMBER,0};

static ProtoField tokenmac_fields[] = {
	{"ll",2,"Major Vector Length",tokenmacfid_ll,(void*)DS_ZERO_EXTEND,2,0,EXOP_NUMBER,1},
	{"dcl",3,"Destination Function Class",tokenmacfid_dcl,(void*)DS_ZERO_EXTEND,-4,16,EXOP_NUMBER,1},
	{"scl",3,"Source Function Class",tokenmacfid_scl,(void*)DS_ZERO_EXTEND,-4,20,EXOP_NUMBER,1},
	{"mvid",4,"Command Byte",tokenmacfid_mvid,(void*)DS_ZERO_EXTEND,1,3,EXOP_NUMBER,0},
	{"len",3,"Subvector Length",tokenmacfid_len,(void*)DS_ZERO_EXTEND,1,4,EXOP_NUMBER,1},
	{"svid",4,"Subvector Type",tokenmacfid_svid,&tokenmacsvidfrmt_struct,1,5,EXOP_STRUCT,1},
	{"beacon",6,"Type",tokenmacfid_beacon,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"naun",4,"Address",tokenmacfid_naun,&tokenmacnaun1_element,6,8,EXOP_ARRAY,0},
	{"lrn",3,"of Sending Station",tokenmacfid_lrn,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"apl",3,"of Target Ring Station",tokenmacfid_apl,(void*)DS_ZERO_EXTEND,4,8,EXOP_NUMBER,0},
	{"stv",3,"Timeout Value",tokenmacfid_stv,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"efc",3,"Class Num Enabled To Transmit",tokenmacfid_efc,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"pad",3,"Padding",tokenmacfid_pad,(void*)DS_ZERO_EXTEND,-4,64,EXOP_NUMBER,2},
	{"aap",3,"Maximaum Allowed",tokenmacfid_aap,(void*)DS_ZERO_EXTEND,-2,78,EXOP_NUMBER,0},
	{"cor",3,"Relate Request and Response",tokenmacfid_cor,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"lnna",4,"Address",tokenmacfid_lnna,&tokenmaclnna1_element,6,8,EXOP_ARRAY,0},
	{"pl",2,"of Sending Ring Station",tokenmacfid_pl,(void*)DS_ZERO_EXTEND,4,8,EXOP_NUMBER,0},
	{"rc",2,"Response Code",tokenmacfid_rc,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"rdcl",4,"Destination Function Class",tokenmacfid_rdcl,(void*)DS_ZERO_EXTEND,-4,80,EXOP_NUMBER,0},
	{"rscl",4,"Source Function Class",tokenmacfid_rscl,(void*)DS_ZERO_EXTEND,-4,84,EXOP_NUMBER,0},
	{"rmvid",5,"Command Byte",tokenmacfid_rmvid,(void*)DS_ZERO_EXTEND,1,11,EXOP_NUMBER,0},
	{"res",3,"Reserved",tokenmacfid_res,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"pcl",3,"Product Classification",tokenmacfid_pcl,(void*)DS_ZERO_EXTEND,-4,68,EXOP_NUMBER,0},
	{"fty",3,"Format type",tokenmacfid_fty,(void*)DS_ZERO_EXTEND,1,9,EXOP_NUMBER,1},
	{"mty",3,"Machine Type, EBCDIC",tokenmacfid_mty,&tokenmacmty1_element,4,10,EXOP_ARRAY,1},
	{"mnum",4,"Machine Model Num, EBCDIC",tokenmacfid_mnum,&tokenmacmnum1_element,3,14,EXOP_ARRAY,1},
	{"snum",4,"Serial Num Modifier, EBCDIC",tokenmacfid_snum,&tokenmacsnum1_element,2,17,EXOP_ARRAY,1},
	{"seqn",4,"Sequence Num, EBCDIC",tokenmacfid_seqn,&tokenmacseqn1_element,7,19,EXOP_ARRAY,1},
	{"rsml",4,"of Sending Station, EBCDIC",tokenmacfid_rsml,&tokenmacrsml1_element,10,8,EXOP_ARRAY,0},
	{"wd",2,"for Lobe Test",tokenmacfid_wd,&tokenmacwd1_element,1500,8,EXOP_ARRAY,2},
	{"macf",4,"Mac Frame",tokenmacfid_macf,&tokenmacmacf1_element,0,8,EXOP_ARRAY,2},
	{"sid",3,"Address",tokenmacfid_sid,&tokenmacsid1_element,6,8,EXOP_ARRAY,0},
	{"rsss",4,"Status",tokenmacfid_rsss,&tokenmacrsss1_element,6,8,EXOP_ARRAY,0},
	{"tsc",3,"Strip Status",tokenmacfid_tsc,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"ga",2,"Lower 4 Bytes of Addr",tokenmacfid_ga,&tokenmacga1_element,4,8,EXOP_ARRAY,0},
	{"fa",2,"Recognized by this Station",tokenmacfid_fa,&tokenmacfa1_element,4,8,EXOP_ARRAY,0},
	{"lerr",4,"Line Error",tokenmacfid_lerr,(void*)DS_ZERO_EXTEND,1,8,EXOP_NUMBER,0},
	{"ierr",4,"Internal Error",tokenmacfid_ierr,(void*)DS_ZERO_EXTEND,1,9,EXOP_NUMBER,0},
	{"berr",4,"Burst Error",tokenmacfid_berr,(void*)DS_ZERO_EXTEND,1,10,EXOP_NUMBER,0},
	{"acerr",5,"A/C Error",tokenmacfid_acerr,(void*)DS_ZERO_EXTEND,1,11,EXOP_NUMBER,0},
	{"adt",3,"Abort Delimiter Transmitted",tokenmacfid_adt,(void*)DS_ZERO_EXTEND,1,12,EXOP_NUMBER,0},
	{"resv",4,"Reserved",tokenmacfid_resv,(void*)DS_ZERO_EXTEND,1,13,EXOP_NUMBER,2},
	{"lferr",5,"Lost Frame Error",tokenmacfid_lferr,(void*)DS_ZERO_EXTEND,1,8,EXOP_NUMBER,0},
	{"rj",2,"Receiver Congestion",tokenmacfid_rj,(void*)DS_ZERO_EXTEND,1,9,EXOP_NUMBER,0},
	{"fcerr",5,"Frame Copied Error",tokenmacfid_fcerr,(void*)DS_ZERO_EXTEND,1,10,EXOP_NUMBER,0},
	{"ferr",4,"Frequency Error",tokenmacfid_ferr,(void*)DS_ZERO_EXTEND,1,11,EXOP_NUMBER,0},
	{"terr",4,"Token Error",tokenmacfid_terr,(void*)DS_ZERO_EXTEND,1,12,EXOP_NUMBER,0},
	{"ec",2,"Code",tokenmacfid_ec,(void*)DS_ZERO_EXTEND,2,8,EXOP_NUMBER,0},
	{"llong",5,"Long Subvector Indicator",tokenmacfid_llong,(void*)DS_ZERO_EXTEND,1,4,EXOP_NUMBER,2},
	{"lsvid",5,"Subvector Type",tokenmacfid_lsvid,&tokenmacsvidfrmt_struct,1,5,EXOP_STRUCT,1},
	{"llen",4,"Subvector Length",tokenmacfid_llen,(void*)DS_ZERO_EXTEND,2,6,EXOP_NUMBER,1},
};

static Enumeration tokenmacfunclass;
static Enumerator tokenmacfunclass_values[] = {
	{"RingStation",11,0},
	{"DLCLANMGR",9,1},
	{"ConfigReportServer",18,4},
	{"RingParmServer",14,5},
	{"RingErrMonitor",14,6},
};

static Enumeration tokenmaccsbit;
static Enumerator tokenmaccsbit_values[] = {
	{"Common",6,0},
	{"Specific",8,1},
};

static Enumeration tokenmacrobit;
static Enumerator tokenmacrobit_values[] = {
	{"Required",8,0},
	{"Optional",8,1},
};

static Enumeration tokenmacmvidcmd;
static Enumerator tokenmacmvidcmd_values[] = {
	{"Response",8,0},
	{"Beacon",6,2},
	{"ClaimToken",10,3},
	{"RingPurge",9,4},
	{"ActiveMonitorPresent",20,5},
	{"StandbyMonitorPresent",21,6},
	{"DuplicateAddrTest",17,7},
	{"LobeTest",8,8},
	{"TransmitForward",15,9},
	{"RemoveRingStation",17,11},
	{"ChangeParameters",16,12},
	{"InitRingStation",15,13},
	{"RequestRingStationAddr",22,14},
	{"RequestRingStationState",23,15},
	{"RequestRingStationAttach",24,16},
	{"RequestInit",11,32},
	{"ReportRingStationAddr",21,34},
	{"ReportRingStationState",22,35},
	{"ReportRingStationAttach",23,36},
	{"ReportNewActiveMonitor",22,37},
	{"ReportNAUNchange",16,38},
	{"ReportNeighborNotificationIncomplete",36,39},
	{"ReportActiveMonitorError",24,40},
	{"ReportSoftError",15,41},
	{"ReportTransmitForward",21,42},
};

static Enumeration tokenmacsubvectype;
static Enumerator tokenmacsubvectype_values[] = {
	{"BeaconType",10,1},
	{"NAUN",4,2},
	{"LocalRingNum",12,3},
	{"AssignPhysicalLocation",22,4},
	{"SoftErrReportTimerValue",23,5},
	{"EnabledFunctionClasses",22,6},
	{"AllowedAccessPriority",21,7},
	{"Correlator",10,8},
	{"LastNeighborNotificationAddr",28,9},
	{"PhysicalLocation",16,10},
	{"ResponseCode",12,32},
	{"Reserved",8,33},
	{"ProductInstanceID",17,34},
	{"RingStationMicrocodeLevel",25,35},
	{"WrapData",8,38},
	{"FrameForward",12,39},
	{"StationIdentifier",17,40},
	{"RingStationStatusSubvector",26,41},
	{"TransmitStatusCode",18,42},
	{"GroupAddr",9,43},
	{"FunctionalAddr",14,44},
	{"IsolatingErrorCounts",20,45},
	{"NonIsolatingErrorCounts",23,46},
	{"ErrorCode",9,48},
};

static Enumeration tokenmacbreason;
static Enumerator tokenmacbreason_values[] = {
	{"RecoveryModeSet",15,1},
	{"SignalLossErr",13,2},
	{"StreamSignalNotClaimToken",25,3},
	{"StreamSignalClaimTokenOrHardErr",31,4},
};

static Enumeration tokenmacerrorcode;
static Enumerator tokenmacerrorcode_values[] = {
	{"MonitorErr",10,1},
	{"DuplicateMonitor",16,2},
	{"DuplicateAddr",13,3},
};

static Enumeration tokenmacprodcode;
static Enumerator tokenmacprodcode_values[] = {
	{"IBMhardware",11,1},
	{"IBMorNonIBMharware",18,3},
	{"IBMsoftware",11,4},
	{"NonIBMhardware",14,9},
	{"NonIBMsoftware",14,12},
	{"IBMorNonIBMsoftware",19,14},
};

static Enumeration tokenmacprodinst;
static Enumerator tokenmacprodinst_values[] = {
	{"MachineSerialNum",16,16},
	{"MachineAndModelSerialNum",24,17},
	{"MachineSerialNumAdditional",26,18},
};

static Enumeration tokenmacrespcode;
static Enumerator tokenmacrespcode_values[] = {
	{"Position",8,1},
	{"MissingMajorVector",18,32769},
	{"MajorVectorLenErr",17,32770},
	{"UnrecognizedMVID",16,32771},
	{"InappropriateSourceClass",24,32772},
	{"SubvectorLenErr",15,32773},
	{"TransmitForwardInvalid",22,32774},
	{"MissingRequiredSubvector",24,32775},
	{"MACFrameTooBig",14,32776},
	{"FunctionRequestedDisabled",25,32777},
};

struct tokenmac {
	ProtoStackFrame _psf;
	unsigned short ll;
	unsigned char dcl:4;
	unsigned char scl:4;
	unsigned char mvid;
	unsigned char len;
	struct tokenmacsvidfrmt svid;
	unsigned short beacon_breason;
	unsigned short beacon_breasonF16:16;
	unsigned char naun[6];
	unsigned short lrn;
	unsigned long apl;
	unsigned short stv;
	unsigned short efc;
	unsigned char pad_u_charF4:4;
	unsigned short pad_u_shortF14:14;
	unsigned char aap:2;
	unsigned short cor;
	unsigned char lnna[6];
	unsigned long pl;
	unsigned short rc;
	unsigned char rdcl:4;
	unsigned char rscl:4;
	unsigned char rmvid;
	unsigned short res;
	unsigned char pcl:4;
	unsigned char fty;
	unsigned char mty[4];
	unsigned char mnum[3];
	unsigned char snum[2];
	unsigned char seqn[7];
	unsigned char rsml[10];
	unsigned char wd[1500];
	unsigned char *macf;
	unsigned char sid[6];
	unsigned char rsss[6];
	unsigned short tsc;
	unsigned char ga[4];
	unsigned char fa[4];
	unsigned char lerr;
	unsigned char ierr;
	unsigned char berr;
	unsigned char acerr;
	unsigned char adt;
	unsigned char resv;
	unsigned char lferr;
	unsigned char rj;
	unsigned char fcerr;
	unsigned char ferr;
	unsigned char terr;
	unsigned short ec_errorcode;
	unsigned short ec_errorcodeF16:16;
	unsigned char llong;
	struct tokenmacsvidfrmt lsvid;
	unsigned short llen;
}; /* tokenmac */

static struct tokenmac tokenmac_frame;

static int	tokenmac_init(void);
static ExprType	tokenmac_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc);
static int	tokenmac_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex);
static void	tokenmac_decode(DataStream *ds, ProtoStack *ps, PacketView *pv);

Protocol tokenmac_proto = {
	"tokenmac",8,"Token Ring Medium Access Control",PRID_TOKENMAC,
	DS_BIG_ENDIAN,7,0,
	tokenmac_init,pr_stub_setopt,pr_stub_embed,pr_stub_resolve,
	tokenmac_compile,pr_stub_match,tokenmac_fetch,tokenmac_decode,
	0,0,0,0
};

static int
tokenmac_init()
{
	if (!pr_register(&tokenmac_proto, tokenmac_fields, lengthof(tokenmac_fields),
			 1	/* scopeload */
			 + lengthof(tokenmacfunclass_values)
			 + lengthof(tokenmaccsbit_values)
			 + lengthof(tokenmacrobit_values)
			 + lengthof(tokenmacmvidcmd_values)
			 + lengthof(tokenmacsubvectype_values)
			 + lengthof(tokenmacbreason_values)
			 + lengthof(tokenmacerrorcode_values)
			 + lengthof(tokenmacprodcode_values)
			 + lengthof(tokenmacprodinst_values)
			 + lengthof(tokenmacrespcode_values))) {
		return 0;
	}
	if (!(pr_nest(&tokenmac_proto, PRID_TOKENRING, 0, 0, 0))) {
		return 0;
	}
	en_init(&tokenmacfunclass, tokenmacfunclass_values, lengthof(tokenmacfunclass_values),
		&tokenmac_proto);
	en_init(&tokenmaccsbit, tokenmaccsbit_values, lengthof(tokenmaccsbit_values),
		&tokenmac_proto);
	en_init(&tokenmacrobit, tokenmacrobit_values, lengthof(tokenmacrobit_values),
		&tokenmac_proto);
	en_init(&tokenmacmvidcmd, tokenmacmvidcmd_values, lengthof(tokenmacmvidcmd_values),
		&tokenmac_proto);
	en_init(&tokenmacsubvectype, tokenmacsubvectype_values, lengthof(tokenmacsubvectype_values),
		&tokenmac_proto);
	en_init(&tokenmacbreason, tokenmacbreason_values, lengthof(tokenmacbreason_values),
		&tokenmac_proto);
	en_init(&tokenmacerrorcode, tokenmacerrorcode_values, lengthof(tokenmacerrorcode_values),
		&tokenmac_proto);
	en_init(&tokenmacprodcode, tokenmacprodcode_values, lengthof(tokenmacprodcode_values),
		&tokenmac_proto);
	en_init(&tokenmacprodinst, tokenmacprodinst_values, lengthof(tokenmacprodinst_values),
		&tokenmac_proto);
	en_init(&tokenmacrespcode, tokenmacrespcode_values, lengthof(tokenmacrespcode_values),
		&tokenmac_proto);

	return 1;
}

static ExprType
tokenmac_compile(ProtoField *pf, Expr *mex, Expr *tex, ProtoCompiler *pc)
{
	return ET_COMPLEX;
} /* tokenmac_compile */

static int
tokenmacsvidfrmt_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tokenmacsvidfrmt *svidfrmt = ps->ps_slink;
	int ok = 0, fid = pf->pf_id;

	if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenmacsvidfrmtfid_cs) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	svidfrmt->cs = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 1, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenmacsvidfrmtfid_ro) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	svidfrmt->ro = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 6, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenmacsvidfrmtfid_codept) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	svidfrmt->codept = rex->ex_val;
out:
	return ok;
} /* tokenmacsvidfrmt_fetch */

static int
tokenmac_fetch(ProtoField *pf, DataStream *ds, ProtoStack *ps, Expr *rex)
{
	struct tokenmac *tokenmac = &tokenmac_frame;
	int ok = 0, fid = pf->pf_id;

	PS_PUSH(ps, &tokenmac->_psf, &tokenmac_proto);
	if (fid >= 74 && fid < 87) {
		switch (fid) {
		  case 74:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->fa[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->fa[pf->pf_cookie];
			ok = 1; goto out;
		  case 75:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->ga[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->ga[pf->pf_cookie];
			ok = 1; goto out;
		  case 76:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->rsss[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->rsss[pf->pf_cookie];
			ok = 1; goto out;
		  case 77:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->sid[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->sid[pf->pf_cookie];
			ok = 1; goto out;
		  case 78:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			tokenmac->macf = renew(tokenmac->macf, (*tokenmac).len-2, unsigned char);
			if (tokenmac->macf == 0) goto out;
			if (!ds_u_char(ds, &tokenmac->macf[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->macf[pf->pf_cookie];
			ok = 1; goto out;
		  case 79:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->wd[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->wd[pf->pf_cookie];
			ok = 1; goto out;
		  case 80:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->rsml[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->rsml[pf->pf_cookie];
			ok = 1; goto out;
		  case 81:
			if (!ds_seek(ds, 17 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->seqn[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->seqn[pf->pf_cookie];
			ok = 1; goto out;
		  case 82:
			if (!ds_seek(ds, 15 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->snum[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->snum[pf->pf_cookie];
			ok = 1; goto out;
		  case 83:
			if (!ds_seek(ds, 12 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->mnum[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->mnum[pf->pf_cookie];
			ok = 1; goto out;
		  case 84:
			if (!ds_seek(ds, 8 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->mty[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->mty[pf->pf_cookie];
			ok = 1; goto out;
		  case 85:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->lnna[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->lnna[pf->pf_cookie];
			ok = 1; goto out;
		  case 86:
			if (!ds_seek(ds, 6 + 1 * pf->pf_cookie, DS_RELATIVE))
				goto out;
			if (!ds_u_char(ds, &tokenmac->naun[pf->pf_cookie]))
				goto out;
			rex->ex_op = EXOP_NUMBER;
			rex->ex_val = tokenmac->naun[pf->pf_cookie];
			ok = 1; goto out;
		}
	}
	if (fid >= 87) {
		ok = tokenmacsvidfrmt_fetch(pf, ds, ps, rex); goto out;
	}
	if (!ds_u_short(ds, &tokenmac->ll))
		goto out;
	if (fid == tokenmacfid_ll) {
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = tokenmac->ll;
		ok = 1; goto out;
	}
	if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenmacfid_dcl) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenmac->dcl = rex->ex_val;
	if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
		goto out;
	if (fid == tokenmacfid_scl) {
		rex->ex_op = EXOP_NUMBER;
		ok = 1; goto out;
	}
	tokenmac->scl = rex->ex_val;
	if (!ds_u_char(ds, &tokenmac->mvid))
		goto out;
	if (fid == tokenmacfid_mvid) {
		rex->ex_op = EXOP_NUMBER;
		rex->ex_val = tokenmac->mvid;
		ok = 1; goto out;
	}
	while (mac_do_subvector(ds)) {
		{ int tell = ds_tellbit(ds);
		if (!ds_u_char(ds, &tokenmac->len))
			goto out;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		if ((*tokenmac).len!=255) {
			if (!ds_u_char(ds, &tokenmac->len))
				goto out;
			if (fid == tokenmacfid_len) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = tokenmac->len;
				ok = 1; goto out;
			}
			if (fid == tokenmacfid_svid) {
				rex->ex_op = EXOP_FIELD;
				rex->ex_field = pf = &tokenmac_fields[5];
				pf->pf_cookie = 5;
				ps->ps_slink = &tokenmac->svid;
				ok = 1; goto out;
			}
			switch ((*tokenmac).svid.codept) {
			  case 1:
				if (!ds_bits(ds, &rex->ex_val, 16, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_beacon) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->beacon_breasonF16 = rex->ex_val;
				break;
			  case 2:
				if (fid == tokenmacfid_naun) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacnaun1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 3:
				if (!ds_u_short(ds, &tokenmac->lrn))
					goto out;
				if (fid == tokenmacfid_lrn) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->lrn;
					ok = 1; goto out;
				}
				break;
			  case 4:
				if (!ds_u_long(ds, &tokenmac->apl))
					goto out;
				if (fid == tokenmacfid_apl) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->apl;
					ok = 1; goto out;
				}
				break;
			  case 5:
				if (!ds_u_short(ds, &tokenmac->stv))
					goto out;
				if (fid == tokenmacfid_stv) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->stv;
					ok = 1; goto out;
				}
				break;
			  case 6:
				if (!ds_u_short(ds, &tokenmac->efc))
					goto out;
				if (fid == tokenmacfid_efc) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->efc;
					ok = 1; goto out;
				}
				break;
			  case 7:
				if (!ds_bits(ds, &rex->ex_val, 14, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_pad) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->pad_u_shortF14 = rex->ex_val;
				if (!ds_bits(ds, &rex->ex_val, 2, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_aap) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->aap = rex->ex_val;
				break;
			  case 8:
				if (!ds_u_short(ds, &tokenmac->cor))
					goto out;
				if (fid == tokenmacfid_cor) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->cor;
					ok = 1; goto out;
				}
				break;
			  case 9:
				if (fid == tokenmacfid_lnna) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmaclnna1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 10:
				if (!ds_u_long(ds, &tokenmac->pl))
					goto out;
				if (fid == tokenmacfid_pl) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->pl;
					ok = 1; goto out;
				}
				break;
			  case 32:
				if (!ds_u_short(ds, &tokenmac->rc))
					goto out;
				if (fid == tokenmacfid_rc) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->rc;
					ok = 1; goto out;
				}
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_rdcl) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->rdcl = rex->ex_val;
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_rscl) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->rscl = rex->ex_val;
				if (!ds_u_char(ds, &tokenmac->rmvid))
					goto out;
				if (fid == tokenmacfid_rmvid) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->rmvid;
					ok = 1; goto out;
				}
				break;
			  case 33:
				if (!ds_u_short(ds, &tokenmac->res))
					goto out;
				if (fid == tokenmacfid_res) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->res;
					ok = 1; goto out;
				}
				break;
			  case 34:
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_pad) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->pad_u_charF4 = rex->ex_val;
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_pcl) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->pcl = rex->ex_val;
				if (!ds_u_char(ds, &tokenmac->fty))
					goto out;
				if (fid == tokenmacfid_fty) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->fty;
					ok = 1; goto out;
				}
				if (fid == tokenmacfid_mty) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacmty1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 4, DS_RELATIVE)) goto out;
				if (fid == tokenmacfid_mnum) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacmnum1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 3, DS_RELATIVE)) goto out;
				if (fid == tokenmacfid_snum) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacsnum1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 2, DS_RELATIVE)) goto out;
				if (fid == tokenmacfid_seqn) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacseqn1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 7, DS_RELATIVE)) goto out;
				break;
			  case 35:
				if (fid == tokenmacfid_rsml) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacrsml1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 10, DS_RELATIVE)) goto out;
				break;
			  case 38:
				if (fid == tokenmacfid_wd) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacwd1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 1500, DS_RELATIVE)) goto out;
				break;
			  case 39:
/* XXX guarded backref: len */
				if (fid == tokenmacfid_macf) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacmacf1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * (*tokenmac).len-2, DS_RELATIVE)) goto out;
				break;
			  case 40:
				if (fid == tokenmacfid_sid) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacsid1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 41:
				if (fid == tokenmacfid_rsss) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacrsss1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 42:
				if (!ds_u_short(ds, &tokenmac->tsc))
					goto out;
				if (fid == tokenmacfid_tsc) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->tsc;
					ok = 1; goto out;
				}
				break;
			  case 43:
				if (fid == tokenmacfid_ga) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacga1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 4, DS_RELATIVE)) goto out;
				break;
			  case 44:
				if (fid == tokenmacfid_fa) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacfa1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 4, DS_RELATIVE)) goto out;
				break;
			  case 45:
				if (!ds_u_char(ds, &tokenmac->lerr))
					goto out;
				if (fid == tokenmacfid_lerr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->lerr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->ierr))
					goto out;
				if (fid == tokenmacfid_ierr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->ierr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->berr))
					goto out;
				if (fid == tokenmacfid_berr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->berr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->acerr))
					goto out;
				if (fid == tokenmacfid_acerr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->acerr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->adt))
					goto out;
				if (fid == tokenmacfid_adt) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->adt;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->resv))
					goto out;
				if (fid == tokenmacfid_resv) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->resv;
					ok = 1; goto out;
				}
				break;
			  case 46:
				if (!ds_u_char(ds, &tokenmac->lferr))
					goto out;
				if (fid == tokenmacfid_lferr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->lferr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->rj))
					goto out;
				if (fid == tokenmacfid_rj) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->rj;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->fcerr))
					goto out;
				if (fid == tokenmacfid_fcerr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->fcerr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->ferr))
					goto out;
				if (fid == tokenmacfid_ferr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->ferr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->terr))
					goto out;
				if (fid == tokenmacfid_terr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->terr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->resv))
					goto out;
				if (fid == tokenmacfid_resv) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->resv;
					ok = 1; goto out;
				}
				break;
			  case 48:
				if (!ds_bits(ds, &rex->ex_val, 16, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_ec) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->ec_errorcodeF16 = rex->ex_val;
				break;
			}
		} else {
			if (!ds_u_char(ds, &tokenmac->llong))
				goto out;
			if (fid == tokenmacfid_llong) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = tokenmac->llong;
				ok = 1; goto out;
			}
			if (fid == tokenmacfid_lsvid) {
				rex->ex_op = EXOP_FIELD;
				rex->ex_field = pf = &tokenmac_fields[49];
				pf->pf_cookie = 5;
				ps->ps_slink = &tokenmac->lsvid;
				ok = 1; goto out;
			}
			if (!ds_u_short(ds, &tokenmac->llen))
				goto out;
			if (fid == tokenmacfid_llen) {
				rex->ex_op = EXOP_NUMBER;
				rex->ex_val = tokenmac->llen;
				ok = 1; goto out;
			}
			switch ((*tokenmac).lsvid.codept) {
			  case 1:
				if (!ds_u_short(ds, &tokenmac->beacon_breason))
					goto out;
				if (fid == tokenmacfid_beacon) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->beacon_breason;
					ok = 1; goto out;
				}
				break;
			  case 2:
				if (fid == tokenmacfid_naun) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacnaun1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 3:
				if (!ds_u_short(ds, &tokenmac->lrn))
					goto out;
				if (fid == tokenmacfid_lrn) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->lrn;
					ok = 1; goto out;
				}
				break;
			  case 4:
				if (!ds_u_long(ds, &tokenmac->apl))
					goto out;
				if (fid == tokenmacfid_apl) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->apl;
					ok = 1; goto out;
				}
				break;
			  case 5:
				if (!ds_u_short(ds, &tokenmac->stv))
					goto out;
				if (fid == tokenmacfid_stv) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->stv;
					ok = 1; goto out;
				}
				break;
			  case 6:
				if (!ds_u_short(ds, &tokenmac->efc))
					goto out;
				if (fid == tokenmacfid_efc) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->efc;
					ok = 1; goto out;
				}
				break;
			  case 7:
				if (!ds_bits(ds, &rex->ex_val, 14, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_pad) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->pad_u_shortF14 = rex->ex_val;
				if (!ds_bits(ds, &rex->ex_val, 2, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_aap) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->aap = rex->ex_val;
				break;
			  case 8:
				if (!ds_u_short(ds, &tokenmac->cor))
					goto out;
				if (fid == tokenmacfid_cor) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->cor;
					ok = 1; goto out;
				}
				break;
			  case 9:
				if (fid == tokenmacfid_lnna) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmaclnna1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 10:
				if (!ds_u_long(ds, &tokenmac->pl))
					goto out;
				if (fid == tokenmacfid_pl) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->pl;
					ok = 1; goto out;
				}
				break;
			  case 32:
				if (!ds_u_short(ds, &tokenmac->rc))
					goto out;
				if (fid == tokenmacfid_rc) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->rc;
					ok = 1; goto out;
				}
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_rdcl) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->rdcl = rex->ex_val;
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_rscl) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->rscl = rex->ex_val;
				if (!ds_u_char(ds, &tokenmac->rmvid))
					goto out;
				if (fid == tokenmacfid_rmvid) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->rmvid;
					ok = 1; goto out;
				}
				break;
			  case 33:
				if (!ds_u_short(ds, &tokenmac->res))
					goto out;
				if (fid == tokenmacfid_res) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->res;
					ok = 1; goto out;
				}
				break;
			  case 34:
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_pad) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->pad_u_charF4 = rex->ex_val;
				if (!ds_bits(ds, &rex->ex_val, 4, DS_ZERO_EXTEND))
					goto out;
				if (fid == tokenmacfid_pcl) {
					rex->ex_op = EXOP_NUMBER;
					ok = 1; goto out;
				}
				tokenmac->pcl = rex->ex_val;
				if (!ds_u_char(ds, &tokenmac->fty))
					goto out;
				if (fid == tokenmacfid_fty) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->fty;
					ok = 1; goto out;
				}
				if (fid == tokenmacfid_mty) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacmty1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 4, DS_RELATIVE)) goto out;
				if (fid == tokenmacfid_mnum) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacmnum1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 3, DS_RELATIVE)) goto out;
				if (fid == tokenmacfid_snum) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacsnum1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 2, DS_RELATIVE)) goto out;
				if (fid == tokenmacfid_seqn) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacseqn1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 7, DS_RELATIVE)) goto out;
				break;
			  case 35:
				if (fid == tokenmacfid_rsml) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacrsml1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 10, DS_RELATIVE)) goto out;
				break;
			  case 38:
				if (fid == tokenmacfid_wd) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacwd1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 1500, DS_RELATIVE)) goto out;
				break;
			  case 39:
/* XXX guarded backref: llen */
				if (fid == tokenmacfid_macf) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacmacf1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * (*tokenmac).llen-4, DS_RELATIVE)) goto out;
				break;
			  case 40:
				if (fid == tokenmacfid_sid) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacsid1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 41:
				if (fid == tokenmacfid_rsss) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacrsss1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 6, DS_RELATIVE)) goto out;
				break;
			  case 42:
				if (!ds_u_short(ds, &tokenmac->tsc))
					goto out;
				if (fid == tokenmacfid_tsc) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->tsc;
					ok = 1; goto out;
				}
				break;
			  case 43:
				if (fid == tokenmacfid_ga) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacga1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 4, DS_RELATIVE)) goto out;
				break;
			  case 44:
				if (fid == tokenmacfid_fa) {
					rex->ex_op = EXOP_FIELD;
					rex->ex_field = &tokenmacfa1_element;
					ok = 1; goto out;
				}
				if (!ds_seek(ds, 1 * 4, DS_RELATIVE)) goto out;
				break;
			  case 45:
				if (!ds_u_char(ds, &tokenmac->lerr))
					goto out;
				if (fid == tokenmacfid_lerr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->lerr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->ierr))
					goto out;
				if (fid == tokenmacfid_ierr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->ierr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->berr))
					goto out;
				if (fid == tokenmacfid_berr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->berr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->acerr))
					goto out;
				if (fid == tokenmacfid_acerr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->acerr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->adt))
					goto out;
				if (fid == tokenmacfid_adt) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->adt;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->resv))
					goto out;
				if (fid == tokenmacfid_resv) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->resv;
					ok = 1; goto out;
				}
				break;
			  case 46:
				if (!ds_u_char(ds, &tokenmac->lferr))
					goto out;
				if (fid == tokenmacfid_lferr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->lferr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->rj))
					goto out;
				if (fid == tokenmacfid_rj) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->rj;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->fcerr))
					goto out;
				if (fid == tokenmacfid_fcerr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->fcerr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->ferr))
					goto out;
				if (fid == tokenmacfid_ferr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->ferr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->terr))
					goto out;
				if (fid == tokenmacfid_terr) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->terr;
					ok = 1; goto out;
				}
				if (!ds_u_char(ds, &tokenmac->resv))
					goto out;
				if (fid == tokenmacfid_resv) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->resv;
					ok = 1; goto out;
				}
				break;
			  case 48:
				if (!ds_u_short(ds, &tokenmac->ec_errorcode))
					goto out;
				if (fid == tokenmacfid_ec) {
					rex->ex_op = EXOP_NUMBER;
					rex->ex_val = tokenmac->ec_errorcode;
					ok = 1; goto out;
				}
				break;
			}
		}
	}
out:
	PS_POP(ps);
	return ok;
} /* tokenmac_fetch */

static void
tokenmacsvidfrmt_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct tokenmacsvidfrmt *svidfrmt = ps->ps_slink;
	long val;

	pv_push(pv, ps->ps_top->psf_proto,
		tokenmacsvidfrmt_struct.pst_parent->pf_name,
		tokenmacsvidfrmt_struct.pst_parent->pf_namlen,
		tokenmacsvidfrmt_struct.pst_parent->pf_title);
	if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	svidfrmt->cs = val;
	pv_showfield(pv, &tokenmacsvidfrmt_fields[0], &val,
		     "%-8s", en_name(&tokenmaccsbit, svidfrmt->cs));
	if (!ds_bits(ds, &val, 1, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	svidfrmt->ro = val;
	pv_showfield(pv, &tokenmacsvidfrmt_fields[1], &val,
		     "%-8s", en_name(&tokenmacrobit, svidfrmt->ro));
	if (!ds_bits(ds, &val, 6, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	svidfrmt->codept = val;
	pv_showfield(pv, &tokenmacsvidfrmt_fields[2], &val,
		     "%-28s", en_name(&tokenmacsubvectype, svidfrmt->codept));
out:	pv_pop(pv);
} /* tokenmacsvidfrmt_decode */

static void
tokenmac_decode(DataStream *ds, ProtoStack *ps, PacketView *pv)
{
	struct tokenmac *tokenmac = &tokenmac_frame;
	Protocol *pr;
	long val;

	PS_PUSH(ps, &tokenmac->_psf, &tokenmac_proto);
	if (!ds_u_short(ds, &tokenmac->ll))
		ds->ds_count = 0;
	pv_showfield(pv, &tokenmac_fields[0], &tokenmac->ll,
		     "%-5u", tokenmac->ll);
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenmac->dcl = val;
	pv_showfield(pv, &tokenmac_fields[1], &val,
		     "%-18s", en_name(&tokenmacfunclass, tokenmac->dcl));
	if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
		ds->ds_count = 0;
	tokenmac->scl = val;
	pv_showfield(pv, &tokenmac_fields[2], &val,
		     "%-18s", en_name(&tokenmacfunclass, tokenmac->scl));
	if (!ds_u_char(ds, &tokenmac->mvid))
		ds->ds_count = 0;
	pv_showfield(pv, &tokenmac_fields[3], &tokenmac->mvid,
		     "%-36s", en_name(&tokenmacmvidcmd, tokenmac->mvid));
	while (mac_do_subvector(ds)) {
		{ int tell = ds_tellbit(ds);
		if (!ds_u_char(ds, &tokenmac->len))
			return;
		ds_seekbit(ds, tell, DS_ABSOLUTE); }
		if ((*tokenmac).len!=255) {
			if (!ds_u_char(ds, &tokenmac->len))
				ds->ds_count = 0;
			pv_showfield(pv, &tokenmac_fields[4], &tokenmac->len,
				     "%-3u", tokenmac->len);
			ps->ps_slink = &tokenmac->svid;
			tokenmacsvidfrmt_struct.pst_parent = &tokenmac_fields[5];
			tokenmacsvidfrmt_decode(ds, ps, pv);
			switch ((*tokenmac).svid.codept) {
			  case 1:
				if (!ds_bits(ds, &val, 16, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->beacon_breasonF16 = val;
				pv_showfield(pv, &tokenmac_fields[6], &val,
					     "%-31s", en_name(&tokenmacbreason, tokenmac->beacon_breasonF16));
				break;
			  case 2:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[18], title[21];
					tokenmacnaun1_element.pf_namlen = nsprintf(name, sizeof name, "naun[%d]", i0);
					nsprintf(title, sizeof title, "Address[%d]", i0);
					tokenmacnaun1_element.pf_name = name, tokenmacnaun1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->naun[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacnaun1_element, &tokenmac->naun[i0],
						     "%-3u", tokenmac->naun[i0]);
				} /* for i0 */
				}
				break;
			  case 3:
				if (!ds_u_short(ds, &tokenmac->lrn))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[8], &tokenmac->lrn,
					     "%-5u", tokenmac->lrn);
				break;
			  case 4:
				if (!ds_u_long(ds, &tokenmac->apl))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[9], &tokenmac->apl,
					     "%-10lu", tokenmac->apl);
				break;
			  case 5:
				if (!ds_u_short(ds, &tokenmac->stv))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[10], &tokenmac->stv,
					     "%-5u", tokenmac->stv);
				break;
			  case 6:
				if (!ds_u_short(ds, &tokenmac->efc))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[11], &tokenmac->efc,
					     "%-5u", tokenmac->efc);
				break;
			  case 7:
				if (!ds_bits(ds, &val, 14, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->pad_u_shortF14 = val;
				tokenmac_fields[12].pf_size = -14;
				pv_showfield(pv, &tokenmac_fields[12], &val,
					     "%-5u", tokenmac->pad_u_shortF14);
				if (!ds_bits(ds, &val, 2, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->aap = val;
				pv_showfield(pv, &tokenmac_fields[13], &val,
					     "%-3u", tokenmac->aap);
				break;
			  case 8:
				if (!ds_u_short(ds, &tokenmac->cor))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[14], &tokenmac->cor,
					     "%-5u", tokenmac->cor);
				break;
			  case 9:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[18], title[21];
					tokenmaclnna1_element.pf_namlen = nsprintf(name, sizeof name, "lnna[%d]", i0);
					nsprintf(title, sizeof title, "Address[%d]", i0);
					tokenmaclnna1_element.pf_name = name, tokenmaclnna1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->lnna[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmaclnna1_element, &tokenmac->lnna[i0],
						     "%-3u", tokenmac->lnna[i0]);
				} /* for i0 */
				}
				break;
			  case 10:
				if (!ds_u_long(ds, &tokenmac->pl))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[16], &tokenmac->pl,
					     "%-10lu", tokenmac->pl);
				break;
			  case 32:
				if (!ds_u_short(ds, &tokenmac->rc))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[17], &tokenmac->rc,
					     "%-25s", en_name(&tokenmacrespcode, tokenmac->rc));
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->rdcl = val;
				pv_showfield(pv, &tokenmac_fields[18], &val,
					     "%-18s", en_name(&tokenmacfunclass, tokenmac->rdcl));
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->rscl = val;
				pv_showfield(pv, &tokenmac_fields[19], &val,
					     "%-18s", en_name(&tokenmacfunclass, tokenmac->rscl));
				if (!ds_u_char(ds, &tokenmac->rmvid))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[20], &tokenmac->rmvid,
					     "%-36s", en_name(&tokenmacmvidcmd, tokenmac->rmvid));
				break;
			  case 33:
				if (!ds_u_short(ds, &tokenmac->res))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[21], &tokenmac->res,
					     "%-5u [%u]", tokenmac->res, 0);
				break;
			  case 34:
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->pad_u_charF4 = val;
				tokenmac_fields[12].pf_size = -4;
				pv_showfield(pv, &tokenmac_fields[12], &val,
					     "%-3u", tokenmac->pad_u_charF4);
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->pcl = val;
				pv_showfield(pv, &tokenmac_fields[22], &val,
					     "%-19s", en_name(&tokenmacprodcode, tokenmac->pcl));
				if (!ds_u_char(ds, &tokenmac->fty))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[23], &tokenmac->fty,
					     "%-26s", en_name(&tokenmacprodinst, tokenmac->fty));
				{ int i0;
				for (i0 = 0; i0 < 4; i0++) {
					char name[17], title[34];
					tokenmacmty1_element.pf_namlen = nsprintf(name, sizeof name, "mty[%d]", i0);
					nsprintf(title, sizeof title, "Machine Type, EBCDIC[%d]", i0);
					tokenmacmty1_element.pf_name = name, tokenmacmty1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->mty[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacmty1_element, &tokenmac->mty[i0],
						     "%-3u", tokenmac->mty[i0]);
				} /* for i0 */
				}
				{ int i0;
				for (i0 = 0; i0 < 3; i0++) {
					char name[18], title[39];
					tokenmacmnum1_element.pf_namlen = nsprintf(name, sizeof name, "mnum[%d]", i0);
					nsprintf(title, sizeof title, "Machine Model Num, EBCDIC[%d]", i0);
					tokenmacmnum1_element.pf_name = name, tokenmacmnum1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->mnum[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacmnum1_element, &tokenmac->mnum[i0],
						     "%-3u", tokenmac->mnum[i0]);
				} /* for i0 */
				}
				{ int i0;
				for (i0 = 0; i0 < 2; i0++) {
					char name[18], title[41];
					tokenmacsnum1_element.pf_namlen = nsprintf(name, sizeof name, "snum[%d]", i0);
					nsprintf(title, sizeof title, "Serial Num Modifier, EBCDIC[%d]", i0);
					tokenmacsnum1_element.pf_name = name, tokenmacsnum1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->snum[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacsnum1_element, &tokenmac->snum[i0],
						     "%-3u", tokenmac->snum[i0]);
				} /* for i0 */
				}
				{ int i0;
				for (i0 = 0; i0 < 7; i0++) {
					char name[18], title[34];
					tokenmacseqn1_element.pf_namlen = nsprintf(name, sizeof name, "seqn[%d]", i0);
					nsprintf(title, sizeof title, "Sequence Num, EBCDIC[%d]", i0);
					tokenmacseqn1_element.pf_name = name, tokenmacseqn1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->seqn[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacseqn1_element, &tokenmac->seqn[i0],
						     "%-3u", tokenmac->seqn[i0]);
				} /* for i0 */
				}
				break;
			  case 35:
				{ int i0;
				for (i0 = 0; i0 < 10; i0++) {
					char name[18], title[40];
					tokenmacrsml1_element.pf_namlen = nsprintf(name, sizeof name, "rsml[%d]", i0);
					nsprintf(title, sizeof title, "of Sending Station, EBCDIC[%d]", i0);
					tokenmacrsml1_element.pf_name = name, tokenmacrsml1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->rsml[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacrsml1_element, &tokenmac->rsml[i0],
						     "%-3u", tokenmac->rsml[i0]);
				} /* for i0 */
				}
				break;
			  case 38:
				{ int i0;
				for (i0 = 0; i0 < 1500; i0++) {
					char name[16], title[27];
					tokenmacwd1_element.pf_namlen = nsprintf(name, sizeof name, "wd[%d]", i0);
					nsprintf(title, sizeof title, "for Lobe Test[%d]", i0);
					tokenmacwd1_element.pf_name = name, tokenmacwd1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->wd[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacwd1_element, &tokenmac->wd[i0],
						     "%-3u", tokenmac->wd[i0]);
				} /* for i0 */
				}
				break;
			  case 39:
/* XXX guarded backref: len */
				{ int i0;
				tokenmac->macf = renew(tokenmac->macf, (*tokenmac).len-2, unsigned char);
				if (tokenmac->macf == 0) goto out;
				for (i0 = 0; i0 < (*tokenmac).len-2; i0++) {
					char name[18], title[23];
					tokenmacmacf1_element.pf_namlen = nsprintf(name, sizeof name, "macf[%d]", i0);
					nsprintf(title, sizeof title, "Mac Frame[%d]", i0);
					tokenmacmacf1_element.pf_name = name, tokenmacmacf1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->macf[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacmacf1_element, &tokenmac->macf[i0],
						     "%-3u", tokenmac->macf[i0]);
				} /* for i0 */
				}
				break;
			  case 40:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[17], title[21];
					tokenmacsid1_element.pf_namlen = nsprintf(name, sizeof name, "sid[%d]", i0);
					nsprintf(title, sizeof title, "Address[%d]", i0);
					tokenmacsid1_element.pf_name = name, tokenmacsid1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->sid[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacsid1_element, &tokenmac->sid[i0],
						     "%-3u", tokenmac->sid[i0]);
				} /* for i0 */
				}
				break;
			  case 41:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[18], title[20];
					tokenmacrsss1_element.pf_namlen = nsprintf(name, sizeof name, "rsss[%d]", i0);
					nsprintf(title, sizeof title, "Status[%d]", i0);
					tokenmacrsss1_element.pf_name = name, tokenmacrsss1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->rsss[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacrsss1_element, &tokenmac->rsss[i0],
						     "%-3u", tokenmac->rsss[i0]);
				} /* for i0 */
				}
				break;
			  case 42:
				if (!ds_u_short(ds, &tokenmac->tsc))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[33], &tokenmac->tsc,
					     "%-5u", tokenmac->tsc);
				break;
			  case 43:
				{ int i0;
				for (i0 = 0; i0 < 4; i0++) {
					char name[16], title[35];
					tokenmacga1_element.pf_namlen = nsprintf(name, sizeof name, "ga[%d]", i0);
					nsprintf(title, sizeof title, "Lower 4 Bytes of Addr[%d]", i0);
					tokenmacga1_element.pf_name = name, tokenmacga1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->ga[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacga1_element, &tokenmac->ga[i0],
						     "%-3u", tokenmac->ga[i0]);
				} /* for i0 */
				}
				break;
			  case 44:
				{ int i0;
				for (i0 = 0; i0 < 4; i0++) {
					char name[16], title[40];
					tokenmacfa1_element.pf_namlen = nsprintf(name, sizeof name, "fa[%d]", i0);
					nsprintf(title, sizeof title, "Recognized by this Station[%d]", i0);
					tokenmacfa1_element.pf_name = name, tokenmacfa1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->fa[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacfa1_element, &tokenmac->fa[i0],
						     "%-3u", tokenmac->fa[i0]);
				} /* for i0 */
				}
				break;
			  case 45:
				if (!ds_u_char(ds, &tokenmac->lerr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[36], &tokenmac->lerr,
					     "%-3u", tokenmac->lerr);
				if (!ds_u_char(ds, &tokenmac->ierr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[37], &tokenmac->ierr,
					     "%-3u", tokenmac->ierr);
				if (!ds_u_char(ds, &tokenmac->berr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[38], &tokenmac->berr,
					     "%-3u", tokenmac->berr);
				if (!ds_u_char(ds, &tokenmac->acerr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[39], &tokenmac->acerr,
					     "%-3u", tokenmac->acerr);
				if (!ds_u_char(ds, &tokenmac->adt))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[40], &tokenmac->adt,
					     "%-3u", tokenmac->adt);
				if (!ds_u_char(ds, &tokenmac->resv))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[41], &tokenmac->resv,
					     "%-3u", tokenmac->resv);
				break;
			  case 46:
				if (!ds_u_char(ds, &tokenmac->lferr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[42], &tokenmac->lferr,
					     "%-3u", tokenmac->lferr);
				if (!ds_u_char(ds, &tokenmac->rj))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[43], &tokenmac->rj,
					     "%-3u", tokenmac->rj);
				if (!ds_u_char(ds, &tokenmac->fcerr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[44], &tokenmac->fcerr,
					     "%-3u", tokenmac->fcerr);
				if (!ds_u_char(ds, &tokenmac->ferr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[45], &tokenmac->ferr,
					     "%-3u", tokenmac->ferr);
				if (!ds_u_char(ds, &tokenmac->terr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[46], &tokenmac->terr,
					     "%-3u", tokenmac->terr);
				if (!ds_u_char(ds, &tokenmac->resv))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[41], &tokenmac->resv,
					     "%-3u", tokenmac->resv);
				break;
			  case 48:
				if (!ds_bits(ds, &val, 16, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->ec_errorcodeF16 = val;
				pv_showfield(pv, &tokenmac_fields[47], &val,
					     "%-16s", en_name(&tokenmacerrorcode, tokenmac->ec_errorcodeF16));
				break;
			}
		} else {
			if (!ds_u_char(ds, &tokenmac->llong))
				ds->ds_count = 0;
			pv_showfield(pv, &tokenmac_fields[48], &tokenmac->llong,
				     "%-3u", tokenmac->llong);
			ps->ps_slink = &tokenmac->lsvid;
			tokenmacsvidfrmt_struct.pst_parent = &tokenmac_fields[49];
			tokenmacsvidfrmt_decode(ds, ps, pv);
			if (!ds_u_short(ds, &tokenmac->llen))
				ds->ds_count = 0;
			pv_showfield(pv, &tokenmac_fields[50], &tokenmac->llen,
				     "%-5u", tokenmac->llen);
			switch ((*tokenmac).lsvid.codept) {
			  case 1:
				if (!ds_u_short(ds, &tokenmac->beacon_breason))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[6], &tokenmac->beacon_breason,
					     "%-31s", en_name(&tokenmacbreason, tokenmac->beacon_breason));
				break;
			  case 2:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[18], title[21];
					tokenmacnaun1_element.pf_namlen = nsprintf(name, sizeof name, "naun[%d]", i0);
					nsprintf(title, sizeof title, "Address[%d]", i0);
					tokenmacnaun1_element.pf_name = name, tokenmacnaun1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->naun[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacnaun1_element, &tokenmac->naun[i0],
						     "%-3u", tokenmac->naun[i0]);
				} /* for i0 */
				}
				break;
			  case 3:
				if (!ds_u_short(ds, &tokenmac->lrn))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[8], &tokenmac->lrn,
					     "%-5u", tokenmac->lrn);
				break;
			  case 4:
				if (!ds_u_long(ds, &tokenmac->apl))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[9], &tokenmac->apl,
					     "%-10lu", tokenmac->apl);
				break;
			  case 5:
				if (!ds_u_short(ds, &tokenmac->stv))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[10], &tokenmac->stv,
					     "%-5u", tokenmac->stv);
				break;
			  case 6:
				if (!ds_u_short(ds, &tokenmac->efc))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[11], &tokenmac->efc,
					     "%-5u", tokenmac->efc);
				break;
			  case 7:
				if (!ds_bits(ds, &val, 14, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->pad_u_shortF14 = val;
				tokenmac_fields[12].pf_size = -14;
				pv_showfield(pv, &tokenmac_fields[12], &val,
					     "%-5u", tokenmac->pad_u_shortF14);
				if (!ds_bits(ds, &val, 2, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->aap = val;
				pv_showfield(pv, &tokenmac_fields[13], &val,
					     "%-3u", tokenmac->aap);
				break;
			  case 8:
				if (!ds_u_short(ds, &tokenmac->cor))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[14], &tokenmac->cor,
					     "%-5u", tokenmac->cor);
				break;
			  case 9:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[18], title[21];
					tokenmaclnna1_element.pf_namlen = nsprintf(name, sizeof name, "lnna[%d]", i0);
					nsprintf(title, sizeof title, "Address[%d]", i0);
					tokenmaclnna1_element.pf_name = name, tokenmaclnna1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->lnna[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmaclnna1_element, &tokenmac->lnna[i0],
						     "%-3u", tokenmac->lnna[i0]);
				} /* for i0 */
				}
				break;
			  case 10:
				if (!ds_u_long(ds, &tokenmac->pl))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[16], &tokenmac->pl,
					     "%-10lu", tokenmac->pl);
				break;
			  case 32:
				if (!ds_u_short(ds, &tokenmac->rc))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[17], &tokenmac->rc,
					     "%-25s", en_name(&tokenmacrespcode, tokenmac->rc));
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->rdcl = val;
				pv_showfield(pv, &tokenmac_fields[18], &val,
					     "%-18s", en_name(&tokenmacfunclass, tokenmac->rdcl));
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->rscl = val;
				pv_showfield(pv, &tokenmac_fields[19], &val,
					     "%-18s", en_name(&tokenmacfunclass, tokenmac->rscl));
				if (!ds_u_char(ds, &tokenmac->rmvid))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[20], &tokenmac->rmvid,
					     "%-36s", en_name(&tokenmacmvidcmd, tokenmac->rmvid));
				break;
			  case 33:
				if (!ds_u_short(ds, &tokenmac->res))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[21], &tokenmac->res,
					     "%-5u [%u]", tokenmac->res, 0);
				break;
			  case 34:
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->pad_u_charF4 = val;
				tokenmac_fields[12].pf_size = -4;
				pv_showfield(pv, &tokenmac_fields[12], &val,
					     "%-3u", tokenmac->pad_u_charF4);
				if (!ds_bits(ds, &val, 4, DS_ZERO_EXTEND))
					ds->ds_count = 0;
				tokenmac->pcl = val;
				pv_showfield(pv, &tokenmac_fields[22], &val,
					     "%-19s", en_name(&tokenmacprodcode, tokenmac->pcl));
				if (!ds_u_char(ds, &tokenmac->fty))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[23], &tokenmac->fty,
					     "%-26s", en_name(&tokenmacprodinst, tokenmac->fty));
				{ int i0;
				for (i0 = 0; i0 < 4; i0++) {
					char name[17], title[34];
					tokenmacmty1_element.pf_namlen = nsprintf(name, sizeof name, "mty[%d]", i0);
					nsprintf(title, sizeof title, "Machine Type, EBCDIC[%d]", i0);
					tokenmacmty1_element.pf_name = name, tokenmacmty1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->mty[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacmty1_element, &tokenmac->mty[i0],
						     "%-3u", tokenmac->mty[i0]);
				} /* for i0 */
				}
				{ int i0;
				for (i0 = 0; i0 < 3; i0++) {
					char name[18], title[39];
					tokenmacmnum1_element.pf_namlen = nsprintf(name, sizeof name, "mnum[%d]", i0);
					nsprintf(title, sizeof title, "Machine Model Num, EBCDIC[%d]", i0);
					tokenmacmnum1_element.pf_name = name, tokenmacmnum1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->mnum[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacmnum1_element, &tokenmac->mnum[i0],
						     "%-3u", tokenmac->mnum[i0]);
				} /* for i0 */
				}
				{ int i0;
				for (i0 = 0; i0 < 2; i0++) {
					char name[18], title[41];
					tokenmacsnum1_element.pf_namlen = nsprintf(name, sizeof name, "snum[%d]", i0);
					nsprintf(title, sizeof title, "Serial Num Modifier, EBCDIC[%d]", i0);
					tokenmacsnum1_element.pf_name = name, tokenmacsnum1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->snum[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacsnum1_element, &tokenmac->snum[i0],
						     "%-3u", tokenmac->snum[i0]);
				} /* for i0 */
				}
				{ int i0;
				for (i0 = 0; i0 < 7; i0++) {
					char name[18], title[34];
					tokenmacseqn1_element.pf_namlen = nsprintf(name, sizeof name, "seqn[%d]", i0);
					nsprintf(title, sizeof title, "Sequence Num, EBCDIC[%d]", i0);
					tokenmacseqn1_element.pf_name = name, tokenmacseqn1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->seqn[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacseqn1_element, &tokenmac->seqn[i0],
						     "%-3u", tokenmac->seqn[i0]);
				} /* for i0 */
				}
				break;
			  case 35:
				{ int i0;
				for (i0 = 0; i0 < 10; i0++) {
					char name[18], title[40];
					tokenmacrsml1_element.pf_namlen = nsprintf(name, sizeof name, "rsml[%d]", i0);
					nsprintf(title, sizeof title, "of Sending Station, EBCDIC[%d]", i0);
					tokenmacrsml1_element.pf_name = name, tokenmacrsml1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->rsml[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacrsml1_element, &tokenmac->rsml[i0],
						     "%-3u", tokenmac->rsml[i0]);
				} /* for i0 */
				}
				break;
			  case 38:
				{ int i0;
				for (i0 = 0; i0 < 1500; i0++) {
					char name[16], title[27];
					tokenmacwd1_element.pf_namlen = nsprintf(name, sizeof name, "wd[%d]", i0);
					nsprintf(title, sizeof title, "for Lobe Test[%d]", i0);
					tokenmacwd1_element.pf_name = name, tokenmacwd1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->wd[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacwd1_element, &tokenmac->wd[i0],
						     "%-3u", tokenmac->wd[i0]);
				} /* for i0 */
				}
				break;
			  case 39:
/* XXX guarded backref: llen */
				{ int i0;
				tokenmac->macf = renew(tokenmac->macf, (*tokenmac).llen-4, unsigned char);
				if (tokenmac->macf == 0) goto out;
				for (i0 = 0; i0 < (*tokenmac).llen-4; i0++) {
					char name[18], title[23];
					tokenmacmacf1_element.pf_namlen = nsprintf(name, sizeof name, "macf[%d]", i0);
					nsprintf(title, sizeof title, "Mac Frame[%d]", i0);
					tokenmacmacf1_element.pf_name = name, tokenmacmacf1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->macf[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacmacf1_element, &tokenmac->macf[i0],
						     "%-3u", tokenmac->macf[i0]);
				} /* for i0 */
				}
				break;
			  case 40:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[17], title[21];
					tokenmacsid1_element.pf_namlen = nsprintf(name, sizeof name, "sid[%d]", i0);
					nsprintf(title, sizeof title, "Address[%d]", i0);
					tokenmacsid1_element.pf_name = name, tokenmacsid1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->sid[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacsid1_element, &tokenmac->sid[i0],
						     "%-3u", tokenmac->sid[i0]);
				} /* for i0 */
				}
				break;
			  case 41:
				{ int i0;
				for (i0 = 0; i0 < 6; i0++) {
					char name[18], title[20];
					tokenmacrsss1_element.pf_namlen = nsprintf(name, sizeof name, "rsss[%d]", i0);
					nsprintf(title, sizeof title, "Status[%d]", i0);
					tokenmacrsss1_element.pf_name = name, tokenmacrsss1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->rsss[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacrsss1_element, &tokenmac->rsss[i0],
						     "%-3u", tokenmac->rsss[i0]);
				} /* for i0 */
				}
				break;
			  case 42:
				if (!ds_u_short(ds, &tokenmac->tsc))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[33], &tokenmac->tsc,
					     "%-5u", tokenmac->tsc);
				break;
			  case 43:
				{ int i0;
				for (i0 = 0; i0 < 4; i0++) {
					char name[16], title[35];
					tokenmacga1_element.pf_namlen = nsprintf(name, sizeof name, "ga[%d]", i0);
					nsprintf(title, sizeof title, "Lower 4 Bytes of Addr[%d]", i0);
					tokenmacga1_element.pf_name = name, tokenmacga1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->ga[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacga1_element, &tokenmac->ga[i0],
						     "%-3u", tokenmac->ga[i0]);
				} /* for i0 */
				}
				break;
			  case 44:
				{ int i0;
				for (i0 = 0; i0 < 4; i0++) {
					char name[16], title[40];
					tokenmacfa1_element.pf_namlen = nsprintf(name, sizeof name, "fa[%d]", i0);
					nsprintf(title, sizeof title, "Recognized by this Station[%d]", i0);
					tokenmacfa1_element.pf_name = name, tokenmacfa1_element.pf_title = title;
					if (!ds_u_char(ds, &tokenmac->fa[i0]))
						ds->ds_count = 0;
					pv_showfield(pv, &tokenmacfa1_element, &tokenmac->fa[i0],
						     "%-3u", tokenmac->fa[i0]);
				} /* for i0 */
				}
				break;
			  case 45:
				if (!ds_u_char(ds, &tokenmac->lerr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[36], &tokenmac->lerr,
					     "%-3u", tokenmac->lerr);
				if (!ds_u_char(ds, &tokenmac->ierr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[37], &tokenmac->ierr,
					     "%-3u", tokenmac->ierr);
				if (!ds_u_char(ds, &tokenmac->berr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[38], &tokenmac->berr,
					     "%-3u", tokenmac->berr);
				if (!ds_u_char(ds, &tokenmac->acerr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[39], &tokenmac->acerr,
					     "%-3u", tokenmac->acerr);
				if (!ds_u_char(ds, &tokenmac->adt))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[40], &tokenmac->adt,
					     "%-3u", tokenmac->adt);
				if (!ds_u_char(ds, &tokenmac->resv))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[41], &tokenmac->resv,
					     "%-3u", tokenmac->resv);
				break;
			  case 46:
				if (!ds_u_char(ds, &tokenmac->lferr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[42], &tokenmac->lferr,
					     "%-3u", tokenmac->lferr);
				if (!ds_u_char(ds, &tokenmac->rj))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[43], &tokenmac->rj,
					     "%-3u", tokenmac->rj);
				if (!ds_u_char(ds, &tokenmac->fcerr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[44], &tokenmac->fcerr,
					     "%-3u", tokenmac->fcerr);
				if (!ds_u_char(ds, &tokenmac->ferr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[45], &tokenmac->ferr,
					     "%-3u", tokenmac->ferr);
				if (!ds_u_char(ds, &tokenmac->terr))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[46], &tokenmac->terr,
					     "%-3u", tokenmac->terr);
				if (!ds_u_char(ds, &tokenmac->resv))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[41], &tokenmac->resv,
					     "%-3u", tokenmac->resv);
				break;
			  case 48:
				if (!ds_u_short(ds, &tokenmac->ec_errorcode))
					ds->ds_count = 0;
				pv_showfield(pv, &tokenmac_fields[47], &tokenmac->ec_errorcode,
					     "%-16s", en_name(&tokenmacerrorcode, tokenmac->ec_errorcode));
				break;
			}
		}
	}
	pr = 0;
	pv_decodeframe(pv, pr, ds, ps);
out:
	PS_POP(ps);
} /* tokenmac_decode */
