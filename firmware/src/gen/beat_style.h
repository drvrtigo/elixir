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
static eb_style_config_t eb_style_kit_config;
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
    eb_style_kit_config = eb_style_config[0];
    eb_style_initialized = 1;
}

static eb_style_config_t *eb_style_settings(uint32_t t)
{
    if (!eb_style_initialized)
        eb_style_reset();
    return t < NTRACK ? &eb_style_config[t] : 0;
}

/* Existing per-track part bytes are the authoritative mapping; fixed rows
 * are never user-editable. No parallel map storage or sound-model coupling. */
static int eb_style_role_editable(uint32_t t)
{
    return t < 3u || t == 4u;
}
static uint8_t eb_style_role(uint32_t t)
{
    (void)eb_style_settings(0);
    return t < NTRACK ? (eb_style_role_editable(t) ? eb_style_config[t].part : EB_STYLE_PART[t]) : BG_OFF;
}
static int eb_style_roles_valid(const uint8_t role[NTRACK])
{
    uint32_t t;
    if (!role) return 0;
    for (t = 0; t < NTRACK; t++)
        if (role[t] >= BG_LANES ||
            (!eb_style_role_editable(t) && role[t] != EB_STYLE_PART[t])) return 0;
    return 1; /* duplicates deliberately allowed for layering */
}
static int eb_style_roles_capture(uint8_t role[NTRACK])
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++) role[t] = eb_style_role(t);
    return eb_style_roles_valid(role);
}
static int eb_style_roles_match(const uint8_t role[NTRACK], uint32_t target)
{
    uint32_t t;
    for (t = 0; t < NTRACK; t++)
        if ((target == NTRACK || target == t) && role[t] != eb_style_role(t)) return 0;
    return 1;
}
static int eb_style_role_set(uint32_t t, uint32_t role)
{
    if (t >= NTRACK || !eb_style_role_editable(t) || role >= BG_LANES) return 0;
    (void)eb_style_settings(0);
    if (eb_style_config[t].part == role) return 0;
    eb_style_config[t].part = (uint8_t)role;
    return 1;
}

/* Main loop only. No transport, sound, kit, project, or pattern writes.
 * One generation call completes before another may use the shared workspace.
 */
static int eb_style_generate_role_context(uint32_t t, const eb_style_config_t *config,
                                     uint32_t epoch, uint32_t pattern)
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
    if (eb_ho.epoch != epoch || eb_ho.pattern != pattern) {
        eb_handoff_submit(0);
        return EB_STYLE_STALE;
    }
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

/* Ordinary callers use the authoritative role, not a copied config.part. */
static int eb_style_generate_context(uint32_t t, const eb_style_config_t *config,
                                     uint32_t epoch, uint32_t pattern)
{
    eb_style_config_t c;
    if (!config || t >= NTRACK) return EB_STYLE_INVALID;
    c = *config;
    c.part = eb_style_role(t);
    return eb_style_generate_role_context(t, &c, epoch, pattern);
}

/* Existing direct callers use the context current at invocation. */
static int eb_style_generate(uint32_t t, const eb_style_config_t *config)
{
    uint32_t f = irq_save();
    uint32_t epoch = eb_context_capture(), pattern = sq.active;
    irq_restore(f);
    return eb_style_generate_context(t, config, epoch, pattern);
}

/* One shared seeded rhythm using an explicitly captured role assignment. */
static int eb_style_generate_kit_map_context(const eb_style_config_t *config,
                                             const uint8_t roles[NTRACK],
                                             uint32_t epoch, uint32_t pattern)
{
    eb_style_config_t c;
    uint8_t role[NTRACK];
    bg_settings_t s;
    bg_detail_t saved_detail;
    uint32_t t, k, length = 1;
    int generated;
    if (!config || !eb_style_roles_valid(roles))
        return EB_STYLE_INVALID;
    memcpy(role, roles, sizeof role);
    c = *config;
    if (c.style >= BG_STYLES || c.energy > 100u || c.complexity > 100u ||
        c.chaos > 100u || c.variation > 100u || c.anchor > 100u)
        return EB_STYLE_INVALID;
    if (!eb_kit_capture())
        return EB_STYLE_BLOCKED;
    if (eb_kit.epoch != epoch || eb_kit.pattern != pattern) {
        eb_kit.state = EB_HO_REJECTED;
        return EB_STYLE_STALE;
    }
    for (t = 0; t < NTRACK; t++) {
        uint32_t n = eb_preview_source(t)->len;
        n = !n ? 16u : n > NUM_STEPS ? NUM_STEPS : n;
        if (n > length) length = n;
    }
    s = (bg_settings_t){c.seed, c.style, c.energy, c.complexity, (uint8_t)length};
    saved_detail = bd;
    bd = (bg_detail_t){c.chaos, c.anchor, c.variation, 0};
    generated = bg_generate(&s, &eb_style_output);
    bd = saved_detail;
    if (!generated) {
        eb_kit.state = EB_HO_REJECTED;
        return EB_STYLE_INVALID;
    }
    for (t = 0; t < NTRACK; t++) {
        eb_handoff_rhythm_t *r = &eb_kit.rhythm[t];
        memset(r, 0, sizeof *r);
        r->hit_volumes_only = 1;
        for (k = 0; k < length; k++) {
            const bg_hit_t *h = &eb_style_output.lane[role[t]][k];
            if (h->on) {
                r->main[k >> 4] |= (uint16_t)(1u << (k & 15u));
                r->volume[k] = eb_lane_volume(h, 96, 127);
            }
        }
    }
    return eb_kit_submit() ? EB_STYLE_QUEUED : EB_STYLE_STALE;
}

static int eb_style_generate_kit_context(const eb_style_config_t *config,
                                         uint32_t epoch, uint32_t pattern)
{
    uint8_t role[NTRACK];
    if (!eb_style_roles_capture(role)) return EB_STYLE_INVALID;
    return eb_style_generate_kit_map_context(config, role, epoch, pattern);
}

static int eb_style_generate_kit(const eb_style_config_t *config)
{
    uint32_t f = irq_save();
    uint32_t epoch = eb_context_capture(), pattern = sq.active;
    irq_restore(f);
    return eb_style_generate_kit_context(config, epoch, pattern);
}

#endif
