/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_ADAPTER_H
#define ELIXIR_BEAT_ADAPTER_H

/*
 * Include after Elixir's pattern_t, track_len() and main_set(),
 * and after beat_gen.h.
 *
 * Caller owns the destination and synchronization.
 * No generation, allocation, transport changes, or sound writes here.
 *
 * Replace main triggers within the target's existing length.
 * Generated hits receive a playable velocity. All other step fields,
 * inactive-step velocities, other tracks and later steps remain intact.
 */
static int eb_write_lane(pattern_t *dst, unsigned track,
                         const bg_pattern_t *generated, unsigned lane,
                         unsigned normal_velocity, unsigned accent_velocity)
{
    unsigned step, length;

    if (!dst || !generated || track >= NTRACK || lane >= BG_LANES ||
        normal_velocity < 1u || normal_velocity > STEP_VOLUME_MASK ||
        accent_velocity < 1u || accent_velocity > STEP_VOLUME_MASK)
        return 0;

    length = track_len(dst, track);
    if (!length || length > NUM_STEPS || length > BG_STEPS)
        return 0;

    for (step = 0; step < length; step++) {
        const bg_hit_t *hit = &generated->lane[lane][step];

        if (hit->on > 1u || hit->acc > 1u)
            return 0;
    }

    for (step = 0; step < length; step++) {
        const bg_hit_t *hit = &generated->lane[lane][step];

        main_set(dst, track, step, hit->on);
        if (hit->on)
            dst->step[track][step].volume =
                (uint8_t)(STEP_ACTIVE_MASK |
                          (hit->acc ? accent_velocity : normal_velocity));
    }

    return 1;
}

#endif
