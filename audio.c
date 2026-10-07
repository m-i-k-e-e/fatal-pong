#include "audio.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// WAV clips are embedded in the executable (EMBED_DIR is set by the Makefile), so the game runs from any
// install location or as a bare payload. If the hadouken clip can't be decoded, a formant-synthesized voice
// stands in.
#define EMBED_WAV(name, file)                                           \
    __asm__(".section .rodata\n.balign 16\n"                           \
            #name ":\n.incbin \"" EMBED_DIR file "\"\n"                \
            #name "_end:\n.previous\n");                               \
    extern const Uint8 name[], name##_end[]

EMBED_WAV(hadouken_wav, "hadouken.wav");
EMBED_WAV(tennis_ball_wav, "tennis-ball.wav");
EMBED_WAV(agassi_wins_wav, "agassi-wins.wav");
EMBED_WAV(nadal_wins_wav, "nadal-wins.wav");
EMBED_WAV(finish_him_wav, "finish-him.wav");
EMBED_WAV(fatality_wav, "fatality.wav");
#define AUDIO_RATE          48000
#define MAX_VOICES          4

typedef struct {
    const Sound *sound;
    Uint32 pos;
} Voice;

static SDL_AudioDeviceID audio_dev = 0;
static SDL_AudioSpec audio_spec;
static Voice voices[MAX_VOICES];    // Touched by the audio thread; lock the device to change
Sound snd_hadouken;
Sound snd_paddle_hit;
Sound snd_agassi_wins;
Sound snd_nadal_wins;
Sound snd_finish_him;
Sound snd_fatality;
Sound snd_splat;
Sound snd_rift;
Sound snd_rift_snap;
Sound snd_mole;
Sound snd_thud;
Sound snd_quake;
Sound snd_ribbit;
Sound snd_tongue;
Sound snd_spit;

// Two-pole resonator used as a vocal-tract formant filter
typedef struct { float y1, y2; } Resonator;

// Feed one sample through the resonator: a peak at `freq` Hz, `bw` Hz wide; returns the filtered sample
static float resonate(Resonator *r, float x, float freq, float bw) {
    float rad = expf(-(float)M_PI * bw / AUDIO_RATE);
    float b1 = 2.0f * rad * cosf(2.0f * (float)M_PI * freq / AUDIO_RATE);
    float b2 = -rad * rad;
    float y = (1.0f - rad) * x + b1 * r->y1 + b2 * r->y2;
    r->y2 = r->y1;
    r->y1 = y;
    return y;
}

// Keyframes for "ha-DOU-ken!": formants, pitch, voiced and aspiration levels (linearly interpolated)
typedef struct { float t, f1, f2, pitch, voice, noise; } VoiceKey;

static const VoiceKey HADOUKEN_KEYS[] = {
    { 0.000f, 750, 1250, 170, 0.0f, 0.0f },
    { 0.020f, 750, 1250, 170, 0.0f, 0.6f },  // h
    { 0.070f, 750, 1250, 170, 0.1f, 0.4f },
    { 0.100f, 750, 1250, 175, 1.0f, 0.05f }, // a
    { 0.190f, 700, 1200, 180, 1.0f, 0.05f },
    { 0.210f, 400, 1600, 185, 0.0f, 0.0f },  // d closure
    { 0.235f, 400, 1600, 190, 0.0f, 0.0f },
    { 0.240f, 420, 1500, 200, 0.3f, 0.8f },  // d burst
    { 0.270f, 450,  850, 215, 1.0f, 0.05f }, // ou (stressed, pitch rises)
    { 0.420f, 420,  800, 225, 1.0f, 0.03f },
    { 0.440f, 400,  800, 200, 0.0f, 0.0f },  // k closure
    { 0.480f, 400, 2200, 200, 0.0f, 0.0f },
    { 0.490f, 500, 2200, 200, 0.0f, 1.0f },  // k burst
    { 0.520f, 550, 1850, 190, 1.0f, 0.15f }, // e
    { 0.620f, 500, 1800, 150, 0.9f, 0.0f },
    { 0.660f, 280, 1700, 140, 0.5f, 0.0f },  // n
    { 0.760f, 280, 1700, 120, 0.0f, 0.0f },
};
#define HADOUKEN_KEY_COUNT (int)(sizeof(HADOUKEN_KEYS) / sizeof(HADOUKEN_KEYS[0]))

// Fallback "Hadouken!" when the clip can't be decoded: formant-synthesized voice from HADOUKEN_KEYS, then a
// fireball whoosh, mixed into snd_hadouken
static void synth_hadouken(void) {
    const float voice_len = HADOUKEN_KEYS[HADOUKEN_KEY_COUNT - 1].t;
    const float whoosh_start = 0.45f, whoosh_len = 0.55f;
    int samples = (int)((whoosh_start + whoosh_len) * AUDIO_RATE);
    float *mix = calloc(samples, sizeof(float));
    if (!mix) return;

    Resonator r1 = {0}, r2 = {0}, r3 = {0}, rw = {0};
    float phase = 0.0f, glottal = 0.0f, peak = 0.0001f;
    int key = 0;

    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        float out = 0.0f;

        if (t < voice_len) {
            while (key < HADOUKEN_KEY_COUNT - 2 && t >= HADOUKEN_KEYS[key + 1].t) key++;
            const VoiceKey *a = &HADOUKEN_KEYS[key], *b = &HADOUKEN_KEYS[key + 1];
            float k = (t - a->t) / (b->t - a->t);
            float f1 = a->f1 + (b->f1 - a->f1) * k;
            float f2 = a->f2 + (b->f2 - a->f2) * k;
            float pitch = a->pitch + (b->pitch - a->pitch) * k;
            float voice = a->voice + (b->voice - a->voice) * k;
            float noise = a->noise + (b->noise - a->noise) * k;

            // Softened sawtooth as the glottal source, plus aspiration noise
            phase += pitch / AUDIO_RATE;
            if (phase >= 1.0f) phase -= 1.0f;
            glottal += 0.25f * ((1.0f - 2.0f * phase) - glottal);
            float src = glottal * voice + ((rand() / (float)RAND_MAX) * 2.0f - 1.0f) * noise * 0.5f;

            out = resonate(&r1, src, f1, 90.0f) * 1.0f +
                  resonate(&r2, src, f2, 110.0f) * 0.6f +
                  resonate(&r3, src, 2600.0f, 160.0f) * 0.25f;
        }

        // Fireball whoosh: noise swept down through a resonator
        if (t >= whoosh_start) {
            float w = (t - whoosh_start) / whoosh_len;
            float env = w < 0.1f ? w / 0.1f : (1.0f - w) / 0.9f;
            float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            out += resonate(&rw, n, 1800.0f - 1400.0f * w, 400.0f) * env * 0.35f;
        }

        mix[i] = out;
        if (fabsf(out) > peak) peak = fabsf(out);
    }

    // Normalize into signed 16-bit mono, the format the device was opened with
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (pcm) {
        for (int i = 0; i < samples; i++) pcm[i] = (Sint16)(mix[i] / peak * 0.9f * 32767.0f);
        snd_hadouken.samples = pcm;
        snd_hadouken.count = samples;
    }
    free(mix);
}

