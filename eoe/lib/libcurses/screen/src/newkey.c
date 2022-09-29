/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)curses:screen/newkey.c	1.4"
#include	"curses_inc.h"

/*
 * Set a new key or a new macro.
 *
 * rcvchars: the pattern identifying the key
 * keyval: the value to return when the key is recognized
 * macro: if this is not a function key but a macro,
 * 	tgetch() will block on macros.
 */

int
newkey(char *rcvchars, int keyval, int macro)
{
    register	_KEY_MAP	**keys, *key_info,
				**prev_keys = cur_term->_keys;
    short	*numkeys = &cur_term->_ksz, len;
    char	*str;

    if ((!rcvchars) || (*rcvchars == '\0') || (keyval < 0) ||
	(((keys = (_KEY_MAP **) malloc(sizeof (_KEY_MAP *) * ((size_t)*numkeys + 1))) == NULL)))
    {
	goto bad;
    }

    len = (short) (strlen(rcvchars) + 1);

    if ((key_info = (_KEY_MAP *) malloc(sizeof (_KEY_MAP) + (size_t) len)) == NULL)
    {
	free (keys);
bad :
	term_errno = TERM_BAD_MALLOC;
#ifdef	DEBUG
	strcpy(term_parm_err, "newkey");
#endif	/* DEBUG */
	return (ERR);
    }

    if (macro)
    {
	(void) memcpy((char *) keys, (char *) prev_keys, ((size_t) *numkeys * sizeof (_KEY_MAP *)));
	keys[*numkeys] = key_info;
    }
    else
    {
	short	*first = &(cur_term->_first_macro);

	(void) memcpy((char *) keys, (char *) prev_keys, ((size_t) *first * sizeof (_KEY_MAP *)));
	(void) memcpy((char *) &(keys[*first + 1]), (char *) &(prev_keys[*first]), ((size_t) (*numkeys - *first) * sizeof (_KEY_MAP *)));
	keys[(*first)++] = key_info;
	cur_term->_lastmacro_ordered++;
    }
    if (prev_keys != NULL)
	free(prev_keys);
    cur_term->_keys = keys;

    (*numkeys)++;
    key_info->_sends = str = (char *) key_info + sizeof(_KEY_MAP);
    (void) memcpy(str, rcvchars, (size_t) len);
    key_info->_keyval = (short) keyval;
    cur_term->funckeystarter[*str] |= (macro ? _MACRO : _KEY);

    return (OK);
}
