# Preset format

Status: design requirement only; production schema and code are not implemented.

Presets must store schema version, stable preset/node/processor/backend IDs and versions, typed parameter values, bypass, graph connections and channel layouts, routing/mixer state, tempo, quality profile, assets with hashes/provenance, controller assignments, and scene definitions/overrides. They must not rely on absolute platform paths or select a different pitch engine silently between platforms.

Before a format is called production-ready, add a versioned JSON Schema, validation and migration code, atomic save/backup, bounded import, archive path-traversal protection, deterministic missing-asset behavior, round-trip fixtures, and Android/Linux/Windows portability tests.
