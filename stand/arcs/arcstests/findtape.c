extern char *inv_findtape();

main()
{
	char *p = inv_findtape();

	printf("tape is: %s\n", p ? p : "NULL");
}
