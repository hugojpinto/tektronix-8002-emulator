// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
// Is a 256-byte page of the Z80 board's firmware PROM an instruction-decoding table indexed by
// the opcode? If the opcode's data lines drive the PROM's A7-A0 in some order, some of them
// inverted, then for the right wiring the stored value should tell a lot about what each
// opcode does; for any other wiring, little. This tries every wiring: 8! line orders x 256
// inversion masks = 10,321,920, and scores each by the mutual information (bits) between the
// PROM value and each opcode property measured by props.c. Unknown PROM bytes are skipped.
// usage: search TABLE PROPS BASE [THREADS]
//   TABLE: lines "ADDR VALUE" (hex; VALUE -1 for unknown), PROPS: props.c's output,
//   BASE: the page's first address (hex). Prints the best wirings per property and overall,
//   and the score distribution (the noise any wiring reaches).
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPROP 11
#define TOP 12
static const char *pname[NPROP] = { "length", "m1", "dreads", "dwrites", "io", "t_clear", "t_set", "flow", "halt", "prefix", "stack" };
static int table[0x10000], base;
static int prop[256][NPROP], pclasses[NPROP];
static int vclass[256], nv;                 // PROM values mapped to small indices
static double nlogn[257];
static int perms[40320][8], nperm;

struct hit { double score; int perm, mask, prop; };
struct res { struct hit best[NPROP][TOP], overall[TOP]; long hist[64]; };
static int nthreads;

static void insert(struct hit *h, struct hit x)
{
	if (x.score <= h[TOP - 1].score) return;
	int i = TOP - 1;
	while (i > 0 && h[i - 1].score < x.score) { h[i] = h[i - 1]; i--; }
	h[i] = x;
}

static void permute(int *a, int k)
{
	if (k == 8) { memcpy(perms[nperm++], a, sizeof(int) * 8); return; }
	for (int i = k; i < 8; i++) {
		int t = a[k]; a[k] = a[i]; a[i] = t;
		permute(a, k + 1);
		t = a[k]; a[k] = a[i]; a[i] = t;
	}
}

