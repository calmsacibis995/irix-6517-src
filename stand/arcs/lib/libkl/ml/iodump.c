#include <sys/types.h>
#include "libsc.h"
#include "libkl.h"
#include <sys/SN/agent.h>

extern char *hub_rrb_err_type[];
extern char *hub_wrb_err_type[];

char *
decode_xrb_err(pi_err_stack_t entry)
{
	if (entry.pi_stk_fmt.sk_rw_rb)
		/* WRB */
		return (hub_wrb_err_type[entry.pi_stk_fmt.sk_err_type]);
	else
		/* RRB */
		return (hub_rrb_err_type[entry.pi_stk_fmt.sk_err_type]);
}


static char crb_status_bits[] = "PVRAWHIT";

void
decode_crb_stat(char *deststring, int stat)
{
	int bit = sizeof(crb_status_bits) - 1;
	deststring[sizeof(crb_status_bits) + 1] = '0' + (stat & 0x3);
	deststring[sizeof(crb_status_bits)] = 'E';
	stat >>= 2;	/* Get rid of the E field. */
	while (bit >= 0) {
		if (stat & (1 << bit))
			deststring[bit] = crb_status_bits[bit];
		else
			deststring[bit] = '-';
		bit --;
	}
	deststring[sizeof(crb_status_bits) + 2] = '\000';
}

/* ARGSUSED */
void
print_spool_entry(pi_err_stack_t entry, int num, int (*pf)(const char *, ...))
{
	char curr_crb_status[12];

	pf(" Entry %d:\n", num);
	decode_crb_stat(curr_crb_status, entry.pi_stk_fmt.sk_crb_sts);
	pf("  Cmd %02x, %s stat: %s CRB #%d, T5 req #%d, supp %d\n",
		entry.pi_stk_fmt.sk_cmd,
		entry.pi_stk_fmt.sk_rw_rb ? "WRB" : "RRB",
		curr_crb_status,
		entry.pi_stk_fmt.sk_crb_num,
		entry.pi_stk_fmt.sk_t5_req,
		entry.pi_stk_fmt.sk_suppl);
	pf("  Error %s, Cache line address 0x%lx\n",
		decode_xrb_err(entry),
		entry.pi_stk_fmt.sk_addr << 7);
		
#if 0
	entry.pi_stk_fmt.sk_addr >> (31 - 7);	/* address */
	entry.pi_stk_fmt.sk_cmd;		/* message command */
	entry.pi_stk_fmt.sk_crb_sts		/* status from RRB or WRB */
	entry.pi_stk_fmt.sk_rw_rb		/* RRB == 0, WRB == 1 */
	entry.pi_stk_fmt.sk_crb_num		/* WRB (0 to 7) or RRB (0 to 4) */
	entry.pi_stk_fmt.sk_t5_req		/* RRB T5 request number */
	entry.pi_stk_fmt.sk_suppl		/* lowest 3 bit of supplemental */
	entry.pi_stk_fmt.sk_err_type		/* error type */
#endif

	return;
}

void
dump_error_spool(nasid_t nasid, int slice, int (*pf)(const char *, ...))
{
	pi_err_stack_t *start, *end, *current;
	int size;
	int entries = 0;

	size = ERR_STACK_SIZE_BYTES(*REMOTE_HUB(nasid, PI_ERR_STACK_SIZE));

	end = *(pi_err_stack_t **)REMOTE_HUB(nasid,
			PI_ERR_STACK_ADDR_A + (slice * PI_STACKADDR_OFFSET));
	end = (pi_err_stack_t *)TO_NODE_UNCAC(nasid, (__psunsigned_t)end);

	start = (pi_err_stack_t *)((__psunsigned_t)end &
				  ~((__psint_t)(size - 1)));
	start = (pi_err_stack_t *)TO_NODE_UNCAC(nasid, (__psunsigned_t)start);

	pf("Error spool - nasid %d, cpu %c\n", nasid, 'A' + slice);
	for (current = start; current < end; current++) {
		print_spool_entry(*current, entries, pf);
		entries++;
	}
	pf("Spool dump complete.\n");

}


