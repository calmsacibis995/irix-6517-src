#*************************************************************************
#                                                                        *
#               Copyright (C) 1993, Silicon Graphics, Inc.               *
#                                                                        *
#  These coded instructions, statements, and computer programs  contain  *
#  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#  are protected by Federal copyright law.  They  may  not be disclosed  *
#  to  third  parties  or copied or duplicated in any form, in whole or  *
#  in part, without the prior written consent of Silicon Graphics, Inc.  *
#                                                                        *
#*************************************************************************

#ident "$Revision: 1.3 $"


# xlv.lex
#   command parser for mkxlv.
#
#   PLEX <plex_name>:<flag1=value1>,<flag2=value2>:<components>,<components>

%{
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/xlv_base.h>

#define true 1
#define false 0

static int obj_type = XLV_OBJ_TYPE_NONE;	/* Type of object found */

#define ST_OBJ 1	/* Looking for VOL, PLEX, etc. */
#define ST_OBJ_NAME 2	/* Looking for vol_name, subvol_name, etc. */
#define ST_FLAGS 3	/* Looking for flag name */
#define ST_VALUE 4	/* Looking for flag value */
#define ST_COMPONENT 5	/* Looking for a component */
#define ST_MAX 6
static int state = ST_OBJ;	/* We start off looking for an object */

static seen_volume_definition = false;	/* Have we seen a VOL directive yet? */
static int continued = false;	/* Is line being continued? */

static char *prev_token = NULL;	/* Previous token scanned */

static char *obj_name;		/* Name of object */

#define MAX_FLAGS 100
static char *flag_name[MAX_FLAGS];	/* Array of flags */
static char *flag_value[MAX_FLAGS];	/*  & associated values */
static int flag_index=-1;

static char *component[MAX_FLAGS];	/* Components of this object */
static int component_index=-1;

extern void xlv_mk_obj (int I_obj_type, char *I_obj_name, char *I_flag_name[],
		        char *I_flag_value[], int I_num_flags,
		        char *I_component[], int I_num_components);
extern void xlv_mk_vol_done (void);
extern void xlv_mk_done (void);


void print_results (void)
{
	int i;

	printf ("Object name: %s\n", obj_name);
	if (flag_index == -1)
		printf ("no flags\n");
	else {
	    printf ("Flags:\n");
	    for (i=0; i <= flag_index; i++)
		printf ("\t%s\t%s\n", flag_name[i], flag_value[i]);
	}
	if (component_index == -1)
	    printf ("no components\n");
	else {
	    printf ("Components:\n");
	    for (i=0; i <= component_index; i++)
		printf ("\t%s\n", component[i]);
	}
	printf ("\n\n");
}

void do_end_of_line (void)
{
	int num_flags, num_components;

        /* First, handle any last tokens */
        if (state == ST_COMPONENT) {
            component [++component_index] = prev_token;
	    num_flags = flag_index+1;
	    num_components = component_index+1;
            xlv_mk_obj (obj_type, obj_name, flag_name, flag_value, 
		num_flags, component, num_components);
        }
	else if ((obj_type == XLV_OBJ_TYPE_VOL) &&
		 (state == ST_OBJ_NAME)) {
	    obj_name = prev_token;
	    xlv_mk_obj (obj_type, obj_name, flag_name, flag_value,
		num_flags, component, num_components);
	}
        else
            printf ("Error, incomplete line\n");

#ifdef DEBUG
        print_results();
#endif

        flag_index = component_index = -1;
        state = ST_OBJ;
        obj_type = XLV_OBJ_TYPE_NONE;
}

/*
 * Action routine invoked when we recognize an object type
 * keyword at the start of a line.  (e.g., VOL, PLEX..).
 */
int do_object_type (int i_obj_type)
{
	if (state == ST_OBJ) {
		obj_name = "";
                obj_type = i_obj_type;
                prev_token = strdup (yytext);
		if ((obj_type == XLV_OBJ_TYPE_DATA_SUBVOL) ||
		    (obj_type == XLV_OBJ_TYPE_LOG_SUBVOL) ||
		    (obj_type == XLV_OBJ_TYPE_RT_SUBVOL) ) {

		    /* Subvolumes do not have names */
		    state = ST_FLAGS;	    /* Look for flags next */
		}
		else {
		    /* Everything else has a name */
                    state = ST_OBJ_NAME;    /* Look for name next */
		}
		return 0;
        }
        else return 1;
}

%}

