#ident  "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/invent.h>
#include <sys/sbd.h>
#include <klib/klib.h>
 
/** The following structures and tables were lifted from hinv.c and
 ** then modified.
 **/

/*
 * coprocessor revision identifiers
 */
union rev_id {
    unsigned int    ri_uint;
    struct {
#ifdef MIPSEB
        unsigned int    Ri_fill:16,
                		Ri_imp:8,       /* implementation id */
                		Ri_majrev:4,    /* major revision */
                		Ri_minrev:4;    /* minor revision */
#endif
#ifdef MIPSEL
        unsigned int    Ri_minrev:4,    /* minor revision */
                		Ri_majrev:4,    /* major revision */
                		Ri_imp:8,       /* implementation id */
                		Ri_fill:16;
#endif
    } Ri;
};
#define ri_imp      Ri.Ri_imp
#define ri_majrev   Ri.Ri_majrev
#define ri_minrev   Ri.Ri_minrev

struct imp_tbl {
	char *it_name;
	char *it_desc;
	unsigned it_imp;
};

/* The code assumes that any number greater than the number in the
 * table is that type of processor.  They must be in decending numerical
 * order.  The below table is in the kernel also, so the values must
 * be updated there also if cpu_imp_tbl is changed in hinv.c.
 */
struct imp_tbl cpu_imp_tbl[] = {
    { "UNKNOWN_CPU", "Unknown CPU type.",  
		C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
    { "R5000", "MIPS R5000 Processor Chip",  
		C0_MAKE_REVID(C0_IMP_R5000,0,0) },
    { "R4650", "MIPS R4650 Processor Chip",  
		C0_MAKE_REVID(C0_IMP_R4650,0,0) },
    { "R4700", "MIPS R4700 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R4700,0,0) },
    { "R4600", "MIPS R4600 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R4600,0,0) },
    { "R8000", "MIPS R8000 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R8000,0,0) },
    { "R12000", "MIPS R12000 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R12000,0,0) },
    { "R10000", "MIPS R10000 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R10000,0,0) },
    { "R6000A", "MIPS R6000A Processor Chip",
		C0_MAKE_REVID(C0_IMP_R6000A,0,0) },
    { "R4400", "MIPS R4400 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R4400,C0_MAJREVMIN_R4400,0) },
    { "R4000", "MIPS R4000 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R4000,0,0) },
    { "R6000", "MIPS R6000 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R6000,0,0) },
    { "R3000A", "MIPS R3000A Processor Chip", 
		C0_MAKE_REVID(C0_IMP_R3000A,C0_MAJREVMIN_R3000A,0) },
    { "R3000", "MIPS R3000 Processor Chip",  
		C0_MAKE_REVID(C0_IMP_R3000,C0_MAJREVMIN_R3000,0) },
    { "R2000A", "MIPS R2000A Processor Chip", 
		C0_MAKE_REVID(C0_IMP_R2000A,C0_MAJREVMIN_R2000A,0) },
    { "R2000", "MIPS R2000 Processor Chip",
		C0_MAKE_REVID(C0_IMP_R2000,0,0) },
    { 0, 0, 0 }
};

/* The code assumes that any number greater than the number in the
 * table is that type of FPU.  They must be in decending numerical
 * order.
 */
struct imp_tbl fp_imp_tbl[] = {
    { "UNKNOWN_FPU", "Unknown FPU type",
		C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
    { "R5000_FPU", "MIPS R5000 Floating Point Coprocessor",
		C0_MAKE_REVID(C0_IMP_R5000,0,0) },
    { "R4650_FPU", "MIPS R4650 Floating Point Coprocessor",  
		C0_MAKE_REVID(C0_IMP_R4650,0,0) },
    { "R4700_FPU", "MIPS R4700 Floating Point Coprocessor",  
		C0_MAKE_REVID(C0_IMP_R4700,0,0) },
    { "R4600_FPU", "MIPS R4600 Floating Point Coprocessor",  
		C0_MAKE_REVID(C0_IMP_R4600,0,0) },
    { "R8010_FPU", "MIPS R8010 Floating Point Chip",     	
		C0_MAKE_REVID(0x10,0,0) },
    { "R12010_FPU", "MIPS R12010 Floating Point Chip",        
		C0_MAKE_REVID(C0_IMP_R12000,0,0) },
    { "R10010_FPU", "MIPS R10010 Floating Point Chip",        
		C0_MAKE_REVID(C0_IMP_R10000,0,0) },
    { "R4000_FPU", "MIPS R4000 Floating Point Coprocessor",  
		C0_MAKE_REVID(5,0,0) },
    { "R6010_FPU", "MIPS R6010 Floating Point Chip",     	
		C0_MAKE_REVID(4,0,0) },
    { "R3010A_FPU", "MIPS R3010A VLSI Floating Point Chip",   
		C0_MAKE_REVID(3,3,0) },
    { "R3010_FPU", "MIPS R3010 VLSI Floating Point Chip",    
		C0_MAKE_REVID(3,2,0) },
    { "R2010A_FPU", "MIPS R2010A VLSI Floating Point Chip",   
		C0_MAKE_REVID(3,1,0) },
    { "R2010_FPU", "MIPS R2010 VLSI Floating Point Chip",    
		C0_MAKE_REVID(2,0,0) },
    { "R2360_FPU", "MIPS R2360 Floating Point Board",        
		C0_MAKE_REVID(1,0,0) },
    { 0, 0, 0 }
};

char *
imp_name(union rev_id ri, struct imp_tbl *itp)
{
    for (; itp->it_name; itp++) {
        if (((ri.ri_imp << 8) | (ri.ri_majrev << 4) 
					| (ri.ri_minrev)) >= itp->it_imp) {
            return(itp->it_name);
		}
	}
    return((char *)NULL);
}

char *
imp_desc(union rev_id ri, struct imp_tbl *itp)
{
    for (; itp->it_name; itp++) {
        if (((ri.ri_imp << 8) | (ri.ri_majrev << 4) 
					| (ri.ri_minrev)) >= itp->it_imp) {
            return(itp->it_desc);
		}
	}
    return((char *)NULL);
}

char *
cpu_name(unsigned int rev)
{
	union rev_id ri;

	ri.ri_uint = rev;
	if (ri.ri_imp == 0) {
		ri.ri_majrev = 1;
		ri.ri_minrev = 5;
	}
	return(imp_name(ri, cpu_imp_tbl));
}

char *
cpu_desc(unsigned int rev)
{
	union rev_id ri;

	ri.ri_uint = rev;
	if (ri.ri_imp == 0) {
		ri.ri_majrev = 1;
		ri.ri_minrev = 5;
	}
	return(imp_desc(ri, cpu_imp_tbl));
}

unsigned int cpu_rev;

static void
_get_cpu_info(unsigned int rev, char *Name)
{
    union rev_id ri;

    ri.ri_uint = rev;
    if (ri.ri_imp == 0) {
        ri.ri_majrev = 1;
        ri.ri_minrev = 5;
    }
    cpu_rev = ri.ri_imp;
    sprintf(Name, "CPU: MIPS %s Processor Chip Revision: %d.%u", 
		cpu_name(rev), ri.ri_majrev, ri.ri_minrev);
}

static void
get_cpu_info(cpu_data_t *cpuinfo, char *Name)
{
	sprintf(Name, "CPU %d at Slice %c: %d Mhz %s (%s)",
		cpuinfo->cpu_id,
		'A' + cpuinfo->cpu_slice,
		cpuinfo->cpu_speed,
		cpu_desc(cpuinfo->cpu_flavor),
		cpuinfo->cpu_enabled ? "enabled" : "disabled");
}

static void
get_fpu_info(unsigned int rev, char *Name)
{
    union rev_id ri;

    ri.ri_uint = rev;
        /*
         * This fix is needed to cover for the early batch
         * of TREX chips that had the FPU rev_id set erroneously
         * to 0x9 (instead of 0xe) and consequently hinv was
         * reporting FPU as R10000 rather then correct R12010.
         * Note, that CPU rev_id was correctly set to 0xe for
         * those chips.
         */
    if (ri.ri_imp == C0_IMP_R10000) {
        if (cpu_rev == C0_IMP_R12000) {
			ri.ri_imp = C0_IMP_R12000;
			sprintf(Name, "FPU: %s Revision: %d.%u", 
				imp_name(ri, fp_imp_tbl), ri.ri_majrev, ri.ri_minrev);
		}
	}
}

/*
 * inv_processor()
 */
static char *
inv_processor(invent_data_t *invp, string_table_t *st, int aflg) 
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char Name[256], *ptr, *description;

	if (!invp) {
		return((char *)NULL);
	}

	/* Get the values from the invent_data_t struct...
	 */
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	/* Clear the name string...
	 */
	Name[0] = 0;

	switch (inv_type) {

        /*
         * CPU/FPU revisions are stored in a compacted
         * form.
         */
        case INV_CPUCHIP:
            _get_cpu_info((unsigned int)inv_state, Name);
			break;

        case INV_FPUCHIP:
            if (inv_state) {
                get_fpu_info((unsigned int)inv_state, Name);
			}
            else {
                sprintf(Name, "No hardware floating point");
			}
            break;

#ifdef NOTYET
        case INV_CPUBOARD: {
            /*
             * HACK to work around the weakness of inventory facility
             * which soly depends logical id. The hack here to insert
             * the entries for disabled cpus.
             */
            if (inv_state == INV_IP19BOARD || inv_state == INV_IP21BOARD || 
							inv_state == INV_IP25BOARD ) {

                if (!first_loop)
                    return (0);

                if ((pinvent->inv_unit & ALL_8BIT) == ALL_8BIT) {
                    display_all_cpus();
                    first_loop = 0;
                }
                else {
                    evcfginfo_t *ep = &evcfg;
                    evbrdinfo_t *brd;
                    int slot = cpuid_to_slot[pinvent->inv_unit];

                    brd = &(ep->ecfg_board[slot]);

                    if (brd->eb_type != EVTYPE_IP19 &&
                        brd->eb_type != EVTYPE_IP21 &&
                        brd->eb_type != EVTYPE_IP25)
                        return (0);

                    if (last_slot != slot)
                        printf("CPU Board at Slot %d: (%s)\n",
                            slot, (brd->eb_enabled ?
                            "Enabled" : "Disabled"));

                    last_slot = slot;

                    if (!brd->eb_enabled)
                        return (0);

                    display_cpubrd(brd, pinvent->inv_unit);
                }
                return (0);
            }

            /* ALL_8BIT means, all cpu has same speed */
            if ((pinvent->inv_unit & ALL_8BIT) == ALL_8BIT) {
                cpunum = sysmp(MP_NPROCS);
                printf("%d ",cpunum);
            }
            else
                printf("Processor %d: ", pinvent->inv_unit);

            if ( pinvent->inv_controller != 0 )
                printf("%d MHZ ",pinvent->inv_controller);
            switch (pinvent->inv_state) {
                case INV_IP19BOARD:
                    printf("IP19 ");
                    break;
                case INV_IP20BOARD:
                    printf("IP20 ");
                    break;
                case INV_IP21BOARD:
                    printf("IP21 ");
                    break;
                case INV_IP22BOARD:
                    mgras_ip_type = MGRAS_IP_I2;
                    printf("IP22 ");
                    break;
                case INV_IP25BOARD:
                    printf("IP25 ");
                    break;
                case INV_IP26BOARD:
                    printf("IP26 ");
                    break;
                case INV_IP27BOARD:
                    printf("IP27 ");
                    break;
                case INV_IP28BOARD:
                    mgras_ip_type = MGRAS_IP_I2;
                    printf("IP28 ");
                    break;
                case INV_IP30BOARD:
                    mgras_ip_type = MGRAS_IP_RACER;
                    printf("IP30 ");
                    break;
                case INV_IP32BOARD:
                    printf("IP32 ");
                    break;
                }
            }
            if ((pinvent->inv_unit & ALL_8BIT) == ALL_8BIT) {
                if ( cpunum > 1 )
                    printf("Processors");
                else
                    printf("Processor");
            }
            printf("\n");
		}
#endif
	}
	if (Name[0]) {
		description = get_string(st, Name, aflg);		
		return(description);
	}
	return((char *)NULL);
}

/*
 * inv_disk()
 */
static char *
inv_disk(invent_data_t *invp, string_table_t *st, int aflg) 
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char Name[256], *ptr, *description;

	if (!invp) {
		return((char *)NULL);
	}

	/* Get the values from the invent_data_s struct...
	 */
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	/* Clear the name string...
	 */
	Name[0] = 0;

	switch (inv_type) {

		case INV_JAGUAR:
			/* Interphase 4210 VME-SCSI controller */
			/* Firmware revision %c%c%c\n",
				(inv_state >> 16)
				(inv_state >> 8), 
				inv_state
			 */
			break;

		case INV_SCSICONTROL:
		case INV_GIO_SCSICONTROL:
			if (inv_type == INV_SCSICONTROL) {
				if ((inv_controller < 1) && (inv_state == INV_ADP7880)) {
					/* PCI SCSI controller */
				}
				else {

					/* Integral SCSI controller 
					 */

					sprintf(Name, "Integral SCSI controller %d: Version ", 
						inv_controller);

					switch(inv_state) {

						case INV_WD93:
							strcat(Name, "WD33C93");
							break;

						case INV_WD93A:
							strcat(Name, "WD33C93A");
							break;

						case INV_WD93B:
							strcat(Name, "WD33C93B");
							break;

						case INV_WD95A:
							strcat(Name, "WD33C95A");
							break;

						case INV_SCIP95:
							strcat(Name, "SCIP/WD33C95A");
							break;

						case INV_QL_REV1:
							strcat(Name, "QL1040");
							break;

						case INV_QL_REV2:
							strcat(Name, "QL1040A");
							break;

						case INV_QL_REV2_4:
							strcat(Name, "QL1040A (rev. 4)");
							break;

						case INV_QL_REV3:
							strcat(Name, "QL1040B");
							break;

						case INV_QL_REV4:
							strcat(Name, "QL1040B (rev. 2)");
							break;

#ifdef NOTYET
						case INV_QL_1240:
							strcat(Name, "QL1240");
							break;

						case INV_QL_1080:
							strcat(Name, "QL1080");
							break;

						case INV_QL_1280:
							strcat(Name, "QL1280");
							break;

						case INV_QL_10160:
							strcat(Name, "QL10160");
							break;
#endif

						case INV_QL:
							strcat(Name, "QL (unknown version)");
							break;

						case INV_FCADP:
							strcat(Name, "Fibre Channel AIC-1160");
							break;

						case INV_ADP7880:
							strcat(Name, "ADAPTEC 7880"); 
							break;

						default:
							strcat(Name, "unknown");
							break;
					}

					ptr = &Name[strlen(Name)];
					switch(inv_state) {
						case INV_WD93:
						case INV_WD93A:
						case INV_WD93B:
						case INV_FCADP:
							if (inv_unit) {
								sprintf(ptr, ", revision %X", inv_unit);
							}
							break;
						case INV_QL_REV1:
						case INV_QL_REV2:
						case INV_QL_REV2_4:
						case INV_QL_REV3:
						case INV_QL_REV4:
#ifdef NOTYET
						case INV_QL_1240:
						case INV_QL_1080:
						case INV_QL_1280:
						case INV_QL_10160:
						case INV_QL_12160:
#endif
							if (inv_unit & 0x80) {
								sprintf(ptr, ", low voltage differential");
							}
							else if (inv_unit & 0x40) {
								sprintf(ptr, ", differential");
							}
							else {
								sprintf(ptr, ", single ended");
							}
							break;

						case INV_WD95A:
						case INV_SCIP95:
							if (inv_unit & 0x80) {
								sprintf(ptr, ", differential");
							}
							else {
								sprintf(ptr, ", single ended");
							}
							ptr = &Name[strlen(Name)];
							if (inv_state == INV_WD95A) {
								sprintf(ptr, ", revision %X", inv_unit & 0x7F);
							}
							break;
					}
				}
			}
			else {
				/* GIO SCSI controller */
			}
			break;

		case INV_PCI_SCSICONTROL:
			/* PCI SCSI controller */
			break;

#ifdef NOT
		case INV_PCCARD: 
#endif
		case INV_SCSIDRIVE:

			/* Get the hw_component name 
			 */
			if (inv_state & INV_RAID5_LUN) {
				sprintf(Name, "RAID lun %d", inv_unit);
			}
			else {
				sprintf(Name, "Disk drive %d", inv_unit);
			}
			break;

		case INV_SCSIFLOPPY:
			break;

		case INV_VSCSIDRIVE:
			break;

		case INV_PCCARD:
			break;

		default:
			break;
	}
	if (Name[0]) {
		description = get_string(st, Name, aflg);		
		return(description);
	}
	return((char *)NULL);
}

/*
 * inv_tape()
 */
static char *
inv_tape(invent_data_t *invp, string_table_t *st, int aflg) 
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char Name[256], *ptr, *description;

	if (!invp) {
		return((char *)NULL);
	}

	/* Get the values from the invent_data_t struct...
	 */
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	/* Clear the name string...
	 */
	Name[0] = 0;

	switch (inv_type) {
		case INV_VSCSITAPE:
		case INV_SCSIQIC:

			sprintf(Name, "Tape drive: unit %d on ", inv_unit);
			if (inv_type == INV_VSCSITAPE) {
				strcat(Name, "VME-");
			}
			ptr = &Name[strlen(Name)];
			sprintf(ptr, "SCSI controller %d: ", inv_controller);
			ptr = &ptr[strlen(ptr)];
			switch(inv_state) {
				case TPQIC24:
						strcat(Name, "QIC 24");
						break;
				case TPQIC150:
						strcat(Name, "QIC 150");
						break;
				case TPQIC1000:
						strcat(Name, "QIC 1000");
						break;
				case TPQIC1350:
						strcat(Name, "QIC 1350");
						break;
				case TPDAT:
						strcat(Name, "DAT");
						break;
				case TP9TRACK:
						strcat(Name, "9 track");
						break;
				case TP8MM_8200:
						strcat(Name, "8mm(8200) cartridge");
						break;
				case TP8MM_8500:
						strcat(Name, "8mm(8500) cartridge");
						break;
				case TP3480:
						strcat(Name, "3480 cartridge");
						break;
				case TPDLT:
				case TPDLTSTACKER:
						strcat(Name, "DLT");
						break;
				case TPD2:
						strcat(Name, "D2 cartridge");
						break;
				case TPMGSTRMP:
				case TPMGSTRMPSTCKR:
						strcat(Name, "IBM Magstar MP 3570");
						break;
				case TPNTP:
				case TPNTPSTACKER:
						strcat(Name, "IBM Magstar 3590");
						break;
				case TPSTK9490:
						strcat(Name, "STK 9490");
						break;
				case TPSTKSD3:
						strcat(Name, "STK SD3");
						break;
				case TPGY10:
						strcat(Name, "SONY GY-10");
						break;
				case TPGY2120:
						strcat(Name, "SONY GY-2120");
						break;
				case TP8MM_8900:
						strcat(Name, "8mm(8900) cartridge");
						break;
				case TP8MM_AIT:
						strcat(Name, "8mm (AIT) cartridge");
						break;
				case TPSTK4781:
						strcat(Name, "STK 4781");
						break;
				case TPSTK4791:
						strcat(Name, "STK 4791");
						break;
				case TPFUJDIANA1:
						strcat(Name, "FUJITSU M1016/M1017");
						break;
				case TPFUJDIANA2:
						strcat(Name, "FUJITSU M2483");
						break;
				case TPFUJDIANA3:
						strcat(Name, "FUJITSU M2488");
						break;
				case TPNCTP:
						strcat(Name, "Philips NCTP");
						break;
				case TPTD3600:
						strcat(Name, "Philips TD3600");
						break;
				case TPOVL490E:
						strcat(Name, "Overland Data L490E");
						break;
				case TPSTK9840:
						strcat(Name, "StorageTek 9840");
						break;
				case TPUNKNOWN:
						strcat(Name, "unknown");
						break;
				default:
						sprintf(ptr, "unknown type %d\n", inv_state);
			}
	}
	if (Name[0]) {
		description = get_string(st, Name, aflg);		
		return(description);
	}
	return((char *)NULL);
}

/*
 * inv_scsi()
 */
static char *
inv_scsi(invent_data_t *invp, string_table_t *st, int aflg) 
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char Name[256], *ptr, *description;

	if (!invp) {
		return((char *)NULL);
	}

	/* Get the values from the invent_data_t struct...
	 */
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	/* Clear the name string...
	 */
	Name[0] = 0;

	switch (inv_type) {
		case INV_PRINTER:   /* SCSI printer */
			sprintf(Name, "Printer");
			break;
		case INV_CPU:   /* SCSI CPU device */
			sprintf(Name, "Processor");
			break;
		case INV_WORM:  /* write once read-many */
			sprintf(Name, "WORM");
			break;
		case INV_CDROM: /* CD-ROM  */
			sprintf(Name, "CDROM");
			break;
		case INV_SCANNER:   /* scanner, like Ricoh IS-11 */
			sprintf(Name, "Scanner");
			break;
		case INV_OPTICAL:   /* read-write optical disks */
			sprintf(Name, "Optical disk");
			break;
		case INV_CHANGER:   /* CDROM jukeboxes */
			sprintf(Name, "Jukebox");
			break;
		case INV_COMM:      /* Communications device */
			sprintf(Name, "Comm device");
			break;
		case INV_RAIDCTLR:  /* RAID controller */
			sprintf(Name, "RAID controller");
			break;
		default:
			ptr = &Name[strlen(Name)];
			sprintf(ptr, "Unknown type %d", inv_type);
			if (inv_state & INV_REMOVE) {
				ptr = &Name[strlen(Name)];
				sprintf(ptr, " / removable media");
			}
	}

	/* Get the rest of the description...
	 */
	ptr = &Name[strlen(Name)];
	sprintf(ptr, ": unit %d", inv_unit);
	ptr = &Name[strlen(Name)];
	if ((inv_state >> 8) & 0xff) {
		ptr = &Name[strlen(Name)];
		sprintf(ptr, ", lun %ld", (inv_state >> 8) & 0xff);
		ptr = &Name[strlen(Name)];
	}
	sprintf(ptr, " on %s controller %d",
		inv_class == INV_VSCSI ? "VME-SCSI" : "SCSI", inv_controller);

	if (Name[0]) {
		description = get_string(st, Name, aflg);		
		return(description);
	}
	return((char *)NULL);
}

/*
 * inv_video()
 */
static char *
inv_video(invent_data_t *invp, string_table_t *st, int aflg) 
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char Name[256], *ptr, *description;

	if (!invp) {
		return((char *)NULL);
	}

	/* Get the values from the invent_data_t struct...
	 */
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	/* Clear the name string...
	 */
	Name[0] = 0;

	/* Get the description...
	 */
	switch (inv_type) {

		case INV_VIDEO_LIGHT:   
			sprintf(Name, "IndigoVideo board: unit %d, revision %d",
				inv_unit, inv_state);
			break;

		case INV_VIDEO_VS2:   
			sprintf(Name, "Multi-Channel Option board installed");
			break;

		case INV_VIDEO_EXPRESS: 
			if (inv_state & INV_GALILEO_JUNIOR) {
				sprintf(Name, "Indigo2 video (ev1): unit %d, revision "
					"%d.  %s  %s",
					inv_unit, inv_state & INV_GALILEO_REV,
					(inv_state & INV_GALILEO_DBOB)
						? "601 option connected." : "",
					(inv_state & INV_GALILEO_INDY_CAM)
						? "Indycam connected." : "");
			}
			else {
				sprintf(Name, "Galileo video %s (ev1): unit %d, revision "
					"%d.  %s  %s",
					(inv_state & INV_GALILEO_ELANTEC)
						? "1.1" : "",
					inv_unit,
					inv_state & INV_GALILEO_REV,
					(inv_state & INV_GALILEO_DBOB)
						? "601 option connected." : "",
					(inv_state & INV_GALILEO_INDY_CAM)
						? "Indycam connected." : "");
			}
			break;

		case INV_VIDEO_VINO:  
			sprintf(Name, "Vino video: unit %d, revision %d%s",
				inv_unit, inv_state & INV_VINO_REV,
				(inv_state & INV_VINO_INDY_NOSW) ? "" :
				((inv_state & INV_VINO_INDY_CAM)
				? ", IndyCam connected"
				: ", IndyCam not connected"));
			break;

		case INV_VIDEO_VO2:
			sprintf(Name, "Sirius video: unit %d revision %d on bus %d with ",
				inv_unit, inv_state & 0x0f, inv_controller);

			if (!(inv_state & 0xf0)) {
				strcat(Name, "no ");
			}
			if (inv_state & 0x80) {
				strcat(Name, "CPI ");
			}
			if (inv_state & 0x40) {
				strcat(Name, "DGI ");
			}
			if (inv_state & 0x20) {
				strcat(Name, "BOB ");
			}
			if (inv_state & 0x10) {
				strcat(Name, "SD1 ");
			}
			strcat(Name, "options ");
			break;

		case INV_VIDEO_INDY:   
			sprintf(Name, "Indy Video (ev1): unit %d, revision %d",
				inv_unit, inv_state);
			break;

		case INV_VIDEO_MVP:
            sprintf (Name, "Video: MVP unit %d version %d.%d",
                inv_unit, INV_MVP_REV(inv_state), INV_MVP_REV_SW(inv_state));

			ptr = &Name[strlen(Name)];
            if (INV_MVP_AV_BOARD(inv_state)) {
                sprintf(ptr, " AV: AV%d Card version %d",
                INV_MVP_AV_BOARD(inv_state), INV_MVP_AV_REV(inv_state));

				ptr = &Name[strlen(Name)];
                if (INV_MVP_CAMERA(inv_state)) {
                	sprintf(ptr, ", O2Cam type %d version %d connected.",
                    	INV_MVP_CAMERA(inv_state), INV_MVP_CAM_REV(inv_state));
                }
                else if (INV_MVP_SDIINF(inv_state)) {
					sprintf(ptr, ", SDI%d Serial Digital Interface "
						   "version %d connected.",
						INV_MVP_SDIINF(inv_state), INV_MVP_SDI_REV(inv_state));
                }
                else {
					sprintf(ptr, ", Camera not connected.");
                }
            }
			else {
				sprintf(ptr, " with no AV Card or Camera.");
            }
            break;

		case INV_VIDEO_INDY_601:
			sprintf(Name, "Indy Video 601 (ev1): unit %d, revision %d",
				inv_unit, inv_state);
			break;

		case INV_VIDEO_MGRAS:
			sprintf(Name, "IMPACT Video (mgv): unit %d, revision %d",
				inv_unit, inv_state & 0xf);
			if (inv_state & 0xf0) {
				ptr = &Name[strlen(Name)];
				sprintf(ptr, ", TMI/CSC: revision %d",
					(inv_state & 0xf0) >> 4);
			}
			break;

		case INV_VIDEO_DIVO: {

			static const char *io_configs[4] = {
				"Input", "Dual-In", "Output", "Dual-Out"
			};
			static const char *dvc_options[4] = {
				"", "DVCPro", "DVCPro Encoder", "DVCPro Decoder"
			};
			static const char *mpeg_options[4] = {
				"", "MPEG-II", "MPEG-II Encoder", "MPEG-II Decoder"
			};

			char str[100];
			unsigned int st = (unsigned int)inv_state;
			unsigned int i;

			sprintf(Name, "DIVO Video: controller %d unit %d: ",
				inv_controller, inv_unit);


			for (i = 0; i <= 1; i ++, st >>= 16) {

				/* i/o config
				 * reverse i/o sense for pipe 1; default of 0x0 means output,
				 * not input.
				 */
				ptr = &Name[strlen(Name)];
				sprintf(ptr, i ? ", %s" : "%s",
					io_configs[(st & 0x3) ^ (i ? 0x2 : 0x0)]);

				/* options
				 */
				str[0] = '\0';
				if ((st >> 4) & 0x3) {
					sprintf(str+strlen(str), " and %s",
							dvc_options[(st >> 4) & 0x3]);
				}
				if ((st >> 6) & 0x3) {
					sprintf(str+strlen(str), " and %s",
							mpeg_options[(st >> 6) & 0x3]);
				}
				ptr = &Name[strlen(Name)];
				sprintf(ptr, str[0] ? " with %s" : "",
					   str + 5 /* skip initial " and " */);
			}
			break;
		}

		case INV_VIDEO_RACER:
			sprintf(Name, "Digital Video: unit %d, revision %d.%d",
				inv_unit, inv_state & 0xf, (inv_state >> 4) & 0xf);
			ptr = &Name[strlen(Name)];
			sprintf(ptr, "TMI: revision %d", (inv_state >> 8) & 0xf);
			ptr = &Name[strlen(Name)];
			sprintf(ptr, ", CSC: revision %d", (inv_state >> 12) & 0xf);
			break;

		case INV_VIDEO_EVO:
			sprintf(Name, "Personal Video: unit %d, revision %d.%d",
				inv_unit, inv_state & 0xf, (inv_state >> 4) & 0xf);
			if ((inv_state >> 8) & 0x0f) {
				ptr = &Name[strlen(Name)];
				sprintf(ptr, ", DigCam version %d.%d connected", 
					(inv_state >> 16) & 0xf, (inv_state >> 12) & 0xf);
			}
			else if ((inv_state >> 20) & 0x0f) {
				ptr = &Name[strlen(Name)];
				sprintf(ptr, ", SDI%d version %d connected",
					(inv_state >> 20) & 0xf, (inv_state >> 24) & 0xff);
			}
			break;

		case INV_VIDEO_XTHD:
			sprintf(Name, "XT-HDIO Video: controller %d, unit %d, "
				"version 0x%x", inv_controller, inv_unit, inv_state);
			break;
	
		default:
			sprintf(ptr, "Unknown type %d class %d", inv_type, inv_class);
	}
	if (Name[0]) {
		description = get_string(st, Name, aflg);
		return(description);
	}
	return((char *)NULL);
}

/*
 * inv_graphics()
 */
static char *
inv_graphics(invent_data_t *invp, string_table_t *st, int aflg) 
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char Name[256], *ptr, *description;

	if (!invp) {
		return((char *)NULL);
	}

	/* Get the values from the invent_data_t struct...
	 */
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	/* Clear the name string...
	 */
	Name[0] = 0;

	/* Get the description...
	 */
	switch (inv_type) {
		case INV_GRODEV:   
			sprintf(Name, "Graphics option installed");
			break;
		case INV_GMDEV:   
			sprintf(Name, "GT Graphics option installed");
			break;
		case INV_VGX:  
			sprintf(Name, "VGX Graphics option installed");
			break;
		case INV_VGXT: 
			sprintf(Name, "VGXT Graphics option installed");
			break;
		case INV_RE:   
			if (inv_state) {
				sprintf(Name, "RealityEngineII Graphics Pipe %d at IO "
					"Slot %d Physical Adapter %d (Fchip rev %d%s)\n",
					inv_unit, inv_controller/8, inv_controller%8, inv_state,
					(inv_state==1)?"<-BAD!":"");
			}
			else {
				sprintf(Name, "RealityEngine Graphics option installed");
			}
			break;

		case INV_VTX:   
			if (inv_state) {
				sprintf(Name, "VTX Graphics Pipe %d at IO Slot %d "
					"Physical Adapter %d (Fchip rev %d%s)\n",
					inv_unit, inv_controller/8, inv_controller%8, inv_state,
					(inv_state==1)?"<-BAD!":"");
			}
			else {
				sprintf(Name, "VTX Graphics option installed");
			}
			break;

		case INV_CG2:   
			sprintf(Name, "Genlock option installed");
			break;

		case INV_LIGHT:
			sprintf(Name, "Graphics board: LG1");
			break;

		case INV_GR2:
			if (inv_state & INV_GR2_GR3) {
				sprintf(Name, "Graphics board: GR3");
			}
			else if (inv_state & INV_GR2_INDY) {
				sprintf(Name, "Graphics board: GR3");
			}
			else if (inv_state & INV_GR2_GR5) {
				sprintf(Name, "Graphics board: GR5");
			}
			else if (inv_state & INV_GR2_GU1) {
				sprintf(Name, "Graphics board: GU1");
			}
			else { 
				sprintf(Name, "Graphics board: GR2");
			}
			switch (inv_state & 0x3c) {
				case INV_GR2_1GE:
					strcat(Name, "-XS");
					if (inv_state & INV_GR2_24) {
						strcat(Name, "24");
					}
					if (inv_state & INV_GR2_Z) {
						strcat(Name, " with Z-buffer");
					}
					break;

				case INV_GR2_2GE:
					strcat(Name, "-XZ");
					if ((inv_state & INV_GR2_24) == 0) {
						strcat(Name, " missing bitplanes");
					}
					if ((inv_state & INV_GR2_Z) == 0) {
						strcat(Name, " missing Z");
					}
					break;

				case INV_GR2_4GE:
					if ((inv_state & INV_GR2_24) && (inv_state & INV_GR2_Z)) {
						if ((inv_state & INV_GR2_INDY) || 
								(inv_state & INV_GR2_GR5)) {
							strcat(Name, "-XZ");
						}
						else {
							strcat(Name, "-Elan");
						}
					}
					else {
						strcat(Name, "-XSM");
					}
					break;

				case INV_GR2_8GE:
					if ((inv_state & INV_GR2_24) && (inv_state & INV_GR2_Z)) {
						strcat(Name, "-Extreme");
					}
					else {
						strcat(Name, "-8GE,");
						if (inv_state & INV_GR2_24) {
							strcat(Name, "24");
						}
						else {
							strcat(Name, "Z");
						}
					}
					break;

				default:
					strcat(Name, "-Unknown configuration");
					break;

			}
			break;

		case INV_NEWPORT:
			strcat(Name, "Graphics board: ");
			if (inv_state & INV_NEWPORT_XL) {
				strcat(Name, "XL");     /* Indigo2 */
			}
			else {
				ptr = &Name[strlen(Name)];
				sprintf(ptr, "Indy %d-bit", 
					(inv_state & INV_NEWPORT_24) ? 24 : 8);
			}
			break;

		case INV_MGRAS: {
			strcat(Name, "Graphics board: ");

			if (((inv_state & INV_MGRAS_GES) == INV_MGRAS_1GE) &&
				((inv_state & INV_MGRAS_RES) == INV_MGRAS_1RE) &&
				((inv_state & INV_MGRAS_TRS) == INV_MGRAS_0TR)){
				switch(inv_state & INV_MGRAS_ARCHS){
					case INV_MGRAS_MOT:
						strcat(Name, "ESI");
						break;
					case INV_MGRAS_HQ4:
						strcat(Name, "SI");
						break;
					case INV_MGRAS_HQ3:
						strcat(Name, "Solid Impact");
						break;
				}
			}
			else if (((inv_state & INV_MGRAS_GES) == INV_MGRAS_1GE) &&
				 ((inv_state & INV_MGRAS_RES) == INV_MGRAS_1RE) &&
				 ((inv_state & INV_MGRAS_TRS) != INV_MGRAS_0TR)){
				switch(inv_state & INV_MGRAS_ARCHS){
					case INV_MGRAS_MOT:
						strcat(Name, "ESI with texture option");
						break;
					case INV_MGRAS_HQ4:
						strcat(Name, "SI with texture option");
						break;
					case INV_MGRAS_HQ3:
						strcat(Name, "High Impact");
						break;
				}
			}
			else if (((inv_state & INV_MGRAS_GES) == INV_MGRAS_2GE) &&
				 ((inv_state & INV_MGRAS_RES) == INV_MGRAS_1RE) &&
				 ((inv_state & INV_MGRAS_TRS) != INV_MGRAS_0TR)) {
				strcat(Name, "High-AA Impact");
			}
			else if (((inv_state & INV_MGRAS_GES) == INV_MGRAS_2GE) &&
				 ((inv_state & INV_MGRAS_RES) == INV_MGRAS_2RE) &&
				 ((inv_state & INV_MGRAS_TRS) != INV_MGRAS_0TR)){
				switch(inv_state & INV_MGRAS_ARCHS){
					case INV_MGRAS_MOT:
						strcat(Name, "EMXI");
						break;
					case INV_MGRAS_HQ4:
						strcat(Name, "MXI");
						break;
					case INV_MGRAS_HQ3:
						strcat(Name, "Maximum Impact");
						break;
				}
			}
			else if (((inv_state & INV_MGRAS_GES) == INV_MGRAS_2GE) &&
				 ((inv_state & INV_MGRAS_RES) == INV_MGRAS_2RE) &&
				 ((inv_state & INV_MGRAS_TRS) == INV_MGRAS_0TR)){
				switch(inv_state & INV_MGRAS_ARCHS){
					case INV_MGRAS_MOT:
						strcat(Name, "ESSI");
						break;
					case INV_MGRAS_HQ4:
						strcat(Name, "SSI");
						break;
					case INV_MGRAS_HQ3:
						printf(Name, "Super Solid Impact");
						break;
				}
			}
			else {
				strcat(Name, "Impact");   /* unknown config */
			}

			if ((inv_state & INV_MGRAS_TRS) > INV_MGRAS_2TR &&
				((inv_state & INV_MGRAS_ARCHS) == INV_MGRAS_HQ3)) {
				printf("/TRAM option card");
			}
			break;
		}

		case INV_IR:
			strcat(Name, "Graphics board: InfiniteReality");
			break;

		case INV_IR2:
			strcat(Name, "Graphics board: InfiniteReality2");
			break;

		case INV_IR2LITE:
			strcat(Name, "Graphics board: Reality");
			break;

		case INV_IR2E:
			strcat(Name, "Graphics board: InfiniteReality2E");
			break;

		case INV_CRIME:
			strcat(Name, "CRM graphics installed");
			break;
	}
	if (Name[0]) {
		description = get_string(st, Name, aflg);
		return(description);
	}
	return((char *)NULL);
}

/* 
 * get_board_name() -- lifted from hinv.c 
 *
 */
void
get_board_name(int type, char *name)
{
        static char     *board_name[] = {"BASEIO","MSCSI","MENET",
                                         "HIPPI-Serial",
                                         "PCI XIO","MIO","FIBRE CHANNEL"};
        static int      board_type[] = {INV_O2000BASEIO,INV_O2000MSCSI,
                                        INV_O2000MENET,INV_O2000HIPPI,
                                        INV_O2000HAROLD,INV_O2000MIO,
                                        INV_O2000FC};
        static int      num_types = sizeof(board_type)/sizeof(int);

        int             i;

        strcpy(name,"I/O");
        for(i = 0 ; i < num_types; i++) {
			if (board_type[i] == type) {
				strcpy(name, board_name[i]);
			}
		}
}

/*
 * inv_description()
 */
char *
inv_description(invent_data_t *invp, string_table_t *st, int aflg)
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	char *ptr, *description, Name[256];

	if (!invp) {
		return((char *)NULL);
	}
	inv_class = invp->inv_class;
	inv_type = invp->inv_type;
	inv_controller = invp->inv_controller;
	inv_unit = invp->inv_unit;
	inv_state = invp->inv_state;

	Name[0] = 0;
	switch (inv_class) {
		case INV_PROCESSOR:
			return(inv_processor(invp, st, aflg));

		case INV_DISK:
			return(inv_disk(invp, st, aflg));

		case INV_MEMORY: {
			switch (inv_type) {
			
				case INV_MAIN_MB:
					sprintf(Name, "Main memory size: %d Mbytes", inv_state);
					break;
			}
			break;
		}

		case INV_SERIAL:
			switch (inv_type) {
				case INV_IOC3_DMA:
					sprintf(Name, "IOC3 serial port: tty%d", inv_controller);
					break;
			}
			break;

		case INV_PARALLEL:
			switch (inv_type) {

				case INV_EPP_ECP_PLP:
					if (inv_state == 1) {
						sprintf(Name, "On-board EPP/ECP parallel port");
					}
					else {
						sprintf(Name, "IOC3 Parallel port: plp%d",
							inv_controller);
					}
					break;
			}
			break;

		case INV_TAPE:
			return(inv_tape(invp, st, aflg));

		case INV_GRAPHICS:
			return(inv_graphics(invp, st, aflg));

		case INV_NETWORK:
			switch (inv_controller) {
				case INV_ETHER_EF:
					if (inv_unit == 0) {
						sprintf(Name, "Integral Fast Ethernet: ef%d, "
							"version %ld", inv_unit, inv_state & 0xf);	
					}
					else {
						sprintf(Name, "Fast Ethernet: ef%d, version %ld", 
							inv_unit, inv_state & 0xf);	
					}
				break;
			}
			break;

		case INV_SCSI:
		case INV_VSCSI:
			return(inv_scsi(invp, st, aflg));

		case INV_AUDIO:
			switch (inv_type) {
				case INV_AUDIO_HDSP:
					sprintf(Name, "Iris Audio Processor: revision %d", 
						inv_state);	
					break;

				case INV_AUDIO_A2:
					break;

				case INV_AUDIO_A3:
					break;

				case INV_AUDIO_RAD:
					sprintf(Name, "Iris Audio Processor: version RAD "
						"revision %d.%d, number %d", 
						(inv_state >> 4) & 0xf,	
						inv_state & 0xf,	
						inv_controller);
					break;
			}
			break;

		case INV_IOBD:
			switch (inv_type) {

				case INV_EVIO:
					sprintf(Name, "I/O board, Ebus slot %d: ", inv_unit);
                    switch (inv_state) {

                        case INV_IO4_REV1:
							strcat(Name, "IO4 revision 1\n");
							break;

                        default:
                        	strcat(Name, "IO4 of unknown revision\n");
                    }
					break;

				case INV_O200IO:
					sprintf(Name, "Origin 200 base I/O, module %d slot %d\n",
                    	inv_controller, inv_unit);
					break;

                case INV_O2000BASEIO:
				case INV_O2000MSCSI:
				case INV_O2000MENET:
				case INV_O2000HIPPI:
				case INV_O2000HAROLD:
				case INV_O2000MIO:
				case INV_O2000FC: {
					char name[50];

					get_board_name(inv_type, name);
                    sprintf(Name, "Origin %s board, module %d "
						"slot %d: Revision %d",
						name, inv_controller, inv_unit, inv_state);
					break;
				}

				case INV_PCIADAP: 
					sprintf(Name, "PCI Adapter");
					ptr = &Name[strlen(Name)];
					if ((inv_controller == 0) && (inv_unit == 0)) {
						sprintf(ptr, ", pci slot %d", inv_state);
					}
					else {
						sprintf(ptr, " ID (vendor %d, device %d) pci slot %d",
							inv_controller, inv_unit, inv_state);
					}
					break;
			}
			break;

		case INV_VIDEO:
			return(inv_video(invp, st, aflg));

		case INV_BUS:
			break;

		case INV_MISC:
			switch(inv_type) {

				case INV_MISC_EPC_EINT:
					sprintf(Name, "EPC external interrupts");
					break;

				case INV_MISC_IOC3_EINT:
					sprintf(Name, "IOC3 external interrupts: %d", 
						inv_controller);
					break;

				case INV_MISC_PCKM:
					if (inv_state == 1) {
						sprintf(Name, "PC keyboard"); 
					}
					else {
						sprintf(Name, "PC Mouse"); 
					}
					break;

				case INV_PCI_BRIDGE:
					sprintf(Name, "ASIC Bridge Revision %d", inv_controller);
					break;

				case INV_MISC_OTHER:
					sprintf(Name, "INV_MISC_OTHER type");
					break;

				default:
					sprintf(Name, "Unknown type %d class %d\n",
						inv_type, inv_class);
					break;
			}
			break;

		case INV_COMPRESSION:
			break;

		case INV_DISPLAY:
			break;

		case INV_UNC_SCSILUN:
			break;

		case INV_PCI:
			break;

		case INV_PCI_NO_DRV:
			break;

		case INV_PROM:
			/*
			get_prom_info(invp, Name);
			*/
			break;

		case INV_IEEE1394:
			break;

		case INV_RPS:
			break;

		case INV_TPU:
			break;

		default:
			break;
	}
	if (Name[0]) {
		description = get_string(st, Name, aflg);		
		return(description);
	}
	return((char *)NULL);
}

/*
 * inv_name()
 */
char *
inv_name(hw_component_t *hwcp, string_table_t *st, int aflg)
{
	int inv_class, inv_type, inv_controller, inv_unit, inv_state;
	k_ptr_t invp; 
	char *ptr, *name, Name[256];

	inv_class = hwcp->c_inv_class;
	inv_type = hwcp->c_inv_type;
	inv_controller = hwcp->c_inv_controller;
	inv_unit = hwcp->c_inv_unit;
	inv_state = hwcp->c_inv_state;

	Name[0] = 0;

	switch (inv_class) {

		case INV_PROCESSOR:
			switch (inv_type) {
				case INV_CPUBOARD:
					break;

				case INV_CPUCHIP:
					strcat(Name, cpu_name((unsigned int)inv_state));
					break;

				case INV_FPUCHIP:
					break;
			}
			break;

		case INV_DISK:
			switch (inv_type) {
				case INV_SCSICONTROL:
				case INV_GIO_SCSICONTROL:
					sprintf(Name, "SCSI_CTLR_%d", inv_controller);
					break;

				case INV_SCSIDRIVE:
					sprintf(Name, "DRIVE_%d", inv_unit);
					break;
			}
			break;

		case INV_MEMORY:
			switch (inv_type) {
			
				case INV_MAIN_MB:
					sprintf(Name, "MAIN_MEMORY_%dMB", inv_state);
					break;
			}
			break;

		case INV_SERIAL:
			switch (inv_type) {
				case INV_IOC3_DMA:
					sprintf(Name, "SERIAL_TTY%d", inv_controller);
					break;
			}
			break;

		case INV_PARALLEL:
			switch (inv_type) {

				case INV_EPP_ECP_PLP:
					if (inv_state == 1) {
						sprintf(Name, "PARALLEL");
					}
					else {
						sprintf(Name, "PARALLEL_PLP%d", inv_controller);
					}
					break;
			}
			break;

		case INV_TAPE:
			sprintf(Name, "TAPE_%d", inv_unit);
			break;

		case INV_GRAPHICS:
			break;

		case INV_NETWORK:
			switch (inv_controller) {
				case INV_ETHER_EF:
					sprintf(Name, "ETHERNET_EF%d", inv_unit);
					break;
				case INV_FORE_HE:
					sprintf(Name, "ATM_FA%d", inv_unit);
					break;
				case INV_FORE_PCA:
					sprintf(Name, "ATM_FA%d", inv_unit);
					break;
			}
			break;

		case INV_SCSI:
		case INV_VSCSI:
			switch(inv_type) {
				case INV_PRINTER:   /* SCSI printer */
					sprintf(Name, "PRINTER");
					break;
				case INV_CPU:   /* SCSI CPU Device*/
					sprintf(Name, "PROCESSOR");
					break;
				case INV_WORM:  /* write once read-many */
					sprintf(Name, "WORM");
					break;
				case INV_CDROM: /* CD-ROM  */
					sprintf(Name, "CDROM");
					break;
				case INV_SCANNER:   /* scanner, like Ricoh IS-11 */
					sprintf(Name, "SCANNER");
					break;
				case INV_OPTICAL:   /* read-write optical disks */
					sprintf(Name, "OPTICAL_DISK");
					break;
				case INV_CHANGER:   /* CDROM jukeboxes */
					sprintf(Name, "JUKEBOX");
					break;
				case INV_COMM:      /* Communications device */
					sprintf(Name, "COMM_DEVICE");
					break;
				case INV_RAIDCTLR:  /* RAID controller */
					sprintf(Name, "RAID_CONTROLLER");
					break;
				default:
					sprintf(Name, "UNKNOWN_SCSI");
					break;
			}
			ptr = &Name[strlen(Name)];
			sprintf(ptr, "_%d", inv_unit);
			break;

		case INV_AUDIO:
			switch (inv_type) {
				case INV_AUDIO_HDSP:
				case INV_AUDIO_A2:
				case INV_AUDIO_A3:
				case INV_AUDIO_RAD:
					sprintf(Name, "AUDIO");
					break;
			}
			break;

		case INV_IOBD:
			switch (inv_type) {

				case INV_EVIO:
					sprintf(Name, "IO4");
					break;

				case INV_O200IO:
					sprintf(Name, "BASEIO");
					break;

                case INV_O2000BASEIO:
				case INV_O2000MSCSI:
				case INV_O2000MENET:
				case INV_O2000HIPPI:
				case INV_O2000HAROLD:
				case INV_O2000MIO:
				case INV_O2000FC:
					get_board_name(inv_type, Name);
					break;

				case INV_PCIADAP: 
					sprintf(Name, "PCI_%d", inv_state);
					break;
			}
			break;

		case INV_VIDEO:
			break;

		case INV_BUS:
			break;

		case INV_MISC:
			switch(inv_type) {

				case INV_MISC_EPC_EINT:
					sprintf(Name, "EPC_EXT_INTERRUPTS");
					break;

				case INV_MISC_IOC3_EINT:
					sprintf(Name, "IOC3_EXT_INTERRUPTS");
					break;

				case INV_MISC_PCKM:
					if (inv_state == 1) {
						sprintf(Name, "KEYBOARD"); 
					}
					else {
						sprintf(Name, "MOUSE"); 
					}
					break;

				case INV_PCI_BRIDGE:
					sprintf(Name, "ASIC_BRIDGE");
					break;

				case INV_MISC_OTHER:
					sprintf(Name, "INV_MISC");
					break;

				default:
					sprintf(Name, "UNKNOWN");
					break;
			}
			break;

		case INV_COMPRESSION:
			break;

		case INV_DISPLAY:
			break;

		case INV_UNC_SCSILUN:
			break;

		case INV_PCI:
			break;

		case INV_PCI_NO_DRV:
			break;

		case INV_PROM:
			break;

		case INV_IEEE1394:
			break;

		case INV_RPS:
			break;

		case INV_TPU:
			break;

		default:
			break;
	}

	if (Name[0]) {
		name = get_string(st, Name, aflg);		
		return(name);
	}
	return((char *)NULL);
}

/*
 * hwcp_description()
 */
char *
hwcp_description(hw_component_t *hwcp, string_table_t *st, int aflg)
{
	char *desc, Desc[256];

	desc = (char *)NULL;
	Desc[0] = 0;

	switch (hwcp->c_category) {

		case HW_CPU:
			get_cpu_info(CPU_DATA(hwcp), Desc);
			break;

		case HW_MEMBANK:
			sprintf(Desc, "Memory Bank %d contains %d MB (%s) DIMMS (%s)",
				MEMBANK_DATA(hwcp)->mb_bnk_number,
				MEMBANK_DATA(hwcp)->mb_size,
				(MEMBANK_DATA(hwcp)->mb_attr & INV_MEM_PREMIUM) ? 
					"Premium" : "Standard",
				(MEMBANK_DATA(hwcp)->mb_flag & INVENT_ENABLED) ?
					"enabled" : "disabled");
			break;
	}
	if (Desc[0]) {
		desc = get_string(st, Desc, aflg);		
		return(desc);
	}
	if (desc = inv_description(&hwcp->c_invent, st, aflg)) {
		return(desc);
	}
	return((char *)NULL);
}
