#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/coff32.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#include <sys/types.h>
#include <filehdr.h>
#include <fcntl.h>
#include <stdio.h>
#include <sym.h>
#include <symconst.h>
#include <syms.h>
#include <cmplrs/stsupport.h>
#include <elf_abi.h>
#include <elf_mips.h>
#include <libelf.h>
#include <sex.h>
#include <ldfcn.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"
#include "eval.h"
#include "libmdebug.h"

/* Mdebug specific external
 */
extern mdebugtab_t     *mdp;

/*
 * sym_defs_and_funcs32() -- Grab all local symbols, procedures, etc., as
 *                           you go through each file descriptor.  Store all
 *                           of this information depending on which
 *                           structure you want to put it in.
 */
static void
sym_defs_and_funcs32(Elf *elfp, symtab_t *stp)
{
#ifdef XXX
	pCFDR pcfd;
	pAUXU paux;
	pSYMR ps, ps2;
	long  isym, ifd, j;
	symdef_t *ndp = (symdef_t *)NULL, *ndp2 = (symdef_t *)NULL;

	/*
	 * Now loop through all of the file descriptors, and go through each
	 * symbol.  If we happen to find a structure in the file as one of the
	 * symbols, then go through the process of storing that symbol.  We
	 * then store that symbol on the symbol list.
	 */
	for (ifd = 0; ifd < _md_st_ifdmax(); ifd++) {
		_md_st_setfd(ifd);
		pcfd = st_pcfd_ifd(ifd);
		for (isym = 0; isym < pcfd->pfd->csym; isym++) {
			ps = &pcfd->psym[isym];
			if ((ps->st == stStruct) || (ps->st == stUnion) ||
				(ps->st == stProc) || (ps->st == stStaticProc) ||
				(ps->st == stMemberProc)) {
				if (!(ndp = (symdef_t *)malloc(sizeof(symdef_t)))) {
					fatal("Cannot create symdef (sym_defs32())!\n");
				}
				ndp->s_name = (char *)st_str_iss(ps->iss);
				ndp->s_ifd = ifd;
				ndp->s_size = ps->value;
				ndp->s_sym = isym;
				ndp->s_member = (symdef_t *)NULL;
				ndp->s_next = stp->st_list;
				stp->st_list = ndp;

				if ((ps->st == stProc) || (ps->st == stStaticProc) ||
					(ps->st == stMemberProc)) {
					paux = st_paux_iaux(ps->index);
					ps2 = &pcfd->psym[paux->isym - 1];
					slist2->s_next = (symaddr_t *)malloc(sizeof(symaddr_t));
					slist2->s_next->s_lowpc = (unsigned)ps->value;
					slist2->s_next->s_highpc =
						(unsigned)ps->value + (unsigned)ps2->value -
						sizeof(Elf32_Addr);
					slist2->s_next->s_name =
						(char *)malloc(strlen((char *)st_str_iss(ps->iss))+1);
					sprintf(slist2->s_next->s_name, "%s", st_str_iss(ps->iss));
					slist2->s_next->s_next = (symaddr_t *)NULL;
					slist2 = slist2->s_next;
					func_cnt++;
					sym_cnt++;
				}

				if ((ps->st == stStruct) || (ps->st == stUnion)) {
					for (j = isym + 1; j < ps->index; j++) {
						ps2 = &pcfd->psym[j];
						if ((ps2->st == stStruct) || (ps2->st == stUnion) ||
							(ps2->st == stEnum) || (ps2->st == stBlock)) {
								j = ps2->index - 1;
						} else if (ps2->st == stMember) {
							ndp2 = (symdef_t *)malloc(sizeof(symdef_t));
							ndp2->s_member = (symdef_t *)NULL;
							ndp2->s_name = (char *)st_str_iss(ps2->iss);
							ndp2->s_ifd = ifd;
							ndp2->s_offset = (ps2->value / 8);
							ndp2->s_size = -1;
							ndp2->s_sym = j;
							if (!(ndp->s_member)) {
								ndp->s_member = ndp2;
							} else {
								ndp->s_member->s_member = ndp2;
								ndp = ndp->s_member;
							}
						}
					}
					isym = ps->index - 1;
				}
			}
		}
	}
#endif
	return;
}

/*
 * sym_addrs32()
 */
