# DJNote

DJNote is a fork of Mixxx aimed at DJs preparing and playing sets. This glossary covers the language of decks, waveforms and track preparation.

## Waveforms

**Scrolling waveform**:
The zoomed-in waveform of a deck that moves past a fixed play marker during playback. Default skins stack all decks' scrolling waveforms together at the top of the screen.
_Avoid_: deck waveform, main waveform, detail waveform

**Overview**:
The whole-track waveform shown inside a deck, at a fixed scale with a moving play marker. This is the waveform DJs usually mean by "the deck waveform".
_Avoid_: deck waveform, mini waveform, track overview

**Play marker**:
The vertical line on a waveform that marks the play position: what the deck is currently playing and you hear.
_Avoid_: playhead, cursor, play position line

**Hover line**:
A vertical line, drawn in the play marker's color, that follows the mouse pointer while it is over the overview. With snapped seeks on, it snaps to the bar line (or, in beats, the beat) nearest the pointer. Depending on the hover mode, it is either the time ruler's line or a separate line.
_Avoid_: cursor, white line, hover marker

**Time ruler**:
The line and time labels (position, remaining time, time distance from the play marker) shown on the overview while right-click is held or during a left-drag seek.
_Avoid_: seek preview, drag ruler

**Hover readout**:
The label next to the hover line that shows the beat distance, in beats or in bars.
_Avoid_: tooltip, beat counter

**Knob adjustment readout**:
The label shown while a knob is being turned, by mouse drag or wheel on screen or from a controller, giving the knob's value. Its style is set on the DJNote preferences page: under the knob (the default), the full value in its own unit (for example dB or percent); or on the knob face, the compact value without its unit, centred on the knob and shrunk to fit, falling back to under the knob when even that doesn't fit. It disappears shortly after the knob stops changing.
_Avoid_: knob tooltip, value popup, knob label

**Cue times**:
The position and time-distance labels shown next to a cue while the pointer is over the cue's line or label. They come from Mixxx and are neither the time ruler nor the hover readout.
_Avoid_: cue tooltip, cue hover time

**Snapped seek**:
A seek from the overview that moves the play position by a whole number of bars or beats along the beat grid, keeping the play position's offset from the grid. Mixxx's Quantize is a different thing.
_Avoid_: quantized seek, grid seek

**Phosphor waveform**:
DJNote's Traktor-like waveform style, offered both as a scrolling waveform and as an overview. Its brightness shows how much of the time the audio spends at each height, and its color shows the audio's frequency content.
_Avoid_: Traktor waveform, Traktor-style waveform, Spectrum waveform

**Density**:
How much of a column's time the audio spends at a given height. On a Phosphor waveform it sets the pixel's brightness, which is why square waves look hollow and noise looks evenly filled.
_Avoid_: grain, texture, glow

**Color track**:
The color of a track's audio over time, derived from its low, mid and high content and smoothed so that it changes gradually. It is separate from the waveform's shape.
_Avoid_: color data, spectrum data

**Waveform detail**:
The preference, from Full down to Low, that sets how much of a loaded track's audio the Phosphor scrolling waveform keeps in memory. Lower detail uses less memory and mostly affects the density, never the outline.
_Avoid_: waveform quality, resolution

**Visual gain**:
A display preference that scales the drawn waveform (All) or its frequency bands (Low, Mid, High). It is not the deck's gain and does not change what you hear.
_Avoid_: waveform gain, display gain

**Follows gain** / **Follows EQ**:
Preferences that decide whether the deck's gain (the gain knob and ReplayGain), and its EQ knobs and kills, change the drawn scrolling waveform. With both off, the waveform shows the track as recorded, as in Traktor.
_Avoid_: Traktor mode, Mixxx mode

**Beat line** / **Bar line** / **Phrase line**:
A vertical line drawn on a waveform at the start of a beat, bar or phrase. They are drawn from the beat grid but are not the beat grid.
_Avoid_: bar grid, phrase grid, grid lines

**Step line**:
A vertical line on a scrolling waveform at each quarter of a beat, between two beat lines: three per beat, sixteen steps per bar. Shown only when zoomed in far enough for the steps to be told apart.
_Avoid_: sixteenth line, subdivision line, in-between line
## Beats

**Beat grid**:
The beat positions analyzed and stored for a track. Beat distance is counted along it.
_Avoid_: beatgrid lines, beats (meaning the grid)

**Beat distance**:
The signed number of beat-grid beats from the play position to another position in the track. It is positive when that position is later in the track. It is counted along the beat grid, so tempo changes, sync and loops do not affect it.
_Avoid_: beats apart, beat offset, beats away

**Bar**:
Four consecutive beats of the beat grid, counted from the grid's first beat. DJNote always assumes 4/4.
_Avoid_: measure

**jkr**:
DJNote's beat detection, offered next to Mixxx's original detectors. A Mixxx detector finds the tempo, then jkr places the beat grid on the kicks with its first beat on a downbeat.
_Avoid_: DJNote detector, refined analyzer

**Downbeat**:
The first beat of a bar. jkr places the beat grid's first beat on the track's first downbeat.
_Avoid_: one, bar start, grid anchor

**Phrase**:
A run of consecutive bars of the configured phrase length (8 by default), counted from the grid's first beat.
_Avoid_: section, block

**Until Mark**:
An existing readout at the play marker that counts beats from the play position to the next cue or marker. It is not the hover readout.
_Avoid_: beat countdown

## Skins

**Skin**:
A complete look and layout for the DJNote window, chosen in Preferences under Interface. Each skin decides which decks, mixer, waveforms and library parts are shown and how they look.
_Avoid_: theme, layout

**Nocturne**:
DJNote's own skin, styled to look as close as possible to Traktor while offering the same layout options as LateNight.
_Avoid_: Traktor skin, Traktor theme, DJNote skin

**Skin settings**:
The options a skin offers for showing or hiding parts of its layout (for example the mixer, effect units, samplers or hotcues), set from the skin's own settings panel rather than from Preferences.
_Avoid_: skin options, layout toggles
