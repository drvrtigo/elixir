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


## STYLE target workflow

STYLE knob 2 is Target: Kit, D1, D2, D3, Snare, Cymbal, Closed Hat,
Open Hat. Kit is the initial scope and generates rhythm across all seven
tracks using the existing role mapping; it never changes sound models.
Selecting a target alone does not generate, commit, or discard previews.
Change a STYLE/STYLE DETAIL parameter or press OCT+ to generate.
Kit and individual tracks retain independent generator settings.

The ACTION page follows the selected scope. Its Kit heading makes grouped
Commit, Undo/redo, and Reset explicit. Accepted previews play and appear in
the normal pattern grid; stored patterns remain unchanged until Commit.
The grouped history uses one shared checkpoint, as documented by the Kit
action backend: new undo saves and reuse of exact source slots supersede it.
No generated pattern or UI target selection is added to the saved format.


## STYLE MAP

STYLE MAP follows STYLE DETAIL, before ACTION. Its four knobs assign D1,
D2, D3, and Cymbal rhythm roles. Defaults remain Kick, Tom, Clap, and Cowbell;
Snare, Closed Hat, and Open Hat stay fixed. There is no dedicated cymbal lane
in the imported generator: Cowbell is a labelled compatibility default.
Duplicate assignments are permitted for intentional layering.

Kit and single-track generation read the same authoritative per-track role
fields. UI requests capture the assignments exactly; affected queued work is
cancelled when an assignment changes. No role hashes or new pattern buffers
are used. Mapping edits are setup-only and retain accepted previews: return
to STYLE and change a generation parameter or press OCT+ to hear the new map.
Mappings are transient, reset at boot, and do not alter sound models, saved
patterns, or project formats. Mapping changes are not part of pattern Undo.