static void *work(void *arg)
{
	long id = (long)arg;
	struct res *r = calloc(1, sizeof *r);
	for (int p = 0; p < NPROP; p++) for (int i = 0; i < TOP; i++) r->best[p][i].score = -1;
	for (int i = 0; i < TOP; i++) r->overall[i].score = -1;
	int map[256], val[256];
	for (int pi = (int)id; pi < nperm; pi += nthreads) {
		const int *pm = perms[pi];          // opcode bit b drives address line pm[b]
		for (int op = 0; op < 256; op++) {
			int a = 0;
			for (int b = 0; b < 8; b++) if (op >> b & 1) a |= 1 << pm[b];
			map[op] = a;
		}
		for (int mask = 0; mask < 256; mask++) {
			int n = 0;
			for (int op = 0; op < 256; op++) {
				int v = table[base + (map[op] ^ mask)];
				val[op] = v < 0 ? -1 : vclass[v];
				if (v >= 0) n++;
			}
			double total = 0;
			for (int p = 0; p < NPROP; p++) {
				int joint[32][32] = {{0}}, mv[32] = {0}, mp[32] = {0};
				for (int op = 0; op < 256; op++) {
					if (val[op] < 0) continue;
					int c = prop[op][p];
					joint[val[op]][c]++; mv[val[op]]++; mp[c]++;
				}
				double s = nlogn[n];
				for (int i = 0; i < nv; i++) {
					s -= nlogn[mv[i]];
					for (int j = 0; j < pclasses[p]; j++) s += nlogn[joint[i][j]];
				}
				for (int j = 0; j < pclasses[p]; j++) s -= nlogn[mp[j]];
				double mi = s / n;
				total += mi;
				struct hit h = { mi, pi, mask, p };
				insert(r->best[p], h);
			}
			struct hit h = { total, pi, mask, -1 };
			insert(r->overall, h);
			int bin = (int)(total * 8); if (bin > 63) bin = 63;
			r->hist[bin]++;
		}
	}
	return r;
}

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: search TABLE PROPS BASE [THREADS]\n"); return 1; }
	for (int i = 0; i < 0x10000; i++) table[i] = -1;
	FILE *f = fopen(argv[1], "r");
	unsigned a; int v;
	while (fscanf(f, "%x %d", &a, &v) == 2) table[a & 0xffff] = v;
	fclose(f);
	f = fopen(argv[2], "r");
	int rawp[256][NPROP];
	for (int op = 0; op < 256; op++) {
		unsigned o; if (fscanf(f, "%x", &o) != 1) return 1;
		for (int p = 0; p < NPROP; p++) if (fscanf(f, "%d", &rawp[op][p]) != 1) return 1;
	}
	fclose(f);
	for (int p = 0; p < NPROP; p++) {          // map each property's values to 0..k-1
		int seen[1024], k = 0;
		for (int op = 0; op < 256; op++) {
			int j = 0; while (j < k && seen[j] != rawp[op][p]) j++;
			if (j == k) seen[k++] = rawp[op][p];
			prop[op][p] = j;
		}
		pclasses[p] = k;
	}
	base = (int)strtol(argv[3], 0, 16);
	int vseen[256], k = 0, known = 0;
	for (int i = 0; i < 256; i++) {
		int x = table[base + i]; if (x < 0) continue;
		known++;
		int j = 0; while (j < k && vseen[j] != x) j++;
		if (j == k) vseen[k++] = x;
		vclass[x] = j;
	}
	nv = k;
	if (nv > 32) { fprintf(stderr, "too many distinct values (%d)\n", nv); return 1; }
	for (int i = 0; i <= 256; i++) nlogn[i] = i ? i * log2((double)i) : 0;
	int id[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	permute(id, 0);
	nthreads = argc > 4 ? atoi(argv[4]) : 8;
	pthread_t th[64];
	for (long t = 0; t < nthreads; t++) pthread_create(&th[t], 0, work, (void *)t);
	struct res all; memset(&all, 0, sizeof all);
	for (int p = 0; p < NPROP; p++) for (int i = 0; i < TOP; i++) all.best[p][i].score = -1;
	for (int i = 0; i < TOP; i++) all.overall[i].score = -1;
	for (int t = 0; t < nthreads; t++) {
		struct res *r; pthread_join(th[t], (void **)&r);
		for (int p = 0; p < NPROP; p++) for (int i = 0; i < TOP; i++) insert(all.best[p], r->best[p][i]);
		for (int i = 0; i < TOP; i++) insert(all.overall, r->overall[i]);
		for (int b = 0; b < 64; b++) all.hist[b] += r->hist[b];
		free(r);
	}
	printf("page %04X: %d known entries, %d distinct values, %d wirings\n", base, known, nv, nperm * 256);
	printf("score (sum of MI over properties) distribution, bits:\n");
	long cum = 0, tot = (long)nperm * 256;
	for (int b = 0; b < 64; b++) if (all.hist[b]) {
		cum += all.hist[b];
		printf("  %5.3f-%5.3f %9ld  (cum %.6f)\n", b / 8.0, (b + 1) / 8.0, all.hist[b], cum / (double)tot);
	}
	printf("best overall:\n");
	for (int i = 0; i < 5; i++) {
		struct hit h = all.overall[i];
		printf("  %.4f  lines", h.score);
		for (int b = 0; b < 8; b++) printf(" D%d->A%d", b, perms[h.perm][b]);
		printf("  invert %02X\n", h.mask);
	}
	printf("best per property (MI bits; the property's own entropy is the ceiling):\n");
	for (int p = 0; p < NPROP; p++) {
		struct hit h = all.best[p][0], h5 = all.best[p][TOP - 1];
		printf("  %-8s %.4f (12th %.4f)  lines", pname[p], h.score, h5.score);
		for (int b = 0; b < 8; b++) printf(" %d", perms[h.perm][b]);
		printf("  invert %02X\n", h.mask);
	}
	return 0;
}
