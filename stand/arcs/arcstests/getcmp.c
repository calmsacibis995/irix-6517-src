main(int argc, char **argv)
{
	void *c = (void *)GetComponent(argv[1]);
	printf("component: %s\n",argv[1]);
	if (c)
		prcomponent(c,0,0);
	else
		printf("invalid\n");
}
