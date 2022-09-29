/***********************************************************************\
*	File:		wd95a_wcs.c					*
*									*
*	Contains the code for the Control Store for the wd95a		*
*									*
*	NOTE:  This file needs to maintain strict coordianation with	*
*	       wd95a_wcs.h						*
*									*
\***********************************************************************/

#include "wd95a_wcs.h"

unsigned int	_wcs_ini_load[] = {
#include "wd95iniwcs.hex"
};

unsigned int _wcs_scip_load[] = {
#include "wd95scipwcs.hex"
};
