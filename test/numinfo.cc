#include <stdint.h>

#if !defined __cplusplus
#include <float.h>
#include <stdbool.h>
#define bool _Bool
#endif

// these are "recommended" in standard, but they are in gcc and unlikely to
// disappear

int8_t int8var;
int16_t int16var;
int32_t int32var;
int64_t int64var;
uint8_t uint8var;
uint16_t uint16var;
uint32_t uint32var;
uint64_t uint64var;

bool boolvar;
float floatvar;
double doublevar;

enum enumt { a, b, c=7 };
#if !defined __cplusplus
enum
#endif
enumt enumvar;

#if defined __cplusplus
float &refvar = floatvar;
#else
float *refvar = &floatvar;
#endif

void *pointervar = &enumvar;
