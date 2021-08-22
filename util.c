#include <ctype.h>
#include <execinfo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>

#include "util.h"

void *
ecalloc(size_t nmemb, size_t size)
{
	void *m;
	if (!(m = calloc(nmemb, size)))
		die("Could not allocate %zu bytes:", nmemb * size);
	return m;
}

void
__ensure(_Bool expr, char *str, char *file, size_t line, const char *fn)
{
	if (expr) return;
	die("Assertion `%s' failed (%s:%s:%zu).", str, file, fn, line);
}

_Noreturn void __attribute__((format(printf, 1, 2)))
die(const char *fmt, ...)
{
	//ui_shutdown();

	fprintf(stderr, "fatal: ");

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		perror(" ");
	} else {
		fputc('\n', stderr);
	}

	char *buf_sz_str = getenv("MEBS_DEBUG");

	if (buf_sz_str == NULL) {
		fprintf(stderr, "NOTE: set $MEBS_DEBUG >0 for a backtrace.\n");
	} else {
		size_t buf_sz = strtol(buf_sz_str, NULL, 10);
		void *buffer[buf_sz];

		int nptrs = backtrace(buffer, buf_sz);
		char **strings = backtrace_symbols(buffer, nptrs);

		if (!strings) {
			/* oopsie daisy, what went wrong here */
			fprintf(stderr, "Unable to provide backtrace.");
			_Exit(EXIT_FAILURE);
		}

		fprintf(stderr, "backtrace:\n");
		for (size_t i = 0; i < (size_t)nptrs; ++i)
			fprintf(stderr, "   %s\n", strings[i]);
		free(strings);
	}

	_Exit(EXIT_FAILURE);
}

char * __attribute__((format(printf, 1, 2)))
format(const char *fmt, ...)
{
	static char buf[8192];
	memset(buf, 0x0, sizeof(buf));
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	ENSURE((size_t) len < sizeof(buf));
	return (char *) &buf;
}
