#ifndef RUST_FFI_H
#define RUST_FFI_H

#include <stddef.h>
#include <stdint.h>

void rust_serial_init(void);
void rust_serial_putchar(unsigned char c);
void rust_serial_print(const unsigned char *s);
unsigned char rust_serial_getchar(void);
int rust_serial_available(void);

size_t rust_strlen(const unsigned char *s);
int rust_strcmp(const unsigned char *a, const unsigned char *b);
int rust_strncmp(const unsigned char *a, const unsigned char *b, size_t n);
unsigned char *rust_strcpy(unsigned char *dest, const unsigned char *src, size_t maxlen);
unsigned char *rust_strcat(unsigned char *dest, const unsigned char *src, size_t maxlen);
const unsigned char *rust_strstr(const unsigned char *haystack, const unsigned char *needle);
const unsigned char *rust_strchr(const unsigned char *s, unsigned char c);
void *rust_memset(void *s, int c, size_t n);
void *rust_memcpy(void *dest, const void *src, size_t n);
int rust_memcmp(const void *a, const void *b, size_t n);

void rust_itoa(long value, unsigned char *buf, size_t maxlen);
void rust_utoa(unsigned long value, unsigned char *buf, size_t maxlen);
void rust_xtoa(unsigned long value, unsigned char *buf, size_t maxlen);
int rust_atoi(const unsigned char *s);

int rust_add(int a, int b);
int rust_sub(int a, int b);
int rust_mul(int a, int b);
int rust_div(int a, int b);
unsigned long long rust_fibonacci(unsigned int n);
unsigned long long rust_factorial(unsigned int n);
int rust_is_prime(unsigned int n);
unsigned int rust_gcd(unsigned int a, unsigned int b);

unsigned int rust_rotate_left(unsigned int val, unsigned int shift);
unsigned int rust_rotate_right(unsigned int val, unsigned int shift);
unsigned int rust_popcount(unsigned long long val);
unsigned int rust_leading_zeros(unsigned long long val);
unsigned int rust_trailing_zeros(unsigned long long val);

unsigned short rust_swap_bytes16(unsigned short val);
unsigned int rust_swap_bytes32(unsigned int val);
unsigned long long rust_swap_bytes64(unsigned long long val);

void rust_info(void);
void rust_to_uppercase(unsigned char *s, unsigned long len);
void rust_to_lowercase(unsigned char *s, unsigned long len);

#endif
