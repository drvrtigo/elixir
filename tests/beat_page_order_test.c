/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../web/emu/elixir_web.c"

int main(void)
{
    static const uint16_t expected[11][4] = {
        {VP_BSTYLE, VP_BPART, VP_BENERGY, VP_BSEED},
        {VP_BCOMPLEX, VP_BCHAOS, VP_BVAR, VP_BANCHOR},
        {VP_ACTION, VP_NONE, VP_NONE, VP_NONE},
        {VP_GMODE, VP_GA, VP_GB, VP_GC},
        {VP_MEL + ML_SHAPE, VP_MEL + ML_SPREAD, VP_MEL + ML_BIAS, VP_MEL + ML_STEPS},
        {VP_MEL + ML_DEJA, VP_MEL + ML_LOOP, VP_MEL + ML_LEN, VP_MEL + ML_LVAR},
        {PAR_TRACK_LENGTH, VP_CLOCK, VP_DIR, VP_SFOLLOW},
        {VP_SROOT, VP_SCALE, VP_SFOLLOW, VP_NONE},
        {VP_BSTART, VP_BPITCH, VP_NONE, VP_NONE},
        {VP_LINK, VP_LSTART, VP_LAMT, VP_DEPTH},
        {VP_LWHEN, VP_LTGT, VP_LEFF, VP_LWIN}
    };
    static const char *const titles[11] = {
        "STYLE: OCT GENERATE", "STYLE DETAIL: OCT", "ACTION", "GENERATOR",
        "MELODY", "MELODY: LOOP", "TRACK", "SCALE", "BOUNCE", "LINKS", "LINK SETUP"
    };
    uint16_t pp[4];
    uint32_t pg, count;
    char title[48];
    web_nor_erase();
    web_boot();
    memset(&ui, 0, sizeof ui);
    ui.held_step = -1;
    sq.running = sq.rec = sq.song_mode = 0;
    sq.active = sq.pending = 0;
    kset.pmem = 0;
    mode_set(M_EUCLID);
    assert(ui.page == PG_BSTYLE && PG_BSTYLE == 0u);
    assert(PG_BDETAIL == 1u && PG_ACTION == 2u && PG_GEN == 3u);
    puts("ok generator section opens on STYLE with page memory off");

    pat[0].gen[0][0] = GEN_TURING;
    seq_gen_sync(0);
    for (uint32_t p = 0; p < NEUCLID; p++) {
        ui.page = (uint8_t)p;
        page_params(pp);
        assert(!memcmp(pp, expected[p], sizeof pp));
        title_text(title, sizeof title, &pg, &count);
        assert(pg == p && count == 11u && !strcmp(title, titles[p]));
        ui.force = 1;
        ui_draw();
        assert(body_is_grid == (p < PG_LINKS));
    }
    assert(eb_ho.state == EB_HO_IDLE);
    puts("ok all eleven page titles/cards align; STYLE pages show the normal grid");

    ui.page = 0;
    for (uint32_t p = 1; p < NEUCLID; p++) {
        web_enc(EN_SELECT, 1);
        ui_input();
        assert(ui.page == p);
    }
    web_enc(EN_SELECT, 1); ui_input();
    assert(ui.page == NEUCLID - 1u);
    for (uint32_t p = NEUCLID - 1u; p > 0; p--) {
        web_enc(EN_SELECT, -1); ui_input();
        assert(ui.page == p - 1u);
    }
    web_enc(EN_SELECT, -1); ui_input();
    assert(ui.page == 0u && eb_ho.state == EB_HO_IDLE);
    puts("ok SELECT walks the new order both ways and stops at the ends without generation");

    ui.page = PG_GEN;
    pat[0].gen[0][0] = GEN_EUCLID;
    page_params(pp);
    assert(pp[1] == PAR_EUKLID_LENGTH && pp[2] == PAR_EUKLID_STEPS && pp[3] == PAR_EUKLID_ROTATION);
    ui.page = PG_ACTION;
    page_params(pp);
    assert(pp[1] == VP_EH2 && pp[2] == VP_ER2 && pp[3] == VP_EOP);
    pat[0].gen[0][0] = GEN_GRIDS;
    page_params(pp);
    assert(pp[1] == VP_GX && pp[2] == VP_GY && pp[3] == VP_GCHAOS);
    puts("ok legacy Euclid and GRIDS page specializations follow their new positions");

    pat[0].gen[0][0] = GEN_OFF;
    ui.page = PG_BSTYLE;
    vp_set(VP_BSEED, 43);
    assert(eb_ho.state == EB_HO_IDLE);
    button_down(B_OCTUP);
    assert(eb_ho.state == EB_HO_READY);
    seq_block();
    assert(eb_preview_on(&pat[0], 0));
    ui.page = PG_ACTION; ui.action = ACT_COMMIT;
    button_down(B_OCTUP);
    assert(!eb_preview_on(&pat[0], 0));
    assert(seq_undo(0));
    seq_block();
    puts("ok generation still requires OCT and ACTION Commit/Undo still work");

    ui.shift = 1;
    button_down(B_SEQ);
    assert(ui.page == PG_LINKS);
    puts("ok SHIFT SEQ still opens LINKS");
    puts("beat_page_order_test: ok");
    return 0;
}
