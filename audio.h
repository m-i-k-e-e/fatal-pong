// Sound loading/synthesis and a small software mixer
#ifndef PONG_AUDIO_H
#define PONG_AUDIO_H

#include "SDL2/SDL.h"

typedef struct {
    Sint16 *samples;    // Mono S16 at 48 kHz, the device format
    Uint32 count;
} Sound;

extern Sound snd_hadouken;
extern Sound snd_paddle_hit;
extern Sound snd_agassi_wins;     // Announcer, made by tools/gen_voice.sh
extern Sound snd_nadal_wins;
extern Sound snd_finish_him;
extern Sound snd_fatality;
extern Sound snd_boomerang;     // Fatality: the thrown racket, the loser's scream as the head explodes
extern Sound snd_scream;
#define GRUNT_COUNT 2
extern Sound snd_grunts[GRUNT_COUNT];   // A player grunting on a hit, picked at random
extern Sound snd_splat;         // Synthesized gore for the fatality
extern Sound snd_rift;          // Synthesized: the rift opening, and it swallowing a player
extern Sound snd_rift_snap;
extern Sound snd_mole;          // Synthesized: the mole's arrival (rumble underground), a ball hitting a molehill
extern Sound snd_thud;
extern Sound snd_quake;         // Synthesized: an earthquake tremor, the ground cracking
extern Sound snd_ribbit;        // Synthesized: a frog's croak
extern Sound snd_tongue;        // Synthesized: a frog's tongue shooting out, and the ball spat back out
extern Sound snd_spit;

void init_audio(void);
void play_sound(const Sound *sound);
void shutdown_audio(void);

#endif
