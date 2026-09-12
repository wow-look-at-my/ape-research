#include <stdio.h>
#include <stdint.h>
#include <string.h>
int main(void) {
	unsigned char *b = (unsigned char *)0x0000000FFFFC0000ULL;
	for (long off = 0; off < 0x4000; off += 0x400) {
		volatile unsigned char c = b[off];
		printf("0x%lx: readable ('%c')\n", off, (c >= 32 && c < 127) ? c : '.');
	}
	return 0;
}
