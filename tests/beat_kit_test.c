/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static pattern_t stored, live_before, expected;
static bg_pattern_t generated;
static eb_handoff_track_t a, b;
static eb_style_config_t config;

static void fixture(void)
{
    memset(&sq, 0, sizeof sq);
    par[PAR_PATTERN_MODE] = 0;
    seq_init();
    sq.active = sq.pending = 0;
    sq.running = sq.rec = sq.song_mode = sq.gsync = 0;
    memset(&ui, 0, sizeof ui);
    ui.held_step = -1;
    ui.mode = M_STEP;
    undo_t = -1;
    eb_style_reset();
    config = *eb_style_settings(0);
    config.seed = 173;
    config.style = BG_INTERLOCK;
    config.energy = 85;
    config.chaos = 24;
    config.variation = 35;
    for (uint32_t t = 0; t < NTRACK; t++) {
        static const uint8_t lengths[NTRACK] = {0, 7, 16, 25, 32, 64, 255};
        pat[0].len[t] = lengths[t];
        pat[0].step[t][0].note = (uint8_t)(60u + t);
        main_set(&pat[0], t, 63, 1);
    }
    stored = pat[0];
}

static void expect_kit(void)
{
    bg_settings_t s = {config.seed, config.style, config.energy, config.complexity, 64};
    bg_detail_t saved = bd;
    bd = (bg_detail_t){config.chaos, config.anchor, config.variation, 0};
    assert(bg_generate(&s, &generated));
    bd = saved;
    expected = pat[0];
    for (uint32_t t = 0; t < NTRACK; t++) {
        if (eb_preview_on(&pat[0], t)) trk_copy(&expected, &gpat, t);
        assert(eb_write_lane(&expected, t, &generated, EB_STYLE_PART[t], 96, 127));
    }
}

static void accepted(void)
{
    assert(eb_kit.state == EB_HO_ACCEPTED);
    assert(eb_pv.on == (1u << NTRACK) - 1u);
    for (uint32_t t = 0; t < NTRACK; t++) {
        assert(eb_preview_on(&pat[0], t));
        assert(tpat(t) == &gpat && view_pat(t) == &gpat);
        eb_handoff_read(&a, &expected, t);
        eb_handoff_read(&b, &gpat, t);
        assert(!memcmp(&a, &b, sizeof a));
    }
    assert(!memcmp(&stored, &pat[0], sizeof stored));
}