static void
sym_addrs32(Elf *elfp, symtab_t *stp, mdebugtab_t *md)
{
#ifdef XXX
	Elf_Scn *esp;
	Elf32_Ehdr *ehdrp;
	Elf32_Shdr *eshdrp;
	Elf_Data *edp;
	int stabno, dstabno;
	Elf32_Sym *sp;
	int i, cnt, ndx; 

	/* now pull out the symbol table information */
	if( (ehdrp = elf32_getehdr(elfp)) == NULL )
		return;

	stabno = dstabno = 0;

	for( i = 0 ; i < ehdrp->e_shnum ; i++ ) {
		/* get a section */
		if( (esp = elf_getscn(elfp,i)) == NULL ) {
			printf("unable to get section %d\n",i);
			continue;
		}

		if( (eshdrp = elf32_getshdr(esp)) == NULL ) {
			printf("unable to get section header %d\n",i);
			continue;
		}

		if( eshdrp->sh_type == SHT_SYMTAB )
			stabno = i;

		if( eshdrp->sh_type == SHT_DYNSYM )
			dstabno = i;
	}

	if( stabno == 0 )
		stabno = dstabno;

	if( stabno == 0 )
		return;

	/* pull up the symbol table */
	if( (esp = elf_getscn(elfp,stabno)) == NULL )
		return;

	if( (eshdrp = elf32_getshdr(esp)) == NULL )
		return;

	if( (edp = elf_getdata(esp, NULL)) == NULL )
		return;

	ndx = eshdrp->sh_link;
	cnt = edp->d_size / sizeof(*sp);
	sp = (Elf32_Sym *)edp->d_buf;

	sp++;
	cnt--;

	/*
	 * Finally, put them into the main list.
	 */
	for (i = 0; i < cnt; i++, sp++) {
		if (ELF32_ST_TYPE(sp->st_info) != STT_FUNC) {
			slist2->s_next = (symaddr_t *)malloc(sizeof(symaddr_t));
			slist2->s_next->s_lowpc = (unsigned)sp->st_value;
			slist2->s_next->s_highpc = (unsigned)0;
			slist2->s_next->s_name = strdup(elf_strptr(elfp,ndx,sp->st_name));
			slist2->s_next->s_next = (symaddr_t *)NULL;
			slist2 = slist2->s_next;
			addr_cnt++;
			sym_cnt++;
		}
	}
#endif
}

/*
 * sym_init32() -- Initialize 32-bit information.
 */
