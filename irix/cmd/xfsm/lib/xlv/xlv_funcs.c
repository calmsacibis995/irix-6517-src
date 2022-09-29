/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *      The XLV functions implementation file.
 */

#ident "xlv_funcs.c: $Revision: 1.13 $"

#include <sys/dvh.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pfmt.h>
#include <pathnames.h>
#include <diskinfo.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_error.h>
#include <xlv_utils.h>
#include <xlv_lab.h>

#include "xfs_utils.h"


extern xlv_vh_entry_t *xlv_vh_list;

extern void		init_admin(void);
extern boolean_t	isTableInitialized(void);

extern xlv_oref_t	*find_object_in_table(char *name, int type);
typedef int (*XLV_OREF_FUNC) (xlv_oref_t *, char** msg); 

static void	dataObj(xlv_oref_t *oref, XLV_OREF_FUNC operation, char** msg);
static void	dataVol(xlv_oref_t *I_oref, XLV_OREF_FUNC operation,
				char** msg);
static int	dataSubvol(xlv_oref_t *I_oref, XLV_OREF_FUNC operation,
				int type, char** msg);
static int	dataPlex(xlv_oref_t *I_oref, XLV_OREF_FUNC operation,
				char** msg);
static int	dataVe (xlv_oref_t *oref, char** msg);

int
runXlvCommand(char *caller,
	      char *command,
	      char *argbuf,
	      char **msg,
	      boolean_t allOutput)
{
	FILE 		*pfp;
	FILE 		*fp;
	char		tname[L_tmpnam+1];
	char 		errbuf[BUFSIZ];
	char 		cmdbuf[512];
	char		*output = NULL;
	int		rval;

	*msg = NULL;

	/*
	 *	Write out data to file
	 */
	if(tmpnam(tname) == NULL) {
		sprintf(errbuf, gettxt(":6",
				"%s: cannot generate temporary file.\n"),
				command);
		add_to_buffer(msg, errbuf);
		return (1);
	}
	if((fp = fopen (tname, "w")) == NULL) {
		sprintf(errbuf, gettxt(":6",
				 "%s: failed to fopen() %s.\n%s.\n"),
				 command, tname, strerror(errno));
		add_to_buffer(msg, errbuf);
		return(1);
	}
	if (fprintf(fp, "%s", argbuf) <= 0 ) {
		sprintf(errbuf, gettxt(":6",
				 "%s: failed to write file.\n%s.\n"),
				 command, strerror(errno));
		add_to_buffer(msg, errbuf);
		fclose(fp);
#ifndef	DEBUG
		unlink(tname);
#endif
		return(1);
	}
	fclose(fp);

	/*
	 *	Execute the command
	 */
	sprintf(cmdbuf, "%s %s", command, tname);
#ifdef	DEBUG
	printf("DBG: cmdbuf '%s' \n", cmdbuf);
#endif
	if((pfp = xfs_popen(cmdbuf, "r", 0)) == NULL) {
		sprintf(errbuf, gettxt(":6",
			"%s: error executing popen '%s %s'.\n"),
			caller, command, argbuf);
		add_to_buffer(msg, errbuf);
#ifndef	DEBUG
		unlink(tname);
#endif
		return(1);
	}

	/*
	 *	Read output from command
	 */
	while(fgets(errbuf, BUFSIZ, pfp) != NULL) {
		add_to_buffer(&output, errbuf);
	}

	/*
	 *	Close and check for error
	 */
	if ((rval = xfs_pclose(pfp))) {
		allOutput = B_TRUE;
		sprintf(errbuf, gettxt(":6",
				"%s: error executing '%s %s':\n"),
				caller, command, argbuf);
		add_to_buffer(msg, errbuf);
	}

	if(allOutput && output != NULL) {
		add_to_buffer(msg, output);
		add_to_buffer(msg, "\n");
	}

#ifndef	DEBUG
	unlink(tname);
#endif
	free(output);

	return(rval);
}

/*
 * Prints out the object specified by oref.
 */
