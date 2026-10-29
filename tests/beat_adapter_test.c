/* SPDX-License-Identifier: GPL-3.0-only */
/* Test the imported rhythm adapter against Elixir's actual native types. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../web/emu/elixir_web.c"
#include "../firmware/src/gen/beat_gen.h"
#include "../firmware/src/gen/beat_adapter.h"

static pattern_t actual, expected, before;
static bg_pattern_t generated;

static void fixture(unsigned track, unsigned raw_length)
{
    unsigned t, s;

    /*
     * Fill every byte, including untouched fields and padding, so the
     * whole-pattern comparison detects unintended writes.
     * This is a data-only fixture: it is never rendered or played.
     */
    memset(&actual, 0x5A, sizeof actual);
    for (t = 0; t < NTRACK; t++)
        actual.len[t] = 16;
    actual.len[track] = (uint8_t)raw_length;

    for (t = 0; t < NTRACK; t++)
        for (s = 0; s < NUM_STEPS; s++) {
            actual.step[t][s].volume =
                (uint8_t)((s & 1u ? STEP_ACTIVE_MASK : 0u) |
                          (1u + s % 100u));
            actual.step[t][s].note = (uint8_t)(36u + s % 48u);
            step_len_set(&actual, t, s, s % NLEN);
        }

    memset(&generated, 0, sizeof generated);
    for (t = 0; t < BG_LANES; t++)
        for (s = 0; s < BG_STEPS; s++) {
            generated.lane[t][s].on =
                (uint8_t)((s + t) % 3u != 0u);
            generated.lane[t][s].acc =
                (uint8_t)((s + t) % 4u == 0u);
        }

    memcpy(&before, &actual, sizeof before);
    memcpy(&expected, &actual, sizeof expected);
}

static void expect_lane(unsigned track, unsigned lane,
                        unsigned length, unsigned normal, unsigned accent)
{
    unsigned s;

    /* Independent expectation: do not use main_set() or the adapter. */
    for (s = 0; s < length; s++) {
        uint16_t bit = (uint16_t)(1u << (s & 15u));
        const bg_hit_t *h = &generated.lane[lane][s];

        if (h->on) {
            expected.main[track][s >> 4] |= bit;
            expected.step[track][s].volume =
                (uint8_t)(STEP_ACTIVE_MASK | (h->acc ? accent : normal));
        } else {
            expected.main[track][s >> 4] &= (uint16_t)~bit;
        }
    }
}

static void unchanged(void)
{
    assert(!memcmp(&actual, &before, sizeof actual));
}

int main(void)
{
    static const unsigned lengths[] = {0, 1, 12, 16, 32, 63, 64};
    unsigned t, lane, l, cases = 0;

    for (t = 0; t < NTRACK; t++)
        for (lane = 0; lane < BG_LANES; lane++)
            for (l = 0; l < sizeof lengths / sizeof lengths[0]; l++) {
                unsigned length = lengths[l] ? lengths[l] : 16u;

                fixture(t, lengths[l]);
                expect_lane(t, lane, length, 96, 127);

                assert(eb_write_lane(&actual, t, &generated, lane,
                                     96, 127));
                assert(!memcmp(&actual, &expected, sizeof actual));
                cases++;
            }

    printf("ok %u native adapter cases: only triggers and hit velocities change\n",
           cases);

    fixture(0, 64);
    assert(!eb_write_lane(NULL, 0, &generated, 0, 96, 127));
    assert(!eb_write_lane(&actual, 0, NULL, 0, 96, 127));
    assert(!eb_write_lane(&actual, NTRACK, &generated, 0, 96, 127));
    assert(!eb_write_lane(&actual, 0, &generated, BG_LANES, 96, 127));
    assert(!eb_write_lane(&actual, 0, &generated, 0, 0, 127));
    assert(!eb_write_lane(&actual, 0, &generated, 0, 128, 127));
    assert(!eb_write_lane(&actual, 0, &generated, 0, 96, 0));
    assert(!eb_write_lane(&actual, 0, &generated, 0, 96, 128));
    unchanged();
    puts("ok invalid pointers, tracks, lanes and velocities write nothing");

    fixture(0, 64);
    generated.lane[0][63].on = 2;
    assert(!eb_write_lane(&actual, 0, &generated, 0, 96, 127));
    unchanged();

    fixture(0, 64);
    generated.lane[0][63].acc = 2;
    assert(!eb_write_lane(&actual, 0, &generated, 0, 96, 127));
    unchanged();
    puts("ok invalid final hit is rejected before any partial write");

    fixture(0, 12);
    generated.lane[0][63].on = 2;
    expect_lane(0, 0, 12, 96, 127);
    assert(eb_write_lane(&actual, 0, &generated, 0, 96, 127));
    assert(!memcmp(&actual, &expected, sizeof actual));
    puts("ok adapter ignores generated data outside the target length");

    {
        bg_settings_t settings = {42, BG_STRAIGHT, 60, 35, 64};

        fixture(0, 64);
        bd = (bg_detail_t){0, 100, 0, 0};
        assert(bg_generate(&settings, &generated));
        expect_lane(0, BG_KICK, 64, 96, 127);
        assert(eb_write_lane(&actual, 0, &generated, BG_KICK, 96, 127));
        assert(!memcmp(&actual, &expected, sizeof actual));
    }
    puts("ok real generated kick lane adapts to native pattern fields");

    puts("beat_adapter_test: ok");
    return 0;
}
