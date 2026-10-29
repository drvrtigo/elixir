/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Drum map subset adapted from juanjiant-bit/JIANT firmware/src/gen_tables.c.
 * Source Git blob: 3a1e917592181dea59104907b36c4679ea108630
 * Map values retained: . rest, 1..9 priority (26*n), X=255.
 * Our density/variation adapter below is NOT JIANT's complete gen_drums().
 */
#ifndef HORTATOR_JIANT_BEATS_H
#define HORTATOR_JIANT_BEATS_H
#define BJ_COUNT 12
static const char bj_maps[BJ_COUNT][8][17]={
    {"X...8...X...8...", "", "....9.......9...", "7.3.7.3.7.3.7.3.", "..8...8...8...8.", "", ".......2......3.", ""},
    {"X...9...X...9...", "", "....6.......6...", "..8...8...8...8.", "", "", "......5.....5..2", ""},
    {"X.....8...8..7..", "....X.......X...", "....5.......5...", "7474747474747474", "", "", "", "6..6..6...6.6..."},
    {"X.6.X.6.X.6.X.6.", "....X.......X...", "....4.......4...", "..7...7...7...7.", "", "", "", ""},
    {"X..6..6.8.6.....", "...3X.3..3.3X.3.", "", "7474757474757474", "..............4.", "", "", ""},
    {"X......6..8.8...", "....X....3..X...", "....3.......3...", "7.5.7.5.7.5.7.5.", "..............3.", "", "", ""},
    {"X......6..9.....", "....9.......9..3", "", "..7...7...7...7.", "......4.......5.", "", ".3..3.3...3..3..", ""},
    {"X.6.......8.....", "....X..3.3..X..3", "", "7.6.7.6.7.6.7.6.", "......5.......5.", "", "", ""},
    {"X.........8.....", "....X.......X..3", "", "6.5.6.5.6.5.6.5.", "", "", ".......3........", ""},
    {"........X.......", "", "", "6.4.6.4.6.4.6.4.", "..............4.", "", "........X.......", ""},
    {"X..X..X.X..X..X.", "....X.......X...", "", "7777777777777777", "", "..........4...5.", "", ""},
    {"X...X...X...X...", "....X.......X...", "....5.......5...", "7676767676767676", "..6...6...6...6.", "..............44", "", ""}
};
static const char *const bg_style_names[BG_STYLES]={"STRAIGHT","BROKEN","SPARSE",
"J HOUSE","J TECHNO","J ELEC","J EBM","J FUNK","J BOOMB","J UKG","J BREAK","J DNB","J 1DROP","J JOYDIV","J NORDER","INTERLCK","FRACTURE"};
static unsigned bj_level(char c) {return c=='X'?255u:c>='1'&&c<='9'?(unsigned)(c-'0')*26u:0u;}
/* Threshold selection preserves the priority ordering; seed perturbs lower-priority
 * positions within each lane. Complexity adds ratchets in later bars. No timbre edits. */
static int bj_generate(const bg_settings_t *s,bg_pattern_t *dst)
{
    static const int offset[8]={40,20,20,0,0,-20,-20,-20};
    unsigned lane,i;
    memset(dst,0,sizeof *dst);
    if(!s->energy)return 1;
    for(lane=0;lane<8;lane++) {
        uint32_t rng=bg_stream(s->seed,s->style,lane);
        for(i=0;i<s->length;i++) {
            bg_hit_t *h=&dst->lane[lane][i];
            unsigned lv=bj_level(bj_maps[s->style-3u][lane][i%16u]);
            unsigned r=bg_rand(&rng),v=bg_rand(&rng);
            int density=20+(int)s->energy*235/100+offset[lane];
            unsigned perturb=s->complexity*40u/100u;
            if(!lv)continue;
            if(lv<234u && perturb)lv+=(r%(perturb+1u));
            if(density<0)density=0;
            if(density>255)density=255;
            if(lv<=255u-(unsigned)density)continue;
            h->on=1;h->acc=(uint8_t)(lv>=234u);
            if(i>=16u && lane>=3u && lv<234u && v%100u<s->complexity/8u)
                h->rat=(uint8_t)(1u+(v>>8)%3u);
        }
    }
    for(i=0;i<s->length;i++)if(dst->lane[BG_OH][i].on)
        memset(&dst->lane[BG_CH][i],0,sizeof(bg_hit_t));
    return 1;
}
#endif
