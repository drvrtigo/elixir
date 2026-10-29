/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static pattern_t before, expected;
static bg_pattern_t generated;
static eb_handoff_track_t actual_track, expected_track;

static void fixture(void)
{
    memset(&sq, 0, sizeof sq);
    par[PAR_PATTERN_MODE] = 0;
    seq_init();
    sq.active = sq.pending = 0;
    sq.running = sq.rec = sq.song_mode = sq.gsync = 0;
    memset(&ui, 0, sizeof ui);
    ui.held_step = -1;
    ui.mode = M_EUCLID;
    ui.page = PG_BSTYLE;
    undo_t = -1;
    eb_style_reset();
}

static void make_expected(uint32_t t, const eb_style_config_t *c)
{
    bg_detail_t saved = bd;
    bg_settings_t s = {c->seed, c->style, c->energy, c->complexity,
                       (uint8_t)track_len(&pat[0], t)};
    bd = (bg_detail_t){c->chaos, c->anchor, c->variation, 0};
    assert(bg_generate(&s, &generated));
    bd = saved;
    expected = pat[0];
    assert(eb_write_lane(&expected, t, &generated, c->part, 96, 127));
}

static uint32_t count_bar(uint32_t t)
{
    uint32_t start = trig_count[t];
    seq_start(1);
    for (uint32_t k = 0; k < 384u; k++)
        seq_tick_one();
    seq_stop_now();
    return trig_count[t] - start;
}

