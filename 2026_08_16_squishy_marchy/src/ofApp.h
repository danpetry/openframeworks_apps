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
		// Grows/shrinks bands (and its sibling per-band vectors) in place,
		// clamped to [numBandsMin, numBandsMax]. Existing per-band state
		// (smoothing history, min/max, enabled flags) is preserved for
		// indices that still exist after the resize.
		void setNumBands(int n);
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
		bool bRecordEnabled = false;
		// The recorder is configured in setup() but not started there -- see
		// update(), which starts it on the first frame fft.bIsProcessed goes
		// true, so the video's first frame is in sync with the audio actually
		// starting, instead of including a few blank frames recorded before
		// the audio pipeline had processed anything.
		bool bRecordingStarted = false;

		// Band extraction: FFT amplitude is sampled on a log-frequency axis and
		// smoothed into `bands`, a 0..1 value per band (low freq -> high freq),
		// for draw() (or a derived sketch) to react to.
		// '=' / '-' grow/shrink numBands at runtime (see setNumBands()), clamped
		// to this range: below numBandsMin the log-frequency spacing gets silly,
		// above numBandsMax neighboring bands start reading almost the same FFT
		// bin (diminishing returns) and, in sketches whose draw() cost scales
		// with numBands (e.g. a numBands x numBands loop), frame cost can climb
		// fast -- keep that in mind before raising this ceiling per-sketch.
		int numBands = 20;
		int numBandsMin = 2;
		int numBandsMax = 64;
		float sampleRate = 44100.0f;
		float freqMin = 20.0f;			// Hz; freqMax is derived as sampleRate/2 (Nyquist)
		// Attack/release smoothing: fast response when a band rises (attack) so
		// hits stay snappy, slow decay when it falls (release) so the motion
		// reads as smooth instead of jittery -- see FftObject::updateBands.
		// 0 = frozen, 1 = instant.
		float bandAttack = 1.0f;
		float bandRelease = 0.01f;
		float dbMin = -90.0f;
		float dbMax = 0.0f;
		std::vector<float> bands;

		// Per-band on/off mask: disabled bands are forced to 0 in update()
		// (before anything reads bands[]), so a muted band reads as silence
		// everywhere -- the bar chart, the stats overlay, and any derived
		// sketch. Left/Right move `selectedBand`, Space toggles it.
		std::vector<bool> bandEnabled;
		int selectedBand = 0;

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
		float trailFadeAlpha = 1.00f;
};
