/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_STYLE_UI_H
#define ELIXIR_BEAT_STYLE_UI_H

static uint32_t eb_style_ui_max(uint32_t p)
{
    return p == VP_BSTYLE ? BG_STYLES - 1u :
           p == VP_BPART ? BG_LANES - 1u : p == VP_BSEED ? 9999u : 100u;
}

static uint32_t eb_style_ui_get(uint32_t p)
{
    const eb_style_config_t *c = eb_style_settings(ui.track);
    if (!c)
        return 0;
    switch (p) {
    case VP_BSTYLE: return c->style;
    case VP_BPART: return c->part;
    case VP_BENERGY: return c->energy;
    case VP_BSEED: return c->seed;
    case VP_BCOMPLEX: return c->complexity;
    case VP_BCHAOS: return c->chaos;
    case VP_BVAR: return c->variation;
    default: return c->anchor;
    }
}

static void eb_style_ui_set(uint32_t p, uint32_t v)
{
    eb_style_config_t *c = eb_style_settings(ui.track);
    uint32_t max = eb_style_ui_max(p);
    if (!c)
        return;
    if (v > max)
        v = max;
    switch (p) {
    case VP_BSTYLE: c->style = (uint8_t)v; break;
    case VP_BPART: c->part = (uint8_t)v; break;
    case VP_BENERGY: c->energy = (uint8_t)v; break;
    case VP_BSEED: c->seed = v; break;
    case VP_BCOMPLEX: c->complexity = (uint8_t)v; break;
    case VP_BCHAOS: c->chaos = (uint8_t)v; break;
    case VP_BVAR: c->variation = (uint8_t)v; break;
    default: c->anchor = (uint8_t)v; break;
    }
}

static void eb_style_ui_generate(void)
{
    int rc = eb_style_generate(ui.track, eb_style_settings(ui.track));
    ui_message(rc == EB_STYLE_QUEUED ? "STYLE QUEUED" :
               rc == EB_STYLE_STALE ? "STYLE CHANGED: RETRY" :
               rc == EB_STYLE_INVALID ? "STYLE SETTINGS INVALID" :
               "STYLE: TRACK BUSY OR GUARDED");
}

#endif
