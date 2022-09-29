/*
 * __xpg4 - defined by BB3.0.
 * 0 -> 'old' bahavior
 * 1 -> implies xpg4 bahavior
 * The intent is to permit libc to do runtime semantic alteration based on
 * this var.
 * Currently set in ABI crt
 */
int __xpg4 = 0;
