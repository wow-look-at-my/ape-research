// Minimal Mach-O load-command editor: list / delete / append.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint32_t rd32(unsigned char *p){ uint32_t v; memcpy(&v,p,4); return v; }
static void wr32(unsigned char *p, uint32_t v){ memcpy(p,&v,4); }

int main(int argc, char **argv) {
	FILE *f = fopen(argv[2], "r+b");
	if (!f) { perror("open"); return 1; }
	fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
	unsigned char *b = malloc(sz);
	if (fread(b, 1, sz, f) != (size_t)sz) { perror("read"); return 1; }
	uint32_t ncmds = rd32(b+16), sizeofcmds = rd32(b+20);
	long off = 32;
	for (uint32_t i = 0; i < ncmds; i++) {
		uint32_t cmd = rd32(b+off), cs = rd32(b+off+4);
		char name[24]; snprintf(name, sizeof name, "%s", "");
		/* a few names for readability */
		const char *nm = "?";
		if (cmd==0x1) nm="LC_SEGMENT_64"; else if (cmd==0x2) nm="LC_SYMTAB";
		else if (cmd==0xc) nm="LC_LOAD_DYLIB"; else if (cmd==0xe) nm="LC_LOAD_DYLINKER";
		else if (cmd==0x1b) nm="LC_UUID"; else if (cmd==0x29) nm="LC_CODE_SIGNATURE";
		else if (cmd==0x32) nm="LC_BUILD_VERSION"; else if (cmd==0x80000028) nm="LC_MAIN";
		else if (cmd==0x22) nm="LC_DYLD_INFO_ONLY"; else if (cmd==0x80000034) nm="LC_DYLD_EXPORTS_TRIE";
		else if (cmd==0x80000035) nm="LC_DYLD_CHAINED_FIXUPS";
		if (strcmp(argv[1],"list")==0) printf("[%u] %-22s cmd=0x%x size=%u @%ld\n", i, nm, cmd, cs, off);
		off += cs;
	}
	if (strcmp(argv[1],"del")==0) {
		uint32_t idx = atoi(argv[3]);
		long o = 32;
		for (uint32_t i=0;i<idx;i++) o += rd32(b+o+4);
		uint32_t cs = rd32(b+o+4);
		memmove(b+o, b+o+cs, (32+sizeofcmds)-(o+cs));
		wr32(b+16, ncmds-1); wr32(b+20, sizeofcmds-cs);
		printf("deleted [%u] size=%u; ncmds=%u sizeofcmds=%u\n", idx, cs, ncmds-1, sizeofcmds-cs);
	}
	if (strcmp(argv[1],"setcmd")==0) {
		uint32_t idx = atoi(argv[3]);
		unsigned int nv; sscanf(argv[4], "%x", &nv);
		long o = 32;
		for (uint32_t i=0;i<idx;i++) o += rd32(b+o+4);
		printf("  [%u] cmd 0x%x -> 0x%x (size %u, bytes keep their place)\n",
		       idx, rd32(b+o), nv, rd32(b+o+4));
		wr32(b+o, nv);
	}
	if (strcmp(argv[1],"addhex")==0) {
		/* argv[3] = hex bytes */
		long o = 32 + sizeofcmds;
		unsigned int n = strlen(argv[3])/2;
		for (unsigned int i=0;i<n;i++){ unsigned int v; sscanf(argv[3]+2*i,"%2x",&v); b[o+i]=v; }
		wr32(b+16, ncmds+1); wr32(b+20, sizeofcmds+n);
		printf("added %u bytes at %ld; ncmds=%u sizeofcmds=%u\n", n, o, ncmds+1, sizeofcmds+n);
	}
	fseek(f, 0, SEEK_SET); fwrite(b, 1, sz, f); fclose(f);
	return 0;
}
