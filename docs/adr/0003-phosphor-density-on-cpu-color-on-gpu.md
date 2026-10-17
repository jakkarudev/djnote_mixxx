# Phosphor density is computed on the CPU and cached per column; color is applied on the GPU

The Phosphor scrolling waveform splits its drawing in two. The CPU computes each column's density profile (hits per row) from the sample buffer and caches it per column, positioned along the track. Each frame only computes the columns scrolling into view, and a zoom change or resize recomputes the lane once on a worker. A new allshader renderer uploads those columns into a ring texture, and its shader applies everything that can change every frame: color from the color track, gain and EQ height scaling, visual gains, clipping and the background. The density depends only on the samples, the zoom and the lane height, so it is cached. Color and gain depend on live controls and preferences, so they stay on the GPU, where changing them costs nothing.

## Considered Options

- **Density in a fragment shader**: rejected. Every pixel would loop over all the samples in its window (up to ~1000 at zoom 10), which is about 1.5 G operations per frame for four decks.
- **Everything on the CPU, including color**: rejected. EQ, gain and the color settings would invalidate the cache every frame while a knob moves.

## Consequences

- Zoom is set per bar (px per bar from the track's BPM), so column spacing does not depend on the tempo, and pitch or sync changes don't invalidate the cache.
- The color track stores smoothed band levels rather than final RGB, so the shader can apply EQ and the color settings live.
