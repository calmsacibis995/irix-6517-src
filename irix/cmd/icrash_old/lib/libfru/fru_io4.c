/*
 * fru_io4.c-
 *
 *	This file contains all of the functions needed to analyze an
 * IO4 board and the ASICs connected to it.
 *
 */

#include "evfru.h"			/* FRU analyzer definitions */

#include <sys/EVEREST/io4.h>            /* IA chip */
#include <sys/EVEREST/dang.h>           /* DANG chip */
#include <sys/EVEREST/fchip.h>          /* F chip */
#include <sys/EVEREST/s1chip.h>         /* S1 chip */
#include <sys/EVEREST/vmecc.h>          /* VMECC chip */

#include "fru_pattern.h"

/* Forward declarations */
short ioa_to_frutype(unsigned char);
void check_epc(evbrdinfo_t *, everror_t *, whatswrong_t *, int);
void check_dang(evbrdinfo_t *, everror_t *, whatswrong_t *, int);
void check_fchip(evbrdinfo_t *, everror_t *, whatswrong_t *, int);
void check_s1(evbrdinfo_t *, everror_t *, whatswrong_t *, int);
void check_scip(evbrdinfo_t *, everror_t *, whatswrong_t *, int);

void check_vmecc(evbrdinfo_t *, everror_t *, whatswrong_t *, int, int);
void check_fcg(evbrdinfo_t *, everror_t *, whatswrong_t *, int, int);
void check_hippi(evbrdinfo_t *, everror_t *, whatswrong_t *, int, int);

/* ARGSUSED */
void check_io4(evbrdinfo_t *eb, everror_t *ee, everror_ext_t *eex,
	     whatswrong_t *ww, evcfginfo_t *ec)
		/* ec is here to let us tell what's _really_ a FRU */
{
    int islot;
    int i_error, e_error;
    int vid;
    fru_element_t e_side, sw;

#ifdef DEBUG
    if (fru_debug)
	    FRU_PRINTF("Check IO4\n");
#endif

    e_side.unit_type = FRU_IOBOARD;
    e_side.unit_num = NO_UNIT;

    vid = eb->eb_io.eb_winnum;

    e_error = ee->io4[vid].ebus_error;

#define IO4_E_WITNESS	IO4_EBUSERROR_DATA_ERR | \
			IO4_EBUSERROR_ADDR_ERR

#define IO4_E_PROBABLE	IO4_EBUSERROR_MY_DATA_ERR | \
			IO4_EBUSERROR_MY_ADDR_ERR

#define IO4_E_POSSIBLE	IO4_EBUSERROR_INVDIRTYCACHE | \
			IO4_EBUSERROR_TRANTIMEOUT

#define IO4_E_SOFTWARE	IO4_EBUSERROR_BADIOA | \
			IO4_EBUSERROR_PIO

#define IO4_E_KNOWNERR	0

    /* Read resource timeouts are a nasty case.  They _usually_ involve the
     * I/O board, but we don't really have a way of knowing which board
     * it was.
     */
    if (e_error & IO4_EBUSERROR_TIMEOUT) {
	fru_element_t bad_ioa;

	bad_ioa.unit_type = FRU_ANIOA;

	update_confidence(ww, POSSIBLE, &bad_ioa, NO_ELEM);
    }

    conditional_update(ww, WITNESS, e_error, IO4_E_WITNESS, &e_side, NO_ELEM);
    conditional_update(ww, POSSIBLE, e_error, IO4_E_POSSIBLE, &e_side, NO_ELEM);
    if (e_error & (IO4_E_SOFTWARE)) {
	sw.unit_type = FRU_IOBOARD | FRUF_SOFTWARE;
	sw.unit_num = NO_UNIT;
	update_confidence(ww, POSSIBLE, &sw, NO_ELEM);
    }
    conditional_update(ww, PROBABLE, e_error, IO4_E_PROBABLE, &e_side, NO_ELEM);

#ifdef DEBUG
    if (fru_debug)
	FRU_PRINTF("    e_error = 0x%x\n", e_error);
#endif

    i_error = ee->io4[vid].ibus_error;

#define IO4_I_WITNESS	
#define IO4_I_PROSSIBLE	IO4_IBUSERROR_DMAWDATA | \
			IO4_IBUSERROR_PIORESPDATA | \
			IO4_IBUSERROR_PIOWDATA | \
			IO4_IBUSERROR_IARESP
#define IO4_I_PROBABLE	IO4_IBUSERROR_1LVLMAPRESP | \
			IO4_IBUSERROR_1LVLMAPDATA | \
			IO4_IBUSERROR_2LVLMAPRESP | \
			IO4_IBUSERROR_2LVL1MAP
#define IO4_I_KNOWNERR	IO4_IBUSERROR_COMMAND | \
			IO4_IBUSERROR_PIORESPCMND | \
			IO4_IBUSERROR_DMAWCMND | \
			IO4_IBUSERROR_PIOWCMND | \
			IO4_IBUSERROR_PIORCMND | \
			IO4_IBUSERROR_GFXWCMND | \
			IO4_IBUSERROR_DMARESPCMND
			
    islot = IO4_IBUSERROR_IOA(i_error);

#ifdef DEBUG
    if (fru_debug)
	FRU_PRINTF("    i_error = 0x%x\n", i_error);
#endif

    for (islot = 1; islot < IO4_MAX_PADAPS; islot++) {
	switch(eb->eb_ioarr[islot].ioa_type) {
	    case IO4_ADAP_SCSI:
		break;

	    case IO4_ADAP_EPC:
		check_epc(eb, ee, ww, islot);
		break;

	    case IO4_ADAP_FCHIP:
	    case IO4_ADAP_VMECC:
	    case IO4_ADAP_HIPPI:
	    case IO4_ADAP_FCG:
		check_fchip(eb, ee, ww, islot);
		break;

	    case IO4_ADAP_DANG:
		check_dang(eb, ee, ww, islot);
		break;

	    case IO4_ADAP_SCIP:
		check_scip(eb, ee, ww, islot);
		break;

	} /* switch */
    } /* for islot */

    return;

}


