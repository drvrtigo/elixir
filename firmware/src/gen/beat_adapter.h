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
static int eb_lane_valid(const bg_pattern_t *generated, unsigned lane,
                         unsigned length, unsigned normal, unsigned accent)
{
    unsigned step;
    if (!generated || lane >= BG_LANES || !length ||
        length > NUM_STEPS || length > BG_STEPS ||
        normal < 1u || normal > STEP_VOLUME_MASK ||
        accent < 1u || accent > STEP_VOLUME_MASK)
        return 0;
    for (step = 0; step < length; step++)
        if (generated->lane[lane][step].on > 1u ||
            generated->lane[lane][step].acc > 1u)
            return 0;
    return 1;
}

static uint8_t eb_lane_volume(const bg_hit_t *hit, unsigned normal, unsigned accent)
{
    return (uint8_t)(STEP_ACTIVE_MASK | (hit->acc ? accent : normal));
}

static int eb_write_lane(pattern_t *dst, unsigned track,
                         const bg_pattern_t *generated, unsigned lane,
                         unsigned normal_velocity, unsigned accent_velocity)
{
    unsigned step, length;

    if (!dst || track >= NTRACK)
        return 0;
    length = track_len(dst, track);
    if (!eb_lane_valid(generated, lane, length, normal_velocity, accent_velocity))
        return 0;

    for (step = 0; step < length; step++) {
        const bg_hit_t *hit = &generated->lane[lane][step];

        main_set(dst, track, step, hit->on);
        if (hit->on)
            dst->step[track][step].volume =
                eb_lane_volume(hit, normal_velocity, accent_velocity);
    }

    return 1;
}

#endif
