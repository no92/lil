#pragma once

#include <stdint.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + sizeof(typeof(int[1 - 2 * !!__builtin_types_compatible_p(typeof(arr), typeof(&arr[0]))])) * 0)

#define STRINGIFY(a) STRINGIFY_(a)
#define STRINGIFY_(a) #a

static inline uint32_t div_u64(uint64_t a, uint32_t b) {
	if(__builtin_constant_p(b))
		return a / b;

	uint32_t lo = a;
	uint32_t hi = a >> 32;

	// clang always picks memory for "rm" ; this is a workaround that doesn't hurt GCC
	asm ("div %2" : "+a,a" (lo), "+d,d" (hi) : "r,m" (b));

	return lo;
}
