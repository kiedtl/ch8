#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>

#define MAX(V,M)     ((V)>(M)?(M):(V))
#define MIN(V,M)     ((V)<(M)?(M):(V))
#define CLAMP(V,H,L) (MIN(MAX(V,H),L))
#define CHKSUB(A,B)  (((A)-(B))>(A)?0:((A)-(B)))
#define UNUSED(VAR)  ((void) (VAR))
#define SIZEOF(ARR)  ((size_t)(sizeof(ARR)/sizeof(*(ARR))))
#define BITSET(V,B)  (((V) & (B)) == (B))

#define log(fmt, ...) fprintf(stderr, "LOG: "fmt"\n", __VA_ARGS__)

void *ecalloc(size_t nmemb, size_t size);

/* a reimplementation of assert(3) that calls die() instead of abort(3) */
#define ENSURE(EXPR) (__ensure((EXPR), #EXPR, __FILE__, __LINE__, __func__))
void __ensure(_Bool expr, char *str, char *file, size_t line, const char *fn);

void die(const char *fmt, ...);
char *format(const char *format, ...);
uint32_t hsl_to_rgb(float _h, float _s, float _l);

#endif
