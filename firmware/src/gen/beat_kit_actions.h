/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_KIT_ACTIONS_H
#define ELIXIR_BEAT_KIT_ACTIONS_H

/* Main-loop backend only. Kit scope means all current STYLE previews, not
 * sound models or native live generators. One critical section per action. */
static int eb_kit_work_pending(void)
{
    return eb_ho.state == EB_HO_BUILDING || eb_ho.state == EB_HO_READY ||
           eb_kit.state == EB_HO_BUILDING || eb_kit.state == EB_HO_READY;
}

static int eb_kit_reset(void)
{
    uint32_t t, f = irq_save();
    int changed = eb_kit_work_pending() || eb_pv.on;
    if (eb_ho.state == EB_HO_BUILDING || eb_ho.state == EB_HO_READY)
        eb_ho.state = EB_HO_REJECTED;
    if (eb_kit.state == EB_HO_BUILDING || eb_kit.state == EB_HO_READY)
        eb_kit.state = EB_HO_REJECTED;
    for (t = 0; t < NTRACK; t++)
        eb_preview_discard(t);
    RING_PUBLISH();
    irq_restore(f);
    return changed;
}

static int eb_kit_commit(void)
{
    uint32_t t, f = irq_save();
    uint8_t mask = eb_pv.on;
    pattern_t *p = &pat[sq.active];
    if (!mask || sq.rec || sq.song_mode || eb_kit_work_pending())
        goto blocked;
    /* Validate the entire group before touching stored data or history. */
    for (t = 0; t < NTRACK; t++)
        if ((mask & (1u << t)) &&
            (!eb_preview_on(p, t) || !eb_preview_source_matches(t)))
            goto blocked;
    eb_kit_undo_invalidate();
    undo_t = -1; /* shared upat now belongs to the grouped action */
    for (t = 0; t < NTRACK; t++) {
        if (!(mask & (1u << t))) continue;
        trk_copy(&upat, p, t);
        trk_copy(p, &gpat, t);
        eb_preview_discard(t);
        eb_handoff_read(eb_preview_source(t), p, t);
    }
    eb_kit_undo.pattern = sq.active;
    eb_kit_undo.epoch = eb_context_capture();
    RING_PUBLISH();
    eb_kit_undo.mask = mask;
    irq_restore(f);
    return 1;
blocked:
    irq_restore(f);
    return 0;
}

static int eb_kit_undo_redo(void)
{
    uint32_t t, f = irq_save();
    uint8_t mask;
    pattern_t *p = &pat[sq.active];
    if (sq.rec || sq.song_mode)
        goto blocked;
    /* Like native Undo-before-Commit: cancel previews, not stored data. */
    if (eb_pv.on || eb_kit_work_pending()) {
        int changed = eb_kit_reset();
        irq_restore(f);
        return changed;
    }
    mask = eb_kit_undo.mask;
    if (!mask)
        goto blocked;
    if (eb_kit_undo.pattern != sq.active ||
        !eb_context_matches(eb_kit_undo.epoch))
        goto stale;
    for (t = 0; t < NTRACK; t++)
        if ((mask & (1u << t)) && !eb_preview_source_matches(t))
            goto stale;
    /* No previews are active: gpat is safe native swap scratch. Validate
     * all tracks first, then swap only the committed mask without resetting
     * transport. Shared pattern settings and other tracks remain untouched. */
    for (t = 0; t < NTRACK; t++) {
        if (!(mask & (1u << t))) continue;
        trk_copy(&gpat, p, t);
        trk_copy(p, &upat, t);
        trk_copy(&upat, &gpat, t);
        eb_handoff_read(eb_preview_source(t), p, t);
    }
    sq.gsync |= mask;
    RING_PUBLISH();
    irq_restore(f);
    return 1;
stale:
    eb_kit_undo_invalidate();
blocked:
    irq_restore(f);
    return 0;
}

#endif
