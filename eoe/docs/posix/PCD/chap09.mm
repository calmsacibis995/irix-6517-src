'\" pcd/chap09.mm 1.5 99/01/20 (C) Copyright 1999 Silicon Graphics, Inc.
.1 9 "System Databases"
.2 1 "System Databases"
.iD
On
\*(sN
systems,
if the initial working directory field is null, an attempt to log in by that
user fails with a message stating that changing directory to the
null string is not possible.
.uS
The \fIgroup\fP system database contains 
the following additional fields: 
.br
An encrypted password field. See the group(4) manpage for details.
.uS
On
\*(sN
systems, the \fIuser\fP system database contains 
the following additional fields: 
.br
An encrypted password field. 
.br
A field for the user's real name.
.P
See the passwd(4) man page for details.
.rN 15
.2 2 "Database Access"
.3 1 "Group Database Access"
.4 3 "Returns"
.mV
On
\*(sN
systems, the return values for the
.f getgrgid
and
.f getgrnam
functions point to static data that is overwritten by each call.
.4 4 "Errors"
.uS
For the
.f getgrgid
and
.f getgrnam
functions, no error conditions are detected. 
.3 2 "User Database Access"
.4 3 "Returns"
.mV 
For the 
.f getpwuid
and
.f getpwnam
functions, the return values 
point to static data that is overwritten on each call.
.4 4 "Errors"
.uS
On
\*(sN
systems, the
.f getpwuid
and
.f getpwnam
functions detect no error conditions. 
.nP
.dM
