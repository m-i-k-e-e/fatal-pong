#!/bin/sh
# Generate the announcer clips (agassi-wins, nadal-wins, graf-wins, sharapova-wins, finish-him, finish-her,
# fatality, da-mole) as .mp3, in a dark, Mortal Kombat style, or hyped like a Ridge Racer race announcer for
# clips marked `arena`. Pass clip names to regenerate only those, e.g.
# `tools/gen_voice.sh fatality`; EXT=wav writes 48 kHz mono 16-bit WAV instead (the game's format).
# Needs Piper TTS with a voice model (the existing clips used en_US-ryan-high), and ffmpeg built with rubberband:
#   uv venv tts && uv pip install --python tts/bin/python piper-tts
#   tts/bin/python -m piper.download_voices en_US-ryan-high
#   PIPER="tts/bin/python -m piper" VOICE=en_US-ryan-high.onnx tools/gen_voice.sh
set -e
PIPER=${PIPER:-"python3 -m piper"}
VOICE=${VOICE:-en_US-ryan-high.onnx}
EXT=${EXT:-mp3}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

PHRASES="Agassi wins!|agassi-wins|dark
Nadal wins!|nadal-wins|dark
Graf wins!|graf-wins|dark
Sharapova wins!|sharapova-wins|dark
Finish him!|finish-him|dark
Finish her!|finish-her|dark
Fatality!|fatality|dark
DA MOLE!|da-mole|arena"

# dark: 0.4 s lead-in so the start isn't swallowed, voice pitched down ~9 semitones plus a layer an octave
# below that, bass, soft saturation and a short, quiet room echo
DARK="[0:a]aresample=48000,apad=pad_dur=0.2,asplit=2[v][sub];
    [v]rubberband=pitch=0.6:tempo=0.92[main];
    [sub]rubberband=pitch=0.3:tempo=0.92,volume=0.5[low];
    [main][low]amix=inputs=2:normalize=0,
        bass=g=7:f=110,
        asoftclip=type=tanh:param=1.2,
        aecho=0.8:0.4:45|100:0.16|0.07,
        adelay=400:all=1,
        apad=pad_dur=0.3,
        loudnorm=I=-14:TP=-1.5"
# arena: bright and excited (pitched up, a touch drawn out), doubled slightly like a stadium PA, compressed,
# presence and air boosted, and a long slap-back echo off the grandstands
ARENA="[0:a]aresample=48000,apad=pad_dur=0.2,asplit=2[v][dbl];
    [v]rubberband=pitch=1.12:tempo=0.88[main];
    [dbl]rubberband=pitch=1.125:tempo=0.88,adelay=18:all=1,volume=0.4[pa];
    [main][pa]amix=inputs=2:normalize=0,
        highpass=f=140,
        acompressor=threshold=0.1:ratio=6:attack=5:release=120:makeup=3,
        equalizer=f=3000:t=q:w=1:g=5,
        treble=g=4:f=7000,
        aecho=0.8:0.55:130|260|420:0.32|0.2|0.1,
        adelay=300:all=1,
        apad=pad_dur=0.6,
        alimiter=level_in=3:limit=0.5:attack=2:release=60,
        loudnorm=I=-12:TP=-1.0"

echo "$PHRASES" | while IFS='|' read -r text clip style; do
    if [ $# -gt 0 ] && ! echo " $* " | grep -q " $clip "; then continue; fi
    out=$clip.$EXT
    if [ "$style" = arena ]; then filter=$ARENA; length=1.2; else filter=$DARK; length=1.3; fi
    if [ "$EXT" = wav ]; then codec="-c:a pcm_s16le"; else codec="-b:a 192k"; fi
    echo "$text" | $PIPER -m "$VOICE" --length-scale $length -f "$TMP/raw.wav"
    ffmpeg -y -loglevel error -i "$TMP/raw.wav" -filter_complex "$filter" -ac 1 -ar 48000 $codec "$out"
    echo "wrote $out"
done
