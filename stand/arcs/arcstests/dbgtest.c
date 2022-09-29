
func1 ()
{
	printf ("func1\n");
}

main(int argc, char **argv, char **envp)
{
	int i;

	printf ("dbgtest: &i=0x%x\n", &i);
	for (i=0; i<5; i++)
		printf ("dbgtest loop\n");
	func1 ();
	printf ("dbgtest done\n");
}
