/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

static pattern_t before, after, saved_live, saved_undo;
static eb_style_config_t config;
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
    ui.mode = M_STEP;
    undo_t = -1;
    eb_style_reset();
    config = *eb_style_settings(0);
    config.energy = 100;
    for (uint32_t t = 0; t < NTRACK; t++)
        pat[0].step[t][2].note = (uint8_t)(60u + t);
    before = pat[0];
}

static void publish(void)
{
    assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
    seq_block();
    assert(eb_pv.on == (1u << NTRACK) - 1u);
}

static void same_tracks(const pattern_t *x, const pattern_t *y, uint8_t mask)
{
    for (uint32_t t = 0; t < NTRACK; t++) {
        if (!(mask & (1u << t))) continue;
        eb_handoff_read(&a, x, t); eb_handoff_read(&b, y, t);
        assert(!memcmp(&a, &b, sizeof a));
    }
}

int main(void)
{
    web_nor_erase(); web_boot();
    fixture(); publish();
    ui.track = 4; ui.step = 2; vp_set(PAR_STEP_NOTE, 91);
    after = gpat;
    sq.idx[0] = 5; sq.tph[0] = 9;
    assert(eb_kit_commit());
    assert(!eb_pv.on && undo_t == -1);
    same_tracks(&pat[0], &after, 127);
    assert(eb_kit_undo.mask == 127);
    for (unsigned n = 0; n < 4; n++) {
        assert(eb_kit_undo_redo());
        same_tracks(&pat[0], n & 1u ? &after : &before, 127);
        assert(sq.idx[0] == 5 && sq.tph[0] == 9);
        seq_block();
    }
    puts("ok grouped Commit includes preview edits; atomic Undo/redo repeats without position reset");

    for (unsigned victim = 0; victim < NTRACK; victim++) {
        fixture(); publish(); saved_undo = upat;
        pat[0].step[victim][63].p2v++;
        after = pat[0]; saved_live = gpat;
        assert(!eb_kit_commit());
        assert(!memcmp(&pat[0], &after, sizeof after));
        assert(!memcmp(&gpat, &saved_live, sizeof saved_live));
        assert(!memcmp(&upat, &saved_undo, sizeof saved_undo));
        assert(!eb_kit_undo.mask);
    }
    puts("ok stale member rejects whole Commit before stored or undo-buffer writes");

    for (unsigned victim = 0; victim < NTRACK; victim++) {
        fixture(); publish(); assert(eb_kit_commit());
        pat[0].timing[victim][63]++;
        after = pat[0]; saved_undo = upat;
        assert(!eb_kit_undo_redo());
        assert(!memcmp(&pat[0], &after, sizeof after));
        assert(!memcmp(&upat, &saved_undo, sizeof saved_undo));
        assert(!eb_kit_undo.mask);
    }
    puts("ok exact post-Commit guard rejects stale Undo without partial swaps");

    fixture(); publish(); assert(eb_kit_undo_redo());
    assert(!eb_pv.on && !memcmp(&pat[0], &before, sizeof before));
    assert(!eb_kit_undo_redo());
    publish(); assert(eb_kit_reset());
    assert(!eb_pv.on && !memcmp(&pat[0], &before, sizeof before));
    assert(!eb_kit_reset());
    assert(eb_style_generate_kit(&config) == EB_STYLE_QUEUED);
    assert(!eb_kit_commit()); assert(eb_kit_reset());
    seq_block(); assert(!eb_pv.on && eb_kit.state == EB_HO_REJECTED);
    assert(eb_handoff_capture(2));
    assert(eb_kit_undo_redo());
    assert(eb_ho.state == EB_HO_REJECTED);
    puts("ok Undo-before-Commit and Reset discard previews and cancel both pending backend scopes");

    fixture(); publish();
    eb_preview_discard(6); after = pat[0];
    assert(eb_kit_commit() && eb_kit_undo.mask == 63);
    same_tracks(&pat[0], &after, 64);
    pat[0].step[6][1].note = 99;
    assert(eb_kit_undo_redo());
    same_tracks(&pat[0], &before, 63);
    assert(pat[0].step[6][1].note == 99);
    assert(eb_kit_undo_redo() && pat[0].step[6][1].note == 99);
    puts("ok subset Commit and history operate only on committed tracks");

    fixture(); publish(); assert(eb_kit_commit());
    assert(eb_kit_reset() == 0 && eb_kit_undo_redo());
    assert(eb_kit_undo_redo());
    seq_undo_save(3); assert(!eb_kit_undo.mask);
    assert(!eb_kit_undo_redo());
    fixture(); publish(); assert(eb_kit_commit());
    assert(eb_style_generate(2, eb_style_settings(2)) == EB_STYLE_QUEUED);
    seq_block(); assert(!eb_kit_undo.mask);
    assert(eb_preview_on(&pat[0], 2));
    assert(eb_kit_undo_redo()); assert(!eb_preview_on(&pat[0], 2));
    fixture(); publish(); assert(eb_kit_commit());
    assert(eb_kit_capture() && !eb_kit_undo.mask);
    assert(eb_kit_reset());
    puts("ok shared undo and source-slot reuse explicitly supersede grouped history");

    fixture(); publish(); assert(eb_kit_commit()); after = pat[0];
    sq.rec = 1; assert(!eb_kit_undo_redo()); sq.rec = 0;
    sq.song_mode = 1; assert(!eb_kit_undo_redo()); sq.song_mode = 0;
    eb_context_changed(); assert(!eb_kit_undo_redo());
    assert(!memcmp(&pat[0], &after, sizeof after));
    fixture(); publish(); assert(eb_kit_commit());
    seq_set_next_pattern(1); seq_set_next_pattern(0);
    assert(!eb_kit_undo_redo());
    fixture(); publish(); assert(eb_kit_commit()); seq_init();
    assert(!eb_kit_undo.mask);
    printf("Kit action metadata: %zu bytes RAMBUF; no new pattern or snapshot buffers\n", sizeof eb_kit_undo);
    puts("beat_kit_actions_test: ok");
    return 0;
}