int
sym_init32()
{
#ifdef XXX
	Elf32_Ehdr ehdr;
	Elf32_Shdr secthdr;
	struct filehdr hdr;
	char *cp, *offset, *strtab, *myname, objsex, hostsex;
	int st_lang, st_merge, fn, swap_needed;

	myname = program;
	
	if ((fn = open (namelist, O_RDONLY, 0)) < 0) {
        fatal("Cannot open namelist in header initialization!\n");
    }

    if (read(fn, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		fatal("Cannot read namelist from header!\n");
    }

	switch (hdr.f_magic) {
		case SMIPSEBMAGIC:
		case SMIPSEBMAGIC_2:
		case SMIPSEBMAGIC_3:
		case SMIPSELMAGIC:
		case SMIPSELMAGIC_2:
		case SMIPSELMAGIC_3:
			swap_filehdr (&hdr, gethostsex());
		case MIPSEBMAGIC:
		case MIPSEBMAGIC_2:
		case MIPSEBMAGIC_3:
		case MIPSELMAGIC:
		case MIPSELMAGIC_2:
		case MIPSELMAGIC_3:
		case MIPSEBUMAGIC:
		case MIPSELUMAGIC:
			if (!hdr.f_symptr || !hdr.f_nsyms) {
				fprintf(KL_ERRORFP, "error: no symbol table, exiting\n");
				exit (1);
			} else {
				lseek(fn, hdr.f_symptr, 0);
				if (st_readst(fn, 'r', 0, 0, -1)) {
					fprintf(KL_ERRORFP,
						"error: corrupted binary will try to dump\n");
				}
				break;
			}
			/* FALL THROUGH */
		default:
			lseek(fn, 0, SEEK_SET);
			if (read(fn, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
				perror(myname);
				exit (1);
			}
			if (IS_ELF(ehdr)) {
				objsex = *((char *)&ehdr + 5);
				hostsex = gethostsex();
				swap_needed =
					!((hostsex == BIGENDIAN && objsex == ELFDATA2MSB) ||
					(hostsex == LITTLEENDIAN && objsex == ELFDATA2LSB));
				if (swap_needed) {
					swap_ehdr(&ehdr);
				}
				offset = (char *)ehdr.e_shoff;
				lseek(fn, offset + (ehdr.e_shstrndx * ehdr.e_shentsize),
					SEEK_SET);
				if (read(fn, &secthdr, sizeof(secthdr)) !=
					sizeof(secthdr)) {
						perror(myname);
						exit (1);
				}
				if (swap_needed) {
					swap_shdr(&secthdr);
				}
				strtab = (char *)malloc(secthdr.sh_size);
				lseek(fn, secthdr.sh_offset, SEEK_SET);
				if (read(fn,strtab,secthdr.sh_size) != secthdr.sh_size) {
					perror(myname);
					exit (1);
				}
				while (ehdr.e_shnum--) {
					lseek(fn, offset, SEEK_SET);
					if (read(fn, &secthdr, sizeof(secthdr)) !=
						sizeof(secthdr)) {
							perror(myname);
							exit (1);
					}
					if (swap_needed) {
						swap_shdr(&secthdr);
					}
					if (!strcmp(strtab + secthdr.sh_name, MIPS_MDEBUG)) {
						lseek(fn, secthdr.sh_offset, SEEK_SET);
						if (st_readst(fn, 'r', 0, 0, -1)) {
							fprintf(ERRORFP(K), "error: corrupted binary?\n");
						}
						return (1);
					}
					offset += ehdr.e_shentsize;
				}
				fprintf(ERRORFP(K), "error: no symbol table, exiting\n");
				exit (1);
			}
			if (st_readbinary(myname, 'r')) {
				fprintf(ERRORFP(K), "error: corrupted binary?\n");
			}
	}
#endif
	return (1);
}

/*
 * sym_fixmember32() -- Fix symdef_t member items by going through every
 *                      member and making sure the offset is set right.
 */
void
sym_fixmember32(symtab_t *stp)
{
#ifdef XXX
	long tsize = 0;
	symdef_t *np, *np2, *npt = (symdef_t *)NULL;

	for (np = stp->st_list; np; np = np->s_next) {
		if (np->s_member != (symdef_t *)NULL) {
			np2 = np->s_member;
			while (np2) {
				if (npt) {
					np2->s_size = np2->s_offset - npt->s_offset;
				}
				npt = np2;
				np2 = np2->s_member;
			}
			if (!np->s_member->s_member) {
				np->s_member->s_size = np->s_size;
			} else {
				np->s_member->s_size = np->s_member->s_member->s_size;
			}
		}
	}
#endif
}

/*
 * sym_defload32() -- Load symbols for a 32-bit machine.
 */
symtab_t *
sym_defload32(Elf *elfp)
{
#ifdef XXX
	int i;
	symtab_t *stp;

	if ((stp = (symtab_t *)malloc(sizeof(symtab_t))) == (symtab_t *)NULL) {
		return ((symtab_t *)NULL);
	}

	stp->st_flags = ST_32BIT;
	stp->symfunc_ptr = (symaddr_t *)NULL;
	stp->symaddr_ptr = (symaddr_t *)NULL;

	/*
	 * Initialize the top of the symaddr list.
	 */
	sym_cnt = 1;
	func_cnt = 0;
	addr_cnt = 0;
	slist = (symaddr_t *)malloc(sizeof(symaddr_t));
	slist->s_highpc = slist->s_lowpc = -1;
	slist->s_name = (char *)NULL;
	slist->s_next = (symaddr_t *)NULL;
	slist2 = slist;

	/*
	 * Initialize the hash addresses.
	 */
	for (i = 0; i < SYM_SLOTS; i++) {
		hash_addrs[i] = (symaddr_t *)0;
	}

	/*
	 * Initialize the symbol table.
	 */
	sym_init32();

	/*
	 * Initialize the MDEBUG table bits of information.  This will be
	 * the future focal point for ALL symbol information.  There will
	 * be no need to look elsewhere for symbol data.
	 */
	mdp = md_init();

	/*
	 * Initialize the definitions, addresses and functions.
	 * The functions and addresses will be on the same list.
	 */
	sym_defs_and_funcs32(elfp, stp);
	sym_addrs32(elfp, stp, mdp);

	/*
	 * Now that we have our list, sort it appropriately.
	 */
	sym_sort_list(stp);
	sym_fixmember32(stp);

#endif
	return (stp);
}

int
get_lineno32(kaddr_t pc)
{
#ifdef XXX
	pPDR ppd;
	pSYMR ps;
	char *ptr;
	pCFDR pcfd;
	long ln, ipd;
	md_type_t *procp;

	if (procp = md_lkup_type_proc((char *)NULL, (unsigned long)pc)) {
		_md_st_setfd(procp->m_ifd);
		pcfd = st_pcfd_ifd(procp->m_ifd);
		for (ipd = 0, ppd = pcfd->ppd; ipd < pcfd->pfd->cpd; ipd++, ppd++) {
			if (ppd->isym == isymNil) {
				continue;
			} else if (pcfd->pfd->csym) {
				ps = &pcfd->psym[ppd->isym];
				ptr = (char *)st_str_iss(ps->iss);
			} else {
				ptr = (char *)0;
			}
			if ((ptr) && (!strcmp(ptr, procp->m_name))) {
				ln = pcfd->pline[ppd->iline+((pc - ps->value)/sizeof(long))];
				return (ln);
			}
		}
		return (-1);
	}
#endif
	return(0);
}

/*
 * get_srcfile32() -- Get the name of the source file for a given
 *                    address.
 */
char *
get_srcfile32(kaddr_t pc)
{
#ifdef XXX
	kaddr_t pcnew;
	symdef_t *symptr;
	char *name, *fname, *fret, *st_str_ifd_iss();
	pCFDR pcfd;
	md_type_t *procp;

	if (procp = md_lkup_type_proc((char *)NULL, (unsigned long)pc)) {
		_md_st_setfd(procp->m_ifd);
		pcfd = st_pcfd_ifd(procp->m_ifd);
		if (!(fname = st_str_ifd_iss(procp->m_ifd, pcfd->pfd->rss))) {
			return ((char *)0);
		}
		fret = (char *)alloc_block(strlen(fname) + 1, B_TEMP);
		strcpy(fret, fname);
		return (fret);
	}
#endif
	return ((char *)0);
}

/*
 * sym_base_value() -- Get symbol base value at an offset in a pointer.
 */
int
sym_base_value(k_ptr_t ptr, symdef_t *sp, k_uint_t *v)
{
#ifdef XXX
	k_uint_t val, mask;
	long iaux, j, k;
	pCFDR pcfd, st_pcfd_ifd();
	pSYMR symr;
	pAUXU paux, st_paux_iaux();
	TIR ti;

	_md_st_setfd(sp->s_ifd);
	
	pcfd = st_pcfd_ifd(sp->s_ifd);
	symr = &pcfd->psym[sp->s_sym];
	iaux = symr->index;
	paux = st_paux_iaux(iaux);
	ti   = paux->ti;

	if (ti.fBitfield == 1) {
		iaux++;
		paux = st_paux_iaux(iaux);
		for (j = 1; j < 32; j *= 2) {
			k = j * 8;
			if (paux->width <= k) {
				bcopy(ptr, &val, sizeof(k_uint_t));
				val = (val << symr->value) >> (sizeof(val) * 8 - paux->width);
				if (DEBUG(K, DC_SYM, 1)) {
					fprintf(ERRORFP(K),
						"sym_base_value: (bit) val=0x%llx, byte_size=%d,"
						" bit_size=%d, bit_offset=%d, name=%s\n",
						val, j, paux->width, symr->value,
						st_str_iss(symr->iss));
				}
				break;
			}
		}
	} else {
		switch (sp->s_size) {
			case 1:
				val = *(unsigned char *)ptr;
				break;
			case 2:
				val = *(short *)ptr;
				break;
			case 4:
				val = *(unsigned long *)ptr;
				break;
			case 8:
				val = *(k_uint_t *)ptr;
				break;
			default:
				break;
		}
		fprintf(ERRORFP(K), "sym_base_value: (not bit) val=0x%llx\n", val);
	}
	*v = val;
#endif
	return (0);
}
