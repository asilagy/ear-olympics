/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 asilagy */
/* The resource file against the C compiler's own reading of the source headers: tests/run_tests.sh builds this with the SOURCE headers
 * (app/res/eo_*.h, the arrays as C reads them) and checks that app/gen_res.py packed every array, byte for byte, where its map
 * (eo_res_map.h) says, 4-byte aligned, in order, with nothing else in the gaps.
 *   cc -std=c99 -Wall -Wextra -Werror -Iapp/build/gen -o app/build/host/res_layout tests/res/res_layout.c
 *   app/build/host/res_layout app/build/gen/eo_res.bin */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../app/res/eo_cast.h"
#include "../../app/res/eo_intro.h"
#include "../../app/res/eo_title.h"
#include "../../app/res/eo_dog.h"
#include "eo_res_map.h"

static const uint8_t *bin;
static size_t bin_n, cursor;           /* the end of the last array checked */
static int fails;

static void check(const char *name, uint32_t off, const void *src, size_t n)
{
    int ok = 1;
    if (off % 4u) {
        printf("FAIL %s: offset %u is not 4-byte aligned\n", name, off);
        ok = 0;
    }
    if (off < cursor) {
        printf("FAIL %s: offset %u overlaps the one before (it ends at %zu)\n", name, off, cursor);
        ok = 0;
    }
    if (off + n > bin_n) {
        printf("FAIL %s: ends at %zu, past the file's %zu bytes\n", name, off + n, bin_n);
        fails++;
        return;
    }
    for (size_t i = cursor; ok && i < off && i < bin_n; i++)
        if (bin[i]) {
            printf("FAIL %s: byte %zu in the gap before it is not zero\n", name, i);
            ok = 0;
        }
    if (ok && memcmp(bin + off, src, n)) {
        size_t i = 0;
        while (bin[off + i] == ((const uint8_t *)src)[i])
            i++;
        printf("FAIL %s: differs from the header's array at byte %zu\n", name, i);
        ok = 0;
    }
    if (ok)
        printf("  ok %-10s at %7u, %6zu bytes\n", name, off, n);
    fails += !ok;
    cursor = off + n;
}

int main(int argc, char **argv)
{
    FILE *f;
    uint16_t one = 1;
    if (argc != 2 || !(f = fopen(argv[1], "rb"))) {
        printf("usage: res_layout eo_res.bin\n");
        return 2;
    }
    if (*(const uint8_t *)&one != 1) {          /* the packer writes little-endian; so does the FM-1 */
        printf("this check needs a little-endian computer\n");
        return 2;
    }
    {
        uint8_t *p = malloc(1 << 20);
        bin_n = fread(p, 1, 1 << 20, f);
        bin = p;
        fclose(f);
    }
    check("CAST_PAL", EO_RES_OFF_CAST_PAL, EO_CAST_PAL, sizeof EO_CAST_PAL);
    check("CAST", EO_RES_OFF_CAST, EO_CAST, sizeof EO_CAST);
    check("INTRO", EO_RES_OFF_INTRO, EO_INTRO, sizeof EO_INTRO);
    check("PRES_PAL", EO_RES_OFF_PRES_PAL, EO_PRES_PAL, sizeof EO_PRES_PAL);
    check("PRES_ROW", EO_RES_OFF_PRES_ROW, EO_PRES_ROW, sizeof EO_PRES_ROW);
    check("PRES_RLE", EO_RES_OFF_PRES_RLE, EO_PRES_RLE, sizeof EO_PRES_RLE);
    check("TITLE_PAL", EO_RES_OFF_TITLE_PAL, EO_TITLE_PAL, sizeof EO_TITLE_PAL);
    check("TITLE_ROW", EO_RES_OFF_TITLE_ROW, EO_TITLE_ROW, sizeof EO_TITLE_ROW);
    check("TITLE_RLE", EO_RES_OFF_TITLE_RLE, EO_TITLE_RLE, sizeof EO_TITLE_RLE);
    check("DOG", EO_RES_OFF_DOG, EO_DOG, sizeof EO_DOG);
    if (EO_RES_LEN != bin_n || (EO_RES_LEN - cursor) >= 4u) {
        printf("FAIL the file is %zu bytes, the map says %u, the arrays end at %zu\n", bin_n, EO_RES_LEN, cursor);
        fails++;
    }
    printf(fails ? "RES LAYOUT FAILED\n" : "the resource file holds every array, where the map says\n");
    return fails ? 1 : 0;
}
