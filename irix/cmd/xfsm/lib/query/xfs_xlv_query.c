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

#ident "xfs_xlv_query.c: $Revision: 1.1 $"

/*
 *	Queries on xlv objects.
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <bstring.h>
#include <wctype.h>
#include <wsregexp.h>
#include <widec.h>

#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_oref.h>
#include <xlv_utils.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <sys/xlv_attr.h>
#include <xlv_lab.h>
#include <xlv_error.h>
#include <sys/debug.h>

#include "xfs_fs_defs.h"
#include "xfs_query.h"
#include "xfs_xlv_query.h"

/*
 *	Adds the XLV object reference to the set
 */

void
xlv_add_to_set(xlv_oref_t *oref, query_set_t **set)
{
	xlv_query_set_t *element;
	xlv_tab_subvol_t *subvolume;
	query_set_t *query_set_elm;

	element = (xlv_query_set_t*) malloc(sizeof(xlv_query_set_t));
	element->oref = oref;
	element->creator = 0;

	/* Set the name of the matching element */
	switch(oref->obj_type)
	{
		case XLV_OBJ_TYPE_VOL:
			sprintf(element->name,XLV_OREF_VOL(oref)->name);
			break;

		case XLV_OBJ_TYPE_SUBVOL:
			subvolume = XLV_OREF_SUBVOL(oref);

			if (subvolume->subvol_type == XLV_SUBVOL_TYPE_LOG)
			{
				sprintf(element->name,"log");
			}
			else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_DATA)
			{
				sprintf(element->name,"data");
			}
			else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_RT)
			{
				sprintf(element->name,"rt");
			}
			break;

		case XLV_OBJ_TYPE_PLEX:
			sprintf(element->name,"%s",XLV_OREF_PLEX(oref)->name);
			break;

		case XLV_OBJ_TYPE_VOL_ELMNT:
			sprintf(element->name,"%s",
				XLV_OREF_VOL_ELMNT(oref)->veu_name);
			break;
	}
			
	/*
	 * Add the element at the begining of the set
	 */
	query_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
	ASSERT(query_set_elm!=NULL); 
	query_set_elm->type = XLV_TYPE_QUERY;
	query_set_elm->value.xlv_obj_ref = element;
	query_set_elm->next = *set;
	*set = query_set_elm;
}	


/*
 *	Adds the XLV subvolume object reference to the set
 */

void
xlv_add_subvolume_to_set(
xlv_tab_vol_entry_t *volume, 
xlv_tab_subvol_t *subvolume, 
query_set_t **set)
{
	xlv_query_set_t *element;
	xlv_oref_t *oref;
	query_set_t *query_set_elm;

	oref = (xlv_oref_t*) malloc(sizeof(xlv_oref_t));
	oref->obj_type = XLV_OBJ_TYPE_SUBVOL;
	oref->subvol = subvolume;

	element = (xlv_query_set_t*) malloc(sizeof(xlv_query_set_t));
	element->oref = oref;
	element->creator = 1;

	/* Set the name of the subvolume */
	memset(element->name,'\0',XLV_QUERY_MATCH_OBJ_NM_LEN);

	if (volume != NULL)
	{
		strcat(element->name,volume->name);
		strcat(element->name,".");
	}
		
	if (subvolume->subvol_type == XLV_SUBVOL_TYPE_LOG)
	{
		sprintf(element->name,"%slog",element->name);
	}
	else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_DATA)
	{
		sprintf(element->name,"%sdata",element->name);
	}
	else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_RT)
	{
		sprintf(element->name,"%srt",element->name);
	}

	/*
	 * Add the element at the begining of the set
	 */
	query_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
	ASSERT(query_set_elm!=NULL); 
	query_set_elm->type = XLV_TYPE_QUERY;
	query_set_elm->value.xlv_obj_ref = element;
	query_set_elm->next = *set;
	*set = query_set_elm;

}	


/*
 *	Adds the XLV plex object reference to the set
 */

