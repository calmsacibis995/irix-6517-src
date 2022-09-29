include	${ROOT}/usr/include/make/commondefs

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS= ldap

SUBDIRS= db dns files ldap mdbm ndbm nis nisserv

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
