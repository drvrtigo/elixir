/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static eb_handoff_rhythm_t rhythm;
static pattern_t stored, live_before;
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
    ui.bscope = 1; /* retain legacy single-track assertions */
    ui.mode = M_STEP;
    kset.accel = 0;
    undo_t = -1;
    for (uint32_t t = 0; t < NTRACK; t++)
        pat[0].step[t][t].note = (uint8_t)(60u + t);
    stored = pat[0];
}

static void publish(uint32_t t, uint16_t bits)
{
    memset(&rhythm, 0, sizeof rhythm);
    rhythm.main[0] = bits;
    rhythm.hit_volumes_only = 1;
    for (uint32_t k = 0; k < NUM_STEPS; k++) rhythm.volume[k] = 100;
    assert(eb_handoff_capture(t));
    assert(eb_handoff_submit(&rhythm));
    seq_block();
    assert(eb_preview_on(&pat[0], t));
}

static void publish_all(void)
{
    for (uint32_t t = 0; t < NTRACK; t++) publish(t, (uint16_t)(1u << t));
    for (uint32_t t = 0; t < NTRACK; t++) {
        assert(eb_preview_on(&pat[0], t));
        assert(tpat(t) == &gpat && view_pat(t) == &gpat);
        assert(gpat.main[t][0] == (1u << t));
    }
}

static void others_unchanged(uint32_t except)
{
    for (uint32_t t = 0; t < NTRACK; t++) {
        if (t == except) continue;
        eb_handoff_read(&a, &live_before, t);
        eb_handoff_read(&b, &gpat, t);
        assert(!memcmp(&a, &b, sizeof a));
    }
}