void
xlv_add_plex_to_set(
xlv_tab_vol_entry_t *volume, 
xlv_tab_subvol_t *subvolume, 
unsigned short plex_no,
xlv_tab_plex_t *plex, 
query_set_t **set)
{
	xlv_query_set_t *element;
	xlv_oref_t *oref;
	query_set_t *query_set_elm;

	oref = (xlv_oref_t*) malloc(sizeof(xlv_oref_t));
	oref->obj_type = XLV_OBJ_TYPE_PLEX;
	oref->plex = plex;

	element = (xlv_query_set_t*) malloc(sizeof(xlv_query_set_t));
	element->oref = oref;
	element->creator = 1;

	/* Set the name of the plex */
	memset(element->name,'\0',XLV_QUERY_MATCH_OBJ_NM_LEN);

	if (volume != NULL)
	{
		strcat(element->name,volume->name);
		strcat(element->name,".");
	}

	if (subvolume != NULL)
	{
		if (subvolume->subvol_type == XLV_SUBVOL_TYPE_LOG)
		{
			sprintf(element->name,"%slog.%u",element->name,plex_no);
		}
		else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_DATA)
		{
			sprintf(element->name,"%sdata.%u",element->name,plex_no);
		}
		else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_RT)
		{
			sprintf(element->name,"%srt.%u",element->name,plex_no);
		}
	}
	else
	{
		sprintf(element->name,"%s",plex->name);
	}

	/*
	 * Add the element at the begining of the set
	 */
	query_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
	ASSERT(query_set_elm!=NULL); 
	query_set_elm->type = XLV_TYPE_QUERY;
	query_set_elm->value.xlv_obj_ref = element;
	query_set_elm->next = *set;
	*set = query_set_elm;
}	
		


/*
 *	Adds the XLV ve object reference to the set
 *	given the plex it belongs to.
 */

void 
xlv_add_ve_to_set2(xlv_tab_plex_t *plex, 
long ve_no, xlv_tab_vol_elmnt_t *ve,
query_set_t **set)
{
	xlv_query_set_t *element;
	xlv_oref_t *oref;
	query_set_t *query_set_elm;

	oref = (xlv_oref_t*) malloc(sizeof(xlv_oref_t));
	oref->obj_type = XLV_OBJ_TYPE_VOL_ELMNT;
	oref->vol_elmnt = ve;

	element = (xlv_query_set_t*) malloc(sizeof(xlv_query_set_t));
	element->oref = oref;
	element->creator = 1;

	/* Set the name of ve */
	memset(element->name,'\0',XLV_QUERY_MATCH_OBJ_NM_LEN);

	sprintf(element->name,"%s.%ld",plex->name, ve_no);

	/*
	 * Add the element at the begining of the set
	 */
	query_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
	ASSERT(query_set_elm!=NULL); 
	query_set_elm->type = XLV_TYPE_QUERY;
	query_set_elm->value.xlv_obj_ref = element;
	query_set_elm->next = *set;
	*set = query_set_elm;
}	
		

/*
 *	Adds the XLV ve object reference to the set
 */

void
xlv_add_ve_to_set(xlv_tab_vol_entry_t *volume, xlv_tab_subvol_t *subvolume,
unsigned short plex_no, long ve_no, xlv_tab_vol_elmnt_t *ve,
query_set_t **set)
{
	xlv_query_set_t *element;
	xlv_oref_t *oref;
	query_set_t *query_set_elm;

	oref = (xlv_oref_t*) malloc(sizeof(xlv_oref_t));
	oref->obj_type = XLV_OBJ_TYPE_VOL_ELMNT;
	oref->vol_elmnt = ve;

	element = (xlv_query_set_t*) malloc(sizeof(xlv_query_set_t));
	element->oref = oref;
	element->creator = 1;

	/* Set the name of ve */
	memset(element->name,'\0',XLV_QUERY_MATCH_OBJ_NM_LEN);

	if (volume != NULL)
	{
		strcat(element->name,volume->name);
		strcat(element->name,".");
	}

	if (subvolume != NULL) 
	{
		if (subvolume->subvol_type == XLV_SUBVOL_TYPE_LOG)
		{
			sprintf(element->name,"%slog.%u.%ld",element->name,plex_no,ve_no);
		}
		else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_DATA)
		{
			sprintf(element->name,"%sdata.%u.%ld",element->name,plex_no,ve_no);
		}
		else if (subvolume->subvol_type == XLV_SUBVOL_TYPE_RT)
		{
			sprintf(element->name,"%srt.%u.%ld",element->name,plex_no,ve_no);
		}
	}
	else
	{
		sprintf(element->name,"%s",ve->veu_name);
	}

	/*
	 * Add the element at the begining of the set
	 */
	query_set_elm = (query_set_t*) malloc(sizeof(query_set_t));
	ASSERT(query_set_elm!=NULL); 
	query_set_elm->type = XLV_TYPE_QUERY;
	query_set_elm->value.xlv_obj_ref = element;
	query_set_elm->next = *set;
	*set = query_set_elm;
}	

