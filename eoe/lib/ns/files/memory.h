#pragma once

void m_release(void *);
void *m_arena(void);
char *m_strdup(char *, size_t, void *);
void *m_malloc(size_t, void *);
void *m_calloc(int, size_t, void *);
void *m_realloc(void *, size_t, void *);
void m_free(void *, void *);
