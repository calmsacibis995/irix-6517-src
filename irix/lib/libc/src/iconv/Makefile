#!smake
#ident "$Revision: 1.15 $"

DEPTH=..

M4SRCS = \
    btowc_tmpl.m4 \
    iconv_arg_in.m4 \
    iconv_argutf8_in.m4 \
    iconv_cnv_7dbcs.m4 \
    iconv_cnv_dbcs.m4 \
    iconv_cnv_euc.m4 \
    iconv_cnv_idx.m4 \
    iconv_cnv_lat1.m4 \
    iconv_cnv_nul.m4 \
    iconv_cnv_uwc.m4 \
    iconv_cnv_xkeys.m4 \
    iconv_ctr.m4 \
    iconv_fl_in.m4 \
    iconv_fl_out.m4 \
    iconv_mbs_out.m4 \
    iconv_tmpl.m4 \
    iconv_top_defn.m4 \
    iconv_top_src.m4 \
    iconv_top_src_lat1.m4 \
    iconv_top_src_latin.m4 \
    iconv_top_src_sbcs.m4 \
    iconv_utf8_in.m4 \
    iconv_utf8_out.m4 \
    iconv_cnv_utf16.m4 \
    iconv_utf16_out.m4 \
    iconv_cnv_bom.m4 \
    iconv_uwc_out.m4 \
    mblen_tmpl.m4 \
    mbstowcs_tmpl.m4 \
    mbtowc_tmpl.m4 \
    stdlib_cnv_dbcswc.m4 \
    stdlib_cnv_eucjp3wc.m4 \
    stdlib_cnv_eucjpwc.m4 \
    stdlib_cnv_euctwwc.m4 \
    stdlib_cnv_sjiswc.m4 \
    stdlib_cnv_big5wc.m4 \
    stdlib_top_defn.m4 \
    stdlib_top_src.m4 \
    stdlib_wcdbcs_out.m4 \
    stdlib_wceucjp3_out.m4 \
    stdlib_wceucjp_out.m4 \
    stdlib_wceuctw_out.m4 \
    stdlib_wcsbcs_out.m4 \
    stdlib_wcsjis_out.m4 \
    stdlib_wcbig5_out.m4 \
    wcstombs_tmpl.m4 \
    wctob_tmpl.m4 \
    wctomb_tmpl.m4

LDIRT=iconv_converter.c stdlib_conv.c iconv.syms stdlib.syms stdlib.dcls.h \
      iconv.dcls.h symtab.h symtab.syms iconv.exports

include $(ROOT)/usr/include/make/libleafdefs

LWOFF=,1209,1174,1552,1110

#if $(LIBRARY) != "libc_G0_noso_32.a" && $(LIBRARY) != "libc_nomp_32_M2_ns.a" && $(LIBRARY) != "libc_nomp_n32_M3_ns.a"
CFILES=stdlib_conv_wrap.c iconv_converter.c nonshrtab.c
BUILDMODE=WITH_STYLE
#else
BUILDMODE=WITHOUT_STYLE
CFILES=stdlib_conv.c iconv_converter.c nonshrtab.c
ASFILES=mbwc_wrap.s
#endif

#
# the old_* files are only for shared C backward compat support
#

include $(LIBLEAFRULES)

mbwc_wrap.o: mbwc_wrap.s ../computed_include/mbwc_wrap.h
	$(ASF) mbwc_wrap.s -o $@

./../computed_include/mbwc_wrap.h : ../computed_include/mbwc_wrap.h

../computed_include/mbwc_wrap.h : ../computed_include/mbwcoffs.c
	cd ../computed_include; $(MAKE) mbwc_wrap.h

../computed_include/make_table : ../computed_include/make_table.c
	cd ../computed_include; $(MAKE) ./make_table

../computed_include/iconv_top_include.m4 : ../computed_include/matrix.c
	cd ../computed_include; $(MAKE) ./iconv_top_include.m4

#if $(BUILDMODE) == "WITH_STYLE"
stdlib_conv_wrap.o : stdlib_conv.o mbwc_wrap.o
	$(LDF) -r -nostdlib -o stdlib_conv_wrap.o mbwc_wrap.o stdlib_conv.o
#endif

nonshrtab.o : ./symtab.h

symtab.h ./symtab.h : iconv.exports ../computed_include/make_table
	cat nonshrsyms.syms iconv.exports > symtab.syms
	../computed_include/make_table symtab.syms symtab.h

iconv.exports : stdlib.syms iconv.syms
	cat stdlib.syms iconv.syms > iconv.exports

#iconv.syms iconv.dcls.h : $(M4SRCS)
#	$(SMAKE) iconv_converter.c

iconv.syms : iconv_converter.c

iconv.dcls.h ./iconv.dcls.h : iconv_converter.c

iconv_converter.c : $(M4SRCS) ../computed_include/iconv_top_include.m4
	$(GNUM4) -B65536 -D__date__="`date`" iconv_top_src.m4 > iconv_converter.c

#stdlib.syms stdlib.dcls.h : $(M4SRCS)
#	$(SMAKE) stdlib_conv.c

stdlib.syms : stdlib_conv.c

stdlib.dcls.h ./stdlib.dcls.h : stdlib_conv.c

stdlib_conv.c : $(M4SRCS) 
	$(GNUM4) -B65536 -D__date__="`date`" stdlib_top_src.m4 > stdlib_conv.c