int main(void)
{
    uint32_t counts[NTRACK];
    web_nor_erase(); web_boot();
    fixture(); publish_all();
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    for (uint32_t t = 0; t < NTRACK; t++) counts[t] = trig_count[t];
    seq_start(1);
    for (uint32_t k = 0; k < 384u; k++) seq_tick_one();
    seq_stop_now();
    for (uint32_t t = 0; t < NTRACK; t++) {
        assert(trig_count[t] - counts[t] == 1u);
        assert(eb_preview_on(&pat[0], t));
    }
    puts("ok seven independently published previews coexist in playback and the grid");

    live_before = gpat;
    publish(2, 0x0810u);
    assert(gpat.main[2][0] == 0x0810u);
    others_unchanged(2);
    for (uint32_t t = 0; t < NTRACK; t++) assert(eb_preview_on(&pat[0], t));
    assert(!memcmp(&stored, &pat[0], sizeof stored));
    puts("ok regenerating one track retains every other preview byte-for-byte");

    live_before = gpat;
    ui.track = 0; ui.step = 0;
    vp_set(PAR_STEP_NOTE, 73);
    others_unchanged(0);
    assert(gpat.step[0][0].note == 73u && pat[0].step[0][0].note == 60u);
    assert(seq_undo(1));
    assert(!eb_preview_on(&pat[0], 1) && tpat(1) == &pat[0]);
    for (uint32_t t = 0; t < NTRACK; t++)
        if (t != 1u) assert(eb_preview_on(&pat[0], t));
    puts("ok step edits and Undo-before-Commit affect only their selected preview");

    live_before = gpat;
    seq_commit(0);
    assert(!eb_preview_on(&pat[0], 0) && pat[0].step[0][0].note == 73u);
    others_unchanged(0);
    assert(seq_undo(0)); seq_block();
    assert(pat[0].step[0][0].note == 60u);
    others_unchanged(0);
    assert(seq_undo(0)); seq_block();
    assert(pat[0].step[0][0].note == 73u);
    others_unchanged(0);
    for (uint32_t t = 2; t < NTRACK; t++) assert(eb_preview_on(&pat[0], t));
    puts("ok native Commit/Undo/redo scratch use does not clobber other live tracks");

    for (uint32_t victim = 0; victim < NTRACK; victim++) {
        fixture(); publish_all();
        pat[0].step[victim][victim].note++;
        seq_block();
        assert(!eb_preview_on(&pat[0], victim));
        for (uint32_t t = 0; t < NTRACK; t++)
            if (t != victim) assert(eb_preview_on(&pat[0], t));
    }
    puts("ok exact source guards work for every snapshot slot and invalidate only the changed track");

    for (uint32_t field = 0; field < 16u; field++) {
        uint32_t v = field % NTRACK;
        fixture(); publish_all();
        switch (field) {
        case 0: pat[0].step[v][0].note++; break;
        case 1: pat[0].timing[v][0]++; break;
        case 2: pat[0].lhi[v][0]++; break;
        case 3: pat[0].cond[v][0]++; break;
        case 4: pat[0].rat[v][0]++; break;
        case 5: pat[0].main[v][0] ^= 1u; break;
        case 6: pat[0].gen[v][1]++; break;
        case 7: pat[0].eu2[v][1]++; break;
        case 8: pat[0].mel[v][ML_SPREAD]++; break;
        case 9: pat[0].len[v]++; break;
        case 10: pat[0].clk[v]++; break;
        case 11: pat[0].dir[v]++; break;
        case 12: pat[0].bnc[v]++; break;
        case 13: pat[0].sfollow ^= (uint8_t)(1u << v); break;
        case 14: pat[0].step[v][0].prob--; break;
        default: pat[0].step[v][0].p1v++; break;
        }
        seq_block();
        for (uint32_t t = 0; t < NTRACK; t++)
            assert(eb_preview_on(&pat[0], t) == (t != v));
    }
    puts("ok direct-field source checks cover all arrays and track metadata without hashes");

    fixture(); publish_all();
    seq_clear_track(4, &pat[0]);
    assert(!eb_preview_on(&pat[0], 4));
    ui.track = 6; ui.mode = M_EUCLID; ui.page = PG_GEN;
    {
        uint32_t i;
        for (i = 0; i < NGEN && GEN_ORDER[i] != GEN_MUTATE; i++) ;
        assert(i < NGEN); vp_set(VP_GMODE, i);
    }
    seq_block();
    assert(!eb_preview_on(&pat[0], 6) && gen_live(&pat[0], 6));
    for (uint32_t t = 0; t < NTRACK; t++)
        if (t != 4u && t != 6u) assert(eb_preview_on(&pat[0], t));
    puts("ok clear and native-generator takeover leave unrelated previews running");

    fixture(); publish_all();
    sq.gsync = (1u << NTRACK) - 1u;
    live_before = gpat;
    seq_gen_syncs();
    for (uint32_t t = 0; t < NTRACK; t++) {
        assert(eb_preview_on(&pat[0], t));
        eb_handoff_read(&a, &live_before, t); eb_handoff_read(&b, &gpat, t);
        assert(!memcmp(&a, &b, sizeof a));
    }
    seq_set_next_pattern(1); seq_set_next_pattern(0); seq_block();
    assert(eb_pv.on == 0u);
    fixture(); publish_all();
    sq.rec = 1; seq_block(); sq.rec = 0;
    assert(eb_pv.on == 0u);
    puts("ok sync preserves all previews; context replacement and recording expire all affected owners");

    fixture();
    pat[0].gen[6][0] = GEN_MUTATE; pat[0].gen[6][1] = 0;
    main_set(&pat[0], 6, 0, 1); seq_gen_sync(6);
    publish(0, 1); publish(1, 2);
    assert(eb_preview_on(&pat[0], 0) && eb_preview_on(&pat[0], 1));
    assert(!eb_preview_on(&pat[0], 6) && tpat(6) == &gpat);
    assert(gpat.gen[6][0] == GEN_MUTATE && main_on(&gpat, 6, 0));
    puts("ok STYLE previews coexist with a native generator on another track");

    printf("preview storage: POOL %zu B, RAMBUF %zu B, total %zu B; handoff plus preview %zu B\n",
           sizeof eb_pv, sizeof eb_pv_extra, sizeof eb_pv + sizeof eb_pv_extra,
           sizeof eb_ho + sizeof eb_pv + sizeof eb_pv_extra);
    puts("beat_multi_preview_test: ok");
    return 0;
}
