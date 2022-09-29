/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.				  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

%{
/* iconv_comp.y
 *
 *			Written by Gianni Mariani 31-Jul-1995
 *  Iconv compiler.
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <ctype.h>
#include <wchar.h>
#include "iconv.h"
#include "iconv_cnv.h"
#include "iconv_int.h"
#include "in_proc.h"
#include "iconv_comp.h"
#include "iconv_comp.c.h"

%}

%token ICC_END			400
%token ICC_NONE			401
%token ICC_STRING		402
%token ICC_IDENTIFIER		403
%token ICC_FILE			404
%token ICC_CLONE		405
%token ICC_JOIN			406
%token ICC_USE			407
%token ICC_MBWC			408
%token ICC_WCMB			409
%token ICC_LOCALE_ALIAS		410
%token ICC_LOCALE_CODESET	411
%token ICC_RESOURCE		412
%token ICC_INTEGER		413


%%

speclist	:
		| speclist spec
		| speclist locale_info
		| speclist resource
		| error
		    {
			yyerror( "Syntax error" );
		    }
		;


spec		: val val val val val val tablelist ICC_END
		    {
			mkentry(
			    $1->val,
			    $2->val,
			    $3->val,
			    $4->val,
			    $5->val,
			    $6->val,
			    $7->tblst
			);
		    }
		| ICC_CLONE '*' val val val val val ICC_USE '*' val val val val val ICC_END
		    {
			mkclone(
			    1, $3->val, $4->val, $5->val, $6->val, $7->val,
			    $10->val, $11->val, $12->val, $13->val, $14->val
			);
		    }
		| ICC_CLONE val '*' val val val val ICC_USE val '*' val val val val ICC_END
		    {
			mkclone(
			    0, $2->val, $4->val, $5->val, $6->val, $7->val,
			    $9->val, $11->val, $12->val, $13->val, $14->val
			);
		    }
		| ICC_JOIN '*' val val val val val ','
		           val '*' val val val val
		  ICC_USE  val val val val ICC_END
		    {
			mkjoin(
			    $3->val, $4->val, $5->val, $6->val, $7->val,
			    $9->val, $11->val, $12->val, $13->val, $14->val,
			    $16->val, $17->val, $18->val, $19->val );
		    }
		|   val val val val
		    ICC_MBWC tablelist
		    ICC_WCMB tablelist ICC_END
		    {
			mkstdlib(
			    $1->val, $2->val, $3->val, $4->val,
			    $6->tblst, $8->tblst
			);
		    }
		;

locale_info	: ICC_LOCALE_ALIAS val val ICC_END
		    {
			$$ = mklocale_alias( $2->val, $3->val );
		    }
		| ICC_LOCALE_CODESET val val ICC_END
		    {
			$$ = mklocale_codeset( $2->val, $3->val );
		    }
		;

resource	: ICC_RESOURCE val '{' values opt_comma '}' ICC_END
		    {
			$$ = mkresource( $2->val, $4->values );
		    }
		;

opt_comma	: ','
		|
		;

values		: value
		    {
			$$ = mkvalues( 0, $1->value );
		    }
		| values ',' value
		    {
			$$ = mkvalues( $1->values, $3->value );
		    }
		| error
		    {
			yyerror( "Syntax error" );
		    }
		;

value		: type_spec ICC_IDENTIFIER '=' val
		    {
			$$ = mkvalue_str( $1->type_spec, $2->val, $4->val );
		    }
		| type_spec ICC_IDENTIFIER '=' ICC_INTEGER
		    {
			$$ = mkvalue_int( $1->type_spec, $2->val, $4->val );
		    }
                | type_spec ICC_IDENTIFIER '=' table
                    {
                        $$ = mkvalue_table( $1->type_spec, $2->val, $4->table );
                    }
		;

type_spec	: ICC_IDENTIFIER
		    {
			$$ = mkchktyp( $1->val );
		    }
		;


tablelist       : table
                  {
			$$ = mktablelist( 0, $1->table );

                  }
                | tablelist table
                  {
                        $$ = mktablelist( $1->tblst, $2->table );
                  }                  
                ;

table		: ICC_FILE val
		    {
			$$ = mktable( $2->val, 0, 1 );
		    }
		| val val
		    {
			$$ = mktable( $1->val, $2->val, 0 );
		    }
		;


val		: ICC_NONE
		    {
			$$ = 0;
		    }
		| ICC_STRING
		    {
			$$ = $1;
		    }
		| ICC_IDENTIFIER
		    {
			$$ = $1;
		    }
		;


%%
