/* SPDX-License-Identifier: GPL-3.0-only */
/* Standalone checks for the imported Hortator rhythm generator.
 * No Elixir sequencer, sound engine, or UI dependencies.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/gen/beat_gen.h"

static void validate(const bg_settings_t *s, const bg_pattern_t *p)
{
    unsigned lane, step;

    for (lane = 0; lane < BG_LANES; lane++)
        for (step = 0; step < BG_STEPS; step++) {
            const bg_hit_t *h = &p->lane[lane][step];

            assert(h->on <= 1u);
            assert(h->acc <= 1u);
            assert(h->rat <= 3u);

            if (step >= s->length)
                assert(!h->on && !h->acc && !h->rat && !h->cond);

            if (!s->energy)
                assert(!h->on);
        }

    for (step = 0; step < s->length; step++)
        assert(!(p->lane[BG_CH][step].on &&
                 p->lane[BG_OH][step].on));
}

int main(void)
{
    static const unsigned lengths[] = {16, 32, 64};
    static const unsigned energies[] = {0, 50, 100};
    static const unsigned complexities[] = {0, 50, 100};
    static const unsigned seeds[] = {0, 1, 9999};
    bg_pattern_t a, b, before;
    bg_settings_t s;
    unsigned style, l, e, c, n, detail, cases = 0;

    for (detail = 0; detail < 2u; detail++) {
        bd = detail ? (bg_detail_t){60, 75, 50, 40}
                    : (bg_detail_t){0, 100, 0, 0};

        for (style = 0; style < BG_STYLES; style++)
            for (l = 0; l < 3u; l++)
                for (e = 0; e < 3u; e++)
                    for (c = 0; c < 3u; c++)
                        for (n = 0; n < 3u; n++) {
                            s = (bg_settings_t){
                                seeds[n], style, energies[e],
                                complexities[c], lengths[l]
                            };

                            assert(bg_generate(&s, &a));
                            assert(bg_generate(&s, &b));
                            assert(!memcmp(&a, &b, sizeof a));
                            validate(&s, &a);
                            cases++;
                        }
    }
    printf("ok %u generation cases: repeatable, bounded, hats exclusive\n",
           cases);

    bd = (bg_detail_t){0, 100, 0, 0};
    s = (bg_settings_t){1, BG_STRAIGHT, 60, 35, 64};
    memset(&a, 0xA5, sizeof a);
    before = a;

    s.style = BG_STYLES;
    assert(!bg_generate(&s, &a));
    assert(!memcmp(&a, &before, sizeof a));

    s.style = BG_STRAIGHT;
    s.length = 0;
    assert(!bg_generate(&s, &a));
    assert(!memcmp(&a, &before, sizeof a));

    puts("ok invalid style and length leave destination untouched");
    {
        uint8_t map[BG_LANES] = {0, 1, 2, 3, 4, 5, 6, BG_OFF};

        assert(bg_mapping_valid(map));

        map[7] = 0;
        assert(!bg_mapping_valid(map));

        map[7] = BG_LANES;
        assert(!bg_mapping_valid(map));

        memset(map, BG_OFF, sizeof map);
        assert(bg_mapping_valid(map));

        assert(!bg_mapping_valid(NULL));
    }
    puts("ok mapping accepts disabled lanes and rejects duplicates/out-of-range");
    puts("beat_gen_test: ok");
    return 0;
}
