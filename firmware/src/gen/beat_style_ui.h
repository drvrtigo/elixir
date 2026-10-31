/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_STYLE_UI_H
#define ELIXIR_BEAT_STYLE_UI_H

static const char *const EB_STYLE_TARGET_NAME[NTRACK + 1] = {
    "KIT", "D1", "D2", "D3", "SNARE", "CYMBAL", "CLOSED HAT", "OPEN HAT"
};

/* Internal request target NTRACK means Kit; ui.track is always a real track. */
static uint32_t eb_style_ui_track(void) { return ui.bscope ? ui.track : NTRACK; }
static eb_style_config_t *eb_style_ui_settings(uint32_t t)
{
    (void)eb_style_settings(0); /* lazy initialize both scopes */
    return t == NTRACK ? &eb_style_kit_config : eb_style_settings(t);
}

static uint32_t eb_style_map_track(uint32_t p)
{
    return p == VP_BMAP_CY ? 4u : p - VP_BMAP_D1;
}

static uint32_t eb_style_ui_max(uint32_t p)
{
    if (p >= VP_BMAP_D1 && p <= VP_BMAP_CY) return BG_LANES - 1u;
    return p == VP_BSTYLE ? BG_STYLES - 1u :
           p == VP_BTARGET ? NTRACK : p == VP_BSEED ? 9999u : 100u;
}

static uint32_t eb_style_ui_get(uint32_t p)
{
    if (p >= VP_BMAP_D1 && p <= VP_BMAP_CY) return eb_style_role(eb_style_map_track(p));
    const eb_style_config_t *c = eb_style_ui_settings(eb_style_ui_track());
    if (p == VP_BTARGET) return ui.bscope ? ui.track + 1u : 0u;
    if (!c) return 0;
    switch (p) {
    case VP_BSTYLE: return c->style;
    case VP_BENERGY: return c->energy;
    case VP_BSEED: return c->seed;
    case VP_BCOMPLEX: return c->complexity;
    case VP_BCHAOS: return c->chaos;
    case VP_BVAR: return c->variation;
    default: return c->anchor;
    }
}

/* Main-loop latest request, plus ownership of an unconsumed UI publication. */
static struct {
    eb_style_config_t config;
    uint32_t epoch;
    uint8_t pattern, track, pending, queued;
    uint8_t role[NTRACK];
} eb_style_live;

static int eb_style_live_valid(void)
{
    return eb_style_roles_match(eb_style_live.role, eb_style_live.track) &&
           eb_style_live.track == eb_style_ui_track() &&
           eb_style_live.pattern < NUM_PATTERN && eb_style_live.pattern == sq.active &&
           eb_context_matches(eb_style_live.epoch) && ui.mode == M_EUCLID &&
           (ui.page == PG_BSTYLE || ui.page == PG_BDETAIL) &&
           !ui.clear_open && !ui.ca && !ui.ca_ask && !ui.kb_open && !ui.sb_open;
}

/* Cancel only UI-owned READY work; never disturb accepted previews or builders. */
static void eb_style_ui_cancel(void)
{
    uint32_t f = irq_save();
    if (eb_style_live.queued) {
        if (eb_style_live.track == NTRACK) {
            if (eb_kit.state == EB_HO_READY && eb_kit.pattern == eb_style_live.pattern &&
                eb_kit.epoch == eb_style_live.epoch)
                eb_kit.state = EB_HO_REJECTED;
        } else if (eb_ho.state == EB_HO_READY && eb_ho.track == eb_style_live.track &&
                   eb_ho.pattern == eb_style_live.pattern && eb_ho.epoch == eb_style_live.epoch)
            eb_ho.state = EB_HO_REJECTED;
    }
    eb_style_live.pending = eb_style_live.queued = 0;
    RING_PUBLISH();
    irq_restore(f);
}

static void eb_style_live_request(uint32_t t)
{
    const eb_style_config_t *c = eb_style_ui_settings(t);
    uint32_t f;
    if (!c) return;
    if (eb_style_live.queued &&
        (eb_style_live.track != t || eb_style_live.pattern != sq.active ||
         !eb_context_matches(eb_style_live.epoch)))
        eb_style_ui_cancel();
    f = irq_save();
    eb_style_live.config = *c;
    (void)eb_style_roles_capture(eb_style_live.role);
    if (t < NTRACK) eb_style_live.config.part = eb_style_live.role[t];
    eb_style_live.epoch = eb_context_capture();
    eb_style_live.pattern = sq.active;
    eb_style_live.track = (uint8_t)t;
    eb_style_live.pending = 1;
    irq_restore(f);
}

