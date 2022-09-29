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

#ident  "tranform.c: $Revision: 1.2 $"

/*
 * tranform.c -- Transforms a list of ranges so that they are of atleast
 * the minimum specified size. 
 */

#include <stdio.h>
#include <stdlib.h>
#include "xfsUtil.h"

#define	MAX_LIST_SZ	256

#define	ORIGINAL_TUPLE	0
#define	GAP_TUPLE	1

static int	transform(tuple_t [], int, int);
static void	merge_list(tuple_t [], int, int, int);
static int	find_max_range(tuple_t [],int, int);

#ifdef	DEBUG
void	print_list(tuple_t [], int);
#endif


tr_normalize(tuple_t list[], int list_size, int normal, double maximum)
{
	int	iter;

#ifdef DEBUG
	printf("tr_normalize: list_size=%d, normal=%d, maximum=%f\n",
			list_size, normal, maximum);
#endif

	if((list_size <= 0) || (normal <= 0)) {
		return(-1);
	}

	for(iter=0; iter < list_size; ++iter) {
		list[iter].lvalue = (list[iter].lvalue * normal) / maximum;
		list[iter].rvalue = (list[iter].rvalue * normal) / maximum;
	}

	return(0);
}


/*
 *	modify_ranges()
 *	The list of tuples is suitably rearranged to ensure that each of them,
 *	is atleast the minimum size. The gaps in the list are treated as 
 *	regular tuples for transformation by creating a new list. The original 
 *	list is suitably modified after the transformation.
 *	On success, 0 is returned.
 *	On failure, -1 is returned.
 */

int
modify_ranges(tuple_t list[],int list_size, int max_range, double min_pct)
{
	int	total_gap_reqd = 0;
	int	returnValue = -1;
	tuple_t	*new_list = NULL;
	int	iter=0;
	int	index=0;
	int	new_list_iter=0;
	int	min_range_size = (min_pct * max_range) / 100;

	if((list_size <= 0) || (min_range_size <= 0)) {
		return(-1);
	}

	/* Allocate memory to hold a copy of the list and the gaps */
	/* The max size of the new list is (2n+1) for n element size list */
	if((new_list = calloc(((2*list_size)+1),sizeof(tuple_t))) == NULL) {
		return(-1);
	}

	/* Create a new list with gaps treated as regular tuples */
	/* Check for gap at the beginning */
	if ((list_size > 0) && (list[0].lvalue > 0))
	{
		new_list[new_list_iter].lvalue = 0;	
		new_list[new_list_iter].rvalue = list[0].lvalue-1;	
		new_list[new_list_iter].type = GAP_TUPLE;
		new_list_iter++;
	}
	
	/* Check for gaps in between tuples */
	for (iter=0;iter < list_size-1; iter++)
	{
		new_list[new_list_iter].lvalue = list[iter].lvalue;	
		new_list[new_list_iter].rvalue = list[iter].rvalue;	
		new_list[new_list_iter].type = ORIGINAL_TUPLE;
		new_list_iter++;

		/* Is there a gap between this tuple & next ? */
		if ((list[iter+1].lvalue - list[iter].rvalue) > 0)
		{
			/* Treat this gap as a regular tuple */
			new_list[new_list_iter].lvalue = list[iter].rvalue+1;	
			new_list[new_list_iter].rvalue = list[iter+1].lvalue-1;	
			new_list[new_list_iter].type = GAP_TUPLE;
			new_list_iter++;
		}
	}
			
	/* Add the last tuple to the new_list */
	new_list[new_list_iter].lvalue = list[list_size-1].lvalue;	
	new_list[new_list_iter].rvalue = list[list_size-1].rvalue;	
	new_list[new_list_iter].type = ORIGINAL_TUPLE;
	new_list_iter++;

	/* Is there a gap from the end of the last tuple to max_range ? */
	if ((max_range - list[list_size-1].rvalue) > 0)
	{
		/* Treat this gap as a regular tuple */
		new_list[new_list_iter].lvalue = list[iter].rvalue+1;	
		new_list[new_list_iter].rvalue = max_range;	
		new_list[new_list_iter].type = GAP_TUPLE;
		new_list_iter++;
	}
		
	
	/* Transform the tuples in the list to meet the minimum range */
	total_gap_reqd = transform(new_list,new_list_iter,min_range_size);

	if (total_gap_reqd == 0)
	{
		index =0;

		/* Copy the new_list to original list */
		for (iter=0;iter < new_list_iter; iter++)
		{
			if (new_list[iter].type == ORIGINAL_TUPLE)
			{
				list[index].lvalue = new_list[iter].lvalue;
				list[index].rvalue = new_list[iter].rvalue;
				list[index].type = new_list[iter].type;
				index++;
			}
		}
		returnValue = 0;			
	}
	else
	{
		returnValue = -1;
	}

	/* Free the new_list */
	free(new_list);

	return(returnValue);
}


/*
 *	merge_list(list, tupleA, tupleB)
 *	The tupleA range is less than the required size,min_range_size. 
 *	The tupleB range could possibly accomodate an extension of tupleA
 *	range by readjusting their boundaries suitably in the list.
 */