/* This guy needs some work. */
/* ARGSUSED */
void check_epc(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww, int islot)
{
	int ibus_error, ia_np_error1, ia_np_error2, error;
/*
	evioacfg_t *ioa;
*/
	int vid;
	fru_element_t epc_side, e_side;

	vid = eb->eb_ioarr[islot].ioa_virtid;

	ibus_error = ee->epc[vid].ibus_error;
	ia_np_error1 = ee->epc[vid].ia_np_error1;
	ia_np_error2 = ee->epc[vid].ia_np_error2;

	error = EPC_IERR_IA(ibus_error);

	/* Analyze errors from IA to IOA. */

	e_side.unit_type = FRU_IA;
	e_side.unit_num = NO_UNIT;

	epc_side.unit_type = FRU_EPC;
	epc_side.unit_num = islot;

	error = EPC_IERR_EPC(ibus_error);

#define EPC_IA_KNOWNERR	EPC_IERR_EPC_PIOR_CMND | \
			EPC_IERR_EPC_DMAR_CMND | \
			EPC_IERR_EPC_DMAW_CMND | \
			EPC_IERR_EPC_INTR_CMND | \
			EPC_IERR_EPC_PIOR_CMNDX | \
			EPC_IERR_EPC_DMAR_CMNDX | \
			EPC_IERR_EPC_DMAW_CMNDX

	conditional_update(ww, DEFINITE, error, EPC_IA_KNOWNERR,
			   &e_side, &epc_side);

#define EPC_ID_KNOWNERR	EPC_IERR_EPC_PIOR_DATA | \
			EPC_IERR_EPC_DMAW_DATA | \
			EPC_IERR_EPC_PIOR_DATAX | \
			EPC_IERR_EPC_DMAW_DATAX

	e_side.unit_type = FRU_ID;

	conditional_update(ww, DEFINITE, error, EPC_ID_KNOWNERR,
			   &e_side, &epc_side);

	/* Analyze errors on EPC side. */
}

