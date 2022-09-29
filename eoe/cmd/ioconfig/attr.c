/*
 * Copyright 1988-1996, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#include <sys/invent.h>
#include <sys/attributes.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include "error.h"
#include "ioconfig.h"

typedef struct ioconfig_entry_s {
	int	sc_ctlr_num;
	char	sc_canonical_name[MAXDEVNAME];
} ioconfig_entry_t;

/*
 * given a canonical path name of a scsi controller 
 * try to read the logical controller number corresponding
 * to it
 */

int
ioc_ctlr_num_get(inventory_t *invp)
{
	extern int debug;
	int ctlr;

	/* Network devices return 'inv_unit' instead of 'inv_controller' */
	ctlr = (invp->inv_class == INV_NETWORK)? invp->inv_unit: invp->inv_controller;

	if (debug)
		printf("ioc_ctlr_num_get: kernel has device numbered %d\n", ctlr);

	return (ctlr);

}
		      
/*
 * update the logical controller number corresponding
 * to scsi controller with the given canonical path name
 * in the hardware graph 
 */
int
ioc_ctlr_num_put(int ctlr_num ,const char *canonical_name)
{
	if (attr_set((char *)canonical_name, INFO_LBL_CONTROLLER_NAME,
		     (char *)&ctlr_num, sizeof(ctlr_num) , 0) == -1)
		ioc_error(ATTR_SET_ERR,"ioc_ctlr_num_put",
		      canonical_name,INFO_LBL_CONTROLLER_NAME);
	return 1;
}

/*
 * check if a logical controller number has already been 
 * assigned to the controller corresponding to the 
 * canonical name. If so it returns that number other
 * it returns -1. ctlr_num gets the highest controller
 * number+1 assigned so far if the function returns -1.
 */
int 
ioconfig_find(const char *canonical_name,int *ctlr_num,char *pattern)
{
	ioconfig_entry_t	cursor;
	int 			rc;
	extern	FILE		*config_fp;
	char			key_pattern[MAX_NAME_LEN];
	extern char		*basename(char *);
	int			pattern_len;
	char			*str,*str1,name[MAXDEVNAME];
	char			line[MAX_LINE_LEN];
	int			line_num = 0,i;
	fseek(config_fp,0,0);

	if (strcmp(pattern,IGNORE_STR) == 0)
		sprintf(key_pattern,"%s",basename((char *)canonical_name));
	else
		sprintf(key_pattern,"%s",pattern);

	pattern_len = strlen(key_pattern);
	if (debug)
		printf("ioconfig_find: KEY PATTERN = %s\n",key_pattern);
	while (fgets(line,MAX_LINE_LEN,config_fp)) {
		line_num++;
		if (debug)
			printf("Line%d : %s\n",line_num,line);
		/* Skip comment lines */
		if (line[0] == POUND_CHAR)
			continue;
		/* Check for a blank line 
		 * This is done for the purpose of
		 * illegal file format detection.
		 */
		i = 0;
		while (isspace(line[i]) || line[i] == '\n')
			i++;
		if (strlen(line) == i)
			continue;

		rc = sscanf(line,"%d %s",&cursor.sc_ctlr_num,
			    cursor.sc_canonical_name);
		if (rc != 2) {
			printf("Line%d in %s: %s: Invalid format\n",
			       line_num,IOCONFIG_CTLR_NUM_FILE,line);
			continue;
		}

		/* check if the current mapping entry's canonical name 
		 * matches with the one we are looking for
		 */
		if (strcmp(canonical_name,cursor.sc_canonical_name) == 0) {
			return cursor.sc_ctlr_num;
		}

		/* Now try to assign the next available controller number
		 * in this particular device's namespace.
		 * The following check looks for the string "pattern"
		 * anywhere in the path name and also makes sure that
		 * it exactly corresponds to one component of the pathname.
		 * Basically it checks if the name is the form
		 *		.../<pattern>/...
		 */
		/* End the name with a "/" so that we know for
		 * sure that pattern must always be preceded and succeeded
		 * by a "/"
		 */
		sprintf(name,"%s/",cursor.sc_canonical_name);
		str = strstr(name, key_pattern);
		
		/* Stop the search for the key_pattern being
		 * a component in the pathname when we hit the
		 * end.
		 */
		while (str) {
			if (debug)
				printf("ioconfig_find: str = %s\n",str);
			/* Make sure that the key pattern found in
			 * the canonical name of the device
			 * is a component.
			 */
			if (*(str-1) == '/' && *(str + pattern_len) == '/') {
				if (cursor.sc_ctlr_num >= *ctlr_num)
					*ctlr_num = cursor.sc_ctlr_num + 1;
				break;
			}
			/* 
			 * If the key pattern found in the pathname is not
			 * exactly a component then continue with
			 * the search further.
			 */
			str1 = str + pattern_len;
			str = strstr(str1,key_pattern);
		}
	}

	return -1;
}

/* add a new entry to the scsi config file  at the end*/
	
void
ioconfig_add(int ctlr_num,const char *canonical_name)
{
	extern FILE		*config_fp;
	fprintf(config_fp,"%d %s\n",ctlr_num,canonical_name);
	fflush(config_fp);

}
	
