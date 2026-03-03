#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "test.h"

_X_NORETURN _X_ATTRIBUTE_PRINTF(1,2)
void die(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);

	exit(1);
}