int main(void)
{
    static const uint8_t lengths[] = {1, 12, 16, 17, 32, 63, 64};
    static const uint8_t energies[] = {0, 60, 100};
    uint32_t cases = 0, t, style, part, l, e;
    uint16_t pp[4];
    char text[32];
    bg_detail_t ambient;
    eb_style_config_t c;
    web_nor_erase();
    web_boot();
    fixture();
    for (style = 0; style < BG_STYLES; style++)
        for (part = 0; part < BG_LANES; part++)
            for (l = 0; l < sizeof lengths / sizeof lengths[0]; l++)
                for (e = 0; e < sizeof energies / sizeof energies[0]; e++) {
                    t = (style + part) % NTRACK;
                    seq_clear_track(t, &pat[0]);
                    pat[0].len[t] = lengths[l];
                    pat[0].step[t][0].note = 63;
                    pat[0].step[t][0].p1v = 41;
                    pat[0].timing[t][0] = 0x71;
                    pat[0].cond[t][0] = COND_FILL;
                    pat[0].rat[t][0] = 2;
                    pat[0].step[t][63].volume = 23;
                    pat[0].main[t][3] = 0x8000u;
                    c = *eb_style_settings(t);
                    c.style = (uint8_t)style;
                    c.part = (uint8_t)part;
                    c.energy = energies[e];
                    before = pat[0];
                    make_expected(t, &c);
                    assert(eb_style_generate(t, &c) == EB_STYLE_QUEUED);
                    assert(eb_ho.state == EB_HO_READY);
                    assert(!memcmp(&before, &pat[0], sizeof before));
                    seq_block();
                    assert(eb_ho.state == EB_HO_ACCEPTED);
                    assert(eb_preview_on(&pat[0], t));
                    eb_handoff_read(&actual_track, &gpat, t);
                    eb_handoff_read(&expected_track, &expected, t);
                    assert(!memcmp(&actual_track, &expected_track, sizeof actual_track));
                    assert(!memcmp(&before, &pat[0], sizeof before));
                    cases++;
                }
    printf("ok %u engine/adapter/handoff cases across every style and part, seven lengths, three energies\n", cases);

    fixture();
    page_params(pp);
    assert(pp[0] == VP_BSTYLE && pp[1] == VP_BPART &&
           pp[2] == VP_BENERGY && pp[3] == VP_BSEED);
    assert(mode_pages() == 11u && PG_ACTION == 1u && PG_LINKS == 7u);
    vp_set(VP_BSTYLE, BG_JTECHNO);
    vp_set(VP_BPART, BG_KICK);
    vp_set(VP_BENERGY, 70);
    vp_set(VP_BSEED, 1234);
    assert(vp_get(VP_BSTYLE) == BG_JTECHNO && vp_get(VP_BSEED) == 1234u);
    card_value(text, sizeof text, VP_BSTYLE, BG_JTECHNO);
    assert(!strcmp(text, bg_style_names[BG_JTECHNO]));
    before = pat[0];
    c = *eb_style_settings(0);
    make_expected(0, &c);
    button_down(B_OCTUP);
    assert(eb_ho.state == EB_HO_READY && !strcmp(ui.msg, "STYLE QUEUED"));
    seq_block();
    assert(tpat(0) == &gpat && view_pat(0) == &gpat);
    eb_handoff_read(&actual_track, &gpat, 0);
    eb_handoff_read(&expected_track, &expected, 0);
    assert(!memcmp(&actual_track, &expected_track, sizeof actual_track));
    assert(!memcmp(&before, &pat[0], sizeof before));
    {
        uint32_t hits = 0;
        for (uint32_t k = 0; k < 16u; k++)
            hits += (uint32_t)main_on(&expected, 0, k);
        assert(hits > 0u && count_bar(0) == hits);
    }
    ui.force = 1; ui_draw();
    puts("ok STYLE controls and OCT feed the expected generated rhythm to native voice triggers");

    ui.page = PG_BDETAIL;
    page_params(pp);
    assert(pp[0] == VP_BCOMPLEX && pp[1] == VP_BCHAOS &&
           pp[2] == VP_BVAR && pp[3] == VP_BANCHOR);
    vp_set(VP_BCOMPLEX, 90); vp_set(VP_BCHAOS, 60);
    vp_set(VP_BVAR, 80); vp_set(VP_BANCHOR, 75);
    assert(vp_get(VP_BCOMPLEX) == 90u && vp_get(VP_BCHAOS) == 60u);
    assert(vp_get(VP_BVAR) == 80u && vp_get(VP_BANCHOR) == 75u);
    c = *eb_style_settings(0);
    ambient = bd = (bg_detail_t){7, 8, 9, 10};
    make_expected(0, &c);
    button_down(B_OCTUP); seq_block();
    assert(!memcmp(&ambient, &bd, sizeof bd));
    assert(gpat.main[0][0] == expected.main[0][0]);
    ui.force = 1; ui_draw();
    ui.track = 1;
    assert(vp_get(VP_BSEED) == 42u);
    ui.track = 0;
    assert(vp_get(VP_BSEED) == 1234u);
    vp_set(VP_BSEED, 10000); assert(vp_get(VP_BSEED) == 9999u);
    vp_set(VP_BENERGY, 101); assert(vp_get(VP_BENERGY) == 100u);
    puts("ok detail controls, per-track settings, clamping, rendering, and engine-global restoration");

    fixture();
    c = *eb_style_settings(0);
    assert(eb_style_generate(0, &c) == EB_STYLE_QUEUED); seq_block();
    assert(eb_style_generate(0, &c) == EB_STYLE_QUEUED);
    assert(eb_style_generate(0, &c) == EB_STYLE_BLOCKED);
    seq_block();
    assert(eb_preview_on(&pat[0], 0));
    c.style = BG_STYLES;
    assert(eb_style_generate(0, &c) == EB_STYLE_INVALID);
    assert(eb_preview_on(&pat[0], 0));
    assert(eb_style_generate(NTRACK, &c) == EB_STYLE_INVALID);
    assert(eb_style_generate(0, 0) == EB_STYLE_INVALID);
    puts("ok busy/invalid generation requests do not discard the current preview");

    fixture();
    c = *eb_style_settings(0);
    assert(eb_style_generate(0, &c) == EB_STYLE_QUEUED);
    seq_set_next_pattern(1); seq_set_next_pattern(0); seq_block();
    assert(eb_ho.state == EB_HO_REJECTED && !eb_preview_on(&pat[0], 0));
    assert(eb_style_generate(0, &c) == EB_STYLE_QUEUED);
    pat[0].step[0][0].note++; seq_block();
    assert(eb_ho.state == EB_HO_REJECTED);
    sq.rec = 1;
    assert(eb_style_generate(0, &c) == EB_STYLE_BLOCKED);
    sq.rec = 0; sq.song_mode = 1;
    assert(eb_style_generate(0, &c) == EB_STYLE_BLOCKED);
    sq.song_mode = 0; pat[0].gen[0][0] = GEN_MUTATE;
    assert(eb_style_generate(0, &c) == EB_STYLE_BLOCKED);
    pat[0].gen[0][0] = GEN_OFF; pat[0].mel[0][ML_SHAPE] = MEL_RANDOM;
    assert(eb_style_generate(0, &c) == EB_STYLE_BLOCKED);
    puts("ok real generation honors context, target-edit, recording, song, and generator guards");

    fixture();
    c = *eb_style_settings(0); c.part = BG_KICK; c.energy = 100;
    assert(eb_style_generate(0, &c) == EB_STYLE_QUEUED); seq_block();
    ui.mode = M_STEP; ui.step = 0;
    vp_set(PAR_STEP_VOLUME, 33); vp_set(PAR_STEP_NOTE, 65);
    c.energy = 0;
    assert(eb_style_generate(0, &c) == EB_STYLE_QUEUED);
    assert(main_on(&gpat, 0, 0));
    seq_block();
    assert(!main_on(&gpat, 0, 0));
    assert((gpat.step[0][0].volume & STEP_VOLUME_MASK) == 33u);
    assert(gpat.step[0][0].note == 65u);
    seq_commit(0);
    assert(pat[0].step[0][0].note == 65u && pat[0].main[0][0] == 0u);
    assert(seq_undo(0)); seq_block();
    assert(pat[0].step[0][0].note == SEQ_DEFAULT_NOTE);
    puts("ok regeneration preserves inactive preview velocities and notes, then Commit/Undo work");

    printf("style workspace/config: %zu bytes; handoff plus preview: %zu bytes\n",
           sizeof bt_work + sizeof eb_style_output + sizeof eb_style_rhythm +
           sizeof eb_style_config + sizeof eb_style_initialized,
           sizeof eb_ho + sizeof eb_pv);
    puts("beat_style_test: ok");
    return 0;
}