%%
^#.*\n	;		/* comment line */
^\n	;		/* blank line */
^VOL	{ if (do_object_type (XLV_OBJ_TYPE_VOL)) {
		REJECT;
		}
	  else {
		/* If we were definining a volume, the definition
		   of a new volume is a signal that the all the
		   pieces that belong to the previous volume should 
		   have been defined. */
		if (seen_volume_definition) {
		    /* The previous volume definition must be complete. */
		    xlv_mk_vol_done();
		}
	        seen_volume_definition = true;
	  }
	}
^DATA	if (do_object_type (XLV_OBJ_TYPE_DATA_SUBVOL)) REJECT;
^LOG	if (do_object_type (XLV_OBJ_TYPE_LOG_SUBVOL)) REJECT;
^RT	if (do_object_type (XLV_OBJ_TYPE_RT_SUBVOL)) REJECT;
^PLEX	if (do_object_type (XLV_OBJ_TYPE_PLEX)) REJECT;
^VE	if (do_object_type (XLV_OBJ_TYPE_VOL_ELMNT)) REJECT;
\=	{
            if (state == ST_FLAGS) {
#ifdef DEBUG
		printf ("= matched, flag: %s\n", prev_token);
#endif
                flag_name [++flag_index] = prev_token;
                state = ST_VALUE;       /* Look for value next */
            }
            else REJECT;
	}
\n	{
	    if (continued)
		continued = false;
	    else
		do_end_of_line();
	}
\\$	continued = true;
^#.*\n	;		/* comment line */
\,	{
#ifdef DEBUG
	    printf ("Matched , in state %d\n", state);
#endif
	    switch (state) {
	    case ST_VALUE:
		flag_value [flag_index] = prev_token;
		state = ST_FLAGS;	/* Look for another flag */
		break;
	    case ST_COMPONENT:
		component [++component_index] = prev_token;
		break;
	    default:
		printf ("Error, unexpected ,\n");
		break;
	    }
	    prev_token = NULL;
	}
\:	{ 
#ifdef DEBUG
	    printf ("Matched : in state %d\n", state);
#endif 
	    switch (state) {
	    case ST_OBJ:
		obj_name = prev_token;
		printf ("Error: missing object type for %s\n", obj_name);
		state = ST_OBJ_NAME;
		break;
	    case ST_OBJ_NAME:
		obj_name = prev_token;
		state = ST_FLAGS;
		break;
	    case ST_FLAGS:	/* No flags specified, ok */
		state = ST_COMPONENT;
		break;
	    case ST_VALUE:
		flag_value [flag_index] = prev_token;
		state = ST_COMPONENT;
		break;
	    case ST_COMPONENT:
		printf ("Error, unexpected : %s\n", yytext);
		break;
	    default:
		printf ("Illegal syntax\n");
		break;
	    }
	    prev_token = NULL;
	}
[a-zA-z\$/]*[/0-9a-zA-Z\$]*	{ 
#ifdef LEX_DEBUG
	    printf ("Got text token: %s (state=%d)\n", yytext, state);
#endif
	    prev_token = strdup (yytext);
	}
[0-9]*	{
#ifdef LEX_DEBUG
	    printf ("Got text token: %s, (state=%d)\n", yytext, state);
#endif
	    prev_token = strdup (yytext);
	}
%%

main (int argc, char **argv)
{
	FILE *file;

	if (argc == 2) {
	    file = fopen (argv[1], "r");
	    if (!file) {
		fprintf (stderr, "Could not open %s\n", argv[1]);
		exit (-1);
	    }
	    yyin = file;
	}
	yylex();
	return (0);
}

/*
 * LEX calls yywrap to handle EOF.
 * 
 * We use this as an indication that the user has specified all
 * the pieces of the volume.
 */
int yywrap(void) {

	xlv_mk_vol_done();
	xlv_mk_done();

	return (1);	/* no more input */
}
