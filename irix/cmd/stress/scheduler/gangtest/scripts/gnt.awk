# Only sherwood routines
BEGIN { time = 0 ; number = 0; size = 0}
/^gangs/ { t += $4; i+=1; size += $3}
END { printf("%d  %f\n",i,t/i) }
