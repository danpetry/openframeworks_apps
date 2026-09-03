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
		void exit();

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
		// FftObject is declared here (not with the rest of "rendering" below)
		// deliberately: C++ destroys members in reverse declaration order, and
		// player.connectTo(fft).connectTo(output) means fft must outlive player
		// and output must outlive fft when they're torn down, or their
		// destructors dereference a dangling ofxSoundObject::outputObjectRef.
		// This order (output, fft, player) destructs as (player, fft, output).
		FftObject fft;
		// these are subclasses of ofSoundObject
		ofxSoundPlayerObject player;

		// If true (default), prompts for an audio file every run and remembers
		// the choice (in bin/data/lastAudioFile.txt). If false, skips the
		// dialog and reloads whichever file was last picked -- falls back to
		// the dialog once if nothing's been remembered yet.
		bool bShowFilePicker = false;

		// rendering
		int fboWidth = 1080;
		int fboHeight = 1920;
		ofFbo fboOutput;
		ofFbo lastFboOutput; // previous frame, for feedback/trail ping-pong
		ofxFastFboReader fboReader;
		ofxFFmpegRecorder recorder;
		ofPixels recordPixels; // reused every frame by fboReader.readToPixels()

		// Recording: writes a timestamped .mp4 to bin/data/ for the whole run
		// when true. Off by default so dev iteration doesn't pile up
		// recordings on disk -- flip to true when you actually want an export.
		bool bRecordEnabled = true;
		// The recorder is configured in setup() but not started there -- see
		// update(), which starts it on the first frame fft.bIsProcessed goes
		// true, so the video's first frame is in sync with the audio actually
		// starting, instead of including a few blank frames recorded before
		// the audio pipeline had processed anything.
		bool bRecordingStarted = false;

		// Band extraction: FFT amplitude is sampled on a log-frequency axis and
		// smoothed into `bands`, a 0..1 value per band (low freq -> high freq),
		// for draw() (or a derived sketch) to react to.
		int numBands = 100;
		float sampleRate = 44100.0f;
		float freqMin = 20.0f;			// Hz; freqMax is derived as sampleRate/2 (Nyquist)
		float bandSmoothing = 0.08f;	// 0 = no smoothing, 1 = instant
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

		// Draws each band's running max as text straight to the window (not
		// into fboOutput), so it's visible live but never ends up baked into
		// a recording. Toggle off if it's in the way.
		bool bShowStatsOverlay = true;

		// Feedback/trail: fraction of background color re-drawn over the previous
		// frame each draw() call. 0 = trails never fade (garbage/full feedback),
		// higher = trails fade faster / are shorter. Tune to taste.
		// Values under 0.1 are best for visible effects.
		float trailFadeAlpha = 0.08f;

		ofPolyline myLine;
		ofPath myPath;
		//std::vector<float> maxBands = { 0.96f, 0.96f, 0.96f, 0.97f, 0.97f, 0.96f, 0.95f, 0.94f, 0.95f, 0.83f, 0.93f, 0.79f, 0.85f, 0.75f, 0.77f, 0.78f, 0.79f, 0.76f, 0.79f, 0.72f };
std::vector<float> maxBands = {
    0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f,
    0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f, 0.82f,
    0.82f, 0.82f, 0.81f, 0.81f, 0.81f, 0.80f, 0.80f, 0.80f, 0.80f, 0.79f,
    0.79f, 0.78f, 0.77f, 0.76f, 0.75f, 0.73f, 0.71f, 0.71f, 0.71f, 0.72f,
    0.72f, 0.72f, 0.72f, 0.72f, 0.70f, 0.66f, 0.57f, 0.58f, 0.58f, 0.59f,
    0.59f, 0.57f, 0.55f, 0.53f, 0.54f, 0.52f, 0.51f, 0.51f, 0.52f, 0.52f,
    0.53f, 0.53f, 0.56f, 0.55f, 0.56f, 0.57f, 0.55f, 0.58f, 0.57f, 0.57f,
    0.57f, 0.55f, 0.56f, 0.54f, 0.57f, 0.60f, 0.61f, 0.54f, 0.56f, 0.58f,
    0.57f, 0.58f, 0.60f, 0.58f, 0.57f, 0.55f, 0.53f, 0.54f, 0.57f, 0.55f,
    0.53f, 0.55f, 0.51f, 0.50f, 0.50f, 0.44f, 0.36f, 0.34f, 0.38f, 0.27f
};
};