// Gore for the fatality: a low thump under a wet, bubbling noise burst that dies away
static void synth_splat(void) {
    int samples = (int)(0.9f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float low = 0.0f, phase = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        phase += (35.0f + 45.0f * expf(-t * 12.0f)) / AUDIO_RATE;
        float thump = sinf(2.0f * (float)M_PI * phase) * expf(-t * 9.0f);
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        low += 0.12f * (n - low);                                   // One-pole low-pass: wet, not hissy
        float bubbles = 0.6f + 0.4f * sinf(2.0f * (float)M_PI * 23.0f * t + 3.0f * sinf(t * 31.0f));
        float squelch = low * 3.5f * bubbles * expf(-t * 5.0f);
        float out = 0.75f * thump + squelch;
        pcm[i] = (Sint16)(SDL_clamp(out, -1.0f, 1.0f) * 0.9f * 32767.0f);
    }
    snd_splat.samples = pcm;
    snd_splat.count = samples;
}

// Rift opening: a low, wobbling hum rising in pitch and loudness over the half second before it snaps
static void synth_rift(void) {
    int samples = (int)(0.55f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f, phase2 = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE, k = t / 0.55f;
        float freq = 70.0f + 90.0f * k * k + 12.0f * sinf(2.0f * (float)M_PI * (6.0f + 20.0f * k) * t);
        phase += freq / AUDIO_RATE;
        phase2 += freq * 1.51f / AUDIO_RATE;                        // Detuned fifth for an eerie beat
        float hum = sinf(2.0f * (float)M_PI * phase) + 0.5f * sinf(2.0f * (float)M_PI * phase2);
        float env = SDL_min(t / 0.05f, 1.0f) * (0.35f + 0.5f * k) * SDL_min((0.55f - t) / 0.03f, 1.0f);
        pcm[i] = (Sint16)(SDL_clamp(hum * env * 0.6f, -1.0f, 1.0f) * 32767.0f);
    }
    snd_rift.samples = pcm;
    snd_rift.count = samples;
}

