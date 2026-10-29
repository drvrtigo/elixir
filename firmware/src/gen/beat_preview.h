/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_PREVIEW_H
#define ELIXIR_BEAT_PREVIEW_H

/* A single transient owner of one gpat track. No saved generator enum. */
static struct {
    volatile uint8_t on;
    uint8_t pattern, track;
    uint32_t epoch;
    eb_handoff_track_t source;
} eb_pv POOL;

static void seq_undo_save(uint32_t t);

static void eb_preview_init(void)
{
    memset(&eb_pv, 0, sizeof eb_pv);
}

static int eb_preview_on(const pattern_t *p, uint32_t t)
{
    return t < NTRACK && p == &pat[sq.active] && eb_pv.on &&
           eb_pv.pattern == sq.active && eb_pv.track == t &&
           eb_context_matches(eb_pv.epoch) && !sq.rec && !sq.song_mode &&
           p->gen[t][0] == GEN_OFF && p->mel[t][ML_SHAPE] == MEL_OFF &&
           p->len[t] == eb_pv.source.len;
}

static pattern_t *eb_preview_edit(pattern_t *p, uint32_t t)
{
    return eb_preview_on(p, t) ? &gpat : p;
}

/* Also cancels work still being prepared/queued for this target. */
static int eb_preview_discard(uint32_t t)
{
    uint32_t f = irq_save();
    int was = eb_preview_on(&pat[sq.active], t);
    if (eb_pv.track == t)
        eb_pv.on = 0;
    if (eb_ho.track == t && eb_ho.pattern == sq.active &&
        eb_ho.state != EB_HO_IDLE) {
        RING_PUBLISH();
        eb_ho.state = EB_HO_REJECTED;
    }
    irq_restore(f);
    return was;
}

/* ISR publication. The accepted track is already validated and complete. */
static void eb_preview_accept(void)
{
    const eb_handoff_track_t *s = &eb_ho.accepted;
    uint32_t t = eb_ho.track;
    eb_pv.on = 0;
    memcpy(gpat.step[t], s->step, sizeof s->step);
    memcpy(gpat.timing[t], s->timing, sizeof s->timing);
    memcpy(gpat.lhi[t], s->lhi, sizeof s->lhi);
    memcpy(gpat.cond[t], s->cond, sizeof s->cond);
    memcpy(gpat.rat[t], s->rat, sizeof s->rat);
    memcpy(gpat.main[t], s->main, sizeof s->main);
    memcpy(gpat.gen[t], s->gen, sizeof s->gen);
    memcpy(gpat.eu2[t], s->eu2, sizeof s->eu2);
    memcpy(gpat.mel[t], s->mel, sizeof s->mel);
    gpat.len[t] = s->len;
    gpat.clk[t] = s->clk;
    gpat.dir[t] = s->dir;
    gpat.bnc[t] = s->bnc;
    gpat.sfollow = (uint8_t)((gpat.sfollow & ~(1u << t)) | ((uint32_t)s->sfollow << t));
    memcpy(&eb_pv.source, &eb_ho.before, sizeof eb_pv.source);
    eb_pv.track = (uint8_t)t;
    eb_pv.pattern = eb_ho.pattern;
    eb_pv.epoch = eb_ho.epoch;
    RING_PUBLISH();
    eb_pv.on = 1;
}

/* ISR only: detect external stored-track edits once per block, not per hit.
 * probe is shared with the handoff consumer, which runs later in this ISR.
 */
static void eb_preview_poll(void)
{
    uint32_t t;
    if (!eb_pv.on)
        return;
    t = eb_pv.track;
    if (!eb_preview_on(&pat[sq.active], t)) {
        eb_preview_discard(t);
        return;
    }
    eb_handoff_read(&eb_ho.probe, &pat[sq.active], t);
    if (memcmp(&eb_pv.source, &eb_ho.probe, sizeof eb_pv.source))
        eb_preview_discard(t);
}

/* Main loop: preserve the native one-track undo/redo mechanism. */
static int eb_preview_commit(uint32_t t)
{
    uint32_t f = irq_save();
    if (!eb_preview_on(&pat[sq.active], t)) {
        irq_restore(f);
        return 0;
    }
    eb_handoff_read(&eb_ho.probe, &pat[sq.active], t);
    if (memcmp(&eb_pv.source, &eb_ho.probe, sizeof eb_pv.source)) {
        eb_preview_discard(t);
        irq_restore(f);
        return 1;
    }
    seq_undo_save(t);
    trk_copy(&pat[sq.active], &gpat, t);
    eb_preview_discard(t);
    irq_restore(f);
    return 1;
}

#endif
