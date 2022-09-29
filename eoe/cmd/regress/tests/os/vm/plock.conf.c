main()
{
	if (getuid() != 0) {
		printf("plock test not run.  Must be root to run this test.\n");
		exit(1);
	}

	exit(0);
}
