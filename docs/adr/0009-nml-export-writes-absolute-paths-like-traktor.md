# NML export writes absolute paths, as Traktor does

An NML export writes every track's `LOCATION` as an absolute path, the way Traktor's own playlist export does. With copying on, the paths point at the copies in the export folder. With copying off, they point at the original files. Export folders still survive being moved to another place or computer, because the readers look for the file next to the `.nml` when the absolute path no longer exists. DJNote's NML import does this explicitly. Traktor does it in practice: the maintainer has moved export folders between PCs and drives many times, and Traktor always found the tracks beside the `.nml`. The research found no documentation of this, though.

## Considered Options

- **Relative paths (relative to the `.nml`)**: rejected. Traktor never writes them, the NML format has no documented relative form (`VOLUME` and `DIR` are always absolute), and nobody has tested whether Traktor accepts one.
- **Mixxx's "use relative paths on export" preference**: not applied. It exists for M3U/PLS, where relative paths are standard. Following it here would produce files Traktor may not read.

## Consequences

- An export folder can be renamed or moved freely, but a `.nml` copied away *without* its tracks points at wherever the tracks were when it was written.
- DJNote's import must always try `<folder of the .nml>/<FILE>` when the absolute path is missing, before asking the DJ to browse or to match in the library.
