#!/usr/bin/env python3
"""Score DJNote's beat analysis against reference beat grids.

Reference grids come from a Traktor collection.nml (one tempo and one grid
anchor per track). Analysis results come from the beat analysis benchmark in
src/test/beatanalysisbench_test.cpp.

  beat_bench.py refs <collection.nml> <refs.tsv> <list.txt>
      Export every existing, single-anchor Traktor grid to refs.tsv and the
      audio paths to list.txt (the benchmark's input).

  beat_bench.py score <refs.tsv> <bench.tsv> [--worst N] [--csv out.tsv]
      Compare a benchmark run to the references.

Grid error is measured along the whole track: every 5 s, the distance from
the reference beat to the nearest analyzed beat. A track's grid error is the
largest such distance. The BPM is "right" when the analyzed tempo is within
0.02 BPM of the reference; other ratios are reported as tempo-octave errors.
"""

import argparse
import math
import os
import statistics
import sys
import xml.etree.ElementTree as ET

LOSSLESS = (".flac", ".wav", ".aiff", ".aif")

TEMPO_RATIOS = {
    "x2": 2.0,
    "x1/2": 0.5,
    "x3/2": 1.5,
    "x2/3": 2.0 / 3.0,
    "x4/3": 4.0 / 3.0,
    "x3/4": 0.75,
}


def nml_path(location):
    return (location.get("VOLUME") + location.get("DIR").replace("/:", "/") +
            location.get("FILE"))


def cmd_refs(args):
    root = ET.parse(args.collection).getroot()
    refs = []
    for entry in root.iter("ENTRY"):
        location = entry.find("LOCATION")
        tempo = entry.find("TEMPO")
        if location is None or tempo is None:
            continue
        anchors = [c for c in entry.findall("CUE_V2") if c.get("TYPE") == "4"]
        if len(anchors) != 1:
            continue  # no grid, or a multi-tempo grid
        path = nml_path(location)
        if not os.path.exists(path):
            continue
        refs.append((path, float(tempo.get("BPM")),
                     float(anchors[0].get("START")) / 1000.0,
                     1 if entry.get("LOCK") == "1" else 0))
    with open(args.refs, "w", encoding="utf-8") as f:
        f.write("path\tbpm\tanchorSecs\tlocked\n")
        for path, bpm, anchor, locked in refs:
            f.write(f"{path}\t{bpm:.6f}\t{anchor:.6f}\t{locked}\n")
    with open(args.list, "w", encoding="utf-8") as f:
        for ref in refs:
            f.write(ref[0] + "\n")
    print(f"{len(refs)} reference grids")


def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        return [dict(zip(header, line.rstrip("\n").split("\t"))) for line in f]


def norm(path):
    return os.path.normcase(os.path.normpath(path))


def grid_error_secs(ref_bpm, ref_anchor, bpm, anchor, duration):
    """Largest distance from a reference beat to the nearest analyzed beat."""
    ref_len = 60.0 / ref_bpm
    beat_len = 60.0 / bpm
    errors = []
    t = 0.0
    while t <= duration:
        ref_beat = ref_anchor + round((t - ref_anchor) / ref_len) * ref_len
        beat = anchor + round((ref_beat - anchor) / beat_len) * beat_len
        errors.append(beat - ref_beat)
        t += 5.0
    return errors


def classify_tempo(ref_bpm, bpm):
    if abs(bpm - ref_bpm) <= 0.02:
        return "right"
    if abs(bpm - ref_bpm) <= 0.5:
        return "close"
    for name, ratio in TEMPO_RATIOS.items():
        if abs(bpm - ref_bpm * ratio) <= 0.5 * ratio:
            return name
    return "wrong"


