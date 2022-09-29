#!/usr/bin/awk -f
BEGIN {
  valid["bev_general_vector"  ] = "380"
  valid["bev_xutlbmiss_vector"] = "280"
  valid["bev_utlbmiss_vector" ] = "200"
  valid["bev_cacheerr_vector" ] = "300"
}
{
  addr = substr($1,6)
  if (valid[$3]==""){
    printf("%s is invalid\n", $3)
    exit 1
  }
  else
    if (valid[$3]!=addr){
      printf("%s is %s, should be %s\n", $3, $1, valid[$3])
    }
}
