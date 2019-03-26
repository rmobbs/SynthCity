#pragma once

#include "SDL.h"
#include "BaseTypes.h"
#include <string>

/*
 * Maximum number of sample frames that will ever be
 * processed in one go. Audio processing callbacks
 * rely on never getting a 'frames' argument greater
 * than this value.
 */
#define	SM_MAXFRAGMENT	256

#define	SM_C0		16.3515978312874


/*--------------------------------------------------------
	Application Interface
--------------------------------------------------------*/

int sm_open(int buffer, int numVoices);
int sm_addvoice();
void sm_capvoices(int numVoices);
void sm_close(void);
int sm_load(int sound, const char *file);
int sm_load(const char *file);
int sm_load_synth(int sound, const char *def);
void sm_unload(int sound);

/*
 * IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT!
 *
 *	The callbacks installed by the functions below
 *	will run in the SDL audio callback context!
 *	Thus, you must be very careful what you do in
 *	these callbacks, and what data you touch.
 *
 * IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT!
 */

/*
 * Install a control tick callback. This will be called
 * once as soon as possible, and then the callback's return
 * value determines how many audio samples to process before
 * the callback is called again.
 *    If the callback returns 0, it is uninstalled and never
 * called again.
 *    Use sm_set_control_cb(NULL) to remove any installed
 * callback instantly.
 */
typedef unsigned (*sm_control_cb)(void* payload);
void sm_set_control_cb(sm_control_cb cb, void* payload);

/*
 * Install an audio processing callback. This callback runs
 * right after the voice mixer, and may be used to analyze
 * or modify the audio stream before it is converted and
 * passed on to the audio output buffer.
 *    The buffer handed to the callback is in 32 bit signed
 * stereo format, and the 'frames' argument is the number
 * of full stereo samples to process.
 *    Use sm_set_audio_cb(NULL) to remove any installed
 * callback instantly.
 */
typedef void (*sm_audio_cb)(Sint32 *buf, int frames);
void sm_set_audio_cb(sm_audio_cb cb);

typedef void(*sm_subdivision_callback)(void* payload);
void sm_set_subdivision_callback(sm_subdivision_callback cb, void *payload);


/*--------------------------------------------------------
	Real Time Control Interface
	(Use only from inside a control callback,
	or with the SDL audio thread locked!)
--------------------------------------------------------*/

/* Start playing 'sound' on 'voice' at L/R volumes 'lvol'/'rvol' */
void sm_play(unsigned voice, unsigned sound, float lvol, float rvol);

/* Set voice decay speed */
void sm_decay(unsigned voice, float decay);

/* If the pending interval > interval, cut it short. */
void sm_force_interval(unsigned interval);

/* Get the pending interval length */
int sm_get_interval(void);

/* Get number of frames left to next control callback */
int sm_get_next_tick(void);

class Mixer {
public:
  class Sound
  {
  public:
    std::string filename;
    Uint8	*data = nullptr;
    Uint8 *readbuf = nullptr;
    Uint32	length = 0;
    Uint8 channels = 0;
    float	decay = 0.0f;

    // These are for synth, should be separated
    float	pitch = 0.0f;		/* Pitch (60.0 <==> middle C) */
    float	fm = 0.0f;		/* Synth FM depth */

    void Unload();
  };

  class Voice
  {
  public:
    int	sound = -1;
    int	position = 0;
    // 8:24 fixed point
    int	lvol = 0;
    int	rvol = 0;
    // 16:16 fixed point
    int	decay = 0;
  };
};