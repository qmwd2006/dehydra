#include <stdint.h>

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
enumt enumvar;

float &refvar = floatvar;
void *pointervar = &enumvar;