/*
 *	For the given XLV object references in in_set, resolve the 
 *	query clause based on the type of the object. 
 *
 *	Return Value: 	0 if Query is successful
 *			-1 on error in resolving query
 */

int
xlv_query(query_clause_t* query_clause,query_set_t *in_set,
query_set_t **out_set)
{

	/* 
	 *	Delegate the query to the appropriate type of object
	 */

	/* Is it a volume ? */
	if ((query_clause->attribute_id>=
			XLV_QRY_VOLUME_ATTR_RANGE_BEGIN) &&
	   (query_clause->attribute_id<=XLV_QRY_VOLUME_ATTR_RANGE_END))
	{
		return(xlv_query_volume(query_clause->attribute_id, 
					query_clause->value, 
					query_clause->operator, 
					in_set, out_set));	
	}
	else if ((query_clause->attribute_id>=
			XLV_QRY_SUBVOL_ATTR_RANGE_BEGIN) &&
	   (query_clause->attribute_id<=XLV_QRY_SUBVOL_ATTR_RANGE_END))
	{
		return(xlv_query_subvolume(query_clause->attribute_id,
					query_clause->value, 
					query_clause->operator, 
					in_set,out_set));	
	}
	else if ((query_clause->attribute_id>=
			XLV_QRY_PLEX_ATTR_RANGE_BEGIN) &&
	   (query_clause->attribute_id<=XLV_QRY_PLEX_ATTR_RANGE_END))
	{
		return(xlv_query_plex(query_clause->attribute_id, 
					query_clause->value, 
					query_clause->operator, 
					in_set, out_set));	
	}
	else if ((query_clause->attribute_id>=
			XLV_QRY_VE_ATTR_RANGE_BEGIN) &&
	   (query_clause->attribute_id<=XLV_QRY_VE_ATTR_RANGE_END))
	{
		return(xlv_query_ve(query_clause->attribute_id, 
					query_clause->value, 
					query_clause->operator, 
					in_set, out_set));	
	}
	else
	{
		fprintf(stderr,"Unknown object type %d",
				query_clause->query_sub_type);
		return(-1);
	}
}
			
			
/*
 *	Query all the volumes in the in_set for the given attribute, value
 *	pair. The out_set contains the references to the volumes which
 *	satisfied the condition.
 *	Return Value: 	0 if Query is successful
 *			-1 on error in resolving query
 */

int 
xlv_query_volume(short attr_id, char *value,
	short oper, query_set_t *in_set, query_set_t **out_set)
{
	xlv_tab_vol_entry_t *volume;
	query_set_t *set_element;
	xlv_oref_t *oref;

	/*
	 * Traverse the in_set and evaluate the query condition for 
	 * each volume encountered.
	 */
	set_element = in_set;
	while (set_element != NULL)
	{
		/*
		 * Check if it is a volume
		 */
		if (set_element->type == XLV_TYPE_QUERY)
		{
			oref = set_element->value.xlv_obj_ref->oref;
			ASSERT (oref != NULL);
			if (oref->obj_type == XLV_OBJ_TYPE_VOL)
			{
				/*
				 * Evaluate the query condition
				 */
				if (xlv_query_volume_eval(
				XLV_OREF_VOL(oref),attr_id,value,oper) == 0)
				{
					/*
					 * Add this match to the out_set
					 */
					xlv_add_to_set(oref, out_set);
				}
			}
		}

		set_element = set_element->next;
	}

	return(0);
}