void crbx(nasid_t nasid, int (*pf)(const char *, ...))
{
  int ind;
  icrba_t crb_a;
  icrbb_t crb_b;
  icrbc_t crb_c;
  icrbd_t crb_d;
  char imsgtype[3];
  char imsg[8];
  char reqtype[6];
  char *xtsize_string[] = {"DW", "QC", "FC", "??"};
  char initiator[6];

  pf("\nNASID %d:\n", nasid);
  pf("   ________________CRB_A____________   _________________CRB_B_______________________________  __________CRB_C__________  ___CRB_D___\n");
  pf("   S S E E L                             C    S     I                      A           I S S  P P B           S   B      T C      T \n");
  pf("   B B R R N M X S T                   B O S  R   U M  I       I     E     C   R   H   N T T  R R T           U   A R    O T C    M \n");
  pf("   T T R R U A E I N             V I   T H I  C   O T  M       N     X     K   E A O   T L L  C P E           P   R R G  V X T    O \n");
  pf("   E E O C C R R D U             L O   E T Z  N   L Y  S       I     C     C   S C L W V I I  N S O           P   O Q B  L T X    U \n");
  pf("   0 1 R D E K R N M  ADDRESS    D W   # R E  D   D P  G       T     L     T   P K D B N B N  T C P PA_BE     L   P D R  D V T    T \n");
  pf("   - - - - - - - - -  -------    - -   - - -- -   - -  -       -     -     -   - - - - - - -  - - - ------    -   - - -  - - -    - \n");
  for (ind = 0; ind < 15; ind++) {
    crb_a.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_A(ind));
    crb_b.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_B(ind));
    crb_c.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_C(ind));
    crb_d.reg_value = REMOTE_HUB_L(nasid, IIO_ICRB_D(ind));
    switch (crb_b.icrbb_field_s.imsgtype) {
    case 0:  
      strcpy(imsgtype, "XT");
      /*
       * In the xtalk fields, Only a few bits are valid.
       * The valid fields bit mask is 0xAF i.e bits 7, 5, 3..0 are
       * the only valid bits. We can ignore the values in the
       * other bit. 
       */
      switch (crb_b.icrbb_field_s.imsg & 0xAF) {
      case 0x00: strcpy(imsg, "PRDC   "); break;
      case 0x20: strcpy(imsg, "BLKRD  "); break;
      case 0x02: strcpy(imsg, "PWRC   "); break;
      case 0x22: strcpy(imsg, "BLKWR  "); break;
      case 0x06: strcpy(imsg, "FCHOP  "); break;
      case 0x08: strcpy(imsg, "STOP   "); break;
      case 0x80: strcpy(imsg, "RDIO   "); break;
      case 0x82: strcpy(imsg, "WRIO   "); break;
      case 0xA2: strcpy(imsg, "PTPWR  "); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    case 1:  
      strcpy(imsgtype, "BT"); 
      switch (crb_b.icrbb_field_s.imsg) {
      case 0x01: strcpy(imsg, "BSHU   "); break;
      case 0x02: strcpy(imsg, "BEXU   "); break;
      case 0x04: strcpy(imsg, "BWINV  "); break;
      case 0x08: strcpy(imsg, "BZERO  "); break;
      case 0x10: strcpy(imsg, "BDINT  "); break;
      case 0x20: strcpy(imsg, "INTR   "); break;
      case 0x40: strcpy(imsg, "BPUSH  "); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    case 2:  
      strcpy(imsgtype, "LN"); 
      switch (crb_b.icrbb_field_s.imsg) {
      case 0x61: strcpy(imsg, "VRD    "); break;
      case 0x62: strcpy(imsg, "VWR    "); break;
      case 0x64: strcpy(imsg, "VXCHG  "); break;
      case 0xe9: strcpy(imsg, "VRPLY  "); break;
      case 0xea: strcpy(imsg, "VWACK  "); break;
      case 0xec: strcpy(imsg, "VXACK  "); break;
      case 0xf9: strcpy(imsg, "VERRA  "); break;
      case 0xfa: strcpy(imsg, "VERRC  "); break;
      case 0xfb: strcpy(imsg, "VERRCA "); break;
      case 0xfc: strcpy(imsg, "VERRP  "); break;
      case 0xfd: strcpy(imsg, "VERRPA "); break;
      case 0xfe: strcpy(imsg, "VERRPC "); break;
      case 0xff: strcpy(imsg, "VERRPCA"); break;
      case 0x00: strcpy(imsg, "RDSH   "); break;
      case 0x01: strcpy(imsg, "RDEX   "); break;
      case 0x02: strcpy(imsg, "READ   "); break;
      case 0x03: strcpy(imsg, "RSHU   "); break;
      case 0x04: strcpy(imsg, "REXU   "); break;
      case 0x05: strcpy(imsg, "UPGRD  "); break;
      case 0x06: strcpy(imsg, "RLQSH  "); break;
      case 0x08: strcpy(imsg, "IRSHU  "); break;
      case 0x09: strcpy(imsg, "IREXU  "); break;
      case 0x0a: strcpy(imsg, "IRDSH  "); break;
      case 0x0b: strcpy(imsg, "IRDEX  "); break;
      case 0x0c: strcpy(imsg, "IRMVE  "); break;
      case 0x0f: strcpy(imsg, "INVAL  "); break;
      case 0x10: strcpy(imsg, "PRDH   "); break;
      case 0x11: strcpy(imsg, "PRDI   "); break;
      case 0x12: strcpy(imsg, "PRDM   "); break;
      case 0x13: strcpy(imsg, "PRDU   "); break;
      case 0x14: strcpy(imsg, "PRIHA  "); break;
      case 0x15: strcpy(imsg, "PRIHB  "); break;
      case 0x16: strcpy(imsg, "PRIRA  "); break;
      case 0x17: strcpy(imsg, "PRIRB  "); break;
      case 0x18: strcpy(imsg, "ODONE  "); break;
      case 0x2f: strcpy(imsg, "LINVAL "); break;
      case 0x30: strcpy(imsg, "PWRH   "); break;
      case 0x31: strcpy(imsg, "PWRI   "); break;
      case 0x32: strcpy(imsg, "PWRM   "); break;
      case 0x33: strcpy(imsg, "PWRU   "); break;
      case 0x34: strcpy(imsg, "PWIHA  "); break;
      case 0x35: strcpy(imsg, "PWIHB  "); break;
      case 0x36: strcpy(imsg, "PWIRA  "); break;
      case 0x37: strcpy(imsg, "PWIRB  "); break;
      case 0x38: strcpy(imsg, "GFXWS  "); break;
      case 0x40: strcpy(imsg, "WB     "); break;
      case 0x41: strcpy(imsg, "WINV   "); break;
      case 0x50: strcpy(imsg, "GFXWL  "); break;
      case 0x51: strcpy(imsg, "PTPWR  "); break;
      case 0x80: strcpy(imsg, "SACK   "); break;
      case 0x81: strcpy(imsg, "EACK   "); break;
      case 0x82: strcpy(imsg, "UACK   "); break;
      case 0x83: strcpy(imsg, "UPC    "); break;
      case 0x84: strcpy(imsg, "UPACK  "); break;
      case 0x85: strcpy(imsg, "AERR   "); break;
      case 0x86: strcpy(imsg, "PERR   "); break;
      case 0x87: strcpy(imsg, "IVACK  "); break;
      case 0x88: strcpy(imsg, "WERR   "); break;
      case 0x89: strcpy(imsg, "WBEAK  "); break;
      case 0x8a: strcpy(imsg, "WBBAK  "); break;
      case 0x8b: strcpy(imsg, "DNACK  "); break;
      case 0x8c: strcpy(imsg, "SXFER  "); break;
      case 0x8d: strcpy(imsg, "PURGE  "); break;
      case 0x8e: strcpy(imsg, "DNGRD  "); break;
      case 0x8f: strcpy(imsg, "XFER   "); break;
      case 0x90: strcpy(imsg, "PACK   "); break;
      case 0x91: strcpy(imsg, "PACKH  "); break;
      case 0x92: strcpy(imsg, "PACKN  "); break;
      case 0x93: strcpy(imsg, "PNKRA  "); break;
      case 0x94: strcpy(imsg, "PNKRB  "); break;
      case 0x95: strcpy(imsg, "GFXCS  "); break;
      case 0x96: strcpy(imsg, "GFXCL  "); break;
      case 0x97: strcpy(imsg, "PTPCR  "); break;
      case 0x98: strcpy(imsg, "WIC    "); break;
      case 0x99: strcpy(imsg, "WACK   "); break;
      case 0x9a: strcpy(imsg, "WSPEC  "); break;
      case 0x9b: strcpy(imsg, "RACK   "); break;
      case 0x9c: strcpy(imsg, "BRMVE  "); break;
      case 0x9d: strcpy(imsg, "DERR   "); break;
      case 0x9e: strcpy(imsg, "PRERR  "); break;
      case 0x9f: strcpy(imsg, "PWERR  "); break;
      case 0xa0: strcpy(imsg, "BINV   "); break;
      case 0xa3: strcpy(imsg, "WTERR  "); break;
      case 0xa4: strcpy(imsg, "RTERR  "); break;
      case 0xa7: strcpy(imsg, "SPRPLY "); break;
      case 0xa8: strcpy(imsg, "ESPRPLY"); break;
      case 0xb0: strcpy(imsg, "PRPLY  "); break;
      case 0xb1: strcpy(imsg, "PNAKW  "); break;
      case 0xb2: strcpy(imsg, "PNKWA  "); break;
      case 0xb3: strcpy(imsg, "PNKWB  "); break;
      case 0xc0: strcpy(imsg, "SRESP  "); break;
      case 0xc1: strcpy(imsg, "SRPLY  "); break;
      case 0xc2: strcpy(imsg, "SSPEC  "); break;
      case 0xc4: strcpy(imsg, "ERESP  "); break;
      case 0xc5: strcpy(imsg, "ERPC   "); break;
      case 0xc6: strcpy(imsg, "ERPLY  "); break;
      case 0xc7: strcpy(imsg, "ESPEC  "); break;
      case 0xc8: strcpy(imsg, "URESP  "); break;
      case 0xc9: strcpy(imsg, "URPC   "); break;
      case 0xca: strcpy(imsg, "URPLY  "); break;
      case 0xcc: strcpy(imsg, "SXWB   "); break;
      case 0xcd: strcpy(imsg, "PGWB   "); break;
      case 0xce: strcpy(imsg, "SHWB   "); break;
      case 0xd8: strcpy(imsg, "BRDSH  "); break;
      case 0xd9: strcpy(imsg, "BRDEX  "); break;
      case 0xda: strcpy(imsg, "BRSHU  "); break;
      case 0xdb: strcpy(imsg, "BREXU  "); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    case 3:  
      strcpy(imsgtype, "CR"); 
      switch (crb_b.icrbb_field_s.imsg) {
      case 0x00: strcpy(imsg, "NOP"); break;
      case 0x01: strcpy(imsg, "WAKEUP"); break;
      case 0x02: strcpy(imsg, "TIMEOUT"); break;
      case 0x04: strcpy(imsg, "EJECT"); break;
      case 0x08: strcpy(imsg, "FLUSH"); break;
      default:   sprintf(imsg, "??0x%02x?",crb_b.icrbb_field_s.imsg); break;
      } 
      break;
    default:
      break;
    } 
    switch (crb_b.icrbb_field_s.reqtype) {
      case  0:  strcpy(reqtype, "PRDC "); break;
      case  1:  strcpy(reqtype, "BLKRD"); break;
      case  2:  strcpy(reqtype, "DXPND"); break;
      case  3:  strcpy(reqtype, "EJPND"); break;
      case  4:  strcpy(reqtype, "BSHU "); break;
      case  5:  strcpy(reqtype, "BEXU "); break;
      case  6:  strcpy(reqtype, "RDEX "); break;
      case  7:  strcpy(reqtype, "WINV "); break;
      case  8:  strcpy(reqtype, "PIORD"); break;
      case  9:  strcpy(reqtype, "PIOWR"); break; /* Warning. spec is wrong */
      case 16:  strcpy(reqtype, "WB   "); break;
      case 17:  strcpy(reqtype, "DEX  "); break;
      default:  sprintf(reqtype, "?0x%02x",crb_b.icrbb_field_s.reqtype); break; 
    } 
    switch(crb_b.icrbb_field_s.initator) {
      case 0: strcpy(initiator, "XTALK"); break;
      case 1: strcpy(initiator, "HubIO"); break;
      case 2: strcpy(initiator, "LgNet"); break;
      case 3: strcpy(initiator, "CRB  "); break;
      case 5: strcpy(initiator, "BTE_1"); break;
      default: sprintf(initiator, "??%d??", crb_b.icrbb_field_s.initator); break;
    }
    pf("%x: %x %x %x %x %x %x %x %x %02x %010lx %x %x   ",
	   ind,
	   crb_a.icrba_fields_s.stall_bte0,       /*   1  */ 
	   crb_a.icrba_fields_s.stall_bte1,       /*   1  */ 
	   crb_a.icrba_fields_s.error,       /*   1  */ 
	   crb_a.icrba_fields_s.ecode,       /*   3  */ 
	   crb_a.icrba_fields_s.lnetuce,     /*   1  */ 
	   crb_a.icrba_fields_s.mark,        /*   1  */ 
	   crb_a.icrba_fields_s.xerr,        /*   1  */ 
	   crb_a.icrba_fields_s.sidn,        /*   4  */ 
	   crb_a.icrba_fields_s.tnum,        /*   5  */ 
	   crb_a.icrba_fields_s.addr<<3,     /*  38  */ 
	   crb_a.icrba_fields_s.valid,       /*   1  */ 
	   crb_a.icrba_fields_s.iow);        /*   1  */
    pf("%x %x %s %03x %x %s %s %s %s ",
	   crb_b.icrbb_field_s.btenum,       /*   1  */ 
	   crb_b.icrbb_field_s.cohtrans,     /*   1  */ 
	   xtsize_string[crb_b.icrbb_field_s.xtsize],       /*   2  */ 
	   crb_b.icrbb_field_s.srcnode,      /*   9  */ 
	   crb_b.icrbb_field_s.useold,       /*   1  */ 
	   imsgtype,
	   imsg,       
	   initiator,
	   reqtype);
    pf("%03x %x %x %x %x %x %x %x  ",
	   crb_b.icrbb_field_s.ackcnt,       /*  11  */ 
	   crb_b.icrbb_field_s.resp,         /*   1  */ 
	   crb_b.icrbb_field_s.ack,          /*   1  */ 
	   crb_b.icrbb_field_s.hold,         /*   1  */ 
	   crb_b.icrbb_field_s.wb_pend,      /*   1  */ 
	   crb_b.icrbb_field_s.intvn,        /*   1  */ 
	   crb_b.icrbb_field_s.stall_ib,     /*   1  */ 
	   crb_b.icrbb_field_s.stall_intr);  /*   1  */
	   
    pf("%x %x %x %09x %03x %x %x %x  %x %x %04x %02x\n",
	   crb_c.icrbc_field_s.pricnt,       /*   4  */
	   crb_c.icrbc_field_s.pripsc,       /*   4  */
	   crb_c.icrbc_field_s.bteop,        /*   1  */
	   crb_c.icrbc_field_s.push_be,      /*  34  */
	   crb_c.icrbc_field_s.suppl,        /*  11  */
	   crb_c.icrbc_field_s.barrop,       /*   1  */
	   crb_c.icrbc_field_s.doresp,       /*   1  */
	   crb_c.icrbc_field_s.gbr,          /*   1  */
	   
	   crb_d.icrbd_field_s.toutvld,      /*   1  */
	   crb_d.icrbd_field_s.ctxtvld,      /*   1  */
	   crb_d.icrbd_field_s.context,      /*  15  */
	   crb_d.icrbd_field_s.timeout);     /*   8  */
  }

}
