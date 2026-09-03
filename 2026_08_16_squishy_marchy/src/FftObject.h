#pragma once

#include "ofMain.h"
#include "ofxFft.h"
#include "ofxSoundObjects.h"

class FftObject : public ofxSoundObject {
public:
	void setup(unsigned int bufferSize = 2048);
	void plot(vector<float>& buffer, const ofRectangle &r, bool bDrawLogScale = true);
    void process(ofSoundBuffer &input, ofSoundBuffer &output);
    void draw(const ofRectangle & r, bool bDrawLogScale = true);

    // Samples FFT amplitude on a log-frequency axis (averaged across
    // channels) and smooths it into `bands` (updated in place; its size
    // determines the number of bands). No-op until audio has actually been
    // processed.
    // Reads from the mutex-protected `middleBins` snapshot, not from the live
    // `fft` object -- `fft` is written concurrently by the audio callback
    // thread in process(), and reading it directly here raced with those
    // writes, producing spurious broadband spikes in the bands.
    // Attack/release smoothing: `attack` is used when the new value is higher
    // than the current one (fast, so hits stay snappy), `release` when it's
    // lower (slow, so decay looks smooth instead of jittery). Both are lerp
    // rates: 0 = frozen, 1 = instant.
    void updateBands(std::vector<float>& bands, float sampleRate, float freqMin, float dbMin, float dbMax, float attack, float release);

    unsigned int bufferSize;

    void setBins(int numChans);

	shared_ptr<ofxFft> fft;

	ofMutex soundMutex;
	vector<vector<float> >drawBins, middleBins, audioBins;

    int numChannels = 0;
    bool bIsProcessed = false;

private:
    // Interpolated amplitude lookup at a fractional bin index, operating on
    // a plain snapshot vector rather than the live ofxFft object.
    static float getAmplitudeAtBin(const std::vector<float>& amplitude, float bin);
};