/*
 *	Evaluate the query condition for the given volume. 
 *	Return Value	:	0 if condition is TRUE
 *				1 if condition is FALSE
 */

short
xlv_query_volume_eval(
	xlv_tab_vol_entry_t *volume, short attr_id, char *value, short oper)
{
	short result = 1;
	long 	value_long;
	int	value_int;

	switch(attr_id) {
		case XLV_VOLUME_NAME:
			if ((oper == QUERY_STRCMP) &&
			(xfs_query_parse_regexp((char*)value,volume->name)==0))
			{
				result = 0;
			}
			break;

		case XLV_VOLUME_FLAGS:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) && 
				(volume->flags == value_long))
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) && 
				(volume->flags != value_long) )
			{
				result = 0;
			}
			break;

		case XLV_VOLUME_STATE:
			sscanf(value,"%d",&value_int); 
			if ( (oper == QUERY_EQUAL) && 
				(volume->state == value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) && 
				(volume->state != value_int) )
			{
				result = 0;
			}
			break;

		default:
			fprintf(stderr,"Unknown Volume attribute %hd",attr_id);
			result = 1;
			break;
	}

	return(result);
}
					
			
/*
 *	Query all the subvolumes in the in_set for the given attribute, value
 *	pair. The out_set contains the references to the subvolumes which
 *	satisfied the condition.
 *	Return Value: 	0 if Query is successful
 *			-1 on error in resolving query
 */

int 
xlv_query_subvolume(short attr_id, char *value,
	short oper, query_set_t *in_set, query_set_t **out_set)
{
	xlv_tab_vol_entry_t *volume;
	xlv_tab_subvol_t *subvolume;
	query_set_t *set_element;
	xlv_oref_t *oref;

	/*
	 * Traverse the in_set and evaluate the query condition for 
	 * each subvolume encountered.
	 */
	set_element = in_set;
	while (set_element != NULL)
	{
		/*
		 * Check if it is a volume
		 */
		if (set_element->type == XLV_TYPE_QUERY)
		{
			oref = set_element->value.xlv_obj_ref->oref;
			ASSERT (oref != NULL);
			if (oref->obj_type == XLV_OBJ_TYPE_VOL)
			{
				/*
				 * Evaluate the query condition for all the 
				 * subvolumes in the volume.
				 */
			
				subvolume = XLV_OREF_VOL(oref)->log_subvol;
				if ( (subvolume != NULL) && 
				(xlv_query_subvolume_eval(subvolume,attr_id,value,oper)==0))
				{
					/*
					 * Add this subvolume to the out_set
					 */
					xlv_add_subvolume_to_set(
						XLV_OREF_VOL(oref),
						subvolume, out_set);
				}

				subvolume = XLV_OREF_VOL(oref)->data_subvol;
				if ( (subvolume != NULL) && 
				(xlv_query_subvolume_eval(subvolume,attr_id,value,oper)==0))
				{
					/*
					 * Add this subvolume to the out_set
					 */
					xlv_add_subvolume_to_set(
						XLV_OREF_VOL(oref),
						subvolume, out_set);
				}

				subvolume = XLV_OREF_VOL(oref)->rt_subvol;
				if ((subvolume != NULL) && 
				(xlv_query_subvolume_eval(subvolume,attr_id,value,oper)==0))
				{
					/*
					 * Add this subvolume to the out_set
					 */
					xlv_add_subvolume_to_set(
						XLV_OREF_VOL(oref), 
						subvolume, out_set);
				}
			}
			else if (oref->obj_type == XLV_OBJ_TYPE_SUBVOL)
			{
				if (xlv_query_subvolume_eval(
					XLV_OREF_SUBVOL(oref),
					attr_id,value,oper)==0)
				{
					xlv_add_to_set(oref, out_set);	
				}
			}
		}

		set_element = set_element->next;
	}

	return(0);
}

