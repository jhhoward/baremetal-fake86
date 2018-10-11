#include "types.h"

void copymem(void *dest, const void *src, uint32_t n)
{
	const char *csrc = (const char *)src;
	char *cdest = (char *)dest;

	while (n--)
		*cdest++ = *csrc++;
}

void setmem(void* dest, int value, uint32_t n)
{
	char *cdest = (char *)dest;

	while (n--)
		*cdest++ = (char)(value);
}