/* ARGSUSED */
void check_s1(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww, int islot)
{
#if S1_LATER
	int ibus_error, csr, ierr_loglo, ierr_loghi;
	evioacfg_t *ioa;
	int vid;
#endif

}

void check_fchip(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww,
		int islot)
{
	int error;
	evioacfg_t *ioa;
	int vid;
	fru_element_t ibus_side, f_side, fci_side;
	fchiperror_t *fe;

#ifdef DEBUG
	if (fru_debug)
	    FRU_PRINTF("Checking F chip...\n");
#endif

	ioa = &(eb->eb_ioarr[islot]);
	vid = ioa->ioa_virtid;

	switch (ioa->ioa_type) {
	    case IO4_ADAP_VMECC:
		fe = &(ee->fvmecc[vid]);
		break;

	    case IO4_ADAP_HIPPI:
		fe = &(ee->fio4hip[vid]);
		break;

	    case IO4_ADAP_FCG:
		fe = &(ee->ffcg[vid]);
		break;

	    case IO4_ADAP_FCHIP:
/*
 * XXX - Is this a bug?  Shall we just assume it's OK?  Do we even save info
 * in this case?
 */
		return;

	    default:
		FRU_PRINTF("evfru: Unknown FCI adapter ioa_type (0x%x)\n",
			ioa->ioa_type);
		return;
	}

	error = fe->error;

	f_side.unit_type = FRU_F;
	f_side.unit_num = islot;

	ibus_side.unit_type = FRU_ID;
	ibus_side.unit_num = NO_UNIT;

#define F_ID_PROBABLE \
	FCHIP_ERROR_TO_IBUS_PIOR_DATA | \
	FCHIP_ERROR_FR_IBUS_DMAR_DATA | \
	FCHIP_ERROR_FR_IBUS_PIOW_DATA | \
	FCHIP_ERROR_TO_IBUS_DMAW_DATA

	conditional_update(ww, PROBABLE, error, F_ID_PROBABLE,
			   &f_side, &ibus_side);

	ibus_side.unit_type = FRU_IA;

#define F_IA_PROBABLE \
	FCHIP_ERROR_TO_IBUS_CMND | \
	FCHIP_ERROR_TO_IBUS_DMAR_TO | \
	FCHIP_ERROR_FR_IBUS_CMND | \
	FCHIP_ERROR_TO_IBUS_DMAW_CMND | \
	FCHIP_ERROR_TO_IBUS_AMAP_CMND | \
	FCHIP_ERROR_FR_IBUS_AMAP_DATA | \
	FCHIP_ERROR_TO_IBUS_AMAP_TO

/*
 * FCHIP_ERROR_FR_IBUS_AMAP_DATA is in here because a failure on the IO
 * side will be a single-level map which is stored in the IA chip map RAM.
 */

	conditional_update(ww, PROBABLE, error, F_IA_PROBABLE,
			   &f_side, &ibus_side);

#define F_IA_DEFINITE \
	FCHIP_ERROR_FR_IBUS_PIOW_INTRNL | \
	FCHIP_ERROR_FR_IBUS_SUPRISE

	conditional_update(ww, DEFINITE, error, F_IA_DEFINITE,
			   &f_side, &ibus_side);

#define F_DEFINITE \
	FCHIP_ERROR_DROP_PIOW_MODE | \
	FCHIP_ERROR_DROP_DMAW_MODE | \
	FCHIP_ERROR_TO_IBUS_INTR_CMND | \
	FCHIP_ERROR_FAKE_DMAR_MODE

	conditional_update(ww, DEFINITE, error, F_DEFINITE,
			   &f_side, NO_ELEM);

	fci_side.unit_type = ioa_to_frutype(ioa->ioa_type);
	fci_side.unit_num = islot;

#define FCI_PROBABLE \
	FCHIP_ERROR_FR_FCI_DMAW_DATA | \
	FCHIP_ERROR_FR_FCI_PIOR_DATA | \
	FCHIP_ERROR_FR_FCI_LOAD_ADDRR

	conditional_update(ww, PROBABLE, error, FCI_PROBABLE,
			   &f_side, &fci_side);

	switch (ioa->ioa_type) {
	    case IO4_ADAP_VMECC:
		check_vmecc(eb, ee, ww, islot, vid);
		break;

	    case IO4_ADAP_HIPPI:
		check_hippi(eb, ee, ww, islot, vid);
		break;

	    case IO4_ADAP_FCG:
		check_fcg(eb, ee, ww, islot, vid);
		break;

	}

}

