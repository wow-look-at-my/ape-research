#include <stdio.h>
#include <stdint.h>
int main(int argc, char **argv) {
	FILE *f = fopen(argv[1], "r+b");
	uint32_t magic, flags;
	fread(&magic, 4, 1, f);
	fseek(f, 0x18, SEEK_SET);
	fread(&flags, 4, 1, f);
	uint32_t nf; sscanf(argv[2], "%x", &nf);
	fseek(f, 0x18, SEEK_SET);
	fwrite(&nf, 4, 1, f);
	printf("  magic=%08x flags %08x -> %08x\n", magic, flags, nf);
	fclose(f);
	return 0;
}
