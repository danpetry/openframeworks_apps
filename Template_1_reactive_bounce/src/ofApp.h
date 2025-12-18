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
		ofxFastFboReader fboReader;
		ofxFFmpegRecorder recorder;
		FftObject fft;
		bool bNewFrame = false;
		
};