def cmd_score(args):
    refs = {norm(r["path"]): r for r in read_tsv(args.refs)}
    rows = []
    seen = set()
    for b in read_tsv(args.bench):
        ref = refs.get(norm(b["path"]))
        if ref is None or norm(b["path"]) in seen:
            continue
        seen.add(norm(b["path"]))
        ext = os.path.splitext(b["path"])[1].lower()
        if args.lossless and ext not in LOSSLESS:
            continue
        ref_bpm = float(ref["bpm"])
        bpm = float(b.get("bpm_" + args.variant, b.get("bpm", 0)))
        first_beat = float(b.get("firstBeat_" + args.variant, b.get("firstBeat", -1)))
        row = {"path": b["path"], "ref_bpm": ref_bpm, "bpm": bpm,
               "ext": os.path.splitext(b["path"])[1].lower(),
               "locked": ref["locked"] == "1"}
        if bpm <= 0:
            row["tempo"] = "failed"
            rows.append(row)
            continue
        sample_rate = float(b["sampleRate"])
        duration = float(b["frames"]) / sample_rate
        anchor = first_beat / sample_rate
        row["tempo"] = classify_tempo(ref_bpm, bpm)
        if row["tempo"] in ("right", "close"):
            errors = grid_error_secs(ref_bpm, float(ref["anchorSecs"]), bpm,
                                     anchor, duration)
            row["max_err_ms"] = max(abs(e) for e in errors) * 1000
            row["mid_err_ms"] = errors[len(errors) // 2] * 1000
            ref_anchor = float(ref["anchorSecs"])
            beats_apart = round((anchor - ref_anchor) / (60.0 / ref_bpm))
            row["bar_ok"] = beats_apart % 4 == 0
        rows.append(row)

    n = len(rows)
    print(f"tracks scored: {n}")
    tempo_counts = {}
    for r in rows:
        tempo_counts[r["tempo"]] = tempo_counts.get(r["tempo"], 0) + 1
    print("tempo:", ", ".join(f"{k} {v} ({100 * v / n:.1f}%)"
                              for k, v in sorted(tempo_counts.items(),
                                                 key=lambda kv: -kv[1])))
    gridded = [r for r in rows if "max_err_ms" in r]
    for limit in (2, 5, 10, 20):
        ok = sum(1 for r in gridded if r["max_err_ms"] <= limit)
        print(f"grid within {limit:>2} ms over the whole track: "
              f"{ok}/{n} ({100 * ok / n:.1f}% of all tracks)")
    if gridded:
        mids = [r["mid_err_ms"] for r in gridded]
        print(f"signed offset at mid-track (analyzed - reference), right tempo: "
              f"median {statistics.median(mids):+.2f} ms, "
              f"mean {statistics.mean(mids):+.2f} ms, "
              f"stdev {statistics.pstdev(mids):.2f} ms")
        by_ext = {}
        for r in gridded:
            by_ext.setdefault(r["ext"], []).append(r["mid_err_ms"])
        for ext, values in sorted(by_ext.items()):
            print(f"  {ext:6} n={len(values):3} median offset "
                  f"{statistics.median(values):+.2f} ms")
    if gridded:
        ok = sum(1 for r in gridded if r["bar_ok"])
        print(f"bar anchor agrees (same downbeat): {ok}/{len(gridded)} of right-tempo grids")
    locked = [r for r in rows if r["locked"]]
    if locked:
        ok = sum(1 for r in locked if r.get("max_err_ms", 1e9) <= 10)
        print(f"locked (hand-checked) references: {ok}/{len(locked)} within 10 ms")

    if args.worst:
        def badness(r):
            return r.get("max_err_ms", 1e6 if r["tempo"] != "failed" else 1e7)
        print(f"\nworst {args.worst}:")
        for r in sorted(rows, key=badness, reverse=True)[:args.worst]:
            err = (f"{r['max_err_ms']:7.1f} ms" if "max_err_ms" in r
                   else "        -")
            print(f"  {r['tempo']:6} ref {r['ref_bpm']:8.3f} got {r['bpm']:8.3f} "
                  f"{err}  {os.path.basename(r['path'])}")
    if args.csv:
        with open(args.csv, "w", encoding="utf-8") as f:
            keys = ["path", "ext", "locked", "ref_bpm", "bpm", "tempo",
                    "max_err_ms", "mid_err_ms", "bar_ok"]
            f.write("\t".join(keys) + "\n")
            for r in rows:
                f.write("\t".join(str(r.get(k, "")) for k in keys) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("refs")
    p.add_argument("collection")
    p.add_argument("refs")
    p.add_argument("list")
    p.set_defaults(func=cmd_refs)
    p = sub.add_parser("score")
    p.add_argument("refs")
    p.add_argument("bench")
    p.add_argument("--worst", type=int, default=0)
    p.add_argument("--variant", default="app",
                   help="which benchmark grid to score (coarse, refined, a sweep name, app)")
    p.add_argument("--lossless", action="store_true",
                   help="only score lossless files (Traktor does not trim MP3/AAC "
                        "decoder delay the way Mixxx does)")
    p.add_argument("--csv")
    p.set_defaults(func=cmd_score)
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    args.func(args)


if __name__ == "__main__":
    main()
