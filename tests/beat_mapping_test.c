/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static pattern_t stored, live_before, expected, kit_live;
static bg_pattern_t generated;
static eb_handoff_track_t a, b;
static uint8_t sound_before[sizeof par];
static const uint8_t editable[4] = {0, 1, 2, 4};

static void fixture(void)
{
    memset(&sq, 0, sizeof sq); par[PAR_PATTERN_MODE] = 0;
    seq_init(); sq.active = sq.pending = 0;
    sq.running = sq.rec = sq.song_mode = sq.gsync = 0;
    memset(&ui, 0, sizeof ui); ui.held_step = -1;
    ui.mode = M_EUCLID; ui.page = PG_BSTYLE;
    kset.accel = 0; undo_t = -1;
    eb_style_reset(); memset(&eb_style_live, 0, sizeof eb_style_live);
    for (uint32_t t = 0; t < NTRACK; t++) {
        pat[0].len[t] = 16;
        pat[0].step[t][0].note = (uint8_t)(60u + t);
    }
    stored = pat[0]; memcpy(sound_before, par, sizeof par);
}
static void same_track(const pattern_t *x, const pattern_t *y, uint32_t t)
{
    eb_handoff_read(&a, x, t); eb_handoff_read(&b, y, t);
    assert(!memcmp(&a, &b, sizeof a));
}
static void rhythm_matches(uint32_t t, const bg_pattern_t *g, uint32_t role)
{
    for (uint32_t k = 0; k < 16; k++) {
        const bg_hit_t *h = &g->lane[role][k];
        assert(main_on(&gpat, t, k) == h->on);
        if (h->on) assert(gpat.step[t][k].volume == eb_lane_volume(h, 96, 127));
    }
}
static void expected_output(const eb_style_config_t *c)
{
    bg_settings_t s = {c->seed, c->style, c->energy, c->complexity, 16};
    bg_detail_t saved = bd;
    bd = (bg_detail_t){c->chaos, c->anchor, c->variation, 0};
    assert(bg_generate(&s, &generated)); bd = saved;
}
int main(void)
{
    uint8_t roles[NTRACK], captured[NTRACK];
    uint16_t pp[4]; char text[64]; uint32_t pg, count;
    web_nor_erase(); web_boot(); fixture();
    assert(eb_style_roles_capture(roles));
    assert(!memcmp(roles, EB_STYLE_PART, sizeof roles));
    assert(!eb_style_role_set(3, BG_KICK));
    assert(!eb_style_role_set(5, BG_OH));
    assert(!eb_style_role_set(6, BG_CH));
    assert(!eb_style_role_set(NTRACK, BG_KICK));
    assert(!eb_style_role_set(1, BG_LANES));
    assert(eb_style_role_set(1, BG_KICK) && eb_style_role_set(2, BG_KICK));
    assert(eb_style_roles_capture(roles)); /* duplicates are valid */
    roles[3] = BG_KICK; assert(!eb_style_roles_valid(roles));
    puts("ok default assignments, fixed Snare/hats, invalid-role rejection and duplicate roles");

    for (uint32_t slot = 0; slot < 4; slot++)
        for (uint32_t role = 0; role < BG_LANES; role++) {
            fixture(); uint32_t t = editable[slot];
            (void)eb_style_role_set(t, role);
            eb_style_config_t c = *eb_style_settings(t);
            c.style = BG_JTECHNO; c.energy = 100; c.seed = 197;
            c.part = (uint8_t)((role + 1u) % BG_LANES); /* deliberately stale copy */
            expected_output(&c);
            assert(eb_style_generate_kit(&c) == EB_STYLE_QUEUED);
            seq_block(); kit_live = gpat;
            for (uint32_t i = 0; i < NTRACK; i++) rhythm_matches(i, &generated, eb_style_role(i));
            assert(eb_style_generate(t, &c) == EB_STYLE_QUEUED); seq_block();
            for (uint32_t i = 0; i < NTRACK; i++) same_track(&kit_live, &gpat, i);
            assert(!memcmp(&stored, &pat[0], sizeof stored));
            assert(!memcmp(sound_before, par, sizeof par));
        }
    puts("ok all eight roles on D1/D2/D3/Cymbal: public Kit and single-track calls share the authoritative map");

    fixture(); ui.page = PG_BMAP; page_params(pp);
    assert(pp[0] == VP_BMAP_D1 && pp[1] == VP_BMAP_D2 &&
           pp[2] == VP_BMAP_D3 && pp[3] == VP_BMAP_CY);
    assert(PG_BMAP == PG_BDETAIL + 1u && PG_ACTION == PG_BMAP + 1u);
    title_text(text, sizeof text, &pg, &count);
    assert(!strcmp(text, "STYLE MAP") && count == NEUCLID);
    card_name(text, sizeof text, VP_BMAP_D2); assert(!strcmp(text, "D2 role"));
    card_value(text, sizeof text, VP_BMAP_CY, BG_COWBELL); assert(!strcmp(text, "COWBELL"));
    web_enc(EN_K1, 1); ui_input(); assert(eb_style_role(0) == BG_SNARE);
    web_enc(EN_K2, 1); ui_input(); assert(eb_style_role(1) == BG_RIM);
    web_enc(EN_K3, 1); ui_input(); assert(eb_style_role(2) == BG_CH);
    web_enc(EN_K4, -1); ui_input(); assert(eb_style_role(4) == BG_RIM);
    assert(!eb_style_live.pending && !eb_pv.on);
    button_down(B_OCTUP); assert(!ui.copy && !eb_style_live.pending);
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    assert(!memcmp(sound_before, par, sizeof par));
    ui.force = 1; ui_draw();
    puts("ok four real map knobs, labels and page title; setup edits and OCT+ do not generate or alter sounds");

    ui.page = PG_BSTYLE; button_down(B_OCTUP); seq_block();
    assert(eb_pv.on == 127); live_before = gpat;
    ui.page = PG_BMAP; vp_set(VP_BMAP_D2, BG_CLAP); ui_input();
    assert(!memcmp(&live_before, &gpat, sizeof gpat) && eb_pv.on == 127);
    ui.page = PG_BSTYLE; vp_set(VP_BTARGET, 2); vp_set(VP_BSEED, 333);
    ui_input(); seq_block();
    expected_output(eb_style_settings(1)); rhythm_matches(1, &generated, BG_CLAP);
    for (uint32_t t = 0; t < NTRACK; t++) if (t != 1) same_track(&live_before, &gpat, t);
    puts("ok map edits retain accepted previews; next single-track generation uses its assigned role only");

    fixture(); assert(eb_handoff_capture(0));
    vp_set(VP_BSEED, 111); ui_input(); assert(eb_style_live.pending);
    assert(!memcmp(eb_style_live.role, EB_STYLE_PART, NTRACK));
    vp_set(VP_BMAP_D2, BG_RIM); assert(!eb_style_live.pending);
    eb_handoff_submit(0); ui_input(); assert(eb_kit.state != EB_HO_READY);
    vp_set(VP_BSEED, 112); ui_input(); assert(eb_kit.state == EB_HO_READY);
    assert(eb_style_live.role[1] == BG_RIM);
    vp_set(VP_BMAP_D2, BG_CLAP); seq_block(); assert(!eb_pv.on && eb_kit.state == EB_HO_REJECTED);
    vp_set(VP_BTARGET, 2); vp_set(VP_BSEED, 200); ui_input(); assert(eb_ho.state == EB_HO_READY);
    vp_set(VP_BMAP_D2, BG_TOM); seq_block(); assert(!eb_pv.on && eb_ho.state == EB_HO_REJECTED);
    vp_set(VP_BSEED, 201); assert(eb_style_live.pending);
    assert(eb_style_role_set(1, BG_RIM)); /* exact guard also catches non-UI assignment changes */
    ui_input(); assert(!eb_style_live.pending);
    vp_set(VP_BSEED, 202); ui_input(); assert(eb_ho.state == EB_HO_READY);
    vp_set(VP_BMAP_D1, BG_TOM); ui_input(); seq_block();
    assert(eb_preview_on(&pat[0], 1));
    puts("ok affected deferred/READY work is cancelled, exact snapshots catch changed roles, unrelated roles do not cancel single-track work");

    fixture(); assert(eb_style_roles_capture(captured));
    eb_style_config_t c = *eb_style_settings(0);
    c.style = BG_JTECHNO; c.energy = 100;
    expected_output(&c); assert(eb_style_role_set(1, BG_RIM));
    assert(eb_style_generate_kit_map_context(&c, captured, eb_context_capture(), 0) == EB_STYLE_QUEUED);
    seq_block(); rhythm_matches(1, &generated, BG_TOM); /* immutable prepared mapping, not later assignment */
    captured[6] = BG_CH;
    assert(eb_style_generate_kit_map_context(&c, captured, eb_context_capture(), 0) == EB_STYLE_INVALID);
    live_before = gpat; ui.page = PG_BMAP; vp_set(VP_BMAP_D2, BG_CLAP);
    assert(eb_kit_commit()); expected = pat[0];
    assert(eb_kit_undo_redo() && !memcmp(&stored, &pat[0], sizeof stored));
    assert(eb_kit_undo_redo() && !memcmp(&expected, &pat[0], sizeof expected));
    assert(eb_style_role(1) == BG_CLAP);
    printf("live request with exact roles: %zu bytes; role snapshot: %u bytes\n", sizeof eb_style_live, NTRACK);
    puts("beat_mapping_test: ok");
    return 0;
}
