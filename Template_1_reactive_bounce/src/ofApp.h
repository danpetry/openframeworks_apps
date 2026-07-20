#pragma once

#include "ofMain.h"
#include "ofxSoundPlayerObject.h"
#include "FftObject.h"
#include "ofxFastFboReader.h"
#include "ofxFFmpegRecorder.h"
//#include "ofVideoGrabber.h"

class ofApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

        // Audio
		ofSoundStream stream;
		ofxSoundOutput output;
		// these are subclasses of ofSoundObject
		ofxSoundPlayerObject player;

		// rendering
		int fboWidth = 1080;
		int fboHeight = 1920;
		ofFbo fboOutput;
		ofFbo lastFboOutput; // previous frame, for feedback/trail ping-pong
		ofxFastFboReader fboReader;
		ofxFFmpegRecorder recorder;
		FftObject fft;
		ofPixels recordPixels; // reused every frame by fboReader.readToPixels()

		// Band extraction: FFT amplitude is sampled on a log-frequency axis and
		// smoothed into `bands`, a 0..1 value per band (low freq -> high freq),
		// for draw() (or a derived sketch) to react to.
		int numBands = 20;
		float sampleRate = 44100.0f;
		float freqMin = 20.0f;			// Hz; freqMax is derived as sampleRate/2 (Nyquist)
		float bandSmoothing = 0.5f;	// 0 = no smoothing, 1 = instant
		float dbMin = -90.0f;
		float dbMax = 0.0f;
		std::vector<float> bands;

		// Diagnostics: running per-band min/max, logged to console periodically.
		// Doubles as a smoke test (nothing moving means the audio pipeline is
		// broken upstream) and tells you the actual dynamic range of a given
		// sample, so you know what values are worth designing around.
		std::vector<float> bandMin, bandMax;
		float lastStatsLogTime = 0.0f;
		float statsLogInterval = 2.0f; // seconds between console summaries

		// Feedback/trail: fraction of background color re-drawn over the previous
		// frame each draw() call. 0 = trails never fade (garbage/full feedback),
		// higher = trails fade faster / are shorter. Tune to taste.
		float trailFadeAlpha = 0.02f;
};
