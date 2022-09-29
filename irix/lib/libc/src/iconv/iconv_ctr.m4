/* __file__ =============================================== *
 *
 *  iconv_control for tables
 */

define(create_control, `
    MAKE_CONTROL_FUNCTION()
')

syscmd(echo "__iconv_control" >> CZZ_SYMSFILE)
syscmd(echo "void *  __iconv_control(
    int		       operation_id,
    void	    *  state,
    ICONV_CNVFUNC      iconv_converter,
    short	       num_table,
    void	    ** iconv_tab
);" >> CZZ_DCLSFILE)


define(`MAKE_CONTROL_FUNCTION',``
/* === Following found in ''__file__`` line ''__line__`` ============= */

void*  __iconv_control(int operation_id,
        void *state, ICONV_CNVFUNC iconv_converter, short num_table, void **iconv_tab)
{
    switch (operation_id) {
    case ICONV_C_DESTROY: /* iconv_close call */
	return (void *) 0;

    case ICONV_C_INITIALIZE: /* iconv_open */
	* ( void ** ) state = (void *) iconv_tab;
	return (void *) 0;
    }
    return ( void * ) -1l;
}

/* === Previous code found in ''__file__`` line ''__line__`` ============= */
'')

/* End __file__ ============================================== */
