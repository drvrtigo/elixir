/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static pattern_t stored, accepted, committed;
static eb_handoff_track_t a, b;

static void fixture(void)
{
    memset(&sq, 0, sizeof sq);
    par[PAR_PATTERN_MODE] = 0;
    seq_init();
    sq.active = sq.pending = 0;
    sq.running = sq.rec = sq.song_mode = sq.gsync = 0;
    memset(&ui, 0, sizeof ui);
    ui.held_step = -1;
    ui.mode = M_EUCLID; ui.page = PG_BSTYLE;
    kset.accel = 0;
    memset(&eb_style_live, 0, sizeof eb_style_live);
    eb_style_reset(); undo_t = -1;
    stored = pat[0];
}
static void all_on(void)
{
    assert(eb_pv.on == (1u << NTRACK) - 1u);
    for (uint32_t t = 0; t < NTRACK; t++)
        assert(eb_preview_on(&pat[0], t) && tpat(t) == &gpat && view_pat(t) == &gpat);
}
static void unchanged_others(uint32_t except)
{
    for (uint32_t t = 0; t < NTRACK; t++) {
        if (t == except) continue;
        eb_handoff_read(&a, &accepted, t); eb_handoff_read(&b, &gpat, t);
        assert(!memcmp(&a, &b, sizeof a));
    }
}
static void kit(void)
{
    ui.page = PG_BSTYLE; vp_set(VP_BTARGET, 0);
    button_down(B_OCTUP); seq_block(); all_on();
}
static void action(uint32_t act)
{
    ui.page = PG_ACTION;
    uint8_t acts[NGACT]; uint32_t n = gen_acts(shown_pat(), ui.track, acts), i;
    for (i = 0; i < n && acts[i] != act; i++);
    assert(i < n); vp_set(VP_ACTION, i + 1u);
    assert(ui.action == act); button_down(B_OCTUP);
    assert(ui.action == ACT_NONE);
}
int main(void)
{
    char text[64]; uint16_t pp[4];
    web_nor_erase(); web_boot();
    assert(!ui.bscope); /* actual boot default, not only the fixture */
    fixture();
    static const char *const names[8] = {"KIT", "D1", "D2", "D3", "SNARE", "CYMBAL", "CLOSED HAT", "OPEN HAT"};
    card_name(text, sizeof text, VP_BTARGET); assert(!strcmp(text, "Target"));
    for (unsigned t = 0; t <= NTRACK; t++) {
        vp_set(VP_BTARGET, t);
        assert(vp_get(VP_BTARGET) == t && vp_max(VP_BTARGET) == NTRACK);
        card_value(text, sizeof text, VP_BTARGET, t); assert(!strcmp(text, names[t]));
        assert(!eb_style_live.pending && !eb_pv.on);
        if (t) assert(ui.track == t - 1u);
    }
    vp_set(VP_BTARGET, 0);
    web_enc(EN_K2, 1); ui_input(); assert(vp_get(VP_BTARGET) == 1);
    web_enc(EN_K2, -1); ui_input(); assert(vp_get(VP_BTARGET) == 0);
    puts("ok actual target knob: Kit first, seven track targets, selection alone does not generate");

    fixture();
    vp_set(VP_BSEED, 100); vp_set(VP_BENERGY, 90); vp_set(VP_BSEED, 101);
    assert(eb_style_live.pending && eb_style_live.track == NTRACK);
    ui_input(); assert(eb_kit.state == EB_HO_READY && !eb_pv.on);
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    seq_block(); all_on(); accepted = gpat;
    assert(eb_style_kit_config.seed == 101 && eb_style_settings(0)->seed == 42);
    vp_set(VP_BTARGET, 4); ui_input(); all_on();
    assert(!memcmp(&accepted, &gpat, sizeof accepted));
    vp_set(VP_BSEED, 222); ui_input(); seq_block(); all_on(); unchanged_others(3);
    assert(eb_style_settings(3)->seed == 222 && eb_style_kit_config.seed == 101);
    vp_set(VP_BTARGET, 0); assert(vp_get(VP_BSEED) == 101);
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    ui.page = PG_BDETAIL; web_enc(EN_K2, 1); ui_input(); seq_block(); all_on();
    assert(eb_style_kit_config.chaos == 1);
    puts("ok coalesced Kit generation, independent scope settings and detail knob; single-track edits retain other previews");

    action(ACT_COMMIT); assert(!eb_pv.on && eb_kit_undo.mask == 127);
    committed = pat[0];
    action(ACT_UNDO); assert(!memcmp(&stored, &pat[0], sizeof stored));
    action(ACT_UNDO); assert(!memcmp(&committed, &pat[0], sizeof committed));
    kit(); action(ACT_RESET); assert(!eb_pv.on);
    assert(!memcmp(&committed, &pat[0], sizeof committed));
    kit(); action(ACT_UNDO); assert(!eb_pv.on);
    assert(!memcmp(&committed, &pat[0], sizeof committed));
    kit(); vp_set(VP_BTARGET, 3); accepted = gpat; action(ACT_COMMIT);
    assert(!eb_preview_on(&pat[0], 2)); unchanged_others(2);
    for (unsigned t = 0; t < NTRACK; t++) if (t != 2) assert(eb_preview_on(&pat[0], t));
    puts("ok real ACTION knob/OCT routes Kit Commit, Undo/redo, Reset and native per-track Commit correctly");

    fixture(); vp_set(VP_BSEED, 111); vp_set(VP_BTARGET, 2); ui_input();
    assert(!eb_style_live.pending && eb_kit.state == EB_HO_IDLE);
    vp_set(VP_BTARGET, 0); vp_set(VP_BSEED, 112); ui_input();
    assert(eb_kit.state == EB_HO_READY);
    vp_set(VP_BTARGET, 1); seq_block(); assert(!eb_pv.on && eb_kit.state == EB_HO_REJECTED);
    vp_set(VP_BSEED, 333); ui_input(); assert(eb_ho.state == EB_HO_READY);
    vp_set(VP_BTARGET, 0); seq_block(); assert(!eb_pv.on && eb_ho.state == EB_HO_REJECTED);
    for (unsigned mode = 0; mode < 5; mode++) {
        fixture(); vp_set(VP_BSEED, 111); ui_input(); assert(eb_kit.state == EB_HO_READY);
        if (mode == 0) ui.page = PG_ACTION;
        if (mode == 1) ui.mode = M_HOME;
        if (mode == 2) ui.clear_open = 1;
        if (mode == 3) eb_context_changed();
        if (mode == 4) { seq_set_next_pattern(1); seq_set_next_pattern(0); }
        ui_input(); seq_block(); assert(!eb_pv.on && eb_kit.state == EB_HO_REJECTED);
    }
    puts("ok queued UI work cannot leak across target, page, mode, dialog or context changes");

    fixture(); vp_set(VP_BENERGY, 100); ui_input();
    assert(eb_kit.state == EB_HO_READY);
    vp_set(VP_BENERGY, 0); ui_input();
    assert(eb_kit.state == EB_HO_READY); seq_block(); all_on();
    for (unsigned t = 0; t < NTRACK; t++)
        for (unsigned k = 0; k < track_len(&pat[0], t); k++)
            assert(!main_on(&gpat, t, k));
    fixture(); assert(eb_handoff_capture(2));
    vp_set(VP_BSEED, 100); ui_input();
    assert(eb_style_live.pending && eb_ho.state == EB_HO_BUILDING);
    vp_set(VP_BSEED, 200); eb_handoff_submit(0); ui_input(); seq_block(); all_on();
    assert(eb_style_kit_config.seed == 200 && !eb_style_live.pending);
    fixture(); vp_set(VP_BSEED, 100); ui.page = PG_ACTION; ui.action = ACT_UNDO;
    button_down(B_OCTUP); ui.page = PG_BSTYLE; ui_input();
    assert(!eb_style_live.pending && eb_kit.state != EB_HO_READY && !eb_pv.on);
    puts("ok unconsumed Kit requests supersede atomically; builders defer latest work; actions cancel deferred UI input");

    fixture();
    for (unsigned t = 0; t < NTRACK; t++) {
        pat[0].gen[t][0] = GEN_MUTATE;
        vp_set(VP_BSEED, 101 + t); ui_input();
        assert(!eb_style_live.pending && !eb_pv.on && eb_kit.state != EB_HO_READY);
        pat[0].gen[t][0] = GEN_OFF;
    }
    sq.rec = 1; vp_set(VP_BSEED, 500); ui_input(); assert(!eb_style_live.pending);
    sq.rec = 0; sq.song_mode = 1; vp_set(VP_BSEED, 501); ui_input(); assert(!eb_style_live.pending);
    sq.song_mode = 0;
    kit(); accepted = gpat;
    vp_set(VP_BSEED, vp_get(VP_BSEED)); ui_input();
    assert(!eb_style_live.pending && !memcmp(&accepted, &gpat, sizeof accepted));
    ui.page = PG_ACTION; page_params(pp); assert(pp[1] == VP_BTARGET);
    ui.action = ACT_NONE; uint32_t pg, count; title_text(text, sizeof text, &pg, &count); assert(!strcmp(text, "ACTION: KIT"));
    ui.force = 1; ui_draw();
    ui.page = PG_BSTYLE; ui.force = 1; ui_draw();
    ui.page = PG_BDETAIL; ui.force = 1; ui_draw();
    puts("beat_kit_ui_test: ok");
    return 0;
}
