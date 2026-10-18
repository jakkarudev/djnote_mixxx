---
status: proposed
---

# The record label is stored as a new library column

The JKR-Parallel skin shows a track's record label on the deck and as a sortable LABEL column in the library, as Traktor does. Mixxx already reads the label from file tags but only keeps it in memory, so DJNote adds a `label` column to the library database, filled when tracks are imported or their tags are re-read. This changes the schema. Mixxx's database upgrades only run forwards, so once DJNote has upgraded a library, upstream Mixxx and older DJNote builds can't safely open it again. We accepted that because DJNote keeps its own library and is not used alongside upstream Mixxx.

## Considered Options

- **Reuse an existing field, such as Grouping, for the label**: rejected. No schema change, but that field could no longer be used for its own purpose, and tags written back to files would put the label in the wrong tag.
- **Read the label from the file's tags when it is shown, without storing it**: rejected. The column could not be sorted or searched, and reading tags while drawing the table is slow.
