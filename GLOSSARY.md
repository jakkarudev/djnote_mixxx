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

**Cue times**:
The position and time-distance labels shown next to a cue while the pointer is over the cue's line or label. They come from Mixxx and are neither the time ruler nor the hover readout.
_Avoid_: cue tooltip, cue hover time

**Snapped seek**:
A seek from the overview that moves the play position by a whole number of bars or beats along the beat grid, keeping the play position's offset from the grid. Mixxx's Quantize is a different thing.
_Avoid_: quantized seek, grid seek

**Beat line** / **Bar line** / **Phrase line**:
A vertical line drawn on a waveform at the start of a beat, bar or phrase. They are drawn from the beat grid but are not the beat grid.
_Avoid_: bar grid, phrase grid, grid lines

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

**Phrase**:
A run of consecutive bars of the configured phrase length (8 by default), counted from the grid's first beat.
_Avoid_: section, block

**Until Mark**:
An existing readout at the play marker that counts beats from the play position to the next cue or marker. It is not the hover readout.
_Avoid_: beat countdown