/*
 *	Query all the plexes in the in_set for the given attribute, value
 *	pair. The out_set contains the references to the plexes which
 *	satisfied the condition.
 *	Return Value: 	0 if Query is successful
 *			-1 on error in resolving query
 */

int 
xlv_query_plex(short attr_id, char *value,
	short oper, query_set_t *in_set, query_set_t **out_set)
{
	xlv_tab_vol_entry_t *volume;
	xlv_tab_subvol_t *subvolume;
	xlv_tab_plex_t *plex;
	query_set_t *set_element;
	xlv_oref_t *oref;
	unsigned short count;

	/*
	 * Traverse the in_set and evaluate the query condition for 
	 * each plex encountered.
	 */
	set_element = in_set;
	while (set_element != NULL)
	{
		if (set_element->type == XLV_TYPE_QUERY)
		{
			oref = set_element->value.xlv_obj_ref->oref;
			ASSERT (oref != NULL);
			if (oref->obj_type == XLV_OBJ_TYPE_VOL)
			{
				/*
				 * Evaluate the query condition for all the 
				 * plexes in the volume.
				 */
			
				subvolume = XLV_OREF_VOL(oref)->log_subvol;
				if (subvolume != NULL) 
				{
					/*
					 * Query the plexes in the log subvol
					 */ 
					for (count = 0;
						count<subvolume->num_plexes;
						count++)
					{
						plex = subvolume->plex[count];

						if ((plex != NULL) &&
						(xlv_query_plex_eval(plex,
						attr_id,value, oper)==0))
						{
							/*
							 * Add plex to out_set
							 */
							xlv_add_plex_to_set(
							XLV_OREF_VOL(oref),
							subvolume, count, plex,
							out_set);
						}
					}
				}

				subvolume = XLV_OREF_VOL(oref)->data_subvol;
				if (subvolume != NULL)  
				{
					/*
					 * Query the plexes in data subvolume
					 */ 
					for (count = 0;
						count<subvolume->num_plexes;
						count++)
					{
						plex = subvolume->plex[count];

						if ((plex != NULL) &&
						(xlv_query_plex_eval(plex,
						attr_id,value,oper)==0))
						{
							/*
							 * Add plex to out_set
							 */
							xlv_add_plex_to_set(
							XLV_OREF_VOL(oref),
							subvolume, count, 
							plex, out_set);
						}
					}
				}

				subvolume = XLV_OREF_VOL(oref)->rt_subvol;
				if (subvolume != NULL)  
				{
					/*
					 * Query the plexes in the rt subvolume
					 */ 
					for (count = 0;
						count<subvolume->num_plexes;
						count++)
					{
						plex = subvolume->plex[count];

						if ((plex != NULL) &&
						(xlv_query_plex_eval(plex,
							attr_id,value,
							oper)==0))
						{
							/*
							 * Add plex to out_set
							 */
							xlv_add_plex_to_set(
							XLV_OREF_VOL(oref),
							subvolume, count, 
							plex, out_set);
						}
					}
				}
			}
			else if (oref->obj_type == XLV_OBJ_TYPE_SUBVOL)
			{
				/*
				 * Query the plexes in the subvolume
				 */ 
				subvolume = XLV_OREF_SUBVOL(oref);
				for (count = 0;count<subvolume->num_plexes;
					count++)
				{
					plex = subvolume->plex[count];
	
					if ((plex != NULL) &&
					(xlv_query_plex_eval(plex,attr_id,
						value,oper)==0))
					{
						/*
						 * Add plex to the out_set
						 */
						xlv_add_plex_to_set(NULL,
							subvolume,count,plex,
							out_set);
					}
				}
			}
			else if (oref->obj_type == XLV_OBJ_TYPE_PLEX)
			{
				if (xlv_query_plex_eval(XLV_OREF_PLEX(oref),
						attr_id,value,oper)==0)
				{
					/*
					 * Add this plex to the out_set
					 */
					xlv_add_to_set(oref, out_set);
				}
			}

		}
		set_element = set_element->next;
	}

	return(0);
}