static void
merge_list(tuple_t list[], int tupleA, int tupleB, int min_range_size)
{
	double	tupleA_range;
	double	gap;
	int	iter;

	tupleA_range = (list[tupleA].rvalue-list[tupleA].lvalue);

	/* Check if there is space in tupleB to extend tupleA */
	gap = ((list[tupleB].rvalue - list[tupleB].lvalue) - min_range_size);
	if (gap > 0)
	{
		if (tupleB > tupleA)
		{
			/* Extend tupleA */
			/* Check if gap is sufficient to meet the minimum
			   range requirement of tupleA */
			if (gap > (min_range_size - tupleA_range))
			{
				list[tupleA].rvalue = list[tupleA].lvalue 
							+ min_range_size;

				/* Move forward all tuples after tupleA 
				   upto tupleB */
				for (iter=tupleA+1;iter<tupleB;iter++)
				{
					list[iter].lvalue += 
						(min_range_size-tupleA_range);
					list[iter].rvalue += 
						(min_range_size-tupleA_range);
				}

				/* Adjust tupleB */
				list[tupleB].lvalue += 
						(min_range_size-tupleA_range);
			}
			else
			{
				list[tupleA].rvalue += gap;
		
				/* Move forward all tuples after tupleA 
				   upto tupleB */
				for (iter=tupleA+1;iter<tupleB;iter++)
				{
					list[iter].lvalue += gap;
					list[iter].rvalue += gap;
				}

				/* Adjust tupleB */
				list[tupleB].lvalue += gap;
			}
		}
		else
		{
			/* Shrink tupleB */
			/* Check if gap is sufficient to meet the minimum
			   range requirement of tupleA */
			if (gap > (min_range_size - tupleA_range))
			{
				list[tupleB].rvalue -= 
					(min_range_size-tupleA_range);

				/* Move back all the tuples upto tupleA */
				for (iter=tupleB+1;iter<tupleA;iter++)
				{
					list[iter].lvalue -= 
						(min_range_size-tupleA_range);
					list[iter].rvalue -= 
						(min_range_size-tupleA_range);
				}

				/* Adjust tupleA */
				list[tupleA].lvalue -= 
						(min_range_size-tupleA_range);
			}
			else
			{
				list[tupleB].rvalue -= gap;

				/* Move back all the tuples upto tupleA */
				for (iter=tupleB+1;iter<tupleA;iter++)
				{
					list[iter].lvalue -= gap;
					list[iter].rvalue -= gap;
				}

				/* Adjust tupleA */
				list[tupleA].lvalue -= gap;
			}
		}		
	}

}


/*
 *	find_max_range()
 *	Finds the tuple in the list which has the maximum range and
 *	returns the index to it in the list. Also the selected tuple
 *	is bigger than the min_range_size. If no suitable tuple is
 *	found, -1 is returned.
 */

static int
find_max_range(tuple_t list[],int list_size,int min_range_size)
{
	int max_range_index = -1;
	double current_range;
	double max_range;
	int iter;

	max_range = min_range_size;

	for (iter=0;iter < list_size; iter++)
	{
		current_range = (list[iter].rvalue - list[iter].lvalue);
		if (current_range > max_range)
		{
			max_range = current_range;
			max_range_index = iter;
		}
	}

	return(max_range_index);
}

/*
 *	transform()
 *	Rearranges the list by shifting tuples to meet the objective of
 *	each tuple having atleast the min_range_size. If it fails to find
 *	enough space in the tuples, it returns the size
 *	of the gap required to acheive this.
 */

static int
transform(tuple_t list[],int list_size,int min_range_size)
{
	int iter;
	int max_range_index;
	int	total_gap_reqd = 0;

#ifdef DEBUG
	printf("Transforming list to minimum range %d\n",min_range_size);
	print_list(list,list_size);
#endif

	/* Iterate through all the tuples */
	for (iter=0;iter < list_size; iter++)
	{
		/* Skip over gaps */
		if (list[iter].type == ORIGINAL_TUPLE)
		{
			max_range_index = 0;

			/* Check if it is less than the minimum expected 
			   range size */
			while (((list[iter].rvalue-list[iter].lvalue) 
						< min_range_size)
				&& (max_range_index != -1))
			{
				/* Find the tuple which has the maximum range */
				max_range_index = find_max_range(list,list_size,
							min_range_size);
				if (max_range_index != -1)
				{
					/* Adjust the two tuples such that 
					   the minimum expected range 
					   requirement is met */ 
#ifdef DEBUG
					printf("Merge(%d,%d)\n",iter,
							max_range_index);
#endif
					merge_list(list,iter,max_range_index,
							min_range_size);
				}
				else
				{
#ifdef DEBUG
					printf("Need a gap of %d\n",
						(min_range_size-
					(list[iter].rvalue-list[iter].lvalue)));
#endif
					total_gap_reqd += 
						(min_range_size-
					(list[iter].rvalue-list[iter].lvalue));
				}
			}
		}
	}
	return(total_gap_reqd);
}

#ifdef	DEBUG
/* 
 *	print_list()
 *	Prints the list of tuples for debug purposes.
 */

void
print_list(tuple_t list[],int list_size)
{
	int iter;

	/* Iterate through all the tuples */
	for (iter=0;iter < list_size; iter++)
	{
		printf("(%f,%f)\n",list[iter].lvalue,list[iter].rvalue);
	}
}
#endif


