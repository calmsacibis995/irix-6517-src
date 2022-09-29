/*  
 * fru_decode.c-
 *
 * This file decodes the various structures returned by the FRU analysis
 * code and prints the results.
 *
 */

#include "evfru.h"

/*
 *Structure for a FRU entry.  FRU is really a misnomer here.  What we're
 * saying is that these are things that can fail whether field replaceable
 * or not.
 */
typedef struct fru_table_entry_s {
	short fru_id;
	char *fru_name;
} fru_table_entry_t;

/*
 * Here's the "FRU" table.  Each "FRU" identifier is made up of a class
 * (e.g. board, bus ASIC, IO adapter) and an instance.  Check out evfru.h
 * for more details.
 */
static fru_table_entry_t fru_table[37] = {
	{FRU_NONE,	"None"},

	{FRU_SOFTWARE,	"Software"},

	{FRU_IPBOARD,	"PROCESSOR BOARD"},
	{FRU_A,		"A CHIP"},
	{FRU_D,		"D CHIP"},
	{FRU_CPU,	"CPU"},
	{FRU_CACHE,	"CACHE"},
	{FRU_CC,	"CC CHIP"},
	{FRU_BBCC,	"BBCC CHIP"},
	{FRU_TAGRAM,	"TAG RAM"},
	{FRU_GCACHE,	"GCACHE"},
	{FRU_DB,	"DB CHIP"},
	{FRU_BUSTAG,	"BUS TAG"},
	{FRU_FPU,	"FPU"},

	{FRU_MCBOARD,	"MEMORY BOARD"},
	{FRU_MA,	"MA CHIP"},
	{FRU_MD,	"MD CHIP"},
	{FRU_BANK,	"MEMORY BANK"},
	{FRU_MD,	"MEMORY SIMM"},

	{FRU_IOBOARD,	"IO4 BOARD"},
	{FRU_IA,	"IA CHIP"},
	{FRU_ID,	"ID CHIP"},
	{FRU_MAPRAM,	"IO4 MAP RAM"},
	{FRU_EPC,	"EPC CHIP"},
	{FRU_S1,	"S1 CHIP"},
	{FRU_SCIP,	"SCIP CHIP"},
	{FRU_F,		"F CHIP"},
	{FRU_VMECC,	"VMECC on VCAM"},
	{FRU_FCG,	"FCG CHIP"},
	{FRU_DANG,	"DANG CHIP"},
	{FRU_HIPPI,	"HIPPI ADAPTER"},

	{FRU_ANIOA,	"IO ADAPTERS"},

	{FRU_VMEDEV,	"VME DEVICE or DRIVER SOFTWARE"},

	{FRU_SCACHE,	"SECONDARY CACHE SIMMS"},
	{FRU_PCACHE,	"PRIMARY CACHE"},
	{FRU_SYSAD,	"SysAd Bus or Other CPUs"},

	{-1,		(char *)0}
};


/*
 * These are the confidence levels we use to call out errors.
 * The percentages are somewhat arbitrary.  We use them only to
 * present something field engineers will be familiar with.
 */
char *confidence_strings[NUM_CONFIDENCE_LEVELS] = {
	"Not involved",
	"10%",		/* Witnessed error */
	"30%",		/* Possible */
	"40%",		/* Possible */
	"70%",		/* Probable */
	"90%",		/* Definite */
	"95%"		/* Exact pattern match */
};


