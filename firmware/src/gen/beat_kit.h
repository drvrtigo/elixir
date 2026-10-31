/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_KIT_H
#define ELIXIR_BEAT_KIT_H

/* Main-loop capture. Existing live sources must already match exactly before
 * borrowing their slots; failed capture never rewrites an active guard. */
static int eb_kit_capture(void)
{
    uint32_t t, f = irq_save();
    const pattern_t *p;
    if (sq.active >= NUM_PATTERN || sq.rec || sq.song_mode ||
        eb_ho.state == EB_HO_BUILDING || eb_ho.state == EB_HO_READY ||
        eb_kit.state == EB_HO_BUILDING || eb_kit.state == EB_HO_READY)
        goto blocked;
    p = &pat[sq.active];
    for (t = 0; t < NTRACK; t++) {
        if (p->gen[t][0] != GEN_OFF || p->mel[t][ML_SHAPE] != MEL_OFF)
            goto blocked;
        if ((eb_pv.on & (1u << t)) &&
            (!eb_preview_on(p, t) || !eb_preview_source_matches(t)))
            goto blocked;
    }
    eb_kit_undo_invalidate();
    for (t = 0; t < NTRACK; t++)
        eb_handoff_read(eb_preview_source(t), p, t);
    eb_kit.pattern = sq.active;
    eb_kit.epoch = eb_context_capture();
    RING_PUBLISH();
    eb_kit.state = EB_HO_BUILDING;
    irq_restore(f);
    return 1;
blocked:
    irq_restore(f);
    return 0;
}

/* Producer has filled every compact rhythm outside the critical section. */
static int eb_kit_submit(void)
{
    uint32_t t, f = irq_save();
    if (eb_kit.state != EB_HO_BUILDING) {
        irq_restore(f);
        return 0;
    }
    if (!eb_context_matches(eb_kit.epoch) || sq.active != eb_kit.pattern ||
        sq.rec || sq.song_mode)
        goto reject;
    for (t = 0; t < NTRACK; t++)
        if (eb_kit.rhythm[t].hit_volumes_only > 1u)
            goto reject;
    RING_PUBLISH();
    eb_kit.state = EB_HO_READY;
    irq_restore(f);
    return 1;
reject:
    RING_PUBLISH();
    eb_kit.state = EB_HO_REJECTED;
    irq_restore(f);
    return 0;
}

/* Audio ISR, after native sync: validate ALL guards before ANY live writes.
 * No fallible operations remain after validation. Main loop cannot observe
 * intermediate rows; all seven owners publish before sequencing this block. */
static void eb_kit_service(void)
{
    uint32_t t, k;
    uint8_t keep;
    const pattern_t *p;
    if (eb_kit.state == EB_HO_ACCEPTED &&
        (!eb_context_matches(eb_kit.epoch) || sq.active != eb_kit.pattern))
        eb_kit.state = EB_HO_REJECTED;
    if (eb_kit.state != EB_HO_READY)
        return;
    RING_PUBLISH();
    if (!eb_context_matches(eb_kit.epoch) || sq.active != eb_kit.pattern ||
        sq.rec || sq.song_mode)
        goto reject;
    for (t = 0; t < NTRACK; t++)
        if (!eb_preview_source_matches(t))
            goto reject;
    p = &pat[sq.active];
    keep = eb_pv.pattern == eb_kit.pattern && eb_pv.epoch == eb_kit.epoch
         ? eb_pv.on : 0u;
    eb_pv.on = 0;
    for (t = 0; t < NTRACK; t++) {
        const eb_handoff_rhythm_t *r = &eb_kit.rhythm[t];
        if (!(keep & (1u << t)))
            trk_copy(&gpat, p, t);
        for (k = 0; k < track_len(p, t); k++) {
            uint16_t bit = (uint16_t)(1u << (k & 15u));
            if (r->main[k >> 4] & bit)
                gpat.main[t][k >> 4] |= bit;
            else
                gpat.main[t][k >> 4] &= (uint16_t)~bit;
            if (!r->hit_volumes_only || (r->main[k >> 4] & bit))
                gpat.step[t][k].volume = r->volume[k];
        }
    }
    eb_pv.pattern = eb_kit.pattern;
    eb_pv.epoch = eb_kit.epoch;
    eb_pv.track = NTRACK - 1u;
    RING_PUBLISH();
    eb_pv.on = (uint8_t)((1u << NTRACK) - 1u);
    eb_kit.state = EB_HO_ACCEPTED;
    return;
reject:
    RING_PUBLISH();
    eb_kit.state = EB_HO_REJECTED;
}

#endif