int
formatData (char *name, char** msg)
{
	xlv_oref_t	*oref;
	int		rval;
	char		str[512];
	int		lastve;
	int		i;
	long long	size;
	long long	total=0;

	if(! isTableInitialized()) {
		init_admin();
	}

	/*
	 *	find object
	 */
	if((oref = find_object_in_table(name, XLV_OBJ_TYPE_NONE)) == NULL) {
		sprintf(str, gettxt(":1",
			":: Requested object '%s' was not found.\n"), name);
		add_to_buffer(msg, str);
		return(1);
	}

	/*
	 * Print out the top-level name first and then all the 
	 * volume elements.
	 */
	*msg = NULL;

        if (XLV_OREF_VOL(oref) != NULL) {
		dataObj (oref, dataVe, msg);
        }
        else if (XLV_OREF_PLEX(oref) != NULL) {
		lastve = oref->plex->num_vol_elmnts;
		/* compute plex size */
		for (i=0; i<lastve; i++) {
			size = (oref->plex->vol_elmnts[i].end_block_no -
			     oref->plex->vol_elmnts[i].start_block_no) +1;
			total += size;
		}
		sprintf(str,"{%s plex {plex %llu 1 {", oref->plex->name, total);
		add_to_buffer(msg, str);
		dataObj (oref, dataVe, msg);

		sprintf(str, " } }\n");
		add_to_buffer(msg,str);
        }
        else if (XLV_OREF_VOL_ELMNT(oref) != NULL) {
		total= (oref->vol_elmnt->end_block_no -
			oref->vol_elmnt->start_block_no) +1;
		sprintf(str,"{ %s ve {ve %llu 1 { {0",
					oref->vol_elmnt->veu_name,total);
		add_to_buffer(msg, str);
		(void) dataVe (oref, msg);
		sprintf(str, " } } }\n");
		add_to_buffer(msg,str);
	}
        else
		ASSERT (0);     /* object must have a top-level name. */

#ifdef	DEBUG
	printf("TAC... begin synopsis \n");
	printf("%s",*msg);
	printf("TAC... end synopsis \n");
#endif
	return(0);
}


/*
 * An iterator function that goes through an object and applies a
 * caller-supplied function to each volume element.
 */
static void
dataObj(xlv_oref_t *oref, XLV_OREF_FUNC operation, char **msg)
{
        if (XLV_OREF_VOL(oref) != NULL) {
		dataVol (oref, operation, msg);
        }
        else if (XLV_OREF_PLEX(oref) != NULL) {
		(void) dataPlex (oref, operation, msg); 
        }
        else if (XLV_OREF_VOL_ELMNT(oref) != NULL) {
		(void) (*operation)(oref, msg);
        }
        else {
		ASSERT (0);		/* invalid object type */
	}
}


static void
dataVol(xlv_oref_t *I_oref, XLV_OREF_FUNC operation, char **msg)
{
	xlv_oref_t  	oref_store;
	xlv_oref_t  	*oref;
	char		str[512];

	oref_store = *I_oref;
	oref = &oref_store;

	sprintf(str, "{%s vol",oref->vol->name);
	add_to_buffer(msg,str);

	if (XLV_OREF_VOL(oref)->log_subvol != NULL) {
		XLV_OREF_SUBVOL(oref) = XLV_OREF_VOL(oref)->log_subvol;
		if ( dataSubvol(oref, operation, XLV_SUBVOL_TYPE_LOG, msg) )
			return;
	}
	if (XLV_OREF_VOL(oref)->data_subvol != NULL) {
                XLV_OREF_SUBVOL(oref) = XLV_OREF_VOL(oref)->data_subvol;
		if ( dataSubvol(oref, operation, XLV_SUBVOL_TYPE_DATA, msg) )
			return;
	}
	if (XLV_OREF_VOL(oref)->rt_subvol != NULL) {
		XLV_OREF_SUBVOL(oref) = XLV_OREF_VOL(oref)->rt_subvol;
		if ( dataSubvol(oref, operation, XLV_SUBVOL_TYPE_RT, msg) )
			return;
	}

	sprintf(str, " }\n");
	add_to_buffer(msg,str);
}