/*
 *	Query all the ve's in the in_set for the given attribute, value
 *	pair. The out_set contains the references to the ve's which
 *	satisfied the condition.
 *	Return Value: 	0 if Query is successful
 *			-1 on error in resolving query
 */

int 
xlv_query_ve(short attr_id, char *value,
	short oper, query_set_t *in_set, query_set_t **out_set)
{
	xlv_tab_vol_entry_t *volume;
	xlv_tab_subvol_t *subvolume;
	xlv_tab_plex_t *plex;
	xlv_tab_vol_elmnt_t *ve;
	query_set_t *set_element;
	xlv_oref_t *oref;
	unsigned short count;
	long ve_count;

	/*
	 * Traverse the in_set and evaluate the query condition for 
	 * each ve encountered.
	 */
	set_element = in_set;
	while (set_element != NULL)
	{
		if (set_element->type == XLV_TYPE_QUERY)
		{
			oref = set_element->value.xlv_obj_ref->oref;
			ASSERT (oref != NULL);
			if (oref->obj_type == XLV_OBJ_TYPE_VOL)
			{
				/*
				 * Evaluate the query condition for all the 
				 * ve's in the volume.
				 */
			
				subvolume = XLV_OREF_VOL(oref)->log_subvol;
				if (subvolume != NULL) 
				{
					for (count = 0;
						count<subvolume->num_plexes;
						count++)
					{
						plex = subvolume->plex[count];

						if (plex != NULL) 
						{
							for (ve_count=0;ve_count<plex->num_vol_elmnts;ve_count++)
							{
							  ve=&(plex->vol_elmnts[ve_count]);
							  if (xlv_query_ve_eval(ve,attr_id,value, oper)==0)
						  	  {
 								/*
								 * Add ve to out_set
								 */
								xlv_add_ve_to_set(XLV_OREF_VOL(oref), subvolume, count, ve_count,ve, out_set);
						  	  }
							}
						}
					}
				}

				subvolume = XLV_OREF_VOL(oref)->data_subvol;
				if (subvolume != NULL)  
				{
					for (count=0;
						count<subvolume->num_plexes;
						count++)
					{
						plex = subvolume->plex[count];

						if (plex != NULL) 
						{
							for (ve_count=0;
					       	  ve_count<plex->num_vol_elmnts;
							  ve_count++)
							{
							  ve=&(plex->vol_elmnts[ve_count]);
							  if (xlv_query_ve_eval(ve,attr_id,value, oper)==0)
						  	  {
								/*
								 * Add ve to out_set
								 */
								xlv_add_ve_to_set(XLV_OREF_VOL(oref), subvolume, count, ve_count,ve, out_set);
						  	  }
							}
						}
					}
				}

				subvolume = XLV_OREF_VOL(oref)->rt_subvol;
				if (subvolume != NULL)  
				{
					for (count=0;count<subvolume->num_plexes;count++)
					{
						plex = subvolume->plex[count];

						if (plex != NULL) 
						{
							for (ve_count=0;
					       	  ve_count<plex->num_vol_elmnts;
							  ve_count++)
							{
							  ve=&(plex->vol_elmnts[ve_count]);
							  if (xlv_query_ve_eval(ve, attr_id,value, oper)==0)
						   	  {
								/*
								 * Add ve to out_set
								 */
								xlv_add_ve_to_set(XLV_OREF_VOL(oref), subvolume, count, ve_count,ve, out_set);
						  	  }
							}
						}
					}
				}
			}
			else if (oref->obj_type == XLV_OBJ_TYPE_SUBVOL)
			{
				/*
				 * Query the plexes in the subvolume
				 */ 
				subvolume = XLV_OREF_SUBVOL(oref);
				for (count = 0;count<subvolume->num_plexes;
					count++)
				{
					plex = subvolume->plex[count];

					if (plex != NULL) 
					{
						for (ve_count=0;
				       	  ve_count<plex->num_vol_elmnts;
						  ve_count++)
						{
						  ve=&(plex->vol_elmnts[ve_count]);
						  if (xlv_query_ve_eval(ve,
						    attr_id,value, oper)==0)
						  {
							/*
							 * Add ve to out_set
							 */
							xlv_add_ve_to_set(NULL, subvolume, count, ve_count,ve, out_set);
					  	  }
						}
					}
				}
			}
			else if (oref->obj_type == XLV_OBJ_TYPE_PLEX)
			{
				plex = XLV_OREF_PLEX(oref);
				for (ve_count=0; ve_count<plex->num_vol_elmnts;
					 ve_count++)
				{
				  ve=&(plex->vol_elmnts[ve_count]);
				  if (xlv_query_ve_eval(ve,
					    attr_id,value, oper)==0)
				  {
					/*
					 * Add ve to out_set
					 */
					xlv_add_ve_to_set2(plex, ve_count,
							ve, out_set);
				  }
				}
			}
			else if (oref->obj_type == XLV_OBJ_TYPE_VOL_ELMNT)
			{
				if (xlv_query_ve_eval(XLV_OREF_VOL_ELMNT(oref),
					attr_id,value, oper)==0)
				{
					/*
					 * Add ve to out_set
					 */
					xlv_add_to_set(oref,out_set);
				}
			}	

		}
		set_element = set_element->next;
	}

	return(0);
}