// Rift swallowing a player: a falling suction sweep with a burst of airy noise
static void synth_rift_snap(void) {
    int samples = (int)(0.5f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f, low = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        phase += (40.0f + 500.0f * expf(-t * 9.0f)) / AUDIO_RATE;
        float sweep = sinf(2.0f * (float)M_PI * phase) * expf(-t * 5.0f);
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        low += 0.3f * (n - low);
        float out = 0.8f * sweep + 0.7f * low * expf(-t * 10.0f);
        pcm[i] = (Sint16)(SDL_clamp(out, -1.0f, 1.0f) * 0.85f * 32767.0f);
    }
    snd_rift_snap.samples = pcm;
    snd_rift_snap.count = samples;
}

// Mole arriving: a low rumble with gritty, scratchy digging bursts, swelling then fading
static void synth_mole(void) {
    int samples = (int)(1.4f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f, low = 0.0f, grit = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        phase += (42.0f + 6.0f * sinf(2.0f * (float)M_PI * 3.0f * t)) / AUDIO_RATE;
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        low += 0.04f * (n - low);                                   // Rumble
        grit += 0.5f * (n - grit);                                  // Scratching
        float scratch = fmodf(t * 7.0f, 1.0f) < 0.35f ? grit * 0.35f : 0.0f;
        float env = sinf((float)M_PI * t / 1.4f);
        float out = (0.5f * sinf(2.0f * (float)M_PI * phase) + 3.0f * low + scratch) * env;
        pcm[i] = (Sint16)(SDL_clamp(out, -1.0f, 1.0f) * 0.85f * 32767.0f);
    }
    snd_mole.samples = pcm;
    snd_mole.count = samples;
}

// Ball hitting a molehill: a short dull thump with a puff of dirt
static void synth_thud(void) {
    int samples = (int)(0.22f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f, low = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        phase += (70.0f + 120.0f * expf(-t * 30.0f)) / AUDIO_RATE;
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        low += 0.2f * (n - low);
        float out = sinf(2.0f * (float)M_PI * phase) * expf(-t * 18.0f) + 0.8f * low * expf(-t * 25.0f);
        pcm[i] = (Sint16)(SDL_clamp(out, -1.0f, 1.0f) * 0.9f * 32767.0f);
    }
    snd_thud.samples = pcm;
    snd_thud.count = samples;
}

// Tremor: a deep rolling rumble with the ground snapping (sparse sharp crackles) as it splits
static void synth_quake(void) {
    int samples = (int)(1.1f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f, low = 0.0f, low2 = 0.0f, crackle = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        phase += (28.0f + 8.0f * sinf(2.0f * (float)M_PI * 1.7f * t)) / AUDIO_RATE;
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        low += 0.03f * (n - low);
        low2 += 0.03f * (low - low2);                               // Two poles: a deep roll
        if (t > 0.05f && t < 0.6f && rand() % 1400 == 0) crackle = 1.0f;
        crackle *= 0.993f;
        float snap = crackle * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
        float env = SDL_min(t / 0.04f, 1.0f) * expf(-t * 2.2f);
        float out = (0.55f * sinf(2.0f * (float)M_PI * phase) + 9.0f * low2) * env + 0.5f * snap;
        pcm[i] = (Sint16)(SDL_clamp(out, -1.0f, 1.0f) * 0.9f * 32767.0f);
    }
    snd_quake.samples = pcm;
    snd_quake.count = samples;
}