/* Print out a particular diagnosed element. */
/* ARGSUSED */
static void print_fru(fru_element_t *element, fru_table_entry_t *entry,
		      evcfginfo_t *ec)
		      /* ec will be used to determine what's actually a FRU */
{
	int class;

	class = element->unit_type & FRU_CLASSMASK;

	if (element->unit_type & FRUF_SOFTWARE)
		if (!fru_ignore_sw)
			class = FRUC_SOFTWARE;
		else
			FRU_PRINTF("Likely SOFTWARE: ");

	/* Use class to determine print format. */
	switch (class) {

		case FRUC_NONE:
			FRU_PRINTF("No error found");
			break;

		case FRUC_SOFTWARE:
			FRU_PRINTF("Error caused by SOFTWARE");
			break;

		case FRUC_BOARD:
			FRU_PRINTF("%s", entry->fru_name);
			break;

		case FRUC_EBUS_ASIC:
			FRU_PRINTF("Integral component (%s)",
				entry->fru_name);
			break;

		case FRUC_MEM_BANK:
			if (element->unit_num != NO_UNIT) {
				int leaf = element->unit_num >> 2;
				int bank = element->unit_num & 3;
				FRU_PRINTF("%s: leaf %d bank %d (%c)",
					entry->fru_name,
					leaf, bank, 
					bank_letter(leaf, bank));
			} else {
				FRU_PRINTF("%s: Can't identify bank",
					entry->fru_name);
			}
			break;

		case FRUC_IOA:
			FRU_PRINTF("%s (IO adapter %d)", entry->fru_name,
			       element->unit_num);
			break;

		case FRUC_FCIDEV:
			FRU_PRINTF("%s attached to F chip IO adapter %d",
				entry->fru_name, element->unit_num);
			break;

		case FRUC_CPUSLICE:
			FRU_PRINTF("CPU slice %d (%s)",
				element->unit_num, entry->fru_name);
			break;

		case FRUC_ONEOF:
			FRU_PRINTF("ONE OF THE %s", entry->fru_name);
			break;

		case FRUC_VMEDEV:
			FRU_PRINTF("%s, VME bus adapter %d, address 0x%llx",
				entry->fru_name, element->unit_num,
				element->unit_address);
			break;

		default:
			FRU_PRINTF("Unknown FRU class (0x%x) %s", class,
							entry->fru_name);
			break;
	}

}

/* Decode a FRU identifier and get its hum,an readable name. */
void decode_fru(fru_element_t *element, evcfginfo_t *ec)
{
	int i;

	for (i = 0; fru_table[i].fru_name; i++) {
		if (fru_table[i].fru_id == (FRU_MASK & element->unit_type)) {
			print_fru(element, &(fru_table[i]), ec);
			return;
		}
	}

	FRU_PRINTF("Can't decode FRU (0x%x)", element->unit_type);

        return;
}


/* Convert a board type to a string. */
char *type_to_string(int board)
{
	switch(board) {
	    case EVTYPE_IP19:
		return "IP19";
	    case EVTYPE_IP21:
		return "IP21";
	    case EVTYPE_MC3:
		return "MC3";
	    case EVTYPE_IO4:
		return "IO4";
	}
	return "Unknown";
}


/* Print which board a display applies to. */
/* ARGSUSED */
void decode_board(whatswrong_t *ww, evcfginfo_t *ec)
		    /* ec will be used to determine what's actually a FRU */
{
	/* Don't print the board if we think it's a software error. */
	if ((FRU_CLASS(ww->src.unit_type) == FRUC_SOFTWARE) ||
	    (!fru_ignore_sw && (ww->src.unit_type & FRUF_SOFTWARE))) {
		FRU_PRINTF(FRU_INDENT "++   ");
		return;
	}
	if (FRU_CLASS(ww->src.unit_type) == FRUC_BOARD)
		FRU_PRINTF(FRU_INDENT "++   ");
	else
		FRU_PRINTF(FRU_INDENT "++   on the ");
		FRU_PRINTF("%s board in slot %d: ",
			type_to_string(ww->board_type),
			ww->slot_num);
}


/*
 * Display the information stored in a whatswrong_t - That is, all information
 * about devices connected to a particular Ebus slot.
 */
void display_whatswrong(whatswrong_t *ww, evcfginfo_t *ec)
{
	FRU_PRINTF(FRU_INDENT "++ ");
	if (ww->src.unit_type && ww->dest.unit_type) {
		decode_fru(&(ww->src), ec);
		FRU_PRINTF("\n" FRU_INDENT "++  and/or ");
		decode_fru(&(ww->dest), ec);
		FRU_PRINTF("\n");
	} else {
		if (ww->src.unit_type)
			decode_fru(&(ww->src), ec);
		else if (ww->dest.unit_type)
			decode_fru(&(ww->dest), ec);
		FRU_PRINTF("\n");
	}
	decode_board(ww, ec);
	
	FRU_PRINTF("%s confidence.\n",
		confidence_strings[ww->confidence]);
	return;
}


