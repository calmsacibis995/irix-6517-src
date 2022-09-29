#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sadmin/admin/RCS/Makefile,v 1.18 1989/06/09 14:45:34 narula Exp $"

include	$(ROOT)/usr/include/make/commondefs

ADMINHOME = /usr/admin
UBIN = /usr/bin

C_CMD= shade
CMDS= setup powerdown sysadm

PARTS = $(CMDS) gettyvalues menu/DESC \
menu/filemgmt/DESC menu/filemgmt/diskuse \
menu/filemgmt/fileage menu/filemgmt/filesize \
menu/machinemgmt/DESC \
menu/machinemgmt/powerdown menu/machinemgmt/reboot menu/machinemgmt/whoson \
menu/syssetup/DESC menu/syssetup/admpasswd \
menu/syssetup/datetime menu/syssetup/nodename menu/syssetup/setup \
menu/syssetup/syspasswd menu/ttymgmt/DESC menu/ttymgmt/lineset \
menu/ttymgmt/mklineset menu/ttymgmt/modtty menu/usermgmt/DESC \
menu/usermgmt/addgroup menu/usermgmt/adduser menu/usermgmt/delgroup \
menu/usermgmt/deluser menu/usermgmt/lsgroup menu/usermgmt/lsuser \
menu/usermgmt/modadduser menu/usermgmt/modgroup/DESC \
menu/usermgmt/modgroup/chgname menu/usermgmt/moduser/DESC \
menu/usermgmt/moduser/chgloginid menu/usermgmt/moduser/chgpasswd \
menu/usermgmt/moduser/chgshell


INSCMD	=$(INSTALL) -idb "std.sw.sysadm"

default:	$(PARTS) $(C_CMD)

clean:

clobber:	clean
	rm -f shade

FRC:

install: default
	$(INSCMD) -dir $(ADMINHOME)
	$(INSCMD) -F $(UBIN) -m 744 $(C_CMD)
	$(INSCMD) -ln $(UBIN)/$(C_CMD) -F $(UBIN) "$(CMDS)"
	$(INSCMD) -F $(ADMINHOME) "$(PARTS)"

# NOTE: this make file used to generate a partial passwd file, and the
# lines were to be included in the normal passwd file.  However, SGI
# always includes these "sysadm" login names in the normal passwd file
# so there isn't any need to generate a partial one here
