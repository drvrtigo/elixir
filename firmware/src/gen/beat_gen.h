/* SPDX-License-Identifier: GPL-3.0-only
 * Hortator GEN foundation plus attributed JIANT drum-map adaptation.
 * Inspired by JIANT's seeded streams and rhythmic priority-map approach.
 */
#ifndef HORTATOR_BEAT_GEN_H
#define HORTATOR_BEAT_GEN_H
#include <stdint.h>
#include <string.h>
#define BG_LANES 8
#define BG_STEPS 64
#define BG_OFF 255u
enum { BG_KICK, BG_SNARE, BG_CLAP, BG_CH, BG_OH, BG_TOM, BG_RIM, BG_COWBELL };
enum { BG_STRAIGHT, BG_BROKEN, BG_SPARSE, BG_JHOUSE, BG_JTECHNO, BG_JELECTRO, BG_JEBM, BG_JFUNK, BG_JBOOMBAP, BG_JUKG, BG_JBREAKS, BG_JDNB, BG_JONEDROP, BG_JJOYDIV, BG_JNEWORDER, BG_INTERLOCK, BG_FRACTURE, BG_STYLES };
typedef struct { uint8_t on, acc, rat, cond; } bg_hit_t;
typedef struct { bg_hit_t lane[BG_LANES][BG_STEPS]; } bg_pattern_t;
typedef struct {
    uint32_t seed;
    uint8_t style, energy, complexity, length;
} bg_settings_t;
/* Each hex digit is a priority: F anchors survive until energy reaches zero.
 * These are original prototype patterns, not genre-authenticity claims. */
static const char *const bg_maps[BG_STYLES][BG_LANES] = {
 {"F020F030F020F042", "0000F0010000F020", "0000800000009020",
  "D4A4D4A4D4A4D4A4", "0080008000800090", "0000000000002046",
  "0020004000200050", "0000500000006000"},
 {"F0030020B0200040", "0000F0200000E003", "0000600000008000",
  "C4A3C6A4C3A5C4A7", "0008000000800080", "0000002000003057",
  "0040003000400060", "0000400000400000"},
 {"F000000080000020", "000000000000F000", "0000000000005000",
  "A000700090007000", "0000005000000040", "0000000000000024",
  "0000300000004000", "0000000000003000"}
};
static uint32_t bg_rand(uint32_t *s)
{
    *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5;
    return *s;
}
static uint32_t bg_stream(uint32_t seed, uint32_t style, uint32_t lane)
{
    uint32_t s = seed ^ ((lane + 1u) * 0x9E3779B9u) ^ ((style + 1u) * 0x85EBCA6Bu) ^ 0x6D2B79F5u;
    if (!s) s = 1;
    bg_rand(&s); bg_rand(&s); bg_rand(&s);
    return s;
}
static unsigned bg_priority(char c)
{
    return c >= 'A' ? (unsigned)(c - 'A' + 10) : (unsigned)(c - '0');
}
#include "jiant_beats.h"
#include "beat_abstract.h"

/* Return 0 without touching dst if invalid. Fixed iteration count; no heap. */
static int bg_generate_base(const bg_settings_t *s, bg_pattern_t *dst)
{
    unsigned l, i;
    if (!s || !dst || s->style >= BG_STYLES || s->energy > 100 ||
        s->complexity > 100 || !s->length || s->length > BG_STEPS) return 0;
    if (s->style >= BG_INTERLOCK) return bt_procedural(s,dst);
    if (s->style >= BG_JHOUSE) return bj_generate(s,dst);
    memset(dst, 0, sizeof *dst);
    for (l = 0; l < BG_LANES; ++l) {
        uint32_t rng = bg_stream(s->seed, s->style, l);
        for (i = 0; i < s->length; ++i) {
            bg_hit_t *h = &dst->lane[l][i];
            unsigned p = bg_priority(bg_maps[s->style][l][i % 16u]);
            unsigned r = bg_rand(&rng) % 100u;
            unsigned v = bg_rand(&rng);
            unsigned chance = p == 15u ? 100u : p * s->energy / 15u;
            if (!s->energy || !p) continue;
            /* Complexity adds bounded variation in later bars, not anchors. */
            if (p != 15u && i >= 16u) {
                unsigned extra = s->complexity / 5u;
                chance = chance + extra > 100u ? 100u : chance + extra;
            }
            if (r >= chance) continue;
            h->on = 1;
            h->acc = (uint8_t)(p >= 13u);
            if (l >= BG_CH && p < 13u && s->complexity &&
                v % 100u < s->complexity / 8u)
                h->rat = (uint8_t)(1u + (v >> 8) % 3u);
        }
    }
    /* Do not schedule both closed and open hats on the same step. */
    for (i = 0; i < s->length; ++i)
        if (dst->lane[BG_OH][i].on) memset(&dst->lane[BG_CH][i], 0, sizeof(bg_hit_t));
    return 1;
}
static int bg_generate(const bg_settings_t *s,bg_pattern_t *dst)
{
    if(bd.chaos>100 || bd.anchor>100 || bd.variation>100 || bd.prob>100)return 0;
    if(!bg_generate_base(s,dst))return 0;
    bt_shape(s,dst);
    return 1;
}

/* Explicit one-to-one role mapping. Duplicates rejected to avoid silent merges. */
static int bg_mapping_valid(const uint8_t map[BG_LANES])
{
    unsigned l, used = 0;
    if (!map) return 0;
    for (l = 0; l < BG_LANES; ++l) {
        unsigned t = map[l];
        if (t == BG_OFF) continue;
        if (t >= BG_LANES || (used & (1u << t))) return 0;
        used |= 1u << t;
    }
    return 1;
}
#endif
