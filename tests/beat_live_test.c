/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static pattern_t stored;
static bg_pattern_t expected;

static void fixture(void)
{
    memset(&sq, 0, sizeof sq);
    par[PAR_PATTERN_MODE] = 0;
    seq_init();
    sq.active = sq.pending = 0;
    sq.running = sq.rec = sq.song_mode = sq.gsync = 0;
    memset(&ui, 0, sizeof ui);
    ui.held_step = -1;
    ui.bscope = 1; /* retain legacy single-track assertions */
    ui.mode = M_EUCLID;
    ui.page = PG_BSTYLE;
    kset.accel = 0;
    eb_style_reset();
    memset(&eb_style_live, 0, sizeof eb_style_live);
    undo_t = -1;
    stored = pat[0];
}

static void expect_latest(uint32_t t)
{
    eb_style_config_t c = *eb_style_settings(t);
    bg_settings_t g = {c.seed, c.style, c.energy, c.complexity,
                       (uint8_t)track_len(&pat[0], t)};
    bg_detail_t saved = bd;
    bd = (bg_detail_t){c.chaos, c.anchor, c.variation, 0};
    assert(bg_generate(&g, &expected));
    bd = saved;
    for (uint32_t k = 0; k < g.length; k++)
        assert(main_on(&gpat, t, k) == expected.lane[c.part][k].on);
}

int main(void)
{
    uint32_t ticks, signature, epoch;
    int8_t index;
    eb_style_config_t c;
    web_nor_erase(); web_boot();

    fixture();
    vp_set(VP_BSEED, 100); vp_set(VP_BENERGY, 80); vp_set(VP_BSEED, 101);
    assert(eb_style_live.pending && eb_style_live.config.seed == 101u);
    assert(eb_ho.state == EB_HO_IDLE);
    ui_input();
    assert(!eb_style_live.pending && eb_ho.state == EB_HO_READY);
    assert(tpat(0) == &pat[0]);
    seq_block();
    assert(eb_preview_on(&pat[0], 0)); expect_latest(0);
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    puts("ok setting changes coalesce and publish automatically after UI input without OCT");

    fixture();
    web_enc(EN_K3, 1); ui_input();
    assert(vp_get(VP_BENERGY) == 61u && eb_ho.state == EB_HO_READY);
    seq_block(); expect_latest(0);
    ui.page = PG_BDETAIL;
    web_enc(EN_K2, 1); ui_input();
    assert(vp_get(VP_BCHAOS) == 1u && eb_ho.state == EB_HO_READY);
    seq_block(); expect_latest(0);
    puts("ok actual knob input on both STYLE pages automatically regenerates");

    fixture();
    vp_set(VP_BENERGY, 100); ui_input();
    assert(eb_ho.state == EB_HO_READY);
    vp_set(VP_BENERGY, 0); ui_input();
    assert(eb_ho.state == EB_HO_READY && eb_ho.rhythm.main[0] == 0u);
    assert(tpat(0) == &pat[0]);
    seq_block();
    assert(gpat.main[0][0] == 0u); expect_latest(0);
    puts("ok newer settings supersede an unconsumed same-track request before playback");

    fixture();
    vp_set(VP_BENERGY, 100); ui_input(); seq_block();
    seq_start(1);
    for (uint32_t k = 0; k < 40u; k++) seq_tick_one();
    ticks = sq.ticks; index = sq.idx[0]; signature = col_sig(0);
    vp_set(VP_BENERGY, 0); ui_input();
    assert(sq.running && sq.ticks == ticks && sq.idx[0] == index);
    assert(col_sig(0) == signature && main_on(tpat(0), 0, 0));
    seq_block();
    assert(sq.running && sq.ticks >= ticks && sq.ticks <= ticks + 1u);
    assert(gpat.main[0][0] == 0u && col_sig(0) != signature);
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    seq_stop_now();
    puts("ok playing updates preserve transport position; the grid changes only at acceptance");

    fixture();
    assert(eb_handoff_capture(0));
    vp_set(VP_BSEED, 100); ui_input();
    assert(eb_style_live.pending && eb_ho.state == EB_HO_BUILDING);
    vp_set(VP_BSEED, 200);
    assert(!eb_handoff_submit(0));
    ui_input(); seq_block(); expect_latest(0);
    assert(!eb_style_live.pending && eb_preview_on(&pat[0], 0));
    puts("ok an occupied builder defers work and retains only the latest settings");

    fixture();
    vp_set(VP_BSEED, 100); track_select(1); ui_input();
    assert(!eb_style_live.pending && eb_ho.state == EB_HO_IDLE);
    track_select(0); vp_set(VP_BSEED, 101); eb_context_changed(); ui_input();
    assert(!eb_style_live.pending && eb_ho.state == EB_HO_IDLE);
    vp_set(VP_BSEED, 102); ui.page = PG_ACTION; ui_input();
    assert(!eb_style_live.pending && eb_ho.state == EB_HO_IDLE);
    ui.page = PG_BSTYLE;
    c = *eb_style_settings(0); epoch = eb_context_capture(); eb_context_changed();
    assert(eb_style_generate_context(0, &c, epoch, sq.active) == EB_STYLE_STALE);
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok deferred requests cannot leak across track, context, or page changes");

    fixture();
    sq.rec = 1; vp_set(VP_BSEED, 100); ui_input();
    assert(!eb_style_live.pending && !eb_preview_on(&pat[0], 0));
    sq.rec = 0; sq.song_mode = 1; vp_set(VP_BSEED, 101); ui_input();
    assert(!eb_style_live.pending);
    sq.song_mode = 0; pat[0].gen[0][0] = GEN_MUTATE;
    vp_set(VP_BSEED, 102); ui_input(); assert(!eb_style_live.pending);
    pat[0].gen[0][0] = GEN_OFF; pat[0].mel[0][ML_SHAPE] = MEL_RANDOM;
    vp_set(VP_BSEED, 103); ui_input(); assert(!eb_style_live.pending);
    assert(eb_ho.state == EB_HO_IDLE);
    puts("ok recording, song, and existing rhythm/melody generators still refuse live takeover");

    fixture();
    vp_set(VP_BSEED, 42); ui_input();
    assert(!eb_style_live.pending && eb_ho.state == EB_HO_IDLE);
    vp_set(VP_BENERGY, 100); ui_input(); seq_block();
    vp_set(VP_BENERGY, 101); ui_input();
    assert(!eb_style_live.pending && eb_ho.state == EB_HO_ACCEPTED);
    button_down(B_OCTUP); assert(eb_ho.state == EB_HO_READY);
    seq_block();
    ui.page = PG_ACTION; ui.action = ACT_COMMIT; button_down(B_OCTUP);
    assert(!eb_preview_on(&pat[0], 0));
    assert(seq_undo(0)); seq_block();
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    puts("ok no-op/clamped settings do not regenerate; manual OCT and Commit/Undo remain usable");

    printf("live request state: %zu bytes\n", sizeof eb_style_live);
    puts("beat_live_test: ok");
    return 0;
}
