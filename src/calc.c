#include "calc.h"

static const char* expr_ptr;

static void skip_spaces() { while (*expr_ptr == ' ') ++expr_ptr; }

static long parse_number() {
    skip_spaces();
    int sign = 1;
    if (*expr_ptr == '+') { ++expr_ptr; }
    else if (*expr_ptr == '-') { sign = -1; ++expr_ptr; }
    long val = 0;
    while (*expr_ptr >= '0' && *expr_ptr <= '9') { val = val*10 + (*expr_ptr - '0'); ++expr_ptr; }
    return sign * val;
}

static long parse_factor();

static long parse_term() {
    long v = parse_factor();
    while (1) {
        skip_spaces();
        if (*expr_ptr == '*') { ++expr_ptr; long r = parse_factor(); v = v * r; }
        else if (*expr_ptr == '/') { ++expr_ptr; long r = parse_factor(); if (r!=0) v = v / r; else v = 0; }
        else break;
    }
    return v;
}

static long parse_factor() {
    skip_spaces();
    if (*expr_ptr == '(') { ++expr_ptr; long v = parse_term(); skip_spaces(); if (*expr_ptr == ')') ++expr_ptr; return v; }
    return parse_number();
}

long calc_eval(const char* s) {
    expr_ptr = s;
    long v = parse_term();
    skip_spaces();
    while (*expr_ptr == '+' || *expr_ptr == '-') {
        char op = *expr_ptr; ++expr_ptr; long rhs = parse_term(); if (op=='+') v = v + rhs; else v = v - rhs;
    }
    return v;
}