/* Once per UI pass. All rhythm generation remains outside the IRQ mask. */
static int eb_style_live_service(void)
{
    eb_style_config_t c;
    uint32_t f, t, epoch, pattern, i;
    int rc;
    if ((eb_style_live.pending || eb_style_live.queued) && !eb_style_live_valid()) {
        eb_style_ui_cancel();
        return EB_STYLE_BLOCKED;
    }
    if (!eb_style_live.pending) return EB_STYLE_BLOCKED;
    f = irq_save();
    t = eb_style_live.track;
    epoch = eb_style_live.epoch;
    pattern = eb_style_live.pattern;
    if (sq.rec || sq.song_mode) goto guarded;
    for (i = 0; i < NTRACK; i++)
        if ((t == NTRACK || t == i) &&
            (pat[pattern].gen[i][0] != GEN_OFF || pat[pattern].mel[i][ML_SHAPE] != MEL_OFF))
            goto guarded;
    if (eb_ho.state == EB_HO_BUILDING || eb_kit.state == EB_HO_BUILDING)
        goto wait;
    if (eb_ho.state == EB_HO_READY) {
        if (t == NTRACK || eb_ho.track != t || eb_ho.pattern != pattern || eb_ho.epoch != epoch)
            goto wait;
        eb_ho.state = EB_HO_REJECTED;
    }
    if (eb_kit.state == EB_HO_READY) {
        if (t != NTRACK || eb_kit.pattern != pattern || eb_kit.epoch != epoch)
            goto wait;
        eb_kit.state = EB_HO_REJECTED;
    }
    c = eb_style_live.config;
    eb_style_live.pending = 0;
    RING_PUBLISH();
    irq_restore(f);
    rc = t == NTRACK ? eb_style_generate_kit_map_context(&c, eb_style_live.role, epoch, pattern) :
                        eb_style_generate_role_context(t, &c, epoch, pattern);
    eb_style_live.queued = (uint8_t)(rc == EB_STYLE_QUEUED);
    if (rc == EB_STYLE_QUEUED) ui.force = 1;
    else ui_message(rc == EB_STYLE_STALE ? "STYLE CHANGED: RETRY" :
                    rc == EB_STYLE_INVALID ? "STYLE SETTINGS INVALID" :
                    "STYLE: TRACK BUSY OR GUARDED");
    return rc;
guarded:
    eb_style_live.pending = 0;
    irq_restore(f);
    eb_style_ui_cancel();
    ui_message("STYLE: TRACK BUSY OR GUARDED");
    return EB_STYLE_BLOCKED;
wait:
    irq_restore(f);
    return EB_STYLE_BLOCKED;
}

static void eb_style_ui_set(uint32_t p, uint32_t v)
{
    eb_style_config_t *c;
    uint32_t max = eb_style_ui_max(p);
    if (v > max) v = max;
    if (v == eb_style_ui_get(p)) return;
    if (p >= VP_BMAP_D1 && p <= VP_BMAP_CY) {
        uint32_t t = eb_style_map_track(p);
        if ((eb_style_live.pending || eb_style_live.queued) &&
            (eb_style_live.track == NTRACK || eb_style_live.track == t))
            eb_style_ui_cancel();
        if (eb_style_role_set(t, v)) ui.force = 1;
        return; /* setup only: keep accepted previews until the next generation */
    }
    if (p == VP_BTARGET) {
        eb_style_ui_cancel();
        ui.bscope = (uint8_t)(v != 0u);
        if (v) track_select(v - 1u);
        ui.action = ACT_NONE;
        ui.force = 1;
        return; /* target selection alone never generates or discards previews */
    }
    c = eb_style_ui_settings(eb_style_ui_track());
    if (!c) return;
    switch (p) {
    case VP_BSTYLE: c->style = (uint8_t)v; break;
    case VP_BENERGY: c->energy = (uint8_t)v; break;
    case VP_BSEED: c->seed = v; break;
    case VP_BCOMPLEX: c->complexity = (uint8_t)v; break;
    case VP_BCHAOS: c->chaos = (uint8_t)v; break;
    case VP_BVAR: c->variation = (uint8_t)v; break;
    default: c->anchor = (uint8_t)v; break;
    }
    eb_style_live_request(eb_style_ui_track());
}

static void eb_style_ui_generate(void)
{
    int rc;
    eb_style_live_request(eb_style_ui_track());
    rc = eb_style_live_service();
    ui_message(rc == EB_STYLE_QUEUED ? "STYLE QUEUED" :
               rc == EB_STYLE_STALE ? "STYLE CHANGED: RETRY" :
               rc == EB_STYLE_INVALID ? "STYLE SETTINGS INVALID" :
               "STYLE: TRACK BUSY OR GUARDED");
}

#endif
