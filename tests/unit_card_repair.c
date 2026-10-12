// SPDX-License-Identifier: MPL-2.0
/* sigil_card_repair on cards built here, so it runs without the save corpus:
 * a VMU data save whose FAT chain was lost is rebuilt where the BIOS put it
 * and confirmed by its VMS header CRC, leaving the save beside it alone; a
 * GameCube card's damaged directory copy is restored from the whole one. */
#include "card_dreamcast.h"
#include "card_gamecube.h"
#include "mem_root.h"
#include <stdio.h>

static int g_fails = 0;

static void fail(const char *where, const char *what) {
    fprintf(stderr, "FAIL %s: %s\n", where, what);
    g_fails++;
}

/* The VMS header CRC as mc.pp.se/dc/vms/fileheader.html gives it, written
 * out here so the test doesn't check sigil against itself. */
static uint16_t vms_crc(const uint8_t *buf, size_t size) {
    unsigned n = 0;
    for (size_t i = 0; i < size; i++) {
        n ^= (unsigned)buf[i] << 8;
        for (int c = 0; c < 8; c++) n = n & 0x8000 ? (n << 1) ^ 4129 : n << 1;
    }
    return (uint16_t)(n & 0xFFFF);
}

/* A data save of `blocks` blocks in .dci form, with a VMS header whose CRC
 * covers the header and data. */
static uint8_t *make_dci(const char *name, uint32_t blocks, uint8_t fill, size_t *len) {
    size_t data_len = (size_t)blocks * VMU_BLOCK_SIZE;
    uint8_t *file = (uint8_t *)calloc(1, data_len);
    memcpy(file, "TEST SAVE       ", 16);
    for (size_t i = 0x80; i < data_len; i++) file[i] = (uint8_t)(fill + i * 7);
    uint32_t payload = (uint32_t)(data_len - 0x80);
    file[0x48] = (uint8_t)payload;
    file[0x49] = (uint8_t)(payload >> 8);
    uint16_t crc = vms_crc(file, data_len);
    file[0x46] = (uint8_t)crc;
    file[0x47] = (uint8_t)(crc >> 8);

    *len = sigil_dreamcast_dci_size(blocks);
    uint8_t *dci = (uint8_t *)calloc(1, *len);
    dci[0] = 0x33;
    memcpy(dci + 4, name, strlen(name));
    dci[0x18] = (uint8_t)blocks;
    for (size_t i = 0; i < data_len; i += 4) {
        for (size_t k = 0; k < 4; k++) dci[VMU_DIR_ENTRY_SIZE + i + k] = file[i + 3 - k];
    }
    free(file);
    return dci;
}

static sigil_card_repair_report *repair_of(const uint8_t *image, size_t len) {
    sigil_io *io = mem_root_io(image, len);
    sigil_card_repair_report *r = NULL;
    int rc = sigil_card_repair(io, &r);
    sigil_io_close(io);
    return rc == SIGIL_OK ? r : NULL;
}

#define VMU_FAT (254u * VMU_BLOCK_SIZE)

static void check_vmu(void) {
    uint8_t *image = (uint8_t *)malloc(VMU_CARD_SIZE);
    size_t keep_len = 0, lost_len = 0;
    uint8_t *keep = make_dci("KEEP________", 2, 0x11, &keep_len);
    uint8_t *lost = make_dci("LOST________", 3, 0x55, &lost_len);
    sigil_dreamcast_format(image);
    if (sigil_dreamcast_inject(image, keep, keep_len) != SIGIL_OK || sigil_dreamcast_inject(image, lost, lost_len) != SIGIL_OK) {
        fail("vmu", "setup failed");
        goto done;
    }
    /* KEEP takes 199 and 198, LOST 197 down to 195. Lose LOST's chain. */
    for (uint32_t b = 195; b <= 197; b++) {
        image[VMU_FAT + 2 * b] = 0xFC;
        image[VMU_FAT + 2 * b + 1] = 0xFF;
    }
    sigil_card_listing *l = NULL;
    sigil_card_repair_report *r = repair_of(image, VMU_CARD_SIZE);
    if (!r || r->entry_count != 1 || strcmp(r->entries[0].name, "LOST________") != 0 || !r->entries[0].repaired ||
        r->entries[0].damage != SIGIL_CARD_DAMAGE_CHAIN_UNWRITTEN || r->entries[0].first_block != 197 || !r->data) {
        fail("vmu", "the lost chain wasn't reported repaired");
    } else if (sigil_dreamcast_card_list(r->data, &l) != SIGIL_OK || l->entry_count != 2 || l->corrupt_count != 0 ||
               sigil_dreamcast_verify(r->data, lost, lost_len) != SIGIL_OK ||
               sigil_dreamcast_verify(r->data, keep, keep_len) != SIGIL_OK) {
        fail("vmu", "the repaired card doesn't hold both saves whole");
    }
    sigil_card_listing_free(l);
    sigil_card_repair_report_free(r);

    /* A byte of LOST's data changed: the CRC refuses the rebuilt chain. */
    image[196u * VMU_BLOCK_SIZE + 9] ^= 0x01;
    r = repair_of(image, VMU_CARD_SIZE);
    if (!r || r->entry_count != 1 || r->entries[0].repaired || r->entries[0].damage != SIGIL_CARD_DAMAGE_CHECKSUM_MISMATCH || r->data) {
        fail("vmu", "a save failing its CRC was repaired");
    }
    sigil_card_repair_report_free(r);
done:
    free(lost);
    free(keep);
    free(image);
}

#define GC_CARD (64u * GC_BLOCK_SIZE)

static void check_gamecube(void) {
    uint8_t *image = (uint8_t *)malloc(GC_CARD);
    if (sigil_gamecube_format(image, GC_CARD, false) != SIGIL_OK) {
        fail("gamecube", "setup failed");
        free(image);
        return;
    }
    sigil_card_repair_report *r = repair_of(image, GC_CARD);
    if (!r || r->entry_count != 0 || r->data) fail("gamecube", "a whole card reported damage");
    sigil_card_repair_report_free(r);

    image[2u * GC_BLOCK_SIZE + 0x20] ^= 0x01;
    r = repair_of(image, GC_CARD);
    if (!r || r->entry_count != 1 || r->entries[0].damage != SIGIL_CARD_DAMAGE_COPY_DAMAGED || !r->entries[0].repaired ||
        r->entries[0].first_block != 2 || !r->data ||
        memcmp(r->data + 2u * GC_BLOCK_SIZE, r->data + GC_BLOCK_SIZE, GC_BLOCK_SIZE) != 0) {
        fail("gamecube", "the damaged directory copy wasn't restored from the whole one");
    }
    sigil_card_repair_report_free(r);
    free(image);
}

int main(void) {
    check_vmu();
    check_gamecube();
    printf("card repair: %d failures\n", g_fails);
    return g_fails ? 1 : 0;
}
