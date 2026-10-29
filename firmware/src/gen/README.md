# Seeded rhythm generator import

Imported from drvrtigo/hortator-gen, commit
ee8a2b1f9e8e3640220cc5caaa25ea8b012d481c.

Imported files:

- beat_gen.h
- jiant_beats.h
- beat_abstract.h

The local import was obtained from the corresponding Hortator checkout.

These files provide rhythm generation only. Hortator's sound generation,
UI, playback adapter, preview ownership and storage code are not imported.

Existing source licence and attribution comments are retained.
The imported JIANT map attribution and original source-blob identifier
are also recorded in the repository's NOTICE.md. Existing source-file
notices remain authoritative and must be preserved.

beat_adapter.h is the Elixir-specific translation layer. It changes
main trigger placement and generated-hit velocities only; it does not
change sound engines, kits, transport, or project storage.
