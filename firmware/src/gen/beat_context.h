/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ELIXIR_BEAT_CONTEXT_H
#define ELIXIR_BEAT_CONTEXT_H

#include <stdint.h>

/*
 * A prepared rhythm belongs to one sequencer/project context.
 * Pattern replacement, reload, project replacement and initialization
 * advance this counter. Ordinary STOP/PLAY must not advance it.
 *
 * Snapshotting this value is not by itself sufficient synchronization.
 * The future request publisher must also validate its target/source
 * state and publish the complete request with the native IRQ/barrier
 * discipline.
 */
static volatile uint32_t eb_context_epoch;

static uint32_t eb_context_capture(void)
{
    return eb_context_epoch;
}

static int eb_context_matches(uint32_t captured)
{
    return captured == eb_context_epoch;
}

static void eb_context_changed(void)
{
    eb_context_epoch++;
    RING_PUBLISH();
}

#endif