#pragma once

#include "ofMain.h"
#include <vector>
#include "ofxFastFboReader.h"
#include "ofxFFmpegRecorder.h"
#include "ofVideoGrabber.h"

struct Ball {
	glm::vec3 pos;
	glm::vec3 vel;
	float radius = 10.0f;
	ofColor color = ofColor::white;
	float mass() const { return radius * radius * radius; } // approximate mass by volume
};

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
		
		// simple ball simulation in 3D
		std::vector<Ball> balls;
		const int NUM_BALLS = 20;

		// 3D rendering / lighting
		ofLight light;
		ofEasyCam cam;
		float zMin = -100.0f;
		float zMax = 100.0f;
		bool lightOn = true;

		// glow/billboard resources
		ofFbo glowFbo;
		ofTexture glowTex;
		float glowScale = 12.0f; // multiplier of radius for glow sprite size

		// rendering
		int fboWidth = 1080;
		int fboHeight = 1920;
		ofFbo fboOutput;
		ofxFastFboReader fboReader;
		ofxFFmpegRecorder recorder;
		bool bNewFrame = false;

};
