#include "calc_app.h"
#include "console.h"
#include "calc.h"
#include <stdbool.h>

static void print_long(long value)
{
	char buf[32];
	int pos = 0;
	if (value == 0) {
		buf[pos++] = '0';
	} else {
		bool neg = false;
		unsigned long uv;
		if (value < 0) {
			neg = true;
			uv = -(unsigned long)value;
		} else {
			uv = (unsigned long)value;
		}
		char rev[32];
		int rp = 0;
		while (uv > 0) {
			rev[rp++] = '0' + (uv % 10);
			uv /= 10;
		}
		if (neg)
			buf[pos++] = '-';
		for (int i = rp - 1; i >= 0; --i)
			buf[pos++] = rev[i];
	}
	buf[pos] = '\0';
	console_print(buf);
}

void calc_app(const char *expression)
{
	if (!expression || expression[0] == '\0') {
		console_print("Usage: calc <expression>\n");
		return;
	}
	long result = calc_eval(expression);
	console_print("Result: ");
	print_long(result);
	console_print("\n");
}
