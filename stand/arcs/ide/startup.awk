BEGIN {
printf "char ide_startup[] = {\n" }
{ printf "\"%s\\n\"\n", $0 }
END {
print "\n};" }