/* ARGSUSED */
void check_vmecc(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww, int islot,
		 int vid)
	/* We'll need eb to figure out what's really a FRU */
{
	int error;
	fru_element_t fci_side, vme_side, vmecc_side;

	error = ee->vmecc[vid].error;

	fci_side.unit_type = FRU_F;
	fci_side.unit_num = islot;

	vme_side.unit_type = FRU_VMEDEV;
	vme_side.unit_num = vid;
	vme_side.unit_address = (long long)ee->vmecc[vid].addrvme;

	vmecc_side.unit_type = FRU_VMECC;
	vmecc_side.unit_num = islot;

#define VMECC_FCI_PROBABLE \
	VMECC_ERROR_FCIDB_TO | VMECC_ERROR_FCI_PIOPAR | VMECC_ERROR_VMEBUS_SLVP

	conditional_update(ww, PROBABLE, error, VMECC_FCI_PROBABLE,
			   &fci_side, &vmecc_side);

#define VME_DEFINITE \
	VMECC_ERROR_VMEBUS_PIOW | VMECC_ERROR_VMEBUS_PIOR | \
	VMECC_ERROR_VMEBUS_TO

	conditional_update(ww, DEFINITE, error, VME_DEFINITE,
			   &vmecc_side, &vme_side);

	return;
}


/* ARGSUSED */
void check_fcg(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww, int islot,
		int vid)
{
	return;
}


/* ARGSUSED */
void check_hippi(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww, int islot,
		int vid)
{
	return;
}

/* ARGSUSED */
void check_dang(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww,
		int islot)
{


}

/* ARGSUSED */
void check_scip(evbrdinfo_t *eb, everror_t *ee, whatswrong_t *ww,
		int islot)
{


}


struct ioa_fru_translation {
	unsigned char ioa_type;
	short fru_type;
} ioa_fru_table[] = {
	{ IO4_ADAP_SCSI,		FRU_S1 },
	{ IO4_ADAP_EPC,			FRU_EPC },
	{ IO4_ADAP_FCHIP,		FRU_F },
	{ IO4_ADAP_SCIP,		FRU_SCIP },
	{ IO4_ADAP_VMECC,		FRU_VMECC },
	{ IO4_ADAP_HIPPI,		FRU_HIPPI },
	{ IO4_ADAP_FCG,			FRU_FCG },
	{ IO4_ADAP_DANG,		FRU_DANG },
	{ IO4_ADAP_NULL,		FRU_NONE }
};

short ioa_to_frutype(unsigned char ioa)
{
	int i;

	for (i = 0; ioa_fru_table[i].ioa_type; i++) {
		if (ioa_fru_table[i].ioa_type == ioa)
			return ioa_fru_table[i].fru_type;
	}

	return FRU_UNKNOWN;
}

#ifdef FRU_PATTERN_MATCHER

int match_io4board(fru_entry_t **token, everror_t *errbuf, evcfginfo_t *ecbuf,
		   whatswrong_t *ww, fru_case_t *case_ptr)
{
	*token = find_board_end(*token);
	return 1;
}

#endif /* FRU_PATTERN_MATCHER */

