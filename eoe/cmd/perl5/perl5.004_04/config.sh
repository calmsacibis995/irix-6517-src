#!/bin/sh
#
# This file was produced by running the Configure script.  It holds all
# the definitions figured out by Configure.  Should you modify any of
# these values, do not forget to propagate your changes by running
# "Configure -S"; or, equivalently, you may run each .SH file yourself.
#

# Configuration time: Wed Dec  3 09:14:22 PST 1997
# Configured by: scotth
# Target system: irix hoshi 6.5-alpha-1274274633 11251326 ip22 

Author=''
Date='$Date'
Header=''
Id='$Id'
Locker=''
Log='$Log'
Mcc='Mcc'
RCSfile='$RCSfile'
Revision='$Revision'
Source=''
State=''
afs='false'
alignbytes='8'
aphostname=''
ar='ar'
archlib='/usr/share/lib/perl5/irix-n32/5.00404'
archlibexp='/usr/share/lib/perl5/irix-n32/5.00404'
archname='irix-n32'
archobjs=''
awk='awk'
baserev='5.0'
bash=''
bin='/usr/sbin'
bincompat3='y'
binexp='/usr/sbin'
bison=''
byacc='byacc'
byteorder='4321'
c='\c'
castflags='0'
cat='cat'
cc='cc -n32 -mips3'
cccdlflags=' '
ccdlflags=''
ccflags='-D_BSD_SIGNALS -D_BSD_TYPES -D_BSD_TIME -woff 1009,1110,1184 -OPT:Olimit=0:space=ON -DLANGUAGE_C -DEMBEDMYMALLOC'
cf_by='scotth'
cf_email='scotth@sgi.com'
cf_time='Wed Dec  3 09:14:22 PST 1997'
chgrp=''
chmod=''
chown=''
clocktype='clock_t'
comm='comm'
compress=''
contains='grep'
cp='cp'
cpio=''
cpp='cpp'
cpp_stuff='42'
cppflags='-D_BSD_SIGNALS -D_BSD_TYPES -D_BSD_TIME -OPT:Olimit=0:space=ON -DLANGUAGE_C -DEMBEDMYMALLOC'
cpplast=''
cppminus=''
cpprun='/usr/lib/cpp'
cppstdin='cppstdin'
cryptlib=''
csh='csh'
d_Gconvert='gcvt((x),(n),(b))'
d_access='define'
d_alarm='define'
d_archlib='define'
d_attribut='undef'
d_bcmp='define'
d_bcopy='define'
d_bincompat3='define'
d_bsd='define'
d_bsdgetpgrp='undef'
d_bsdpgrp='undef'
d_bsdsetpgrp='undef'
d_bzero='define'
d_casti32='define'
d_castneg='define'
d_charvspr='undef'
d_chown='define'
d_chroot='define'
d_chsize='undef'
d_closedir='define'
d_const='define'
d_crypt='define'
d_csh='define'
d_cuserid='define'
d_dbl_dig='define'
d_difftime='define'
d_dirnamlen='undef'
d_dlerror='define'
d_dlopen='define'
d_dlsymun='undef'
d_dosuid='undef'
d_dup2='define'
d_eofnblk='define'
d_eunice='undef'
d_fchmod='define'
d_fchown='define'
d_fcntl='define'
d_fd_macros='define'
d_fd_set='define'
d_fds_bits='define'
d_fgetpos='define'
d_flexfnam='define'
d_flock='define'
d_fork='define'
d_fpathconf='define'
d_fsetpos='define'
d_ftime='undef'
d_getgrps='define'
d_setgrps='define'
d_gethent='define'
d_gethname='undef'
d_getlogin='define'
d_getpgid='define'
d_getpgrp2='undef'
d_getpgrp='define'
d_getppid='define'
d_getprior='define'
d_gettimeod='define'
d_gnulibc='undef'
d_htonl='define'
d_index='undef'
d_inetaton='define'
d_isascii='define'
d_killpg='define'
d_link='define'
d_locconv='define'
d_lockf='define'
d_lstat='define'
d_mblen='define'
d_mbstowcs='define'
d_mbtowc='define'
d_memcmp='define'
d_memcpy='define'
d_memmove='define'
d_memset='define'
d_mkdir='define'
d_mkfifo='define'
d_mktime='define'
d_msg='define'
d_msgctl='define'
d_msgget='define'
d_msgrcv='define'
d_msgsnd='define'
d_mymalloc='define'
d_nice='define'
d_oldarchlib='define'
d_oldsock='undef'
d_open3='define'
d_pathconf='define'
d_pause='define'
d_phostname='undef'
d_pipe='define'
d_poll='define'
d_portable='define'
d_pwage='define'
d_pwchange='undef'
d_pwclass='undef'
d_pwcomment='define'
d_pwexpire='undef'
d_pwquota='undef'
d_readdir='define'
d_readlink='define'
d_rename='define'
d_rewinddir='define'
d_rmdir='define'
d_safebcpy='define'
d_safemcpy='define'
d_sanemcmp='define'
d_seekdir='define'
d_select='define'
d_sem='define'
d_semctl='define'
d_semget='define'
d_semop='define'
d_setegid='define'
d_seteuid='define'
d_setlinebuf='define'
d_setlocale='define'
d_setpgid='define'
d_setpgrp2='undef'
d_setpgrp='define'
d_setprior='define'
d_setregid='define'
d_setresgid='undef'
d_setresuid='undef'
d_setreuid='define'
d_setrgid='define'
d_setruid='define'
d_setsid='define'
d_sfio='undef'
d_shm='define'
d_shmat='define'
d_shmatprototype='define'
d_shmctl='define'
d_shmdt='define'
d_shmget='define'
d_sigaction='define'
d_sigsetjmp='define'
d_socket='define'
d_sockpair='define'
d_statblks='define'
d_stdio_cnt_lval='define'
d_stdio_ptr_lval='define'
d_stdiobase='define'
d_stdstdio='define'
d_strchr='define'
d_strcoll='define'
d_strctcpy='define'
d_strerrm='strerror(e)'
d_strerror='define'
d_strtod='define'
d_strtol='define'
d_strtoul='define'
d_strxfrm='define'
d_suidsafe='define'
d_symlink='define'
d_syscall='define'
d_sysconf='define'
d_sysernlst=''
d_syserrlst='define'
d_system='define'
d_tcgetpgrp='define'
d_tcsetpgrp='define'
d_telldir='define'
d_telldir_prototype='define'
d_time='define'
d_times='define'
d_truncate='define'
d_tzname='define'
d_umask='define'
d_uname='define'
d_vfork='undef'
d_void_closedir='undef'
d_voidsig='define'
d_voidtty=''
d_volatile='define'
d_vprintf='define'
d_wait4='undef'
d_waitpid='define'
d_wcstombs='define'
d_wctomb='define'
d_xenix='undef'
date='date'
db_hashtype='u_int32_t'
db_prefixtype='size_t'
defvoidused='15'
direntrytype='struct dirent'
dlext='so'
dlsrc='dl_dlopen.xs'
dynamic_ext='DB_File Fcntl IO NDBM_File ODBM_File Opcode POSIX SDBM_File Socket'
eagain='EAGAIN'
echo='echo'
egrep='egrep'
emacs=''
eunicefix=':'
exe_ext=''
expr='expr'
extensions='DB_File Fcntl IO NDBM_File ODBM_File Opcode POSIX SDBM_File Socket'
find='find'
firstmakefile='makefile'
flex=''
fpostype='fpos_t'
freetype='void'
full_csh='/sbin/csh'
full_sed='/sbin/sed'
gcc=''
gccversion=''
gidtype='gid_t'
glibpth='/usr/shlib  /shlib /usr/lib/pa1.1 /usr/lib/large /lib /usr/lib /usr/lib/386 /lib/386 /lib/large /usr/lib/small /lib/small /usr/ccs/lib /usr/ucblib /usr/local/lib '
grep='grep'
groupcat=''
groupstype='gid_t'
gzip='gzip'
h_fcntl='false'
h_sysfile='true'
hint='recommended'
hostcat='ypcat hosts'
huge=''
i_bsdioctl=''
i_db='define'
i_dbm='define'
i_dirent='define'
i_dld='undef'
i_dlfcn='define'
i_fcntl='undef'
i_float='define'
i_gdbm='undef'
i_grp='define'
i_limits='define'
i_locale='define'
i_malloc='define'
i_math='define'
i_memory='undef'
i_ndbm='define'
i_neterrno='undef'
i_niin='define'
i_pwd='define'
i_rpcsvcdbm='undef'
i_sfio='undef'
i_sgtty='undef'
i_stdarg='define'
i_stddef='define'
i_stdlib='define'
i_string='define'
i_sysdir='define'
i_sysfile='define'
i_sysfilio='define'
i_sysin='undef'
i_sysioctl='define'
i_sysndir='undef'
i_sysparam='define'
i_sysresrc='define'
i_sysselct='define'
i_syssockio=''
i_sysstat='define'
i_systime='define'
i_systimek='undef'
i_systimes='define'
i_systypes='define'
i_sysun='define'
i_syswait='define'
i_termio='undef'
i_termios='define'
i_time='undef'
i_unistd='define'
i_utime='define'
i_values='define'
i_varargs='undef'
i_varhdr='stdarg.h'
i_vfork='undef'
incpath=''
inews=''
installarchlib='/usr/share/lib/perl5/irix-n32/5.00404'
installbin='/usr/sbin'
installman1dir='/usr/catman/local/man1'
installman3dir='/usr/catman/local/man3/perl5'
installprivlib='/usr/share/lib/perl5'
installscript='/usr/sbin'
installsitearch='/usr/share/lib/perl5/site_perl/irix-n32'
installsitelib='/usr/share/lib/perl5/site_perl'
intsize='4'
known_extensions='DB_File Fcntl GDBM_File IO NDBM_File ODBM_File Opcode POSIX SDBM_File Socket'
ksh=''
large=''
ld='cc'
lddlflags='-n32 -mips3 -shared'
ldflags='-n32'
less='less'
lib_ext='.a'
libc='/usr/lib32/libc.so'
libperl='libperl.so.4.4'
libpth='/usr/lib32 /lib32'
libs='-lm'
libswanted='sfio net inet nm ndbm gdbm dbm db malloc dld ld m c cposix posix ndir dir ucb BSD x'
line='line'
lint=''
lkflags=''
ln='ln'
lns='/sbin/ln -s'
locincpth='/usr/local/include /opt/local/include /usr/gnu/include /opt/gnu/include /usr/GNU/include /opt/GNU/include'
loclibpth='/usr/local/lib /opt/local/lib /usr/gnu/lib /opt/gnu/lib /usr/GNU/lib /opt/GNU/lib'
longsize='4'
lp=''
lpr=''
ls='ls'
lseektype='off_t'
mail=''
mailx=''
make='/sbin/make'
make_set_make='#'
mallocobj='malloc.o'
mallocsrc='malloc.c'
malloctype='void *'
man1dir='/usr/catman/local/man1'
man1direxp='/usr/catman/local/man1'
man1ext='1'
man3dir='/usr/catman/local/man3/perl5'
man3direxp='/usr/catman/local/man3/perl5'
man3ext='3'
medium=''
mips=''
mips_type='System V'
mkdir='mkdir'
models='none'
modetype='mode_t'
more='more'
mv=''
myarchname='IP22-irix'
mydomain='.engr.sgi.com'
myhostname='hoshi'
myuname='irix hoshi 6.5 11251326 ip22 '
n=''
nm_opt='-p'
nm_so_opt='-p'
nroff='nroff'
o_nonblock='O_NONBLOCK'
obj_ext='.o'
oldarchlib='/usr/share/lib/perl5/sgi_perl'
oldarchlibexp='/usr/share/lib/perl5/sgi_perl'
optimize='-O3 -mips3'
orderlib='false'
osname='irix'
osvers='6.5'
package='perl5'
pager='/usr/bsd/more'
passcat=''
patchlevel='4'
path_sep=':'
perl='perl'
perladmin='none'
perlpath='/usr/sbin/perl'
pg='pg'
phostname='hostname'
plibpth='/usr/lib32 /lib32 /usr/ccs/lib'
pmake=''
pr=''
prefix='/usr/share'
prefixexp='/usr/share'
privlib='/usr/share/lib/perl5'
privlibexp='/usr/share/lib/perl5'
prototype='define'
randbits='48'
ranlib=':'
rd_nodata='-1'
rm='rm'
rmail=''
runnm='true'
scriptdir='/usr/sbin'
scriptdirexp='/usr/sbin'
sed='sed'
selecttype='fd_set *'
sendmail='sendmail'
sh='/bin/sh'
shar=''
sharpbang='#!'
shmattype='void *'
shortsize='2'
shrpdir='/usr/lib32'
shrpenv=''
shrplib='/usr/lib32'
shsharp='true'
sig_name='ZERO HUP INT QUIT ILL TRAP ABRT EMT FPE KILL BUS SEGV SYS PIPE ALRM TERM USR1 USR2 CHLD PWR WINCH URG IO STOP TSTP CONT TTIN TTOU VTALRM PROF XCPU XFSZ K32 CKPT RESTART UME NUM36 NUM37 NUM38 NUM39 NUM40 NUM41 NUM42 NUM43 NUM44 NUM45 NUM46 PTINTR PTRESCHED RTMIN NUM50 NUM51 NUM52 NUM53 NUM54 NUM55 NUM56 NUM57 NUM58 NUM59 NUM60 NUM61 NUM62 NUM63 RTMAX IOT CLD POLL '
sig_num='0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 6 18 22 '
signal_t='void'
sitearch='/usr/share/lib/perl5/site_perl/irix-n32'
sitearchexp='/usr/share/lib/perl5/site_perl/irix-n32'
sitelib='/usr/share/lib/perl5/site_perl'
sitelibexp='/usr/share/lib/perl5/site_perl'
sizetype='size_t'
sleep=''
smail=''
small=''
so='so'
sockethdr=''
socketlib=''
sort='sort'
spackage='Perl5'
spitshell='cat'
split=''
ssizetype='ssize_t'
startperl='#!/usr/sbin/perl'
startsh='#!/bin/sh'
static_ext=' '
stdchar='unsigned char'
stdio_base='((fp)->_base)'
stdio_bufsiz='((fp)->_cnt + (fp)->_ptr - (fp)->_base)'
stdio_cnt='((fp)->_cnt)'
stdio_ptr='((fp)->_ptr)'
strings='/usr/include/string.h'
submit=''
subversion='4'
sysman='/usr/man/man1'
tail=''
tar=''
tbl=''
test='test'
timeincl='/usr/include/sys/time.h '
timetype='time_t'
touch='touch'
tr='tr'
troff=''
uidtype='uid_t'
uname='uname'
uniq='uniq'
usedl='define'
usemymalloc='y'
usenm='true'
useopcode='true'
useperlio='define'
useposix='true'
usesfio='false'
useshrplib='true'
usevfork='false'
usrinc='/usr/include'
uuname=''
vi=''
voidflags='15'
xlibpth='/usr/lib/386 /lib/386'
zcat=''
zip='zip'
PATCHLEVEL=4
SUBVERSION=4
CONFIG=true
# Variables propagated from previous config.sh file.
pp_sys_cflags='ccflags="$ccflags -DHAS_TELLDIR_PROTOTYPE"'
