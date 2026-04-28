#ifndef SONIC_H_
#define SONIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SONIC_MIN_PITCH
#define SONIC_MIN_PITCH 65
#endif
#ifndef SONIC_MAX_PITCH
#define SONIC_MAX_PITCH 400
#endif

#define SONIC_MIN_VOLUME 0.01f
#define SONIC_MAX_VOLUME 100.0f
#define SONIC_MIN_SPEED 0.05f
#define SONIC_MAX_SPEED 20.0f
#define SONIC_MIN_PITCH_SETTING 0.05f
#define SONIC_MAX_PITCH_SETTING 20.0f
#define SONIC_MIN_RATE 0.05f
#define SONIC_MAX_RATE 20.0f
#define SONIC_MIN_SAMPLE_RATE 1000
#define SONIC_MAX_SAMPLE_RATE 500000
#define SONIC_MIN_CHANNELS 1
#define SONIC_MAX_CHANNELS 32
#define SONIC_AMDF_FREQ 4000

struct sonicStreamStruct;
typedef struct sonicStreamStruct* sonicStream;

sonicStream sonicCreateStream(int sampleRate, int numChannels);
void sonicDestroyStream(sonicStream stream);
int sonicWriteShortToStream(sonicStream stream, const short* samples, int numSamples);
int sonicReadShortFromStream(sonicStream stream, short* samples, int maxSamples);
int sonicFlushStream(sonicStream stream);
int sonicSamplesAvailable(sonicStream stream);
float sonicGetSpeed(sonicStream stream);
void sonicSetSpeed(sonicStream stream, float speed);
void sonicSetPitch(sonicStream stream, float pitch);
void sonicSetRate(sonicStream stream, float rate);
void sonicSetVolume(sonicStream stream, float volume);
void sonicSetQuality(sonicStream stream, int quality);

#ifdef __cplusplus
}
#endif

#endif