static int
dataSubvol(xlv_oref_t *I_oref, XLV_OREF_FUNC operation, int type, char **msg)
{
	xlv_oref_t		oref;
	xlv_tab_subvol_t	*subvol;
	int			p;
	int			end_scan = 0;
	char			str[512];
	char			val[64];
	int			lastve;
	int			i;
	long long		size;
	long long		total;

	oref = *I_oref;

	subvol = XLV_OREF_SUBVOL(&oref);
	if (subvol == NULL)
		return 0;

	if (type == XLV_SUBVOL_TYPE_RT){
			sprintf(str," {rt ");
	}
	else if (type == XLV_SUBVOL_TYPE_LOG){
			sprintf(str, " {log ");
	}
	else if (type == XLV_SUBVOL_TYPE_DATA){
			sprintf(str, " {data ");
	}

	for (p = 0, total = 0; p < subvol->num_plexes && !end_scan; p++) {
		XLV_OREF_PLEX(&oref) = subvol->plex[p];
		lastve = subvol->plex[p]->num_vol_elmnts;
		for (i=0; i<lastve; i++) {
			size = (subvol->plex[p]->vol_elmnts[i].end_block_no -
			     subvol->plex[p]->vol_elmnts[i].start_block_no) +1;
			total += size;
		}
	}

       	sprintf (val, "%llu %d", total, subvol->num_plexes);
	strcat(str, val);
	add_to_buffer(msg,str);

	for (p = 0; p < subvol->num_plexes && !end_scan; p++) {
		XLV_OREF_PLEX(&oref) = subvol->plex[p];
		sprintf(str, " {");
		add_to_buffer(msg,str);
		end_scan = dataPlex (&oref, operation, msg);
	}

	sprintf(str, " }");
	add_to_buffer(msg,str);
	return (end_scan);
}


static int
dataPlex(xlv_oref_t *I_oref, XLV_OREF_FUNC operation, char **msg)
{
        xlv_oref_t      oref;
	xlv_tab_plex_t	*plex;
	int		ve;
	int		end_scan;
	char		str[512];

        oref = *I_oref;
	plex = XLV_OREF_PLEX(&oref);
	if (plex == NULL)
		return (0);		/* nothing there */

	for (ve=0; ve < plex->num_vol_elmnts; ve++) {
		sprintf(str," {%d",ve);
		add_to_buffer(msg,str);
		XLV_OREF_VOL_ELMNT (&oref) = &(plex->vol_elmnts[ve]);
		end_scan = (*operation) (&oref, msg);
		if (end_scan)
			break;
	}
	sprintf(str, " }");
	add_to_buffer(msg,str);
	
	return (end_scan);		/* 0 if complete scan */
}


/*
 * Function to print out a volume element.
 * 
 * Sample output:
 *	vol_wei.log.1.2	0000078c-0000-0000-0000-000000000000
 *	    start=0, end=1375, (stripe)grp_size=1, stripe_unit_size=0
 * 	    /dev/dsk/dks1d5s5 (xxx blks)  00000796-0000-0000-0000-000000000000
 */
static int
dataVe(xlv_oref_t *oref, char **msg)
{
	xlv_tab_vol_elmnt_t	*ve;
	char			str[512];

	ve = XLV_OREF_VOL_ELMNT (oref);
        if (ve == NULL) {
                return (0);
        }

        sprintf (str, " %llu %llu}", ve->start_block_no, ve->end_block_no);
	add_to_buffer(msg,str);

	return (0);	/* continue */
}


/*
 *	TAC generate in use list for GUI
 */
int
xlvPartsUse(XLV_OREF_FUNC operation, char** msg)
{
	xlv_vh_entry_t		*vh;
	xlv_vh_disk_label_t	*label;
	xlv_vh_disk_label_entry_t *lep;         /* label entry pointer */
	int			i;
	uint 			st;
	char			str[512];

	if(! isTableInitialized()) {
		init_admin();
	}

	for (vh = xlv_vh_list; vh != NULL; vh = vh->next) {
		label = XLV__LAB1_PRIMARY (vh);

		for (i=0; i < XLV_MAX_DISK_PARTS; i++) {
			lep = &label->label_entry[i];

			if (vh->vh.vh_pt[i].pt_nblks == 0) {
				continue;
			}

			if (lep->disk_part.size != vh->vh.vh_pt[i].pt_nblks) {
				continue;
			}

			/*
		         * See if we have a valid disk part.
		         */
		        if (uuid_is_nil (&(lep->disk_part.uuid),&st)) {
				continue;
        		} else {
				sprintf(str, "{%s XLV} ",
				pathtoalias(devtopath(lep->disk_part.dev, 0)));
				add_to_buffer(msg,str);
			}
		}
	}

	return (0);
}
