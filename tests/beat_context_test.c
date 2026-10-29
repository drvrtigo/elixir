/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../web/emu/elixir_web.c"

int main(void)
{
    uint32_t captured;

    /*
     * Data/control-path test only: no web_render(), audio, or flash writes.
     * Disable automatic kit loading for pattern-activation checks.
     */
    memset(&sq, 0, sizeof sq);
    par[PAR_PATTERN_MODE] = 0;
    seq_init();
    sq.running = 0;
    sq.active = sq.pending = 0;

    captured = eb_context_capture();
    assert(eb_context_matches(captured));
    puts("ok unchanged context accepts its captured token");

    seq_set_next_pattern(1);
    assert(sq.active == 1u);
    assert(!eb_context_matches(captured));

    seq_set_next_pattern(0);
    assert(sq.active == 0u);
    assert(!eb_context_matches(captured));
    puts("ok leaving and revisiting a pattern invalidates old work");

    captured = eb_context_capture();
    seq_set_next_pattern(sq.active);
    assert(!eb_context_matches(captured));
    puts("ok same-slot pattern reload invalidates old work");

    captured = eb_context_capture();
    sq.pending = sq.active;
    sq.load_pending = 0;
    seq_pattern_activate();
    assert(eb_context_matches(captured));
    puts("ok activation without replacement leaves the context intact");

    captured = eb_context_capture();
    seq_init();
    assert(!eb_context_matches(captured));
    puts("ok sequencer initialization invalidates old work");

    captured = eb_context_capture();
    eb_context_changed();
    assert(!eb_context_matches(captured));
    puts("ok explicit context replacement invalidates old work");

    puts("beat_context_test: ok");
    return 0;
}