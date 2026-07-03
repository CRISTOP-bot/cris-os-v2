#include "kstring.h"

size_t kstrlen(const char *s)
{
	size_t len = 0;
	while (s[len])
		++len;
	return len;
}

int kstrcmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b)
			return (unsigned char)*a - (unsigned char)*b;
		++a;
		++b;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n)
{
	for (size_t i = 0; i < n; ++i) {
		if (a[i] != b[i])
			return (unsigned char)a[i] - (unsigned char)b[i];
		if (!a[i])
			return 0;
	}
	return 0;
}

char *kstrcpy(char *dest, const char *src, size_t maxlen)
{
	size_t i = 0;
	while (i + 1 < maxlen && src[i]) {
		dest[i] = src[i];
		++i;
	}
	if (maxlen > 0)
		dest[i] = '\0';
	return dest;
}

char *kstrcat(char *dest, const char *src, size_t maxlen)
{
	size_t i = 0;
	while (i < maxlen && dest[i])
		++i;
	size_t j = 0;
	while (i + j < maxlen - 1 && src[j]) {
		dest[i + j] = src[j];
		++j;
	}
	if (maxlen > 0)
		dest[i + j] = '\0';
	return dest;
}

const char *kstrstr(const char *haystack, const char *needle)
{
	if (!*needle)
		return haystack;
	while (*haystack) {
		const char *h = haystack;
		const char *n = needle;
		while (*h && *n && *h == *n) {
			++h;
			++n;
		}
		if (!*n)
			return haystack;
		++haystack;
	}
	return 0;
}

const char *kstrchr(const char *s, char c)
{
	while (*s) {
		if (*s == c)
			return s;
		++s;
	}
	return 0;
}

void *kmemset(void *s, int c, size_t n)
{
	unsigned char *p = (unsigned char *)s;
	while (n--)
		*p++ = (unsigned char)c;
	return s;
}

void *kmemcpy(void *dest, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;
	while (n--)
		*d++ = *s++;
	return dest;
}

int kmemcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *pa = (const unsigned char *)a;
	const unsigned char *pb = (const unsigned char *)b;
	for (size_t i = 0; i < n; ++i) {
		if (pa[i] != pb[i])
			return (int)pa[i] - (int)pb[i];
	}
	return 0;
}

void kitoa(long value, char *buf, size_t maxlen)
{
	if (maxlen == 0)
		return;
	size_t pos = 0;
	if (value == 0) {
		if (pos + 1 < maxlen)
			buf[pos++] = '0';
	} else {
		int neg = 0;
		unsigned long uv;
		if (value < 0) {
			neg = 1;
			uv = -(unsigned long)value;
		} else {
			uv = (unsigned long)value;
		}
		char rev[32];
		size_t rp = 0;
		while (uv > 0 && rp < sizeof(rev)) {
			rev[rp++] = (char)('0' + (uv % 10));
			uv /= 10;
		}
		if (neg && pos + 1 < maxlen)
			buf[pos++] = '-';
		while (rp > 0 && pos + 1 < maxlen)
			buf[pos++] = rev[--rp];
	}
	buf[pos] = '\0';
}
