/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_STYLE_H
#define ELIXIR_BEAT_STYLE_H

/* Elixir supplies its memory functions before this header. */
#define BG_EXTERNAL_MEMORY_FUNCTIONS 1
/* Main-loop generation scratch uses the existing RAM buffer section. */
#define BG_STORAGE RAMBUF
#include "beat_gen.h"
#undef BG_STORAGE
#undef BG_EXTERNAL_MEMORY_FUNCTIONS
#include "beat_adapter.h"

typedef struct {
    uint32_t seed;
    uint8_t style, part, energy, complexity, chaos, variation, anchor;
} eb_style_config_t;

static eb_style_config_t eb_style_config[NTRACK];
static uint8_t eb_style_initialized;
static bg_pattern_t eb_style_output RAMBUF;
static eb_handoff_rhythm_t eb_style_rhythm POOL;
static const uint8_t EB_STYLE_PART[NTRACK] = {
    BG_KICK, BG_TOM, BG_CLAP, BG_SNARE, BG_COWBELL, BG_CH, BG_OH
};
static const char *const EB_STYLE_PART_NAME[BG_LANES] = {
    "KICK", "SNARE", "CLAP", "CH HAT", "OH HAT", "TOM", "RIM", "COWBELL"
};

enum { EB_STYLE_INVALID = -2, EB_STYLE_STALE = -1,
       EB_STYLE_BLOCKED = 0, EB_STYLE_QUEUED = 1 };

static void eb_style_reset(void)
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++)
        eb_style_config[t] = (eb_style_config_t){
            42, BG_STRAIGHT, EB_STYLE_PART[t], 60, 35, 0, 0, 100
        };
    eb_style_initialized = 1;
}

static eb_style_config_t *eb_style_settings(uint32_t t)
{
    if (!eb_style_initialized)
        eb_style_reset();
    return t < NTRACK ? &eb_style_config[t] : 0;
}

/* Main loop only. No transport, sound, kit, project, or pattern writes.
 * One generation call completes before another may use the shared workspace.
 */
static int eb_style_generate(uint32_t t, const eb_style_config_t *config)
{
    eb_style_config_t c;
    bg_settings_t s;
    bg_detail_t saved_detail;
    unsigned k, length;
    int generated;
    if (!config || t >= NTRACK)
        return EB_STYLE_INVALID;
    c = *config;
    if (c.style >= BG_STYLES || c.part >= BG_LANES || c.energy > 100u ||
        c.complexity > 100u || c.chaos > 100u || c.variation > 100u || c.anchor > 100u)
        return EB_STYLE_INVALID;
    if (!eb_handoff_capture(t))
        return EB_STYLE_BLOCKED;
    length = eb_ho.before.len;
    length = !length ? 16u : length > NUM_STEPS ? NUM_STEPS : length;
    s = (bg_settings_t){c.seed, c.style, c.energy, c.complexity, (uint8_t)length};
    saved_detail = bd;
    bd = (bg_detail_t){c.chaos, c.anchor, c.variation, 0};
    generated = bg_generate(&s, &eb_style_output);
    bd = saved_detail;
    if (!generated || !eb_lane_valid(&eb_style_output, c.part, length, 96, 127)) {
        eb_handoff_submit(0); /* abort this builder without discarding an existing preview */
        return EB_STYLE_INVALID;
    }
    memset(&eb_style_rhythm, 0, sizeof eb_style_rhythm);
    eb_style_rhythm.hit_volumes_only = 1;
    for (k = 0; k < length; k++) {
        const bg_hit_t *h = &eb_style_output.lane[c.part][k];
        if (h->on) {
            eb_style_rhythm.main[k >> 4] |= (uint16_t)(1u << (k & 15u));
            eb_style_rhythm.volume[k] = eb_lane_volume(h, 96, 127);
        }
    }
    return eb_handoff_submit(&eb_style_rhythm) ? EB_STYLE_QUEUED : EB_STYLE_STALE;
}

#endif
