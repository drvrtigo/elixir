/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static eb_handoff_rhythm_t rhythm;
static pattern_t original;

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
    kset.accel = 0;
    undo_t = -1;
    memset(&rhythm, 0, sizeof rhythm);
    rhythm.main[0] = 0x0011u;
    for (uint32_t k = 0; k < NUM_STEPS; k++)
        rhythm.volume[k] = 100;
    original = pat[0];
}

static void publish(uint32_t t)
{
    assert(eb_handoff_capture(t));
    assert(eb_handoff_submit(&rhythm));
    seq_block();
    assert(eb_ho.state == EB_HO_ACCEPTED);
    assert(eb_preview_on(&pat[0], t));
}

static uint32_t play_bar(void)
{
    uint32_t before = trig_count[0];
    seq_start(1);
    for (uint32_t k = 0; k < 384u; k++)
        seq_tick_one();
    seq_stop_now();
    return trig_count[0] - before;
}

int main(void)
{
    uint32_t k, oldsig;
    uint16_t pp[4];
    uint8_t acts[NGACT];
    int found;
    web_nor_erase();
    web_boot();

    fixture();
    oldsig = col_sig(0);
    publish(0);
    assert(tpat(0) == &gpat && view_pat(0) == &gpat);
    assert(tpat(1) == &pat[0] && view_pat(1) == &pat[0]);
    assert(col_sig(0) != oldsig);
    assert(!memcmp(&original, &pat[0], sizeof original));
    assert(play_bar() == 2u);
    assert(eb_preview_on(&pat[0], 0));
    puts("ok preview selects the live grid and reaches two native voice triggers per bar");

    step_toggle(&pat[0], 0, 0);
    assert(!main_on(tpat(0), 0, 0));
    assert(play_bar() == 1u);
    step_toggle(&pat[0], 0, 1);
    assert(main_on(tpat(0), 0, 1));
    assert(play_bar() == 2u);
    assert(!memcmp(&original, &pat[0], sizeof original));
    puts("ok step toggles use preview state, affect playback, and leave stored data untouched");

    ui.step = 1;
    vp_set(PAR_STEP_VOLUME, 83);
    vp_set(PAR_STEP_NOTE, 62);
    vp_set(PAR_STEP_PROB, 73);
    vp_set(PAR_STEP_FLAM, 2);
    vp_set(PAR_STEP_SHIFT, 8);
    vp_set(VP_COND, COND_FILL);
    vp_set(VP_LEN, LEN_8);
    vp_set(VP_RAT_N, 2);
    vp_set(VP_RAT_RATE, 1);
    vp_set(VP_RAT_FADE, RF_DOWN);
    vp_set(VP_RAT_SHAPE, 1);
    vp_set(PAR_P1_DEST, 1);
    vp_set(PAR_P1_VAL, 45);
    vp_set(PAR_P2_DEST, 1);
    vp_set(PAR_P2_VAL, 57);
    assert(vp_get(PAR_STEP_VOLUME) == 83u);
    assert(vp_get(PAR_STEP_PROB) == 73u);
    assert(vp_get(PAR_STEP_FLAM) == 2u);
    assert(vp_get(PAR_STEP_SHIFT) == 8u);
    assert(vp_get(VP_COND) == COND_FILL);
    assert(vp_get(VP_LEN) == LEN_8);
    assert(vp_get(VP_RAT_N) == 2u);
    assert(vp_get(VP_RAT_RATE) == 1u);
    assert(vp_get(VP_RAT_FADE) == RF_DOWN);
    assert(vp_get(VP_RAT_SHAPE) == 1u);
    assert(vp_get(PAR_P1_VAL) == 45u && vp_get(PAR_P2_VAL) == 57u);
    assert(lock_get(&gpat, 0, 1, 0) == par_to_autodest(lock_target_par(1)));
    assert(lock_get(&gpat, 0, 1, 1) == par_to_autodest(lock_target_par(1)));
    assert(ui_step_note_edit(65));
    assert(vp_get(PAR_STEP_NOTE) == 65u);
    live_step_sync(&pat[0], 0, 1);
    assert(gpat.step[0][1].note == 65u);
    assert(!memcmp(&original, &pat[0], sizeof original));
    puts("ok all step-card setters, lock lanes, and MIDI step-note edits stay in preview");

    ui.mode = M_SOUND;
    ui.sect = S_OSC;
    ui.held_step = 2;
    page_params(pp);
    for (k = 0; k < 4u && !par_lockable(pp[k]); k++)
        ;
    assert(k < 4u);
    knob_turn(k, 1);
    assert(main_on(&gpat, 0, 2));
    assert(lock_get(&gpat, 0, 2, 0) == par_to_autodest(pp[k]));
    sq.armed_track = 0;
    sq.armed_step = 3;
    seq_record_automation(0, pp[k], 77);
    assert(gpat.step[0][3].p1v == 77u);
    assert(!memcmp(&original, &pat[0], sizeof original));
    puts("ok held-step and armed-step locks edit the preview without writing the stored pattern");

    fixture(); publish(0);
    ui.step = 4;
    vp_set(PAR_STEP_NOTE, 66);
    assert(eb_handoff_capture(0));
    assert(tpat(0) == &gpat && gpat.step[0][4].note == 66u);
    rhythm.main[0] = 0x0101u;
    assert(eb_handoff_submit(&rhythm));
    assert(gpat.main[0][0] == 0x0011u);
    seq_block();
    assert(gpat.main[0][0] == 0x0101u && gpat.step[0][4].note == 66u);
    puts("ok replacement preparation keeps the old preview playing and preserves its step edits");

    fixture(); publish(0);
    found = 0;
    for (k = 0; k < gen_acts(&pat[0], 0, acts); k++)
        found |= acts[k] == ACT_COMMIT;
    assert(found);
    ui.step = 4;
    vp_set(PAR_STEP_NOTE, 67);
    ui.mode = M_EUCLID;
    ui.page = PG_ACTION;
    ui.action = ACT_COMMIT;
    button_down(B_OCTUP);
    assert(!eb_preview_on(&pat[0], 0));
    assert(pat[0].main[0][0] == 0x0011u && pat[0].step[0][4].note == 67u);
    assert(undo_t == 0 && undo_p == 0u);
    assert(seq_undo(0));
    seq_block();
    assert(!memcmp(&original, &pat[0], sizeof original));
    assert(seq_undo(0));
    seq_block();
    assert(pat[0].main[0][0] == 0x0011u && pat[0].step[0][4].note == 67u);
    puts("ok native ACTION Commit stores preview; native Undo/redo restores the before/after track");

    fixture(); publish(0);
    assert(seq_undo(0));
    assert(tpat(0) == &pat[0] && !memcmp(&original, &pat[0], sizeof original));
    puts("ok Undo before Commit discards preview without requiring an older undo entry");

    fixture(); publish(0);
    seq_start(1); seq_stop_now(); seq_start(1);
    sq.gsync |= 1u;
    seq_gen_syncs();
    assert(eb_preview_on(&pat[0], 0) && gpat.main[0][0] == 0x0011u);
    seq_stop_now();
    ui.mode = M_EUCLID; ui.page = PG_ACTION; ui.action = ACT_RESET;
    button_down(B_OCTUP);
    assert(!eb_preview_on(&pat[0], 0));
    puts("ok STOP/PLAY and sync preserve preview; explicit ACTION Reset discards it");

    fixture(); publish(0);
    for (k = 0; k < NGEN && GEN_ORDER[k] != GEN_MUTATE; k++)
        ;
    assert(k < NGEN);
    vp_set(VP_GMODE, k);
    assert(!eb_preview_on(&pat[0], 0) && pat[0].gen[0][0] == GEN_MUTATE);
    seq_block();
    assert(gen_live(&pat[0], 0));
    fixture(); publish(0);
    vp_set(VP_MEL + ML_SHAPE, MEL_RANDOM);
    assert(!eb_preview_on(&pat[0], 0) && mel_on(&pat[0], 0));
    puts("ok rhythm and melody generator takeovers end preview");

    fixture(); publish(0);
    seq_set_next_pattern(1); seq_set_next_pattern(0);
    assert(tpat(0) == &pat[0]);
    seq_block();
    assert(!eb_pv.on);
    fixture(); publish(0);
    pat[0].step[0][0].note++;
    seq_block();
    assert(!eb_preview_on(&pat[0], 0));
    fixture(); publish(0);
    seq_clear_track(0, &pat[0]);
    assert(!eb_preview_on(&pat[0], 0));
    puts("ok pattern replacement, external target edits, and clear discard preview");

    fixture(); publish(0);
    vp_set(PAR_TRACK_LENGTH, 17);
    assert(!eb_preview_on(&pat[0], 0) && pat[0].len[0] == 17u);
    fixture(); publish(0);
    sq.rec = 1;
    assert(tpat(0) == &pat[0]);
    seq_block();
    sq.rec = 0;
    assert(!eb_preview_on(&pat[0], 0));
    puts("ok structural edits and recording discard rather than resurrect preview");

    fixture(); publish(0);
    ui.track = 1; ui.step = 0;
    vp_set(PAR_STEP_NOTE, 70);
    assert(eb_preview_on(&pat[0], 0) && pat[0].step[1][0].note == 70u);
    seq_block();
    assert(eb_preview_on(&pat[0], 0));
    puts("ok another track's edits do not disturb preview ownership");

    fixture(); publish(0);
    assert(proj_save(0, "PREVIEW ") == 0);
    assert(proj_load(0) == 0);
    assert(!eb_preview_on(&pat[sq.active], 0));
    assert(pat[sq.active].main[0][0] == 0u);
    puts("ok emulator project save excludes uncommitted preview and load invalidates it");

    printf("preview state: %zu bytes; handoff plus preview: %zu bytes\n",
           sizeof eb_pv + sizeof eb_pv_extra, sizeof eb_pv + sizeof eb_pv_extra + sizeof eb_ho);
    puts("beat_preview_test: ok");
    return 0;
}
