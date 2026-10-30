/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_PREVIEW_H
#define ELIXIR_BEAT_PREVIEW_H

/* Independent transient owners of gpat tracks. No saved generator enum.
 * Exact source snapshots are split across existing memory sections to keep
 * the RAM/POOL reserve checks intact. This is not a Kit transaction queue.
 */
#define EB_PREVIEW_POOL_TRACKS 3u
static struct {
    volatile uint8_t on; /* bit t: track t has a preview */
    uint8_t pattern, track; /* track: most recently published, for diagnostics */
    uint32_t epoch;
    eb_handoff_track_t source[EB_PREVIEW_POOL_TRACKS];
} eb_pv POOL;
static eb_handoff_track_t eb_pv_extra[NTRACK - EB_PREVIEW_POOL_TRACKS] RAMBUF;

static eb_handoff_track_t *eb_preview_source(uint32_t t)
{
    return t < EB_PREVIEW_POOL_TRACKS ? &eb_pv.source[t]
                                    : &eb_pv_extra[t - EB_PREVIEW_POOL_TRACKS];
}

/* Compare fields directly: no full-track probe copy on every active row. */
static int eb_preview_source_matches(uint32_t t)
{
    const eb_handoff_track_t *s = eb_preview_source(t);
    const pattern_t *p = &pat[sq.active];
    return !memcmp(s->step, p->step[t], sizeof s->step) &&
           !memcmp(s->timing, p->timing[t], sizeof s->timing) &&
           !memcmp(s->lhi, p->lhi[t], sizeof s->lhi) &&
           !memcmp(s->cond, p->cond[t], sizeof s->cond) &&
           !memcmp(s->rat, p->rat[t], sizeof s->rat) &&
           !memcmp(s->main, p->main[t], sizeof s->main) &&
           !memcmp(s->gen, p->gen[t], sizeof s->gen) &&
           !memcmp(s->eu2, p->eu2[t], sizeof s->eu2) &&
           !memcmp(s->mel, p->mel[t], sizeof s->mel) &&
           s->len == p->len[t] && s->clk == p->clk[t] &&
           s->dir == p->dir[t] && s->bnc == p->bnc[t] &&
           s->sfollow == ((p->sfollow >> t) & 1u);
}

static void seq_undo_save(uint32_t t);

static void eb_preview_init(void)
{
    memset(&eb_pv, 0, sizeof eb_pv);
    memset(eb_pv_extra, 0, sizeof eb_pv_extra);
}

static int eb_preview_on(const pattern_t *p, uint32_t t)
{
    return t < NTRACK && p == &pat[sq.active] && (eb_pv.on & (1u << t)) &&
           eb_pv.pattern == sq.active &&
           eb_context_matches(eb_pv.epoch) && !sq.rec && !sq.song_mode &&
           p->gen[t][0] == GEN_OFF && p->mel[t][ML_SHAPE] == MEL_OFF &&
           p->len[t] == eb_preview_source(t)->len;
}

static pattern_t *eb_preview_edit(pattern_t *p, uint32_t t)
{
    return eb_preview_on(p, t) ? &gpat : p;
}

/* Also cancels work still being prepared/queued for this target. */
static int eb_preview_discard(uint32_t t)
{
    uint32_t f;
    int was;
    if (t >= NTRACK)
        return 0;
    f = irq_save();
    was = eb_preview_on(&pat[sq.active], t);
    eb_pv.on &= (uint8_t)~(1u << t);
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
    uint8_t keep = eb_pv.pattern == eb_ho.pattern && eb_pv.epoch == eb_ho.epoch ? eb_pv.on : 0u;
    eb_pv.on = (uint8_t)(keep & ~(1u << t));
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
    memcpy(eb_preview_source(t), &eb_ho.before, sizeof eb_ho.before);
    eb_pv.track = (uint8_t)t;
    eb_pv.pattern = eb_ho.pattern;
    eb_pv.epoch = eb_ho.epoch;
    RING_PUBLISH();
    eb_pv.on = (uint8_t)(keep | (1u << t));
}

/* ISR only: exact source checks once per block, not per hit. */
static void eb_preview_poll(void)
{
    uint32_t t;
    if (!eb_pv.on)
        return;
    for (t = 0; t < NTRACK; t++) {
        if (!(eb_pv.on & (1u << t)))
            continue;
        if (!eb_preview_on(&pat[sq.active], t)) {
            eb_preview_discard(t);
            continue;
        }
        if (!eb_preview_source_matches(t))
            eb_preview_discard(t);
    }
}

/* Main loop: preserve the native one-track undo/redo mechanism. */
static int eb_preview_commit(uint32_t t)
{
    uint32_t f = irq_save();
    if (!eb_preview_on(&pat[sq.active], t)) {
        irq_restore(f);
        return 0;
    }
    if (!eb_preview_source_matches(t)) {
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