// Croak: two buzzy "rib-bit" pulses, a rough sawtooth rolled at ~45 Hz, through a throaty resonance
static void synth_ribbit(void) {
    int samples = (int)(0.32f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    Resonator throat = { 0 };
    float phase = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        bool second = t >= 0.14f;
        float local = second ? t - 0.14f : t, len = second ? 0.16f : 0.1f;
        float env = local < len ? sinf((float)M_PI * local / len) : 0.0f;
        phase += (second ? 150.0f : 175.0f) * (1.0f - 0.25f * local / len) / AUDIO_RATE;
        float saw = 2.0f * (phase - floorf(phase)) - 1.0f;
        float roll = 0.55f + 0.45f * sinf(2.0f * (float)M_PI * 45.0f * t);
        float voice = resonate(&throat, saw * roll, second ? 820.0f : 980.0f, 260.0f);
        pcm[i] = (Sint16)(SDL_clamp(voice * env * 1.6f, -1.0f, 1.0f) * 0.85f * 32767.0f);
    }
    snd_ribbit.samples = pcm;
    snd_ribbit.count = samples;
}

// Tongue: a quick wet "thwip", a sine whipping up in pitch over a short hiss
static void synth_tongue(void) {
    int samples = (int)(0.12f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE, k = t / 0.12f;
        phase += (300.0f + 1500.0f * k * k) / AUDIO_RATE;
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        float env = SDL_min(t / 0.005f, 1.0f) * (1.0f - k);
        float out = (0.8f * sinf(2.0f * (float)M_PI * phase) + 0.25f * n) * env;
        pcm[i] = (Sint16)(SDL_clamp(out, -1.0f, 1.0f) * 0.8f * 32767.0f);
    }
    snd_tongue.samples = pcm;
    snd_tongue.count = samples;
}

// Spit: a "ptoo", a lip pop then a puff of breathy noise
static void synth_spit(void) {
    int samples = (int)(0.2f * AUDIO_RATE);
    Sint16 *pcm = SDL_malloc(samples * sizeof(Sint16));
    if (!pcm) return;
    float phase = 0.0f, low = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / AUDIO_RATE;
        phase += (220.0f * expf(-t * 25.0f) + 90.0f) / AUDIO_RATE;
        float n = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        low += 0.35f * (n - low);
        float pop = sinf(2.0f * (float)M_PI * phase) * expf(-t * 60.0f);
        float puff = low * (t > 0.015f ? expf(-(t - 0.015f) * 22.0f) : 0.0f);
        pcm[i] = (Sint16)(SDL_clamp(pop + 0.9f * puff, -1.0f, 1.0f) * 0.85f * 32767.0f);
    }
    snd_spit.samples = pcm;
    snd_spit.count = samples;
}

// Decode an embedded WAV clip and convert it to the device format; false if unusable
static bool load_wav(const Uint8 *start, const Uint8 *end, Sound *out) {
    SDL_AudioSpec wav_spec;
    Uint8 *wav_buf;
    Uint32 wav_len;
    if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(start, (int)(end - start)), 1, &wav_spec, &wav_buf, &wav_len)) return false;

    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq,
                          audio_spec.format, audio_spec.channels, audio_spec.freq) < 0) {
        SDL_FreeWAV(wav_buf);
        return false;
    }

    cvt.len = wav_len;
    cvt.buf = SDL_malloc(wav_len * cvt.len_mult);
    if (!cvt.buf) { SDL_FreeWAV(wav_buf); return false; }
    SDL_memcpy(cvt.buf, wav_buf, wav_len);
    SDL_FreeWAV(wav_buf);

    if (cvt.needed && SDL_ConvertAudio(&cvt) < 0) { SDL_free(cvt.buf); return false; }
    out->samples = (Sint16 *)cvt.buf;
    out->count = (cvt.needed ? cvt.len_cvt : cvt.len) / sizeof(Sint16);
    return true;
}

