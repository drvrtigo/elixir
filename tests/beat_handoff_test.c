/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static eb_handoff_rhythm_t rhythm;
static pattern_t saved_pattern;

static void fixture(void)
{
    uint32_t k;
    memset(&sq, 0, sizeof sq);
    par[PAR_PATTERN_MODE] = 0;
    seq_init();
    sq.running = sq.rec = sq.song_mode = 0;
    sq.active = sq.pending = 0;
    sq.gsync = 0;
    seq_clear_pattern(&pat[0]);
    seq_clear_pattern(&pat[1]);
    memset(&eb_ho, 0, sizeof eb_ho);
    memset(&rhythm, 0, sizeof rhythm);
    rhythm.main[0] = 0x0011u;
    for (k = 0; k < NUM_STEPS; k++)
        rhythm.volume[k] = 100;
    rhythm.volume[0] |= STEP_ACTIVE_MASK;
}

int main(void)
{
    fixture();
    pat[0].step[0][0].note = 63;
    pat[0].step[0][0].p1v = 41;
    pat[0].timing[0][0] = 0x71u;
    pat[0].cond[0][0] = COND_FILL;
    saved_pattern = pat[0];
    assert(eb_handoff_capture(0));
    assert(!eb_handoff_capture(1));
    assert(eb_handoff_submit(&rhythm));
    assert(eb_ho.state == EB_HO_READY);
    assert(!memcmp(&saved_pattern, &pat[0], sizeof saved_pattern));
    seq_block();
    assert(eb_ho.state == EB_HO_ACCEPTED);
    assert(eb_ho.accepted.main[0] == 0x0011u);
    assert(eb_ho.accepted.step[0].volume == (100u | STEP_ACTIVE_MASK));
    assert(eb_ho.accepted.step[0].note == 63u);
    assert(eb_ho.accepted.step[0].p1v == 41u);
    assert(eb_ho.accepted.timing[0] == 0x71u);
    assert(eb_ho.accepted.cond[0] == COND_FILL);
    assert(!memcmp(&saved_pattern, &pat[0], sizeof saved_pattern));
    puts("ok complete request accepted at seq_block without editing the pattern");

    fixture();
    assert(eb_handoff_capture(0));
    eb_context_changed();
    assert(!eb_handoff_submit(&rhythm));
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok context change during preparation rejects submission");

    fixture();
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    seq_set_next_pattern(1);
    seq_set_next_pattern(0);
    seq_block();
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok queued work cannot survive leaving and revisiting a slot");

    fixture();
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    pat[0].step[0][0].note++;
    seq_block();
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok changed target data rejects queued work");

    fixture();
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    pat[0].len[0] = 32;
    seq_block();
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok changed target length rejects queued work");

    fixture();
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    pat[0].step[1][0].note++;
    saved_pattern = pat[0];
    seq_block();
    assert(eb_ho.state == EB_HO_ACCEPTED);
    assert(!memcmp(&saved_pattern, &pat[0], sizeof saved_pattern));
    puts("ok another track's edit is preserved and does not reject the request");

    fixture();
    pat[0].len[0] = 17;
    pat[0].main[0][3] = 0x8000u;
    pat[0].step[0][63].volume = 23;
    rhythm.main[1] = 1;
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    seq_block();
    assert(eb_ho.state == EB_HO_ACCEPTED);
    assert(eb_ho.accepted.main[1] == 1u);
    assert(eb_ho.accepted.main[3] == 0x8000u);
    assert(eb_ho.accepted.step[63].volume == 23u);
    puts("ok partial-bar length preserves steps beyond the target length");

    fixture();
    assert(!eb_handoff_capture(NTRACK));
    pat[0].gen[0][0] = GEN_MUTATE;
    assert(!eb_handoff_capture(0));
    pat[0].gen[0][0] = GEN_OFF;
    pat[0].mel[0][ML_SHAPE] = (uint8_t)(MEL_OFF + 1u);
    assert(!eb_handoff_capture(0));
    puts("ok invalid target and existing generators refuse capture");

    fixture();
    sq.rec = 1;
    assert(!eb_handoff_capture(0));
    sq.rec = 0;
    sq.song_mode = 1;
    assert(!eb_handoff_capture(0));
    sq.song_mode = 0;
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    sq.rec = 1;
    seq_block();
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok recording/song guards apply at capture and consumption");

    fixture();
    assert(eb_handoff_capture(0));
    assert(eb_handoff_submit(&rhythm));
    seq_reset_tracks();
    seq_block();
    assert(eb_ho.state == EB_HO_ACCEPTED);
    eb_context_changed();
    seq_block();
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok position reset keeps the request; context replacement expires acceptance");

    printf("handoff storage: %zu bytes\n", sizeof eb_ho);
    puts("beat_handoff_test: ok");
    return 0;
}
