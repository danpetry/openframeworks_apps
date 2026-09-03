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

    // Samples FFT amplitude on a log-frequency axis and smooths it into
    // `bands` (updated in place; its size determines the number of bands).
    // No-op until audio has actually been processed.
    void updateBands(std::vector<float>& bands, float sampleRate, float freqMin, float dbMin, float dbMax, float smoothing);

    unsigned int bufferSize;

    void setBins(int numChans);
    
	shared_ptr<ofxFft> fft;
	
	ofMutex soundMutex;
	vector<vector<float> >drawBins, middleBins, audioBins;

    int numChannels = 0;
    bool bIsProcessed = false;
};