/*
 *	Evaluate the query condition for the given subvolume. 
 *	Return Value	:	0 if condition is TRUE
 *				1 if condition is FALSE
 */

short
xlv_query_subvolume_eval(
	xlv_tab_subvol_t *subvolume, short attr_id, char *value, short oper)
{
	short result = 1;
	long value_long;
	short value_short;
	unsigned short value_ushort;

	switch(attr_id) {
		case XLV_SUBVOLUME_FLAGS:
			sscanf(value,"%ld",&value_long); 
			if ( (oper == QUERY_EQUAL) && 
				(subvolume->flags == value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) && 
				(subvolume->flags != value_long) )
			{
				result = 0;
			}
			break;

		case XLV_SUBVOLUME_TYPE:
			sscanf(value,"%hd",&value_short);
			if ( (oper == QUERY_EQUAL) && 
				(subvolume->subvol_type == value_short) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) && 
				(subvolume->subvol_type != value_short) )
			{
				result = 0;
			}
			break;

		case XLV_SUBVOLUME_SIZE:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) &&
				(subvolume->subvol_size == value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(subvolume->subvol_size != value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
				(subvolume->subvol_size > value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
				(subvolume->subvol_size >= value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
				(subvolume->subvol_size < value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
				(subvolume->subvol_size <= value_long) )
			{
				result = 0;
			}
			break;
				
		case XLV_SUBVOLUME_INL_RD_SLICE:
			sscanf(value,"%hu",&value_ushort); 
			if ( (oper == QUERY_EQUAL) &&
			(subvolume->initial_read_slice == value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
			(subvolume->initial_read_slice != value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
			(subvolume->initial_read_slice > value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
			(subvolume->initial_read_slice >= value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
			(subvolume->initial_read_slice < value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
			(subvolume->initial_read_slice <= value_ushort) )
			{
				result = 0;
			}
			break;

		case XLV_SUBVOLUME_NO_PLEX:
			sscanf(value,"%hu",&value_ushort); 
			if ( (oper == QUERY_EQUAL) &&
			(subvolume->num_plexes == value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
			(subvolume->num_plexes != value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
			(subvolume->num_plexes > value_ushort))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
			(subvolume->num_plexes >= value_ushort) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
			(subvolume->num_plexes < value_ushort) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
			(subvolume->num_plexes <= value_ushort) )
			{
				result = 0;
			}
			break;

		default:
			fprintf(stderr,"Unknown SubVolume attribute %hd",
					attr_id);
			result = 1;
			break;
	}

	return(result);
}

/*
 *	Evaluate the query condition for the given plex. 
 *	Return Value	:	0 if condition is TRUE
 *				1 if condition is FALSE
 */

short
xlv_query_plex_eval(
	xlv_tab_plex_t *plex, short attr_id, char *value, short oper)
{
	short result = 1;
	long value_long;

	switch(attr_id) {
		case XLV_PLEX_NAME:
			if ( (oper == QUERY_STRCMP) && 
			(xfs_query_parse_regexp((char*)value,plex->name)==0))
			{
				result = 0;
			}
			break;

		case XLV_PLEX_FLAGS:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) && 
				(plex->flags == value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) && 
				(plex->flags != value_long) )
			{
				result = 0;
			}
			break;

		case XLV_PLEX_NO_VE:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) &&
				(plex->num_vol_elmnts == value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(plex->num_vol_elmnts != value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
				(plex->num_vol_elmnts > value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
				(plex->num_vol_elmnts >= value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
				(plex->num_vol_elmnts < value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
				(plex->num_vol_elmnts <= value_long) )
			{
				result = 0;
			}
			break;
				
		case XLV_PLEX_MAX_VE:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) &&
				(plex->max_vol_elmnts == value_long))
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(plex->max_vol_elmnts != value_long))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
				(plex->max_vol_elmnts > value_long))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
				(plex->max_vol_elmnts >= value_long))
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
				(plex->max_vol_elmnts < value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
				(plex->max_vol_elmnts <= value_long) )
			{
				result = 0;
			}
			break;

		default:
			fprintf(stderr,"Unknown Plex attribute %hd",attr_id);
			result = 1;
			break;
	}

	return(result);
}

/*
 *	Evaluate the query condition for the given ve. 
 *	Return Value	:	0 if condition is TRUE
 *				1 if condition is FALSE
 */

short
xlv_query_ve_eval(
	xlv_tab_vol_elmnt_t *ve, short attr_id, char *value, short oper)
{
	short result = 1;
	short value_short;
	int value_int;
	long value_long;

	switch(attr_id) {
		case XLV_VE_NAME:
			if ( (oper == QUERY_STRCMP) && 
			(xfs_query_parse_regexp((char*)value,ve->veu_name)==0))
			{
				result = 0;
			}
			break;

		case XLV_VE_TYPE:
			sscanf(value,"%d",&value_int);
			if ( (oper == QUERY_EQUAL) && 
				(ve->type == value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) && 
				(ve->type != value_int) )
			{
				result = 0;
			}
			break;

		case XLV_VE_STATE:
			sscanf(value,"%d",&value_int);
			if ( (oper == QUERY_EQUAL) &&
				(ve->state == value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(ve->state != value_int) )
			{
				result = 0;
			}
			break;

		case XLV_VE_GRP_SIZE:
			sscanf(value,"%d",&value_int);
			if ( (oper == QUERY_EQUAL) &&
				(ve->grp_size == value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(ve->grp_size != value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
				(ve->grp_size > value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
				(ve->grp_size >= value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
				(ve->grp_size < value_int) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
				(ve->grp_size <= value_int) )
			{
				result = 0;
			}
			break;
				
		case XLV_VE_STRIPE_UNIT_SIZE:
			sscanf(value,"%hd",&value_short);
			if ( (oper == QUERY_EQUAL) &&
				(ve->stripe_unit_size == value_short))
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(ve->stripe_unit_size != value_short))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GT) &&
				(ve->stripe_unit_size > value_short))
			{
				result = 0;
			}
			else if ( (oper == QUERY_GE) &&
				(ve->stripe_unit_size >= value_short))
			{
				result = 0;
			}
			else if ( (oper == QUERY_LT) &&
				(ve->stripe_unit_size < value_short) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_LE) &&
				(ve->stripe_unit_size <= value_short) )
			{
				result = 0;
			}
			break;

		case XLV_VE_START_BLK_NO:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) &&
				(ve->start_block_no == value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(ve->start_block_no != value_long) )
			{
				result = 0;
			}
			break;

		case XLV_VE_END_BLK_NO:
			sscanf(value,"%ld",&value_long);
			if ( (oper == QUERY_EQUAL) &&
				(ve->end_block_no == value_long) )
			{
				result = 0;
			}
			else if ( (oper == QUERY_NE) &&
				(ve->end_block_no != value_long) )
			{
				result = 0;
			}
			break;

		default:
			fprintf(stderr,"Unknown VolumeElement attribute %hd",
					attr_id);
			result = 1;
			break;
	}

	return(result);
}
		
