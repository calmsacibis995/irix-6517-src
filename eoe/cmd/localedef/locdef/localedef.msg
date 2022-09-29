$ localdef.msg
$ Source file for localedef messages.

$ Operational errors
MSGID_USAGE Usage: localedef [-c] [-f charmap] [-i sourcefile] name\n
MSGID_NOMEM Error: could not allocate enough memory\n
MSGID_FILOP Error: cannot open file %s: %s\n
MSGID_FILE_CREATE Error: cannot create file %s: %s\n
MSGID_FILE_READ Error: cannot read file %s: %s\n
MSGID_FILE_WRITE Error: cannot write file %s: %s\n
MSGID_FILE_DELETE Error: cannot delete file %s: %s\n
MSGID_TMPFIL Error: tmpfile generation error\n
MSGID_FORKERR Error: could not create new process\n
MSGID_WAITERR Error: wait interrupted by signal.\n
MSGID_NOWRITE Error: no write permission in %s\n
MSGID_ERRCHOWN Error: could not chown %s\n
MSGID_ERRGRP Error: could not get owner/group information\n
MSGID_DIREXIST Error: %s locale directory exists\n
MSGID_BUFLEN Warning: buffer length exceeded during processing\n

$ Operational information
MSGID_RUNCMD Running %s... 
MSGID_RUNDONE done\n
MSGID_PARSING_OK All parsing completed successfully.\n

$ Syntax/semantic errors/warnings
MSGID_EOF Error: unexpected end-of-file in file %s\n
MSGID_NOCOPY Error: could not process COPY command\n
MSGID_CHARMAP_SYNTAX Error: syntax error in charmap file at line %d\n
MSGID_SYNTAX Error: syntax error in source at line %d file near token %s\n
MSGID_UNKNOWN_SYM Error: unknown symbolic name %s\n
MSGID_VALIDCM Warning: no valid symbol on CHARMAP line, ignoring\n --> %s\n
MSGID_DUPSYM Warning: duplicate symbol %s\n
MSGID_NOCODE Warning: no encoding for symbol %s\n
MSGID_TOO_MANY_WEIGHTS Warning: too many weights defined at line %d\n
MSGID_MAX_WEIGHTS Warning: only %d weights are supported\n
MSGID_MONTBL_LIMIT Warning: localedef currently limits %s to 1\n
MSGID_TOO_MANY_TIME Warning: too many LC_TIME optional keywords at line %d\n

