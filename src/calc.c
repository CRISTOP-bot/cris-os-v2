#include "calc.h"

#ifndef LONG_MAX
#if __WORDSIZE == 64
#define LONG_MAX 9223372036854775807L
#else
#define LONG_MAX 2147483647L
#endif
#endif
#ifndef LONG_MIN
#if __WORDSIZE == 64
#define LONG_MIN (-9223372036854775807L - 1)
#else
#define LONG_MIN (-2147483647L - 1)
#endif
#endif

static const char* expr_ptr;

static void skip_spaces(void) { while (*expr_ptr == ' ') ++expr_ptr; }

static long parse_number(void) {
    skip_spaces();
    int sign = 1;
    if (*expr_ptr == '+') { ++expr_ptr; }
    else if (*expr_ptr == '-') { sign = -1; ++expr_ptr; }
    long val = 0;
    while (*expr_ptr >= '0' && *expr_ptr <= '9') {
        long digit = *expr_ptr - '0';
        if (val > (LONG_MAX - digit) / 10) {
            val = sign == 1 ? LONG_MAX : LONG_MIN;
            while (*expr_ptr >= '0' && *expr_ptr <= '9') ++expr_ptr;
            break;
        }
        val = val * 10 + digit;
        ++expr_ptr;
    }
    return sign * val;
}

static long parse_factor(void);

static long parse_term(void) {
    long v = parse_factor();
    while (1) {
        skip_spaces();
        if (*expr_ptr == '*') { ++expr_ptr; long r = parse_factor(); v = v * r; }
        else if (*expr_ptr == '/') { ++expr_ptr; long r = parse_factor(); if (r!=0) v = v / r; else v = 0; }
        else break;
    }
    return v;
}

static long calc_expr(void);

static long parse_factor(void) {
    skip_spaces();
    if (*expr_ptr == '(') {
        ++expr_ptr;
        long v = calc_expr();
        skip_spaces();
        if (*expr_ptr == ')') ++expr_ptr;
        return v;
    }
    return parse_number();
}

static long calc_expr(void) {
    long v = parse_term();
    skip_spaces();
    while (*expr_ptr == '+' || *expr_ptr == '-') {
        char op = *expr_ptr;
        ++expr_ptr;
        long rhs = parse_term();
        if (op == '+') v = v + rhs;
        else v = v - rhs;
        skip_spaces();
    }
    return v;
}

long calc_eval(const char* s) {
    expr_ptr = s;
    return calc_expr();
}
