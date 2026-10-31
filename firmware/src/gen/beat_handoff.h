/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_HANDOFF_H
#define ELIXIR_BEAT_HANDOFF_H

/* Main-loop producer, audio-ISR consumer; not a core-1 interface.
 * Include after pattern_t, sq, track_len and melody constants exist.
 * Completed requests also publish to the transient preview owner.
 */
static uint32_t irq_save(void);
static void irq_restore(uint32_t f);

enum { EB_HO_IDLE, EB_HO_BUILDING, EB_HO_READY,
       EB_HO_ACCEPTED, EB_HO_REJECTED };

typedef struct {
    step_t step[NUM_STEPS];
    uint8_t timing[NUM_STEPS], lhi[NUM_STEPS];
    uint8_t cond[NUM_STEPS], rat[NUM_STEPS];
    uint16_t main[4];
    uint8_t gen[4], eu2[3], mel[8];
    uint8_t len, clk, dir, bnc, sfollow;
} eb_handoff_track_t;

typedef struct {
    uint16_t main[4];
    uint8_t volume[NUM_STEPS];
    uint8_t hit_volumes_only; /* 0: legacy full replacement; 1: preserve off-step volumes */
} eb_handoff_rhythm_t;

static struct {
    volatile uint8_t state;
    uint8_t pattern, track;
    uint32_t epoch;
    eb_handoff_track_t before, probe, accepted;
    eb_handoff_rhythm_t rhythm;
} eb_ho POOL;

/* Compact Kit payload; exact guards reuse the seven preview source slots. */
static struct {
    volatile uint8_t state;
    uint8_t pattern;
    uint32_t epoch;
    eb_handoff_rhythm_t rhythm[NTRACK];
} eb_kit RAMBUF;
static void eb_kit_service(void);

static void eb_handoff_init(void)
{
    memset(&eb_ho, 0, sizeof eb_ho);
    memset(&eb_kit, 0, sizeof eb_kit);
}

/* Zero padding as well as fields, so byte comparison is deterministic. */
static void eb_handoff_read(eb_handoff_track_t *d,
                            const pattern_t *p, uint32_t t)
{
    memset(d, 0, sizeof *d);
    memcpy(d->step, p->step[t], sizeof d->step);
    memcpy(d->timing, p->timing[t], sizeof d->timing);
    memcpy(d->lhi, p->lhi[t], sizeof d->lhi);
    memcpy(d->cond, p->cond[t], sizeof d->cond);
    memcpy(d->rat, p->rat[t], sizeof d->rat);
    memcpy(d->main, p->main[t], sizeof d->main);
    memcpy(d->gen, p->gen[t], sizeof d->gen);
    memcpy(d->eu2, p->eu2[t], sizeof d->eu2);
    memcpy(d->mel, p->mel[t], sizeof d->mel);
    d->len = p->len[t];
    d->clk = p->clk[t];
    d->dir = p->dir[t];
    d->bnc = p->bnc[t];
    d->sfollow = (uint8_t)((p->sfollow >> t) & 1u);
}

/* Main loop only. No generation runs with interrupts masked. */
static int eb_handoff_capture(uint32_t t)
{
    uint32_t f = irq_save();
    const pattern_t *p;
    if (t >= NTRACK || sq.active >= NUM_PATTERN || sq.rec || sq.song_mode ||
        eb_ho.state == EB_HO_BUILDING || eb_ho.state == EB_HO_READY ||
        eb_kit.state == EB_HO_BUILDING || eb_kit.state == EB_HO_READY) {
        irq_restore(f);
        return 0;
    }
    p = &pat[sq.active];
    if (p->gen[t][0] != GEN_OFF || p->mel[t][ML_SHAPE] != MEL_OFF) {
        irq_restore(f);
        return 0;
    }
    eb_ho.epoch = eb_context_capture();
    eb_ho.pattern = sq.active;
    eb_ho.track = (uint8_t)t;
    eb_handoff_read(&eb_ho.before, p, t);
    RING_PUBLISH();
    eb_ho.state = EB_HO_BUILDING;
    irq_restore(f);
    return 1;
}

/* Main loop only. Input is an already prepared rhythm, not settings.
 * volume is the native full volume/accent byte. Bits/steps outside the
 * effective track length are ignored by the consumer.
 */
static int eb_handoff_submit(const eb_handoff_rhythm_t *r)
{
    uint32_t f = irq_save();
    if (eb_ho.state != EB_HO_BUILDING) {
        irq_restore(f);
        return 0;
    }
    if (!r || r->hit_volumes_only > 1u || !eb_context_matches(eb_ho.epoch) ||
        sq.active != eb_ho.pattern) {
        eb_ho.state = EB_HO_REJECTED;
        irq_restore(f);
        return 0;
    }
    memcpy(&eb_ho.rhythm, r, sizeof *r);
    RING_PUBLISH();
    eb_ho.state = EB_HO_READY;
    irq_restore(f);
    return 1;
}

/* Audio ISR only, after seq_gen_syncs(). No generation or allocation. */
static void eb_handoff_service(void)
{
    uint32_t t, k, n;
    const pattern_t *p;
    eb_kit_service();
    if (eb_ho.state == EB_HO_ACCEPTED &&
        (!eb_context_matches(eb_ho.epoch) || sq.active != eb_ho.pattern)) {
        RING_PUBLISH();
        eb_ho.state = EB_HO_REJECTED;
    }
    if (eb_ho.state != EB_HO_READY)
        return;
    RING_PUBLISH();
    if (!eb_context_matches(eb_ho.epoch) || sq.active != eb_ho.pattern ||
        sq.rec || sq.song_mode)
        goto reject;
    t = eb_ho.track;
    p = &pat[sq.active];
    eb_handoff_read(&eb_ho.probe, p, t);
    if (memcmp(&eb_ho.before, &eb_ho.probe, sizeof eb_ho.before))
        goto reject;
    if (eb_preview_on(p, t))
        eb_handoff_read(&eb_ho.accepted, &gpat, t);
    else
        memcpy(&eb_ho.accepted, &eb_ho.before, sizeof eb_ho.accepted);
    n = track_len(p, t);
    for (k = 0; k < n; k++) {
        uint16_t bit = (uint16_t)(1u << (k & 15u));
        uint32_t word = k >> 4;
        if (eb_ho.rhythm.main[word] & bit)
            eb_ho.accepted.main[word] |= bit;
        else
            eb_ho.accepted.main[word] &= (uint16_t)~bit;
        if (!eb_ho.rhythm.hit_volumes_only || (eb_ho.rhythm.main[word] & bit))
            eb_ho.accepted.step[k].volume = eb_ho.rhythm.volume[k];
    }
    RING_PUBLISH();
    eb_ho.state = EB_HO_ACCEPTED;
    eb_preview_accept();
    return;
reject:
    RING_PUBLISH();
    eb_ho.state = EB_HO_REJECTED;
}

#endif