// Linear below 70% of full scale, then compresses smoothly so overlapping clips don't hard-clip
static Sint16 soft_limit(int acc) {
    float s = acc / 32768.0f;
    float mag = fabsf(s);
    if (mag > 0.7f) {
        float over = mag - 0.7f;
        s = copysignf(0.7f + 0.3f * over / (over + 0.3f), s);
    }
    return (Sint16)(s * 32767.0f);
}

// Audio thread: sum all active voices through the limiter
static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    Sint16 *out = (Sint16 *)stream;
    int frames = len / (int)sizeof(Sint16);

    for (int i = 0; i < frames; i++) {
        int acc = 0;
        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &voices[v];
            if (!voice->sound) continue;
            acc += voice->sound->samples[voice->pos++];
            if (voice->pos >= voice->sound->count) voice->sound = NULL;
        }
        out[i] = soft_limit(acc);
    }
}

// Open the audio device (mono S16 at AUDIO_RATE, which the mixer relies on), decode the embedded clips and
// synthesize the rest. Any failure is logged and leaves that sound (or all audio) silent rather than stopping
// the game.
void init_audio(void) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        printf("[pong] audio init failed, playing silent: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec want = {0};
    want.freq = AUDIO_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_callback;
    // Exact format required: the mixer assumes mono S16 at AUDIO_RATE
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &audio_spec, 0);
    if (!audio_dev) {
        printf("[pong] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }

    if (!load_wav(hadouken_wav, hadouken_wav_end, &snd_hadouken)) {
        printf("[pong] hadouken.wav unusable (%s), using synthesized hadouken\n", SDL_GetError());
        synth_hadouken();
    }
    if (!load_wav(tennis_ball_wav, tennis_ball_wav_end, &snd_paddle_hit))
        printf("[pong] tennis-ball.wav unusable (%s), paddle hits are silent\n", SDL_GetError());
    if (!load_wav(agassi_wins_wav, agassi_wins_wav_end, &snd_agassi_wins) ||
        !load_wav(nadal_wins_wav, nadal_wins_wav_end, &snd_nadal_wins))
        printf("[pong] announcer clip unusable (%s), wins are silent\n", SDL_GetError());
    if (!load_wav(finish_him_wav, finish_him_wav_end, &snd_finish_him) ||
        !load_wav(fatality_wav, fatality_wav_end, &snd_fatality))
        printf("[pong] fatality announcer clip unusable (%s)\n", SDL_GetError());
    synth_splat();
    synth_rift();
    synth_rift_snap();
    synth_mole();
    synth_thud();
    synth_quake();
    synth_ribbit();
    synth_tongue();
    synth_spit();

    SDL_PauseAudioDevice(audio_dev, 0);
}

// Start a sound on a free voice, or steal the one closest to finishing
void play_sound(const Sound *sound) {
    if (!audio_dev || !sound->samples) return;
    SDL_LockAudioDevice(audio_dev);
    int slot = 0;
    Uint32 least_left = UINT32_MAX;
    for (int v = 0; v < MAX_VOICES; v++) {
        if (!voices[v].sound) { slot = v; break; }
        Uint32 left = voices[v].sound->count - voices[v].pos;
        if (left < least_left) { least_left = left; slot = v; }
    }
    voices[slot].sound = sound;
    voices[slot].pos = 0;
    SDL_UnlockAudioDevice(audio_dev);
}

// Close the device, then free every sound's samples
void shutdown_audio(void) {
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_free(snd_hadouken.samples);
    SDL_free(snd_paddle_hit.samples);
    SDL_free(snd_agassi_wins.samples);
    SDL_free(snd_nadal_wins.samples);
    SDL_free(snd_finish_him.samples);
    SDL_free(snd_fatality.samples);
    SDL_free(snd_splat.samples);
    SDL_free(snd_rift.samples);
    SDL_free(snd_rift_snap.samples);
    SDL_free(snd_mole.samples);
    SDL_free(snd_thud.samples);
    SDL_free(snd_quake.samples);
    SDL_free(snd_ribbit.samples);
    SDL_free(snd_tongue.samples);
    SDL_free(snd_spit.samples);
}