int main(void)
{
    web_nor_erase(); web_boot();
    fixture(); expect_kit();
    live_before = gpat;
    sq.idx[0] = 5; sq.tph[0] = 9;
    assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
    assert(eb_kit.state == EB_HO_READY && !eb_pv.on);
    assert(!memcmp(&live_before, &gpat, sizeof gpat));
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    assert(!eb_handoff_capture(0));
    assert(!eb_kit_capture());
    seq_block(); accepted();
    assert(sq.idx[0] == 5 && sq.tph[0] == 9);
    puts("ok seven tracks publish together, normal grid follows, stored pattern and position remain intact");

    ui.track = 3; ui.step = 2;
    vp_set(PAR_STEP_NOTE, 79);
    config.seed++;
    expect_kit();
    assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
    seq_block(); accepted();
    assert(gpat.step[3][2].note == 79);
    live_before = gpat;
    assert(eb_style_generate(2, eb_style_settings(2)) == EB_STYLE_QUEUED);
    seq_block();
    for (uint32_t t = 0; t < NTRACK; t++) {
        if (t == 2) continue;
        eb_handoff_read(&a, &live_before, t);
        eb_handoff_read(&b, &gpat, t);
        assert(!memcmp(&a, &b, sizeof a) && eb_preview_on(&pat[0], t));
    }
    puts("ok replacement preserves native step edits; subsequent single-track generation retains other previews");

    for (uint32_t victim = 0; victim < NTRACK; victim++) {
        fixture(); live_before = gpat;
        assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
        pat[0].step[victim][63].p2v++;
        seq_block();
        assert(eb_kit.state == EB_HO_REJECTED && !eb_pv.on);
        assert(!memcmp(&live_before, &gpat, sizeof gpat));
    }
    for (uint32_t field = 0; field < 16; field++) {
        fixture(); live_before = gpat;
        assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
        switch (field) {
        case 0: pat[0].step[6][63].prob++; break;
        case 1: pat[0].timing[6][63]++; break;
        case 2: pat[0].lhi[6][63]++; break;
        case 3: pat[0].cond[6][63]++; break;
        case 4: pat[0].rat[6][63]++; break;
        case 5: pat[0].main[6][3] ^= 1; break;
        case 6: pat[0].gen[6][3]++; break;
        case 7: pat[0].eu2[6][2]++; break;
        case 8: pat[0].mel[6][7]++; break;
        case 9: pat[0].len[6]--; break;
        case 10: pat[0].clk[6]++; break;
        case 11: pat[0].dir[6]++; break;
        case 12: pat[0].bnc[6]++; break;
        case 13: pat[0].sfollow ^= 1u << 6; break;
        case 14: pat[0].gen[6][0] = GEN_MUTATE; break;
        default: pat[0].mel[6][ML_SHAPE] = MEL_RANDOM; break;
        }
        seq_block();
        assert(eb_kit.state == EB_HO_REJECTED && !eb_pv.on);
        assert(!memcmp(&live_before, &gpat, sizeof gpat));
    }
    puts("ok exact guards cover every track, all guarded arrays and metadata; late failure makes no partial writes");

    for (uint32_t mode = 0; mode < 5; mode++) {
        fixture(); live_before = gpat;
        assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
        if (mode == 0) eb_context_changed();
        if (mode == 1) sq.rec = 1;
        if (mode == 2) sq.song_mode = 1;
        if (mode == 3) { seq_set_next_pattern(1); seq_set_next_pattern(0); }
        if (mode == 4) eb_preview_discard(6);
        /* Separate native pattern-reload sync writes from Kit publication. */
        if (sq.gsync) seq_gen_syncs();
        live_before = gpat;
        seq_block();
        assert(eb_kit.state == EB_HO_REJECTED && !eb_pv.on);
        assert(!memcmp(&live_before, &gpat, sizeof gpat));
    }
    puts("ok context replacement, pattern round-trip, recording, song mode and per-track cancellation reject the whole Kit");

    fixture(); expect_kit();
    assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
    seq_block(); accepted(); live_before = gpat;
    assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
    pat[0].step[6][63].note++;
    seq_block();
    assert(eb_kit.state == EB_HO_REJECTED);
    assert(!memcmp(&live_before, &gpat, sizeof gpat));
    for (uint32_t t = 0; t < 6; t++) assert(eb_preview_on(&pat[0], t));
    assert(!eb_preview_on(&pat[0], 6));
    puts("ok failed Kit replacement retains unaffected previous previews");

    fixture();
    assert(eb_handoff_capture(0));
    assert(eb_style_generate_kit(&config) == EB_STYLE_BLOCKED);
    eb_handoff_submit(0);
    config.energy = 101;
    assert(eb_style_generate_kit(&config) == EB_STYLE_INVALID);
    config.energy = 60;
    assert(eb_style_generate_kit_context(&config, eb_context_capture() - 1u, 0) == EB_STYLE_STALE);
    for (uint32_t t = 0; t < NTRACK; t++) {
        pat[0].gen[t][0] = GEN_MUTATE;
        assert(eb_style_generate_kit(&config) == EB_STYLE_BLOCKED);
        pat[0].gen[t][0] = GEN_OFF;
    }
    assert(eb_kit_capture());
    assert(!eb_handoff_capture(0));
    eb_kit.rhythm[6].hit_volumes_only = 2;
    assert(!eb_kit_submit() && eb_kit.state == EB_HO_REJECTED);
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    printf("Kit payload/state: %zu bytes RAMBUF; no additional source snapshots\n", sizeof eb_kit);
    puts("beat_kit_test: ok");
    return 0;
}
