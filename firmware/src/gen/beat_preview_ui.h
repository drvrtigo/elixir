/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_PREVIEW_UI_H
#define ELIXIR_BEAT_PREVIEW_UI_H

static int eb_preview_step_param(uint32_t p)
{
    switch (p) {
    case PAR_STEP_VOLUME: case PAR_STEP_NOTE: case PAR_STEP_PROB:
    case PAR_STEP_FLAM: case PAR_STEP_SHIFT:
    case PAR_P1_DEST: case PAR_P2_DEST: case PAR_P1_VAL: case PAR_P2_VAL:
    case VP_COND: case VP_LEN: case VP_RAT_N: case VP_RAT_RATE:
    case VP_RAT_FADE: case VP_RAT_SHAPE:
        return 1;
    default:
        return 0;
    }
}

/* Structural edits and competing generators end this transient preview.
 * Their existing native behavior then operates on the stored pattern.
 */
static int eb_preview_track_param(uint32_t p)
{
    switch (p) {
    case PAR_TRACK_LENGTH:
    case PAR_EUKLID_LENGTH: case PAR_EUKLID_STEPS: case PAR_EUKLID_ROTATION:
    case VP_EH2: case VP_ER2: case VP_EOP:
    case VP_GMODE: case VP_GA: case VP_GB: case VP_GC:
    case VP_CLOCK: case VP_DIR: case VP_BSTART: case VP_BPITCH:
    case VP_SFOLLOW:
    case VP_MEL ... VP_MEL_LAST:
        return 1;
    default:
        return 0;
    }
}

#endif
