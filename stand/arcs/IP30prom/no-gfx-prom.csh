#!/bin/csh
#
# Builds a non-gfx prom
#
setenv CONF_FILE    $WORKAREA/stand/arcs/IP30prom/IP30conf.cf
setenv BAKCONF_FILE /tmp/IP30conf.bak
setenv SEDCONF_FILE /tmp/IP30conf.sed

# Remove temporary files
rm -f $BAKCONF_FILE
rm -f $SEDCONF_FILE

# Clobber object files
cd $WORKAREA/stand/arcs/IP30prom
make clobber

# Clobber the pide and ide libraries
cd $WORKAREA/stand/arcs/ide/IP30/pon
make clobber LIBTYPE=p
make clobber LIBTYPE=

# Clobber the libsk library
cd $WORKAREA/stand/arcs/lib/libsk
make clobber


# Make backup of conf file
cp -f $CONF_FILE $BAKCONF_FILE

# Create a conf file with graphics stub
sed -e 's/node: mgras/node! mgras/' $CONF_FILE >! $SEDCONF_FILE

# Copy file to conf file
cp -f $SEDCONF_FILE $CONF_FILE

# Make flash prom
cd $WORKAREA/stand/arcs
make fprom.bin

# Restore conf file
cp -pf $BAKCONF_FILE $CONF_FILE

cp -pf $WORKAREA/stand/arcs/IP30prom/RACER.O/fprom.bin $WORKAREA/stand/arcs/IP30prom/NoGFXProm.bin

# Clean up IP30/pon directory and make it ready for ide
cd $WORKAREA/stand/arcs/ide/IP30/pon
make clobber
make mfgstuff

