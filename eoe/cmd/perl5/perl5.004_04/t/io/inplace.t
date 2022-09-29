#!./perl

$^I = '.bak';

# $RCSfile: inplace.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:29 $

print "1..2\n";

@ARGV = ('.a','.b','.c');
if ($^O eq 'MSWin32') {
  $CAT = '.\perl -e "print<>"';
  `.\\perl -le "print 'foo'" > .a`;
  `.\\perl -le "print 'foo'" > .b`;
  `.\\perl -le "print 'foo'" > .c`;
}
else {
  $CAT = 'cat';
  `echo foo | tee .a .b .c`;
}
while (<>) {
    s/foo/bar/;
}
continue {
    print;
}

if (`$CAT .a .b .c` eq "bar\nbar\nbar\n") {print "ok 1\n";} else {print "not ok 1\n";}
if (`$CAT .a.bak .b.bak .c.bak` eq "foo\nfoo\nfoo\n") {print "ok 2\n";} else {print "not ok 2\n";}

unlink '.a', '.b', '.c', '.a.bak', '.b.bak', '.c.bak';
