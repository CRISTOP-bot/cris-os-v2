#ifndef CRISOS_STRING_H
#define CRISOS_STRING_H

#include <stddef.h>

size_t kstrlen(const char *s);
int kstrcmp(const char *a, const char *b);
int kstrncmp(const char *a, const char *b, size_t n);
char *kstrcpy(char *dest, const char *src, size_t maxlen);
char *kstrcat(char *dest, const char *src, size_t maxlen);
const char *kstrstr(const char *haystack, const char *needle);
const char *kstrchr(const char *s, char c);
void *kmemset(void *s, int c, size_t n);
void *kmemcpy(void *dest, const void *src, size_t n);
int kmemcmp(const void *a, const void *b, size_t n);
void kitoa(long value, char *buf, size_t maxlen);

static inline const char *kskip_spaces(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		++s;
	return s;
}

static inline int kstreq(const char *a, const char *b)
{
	while (*a && *b)
		if (*a++ != *b++)
			return 0;
	return *a == *b;
}

#endif
