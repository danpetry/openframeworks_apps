#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetBackgroundColor(ofColor::black);

	// Set up audio stream
	ofSetLogLevel(OF_LOG_VERBOSE);
	int bufferSize = 512;
	ofSoundStreamSettings streamSettings;
	streamSettings.numInputChannels = 2;
	streamSettings.numOutputChannels = 2;
	streamSettings.sampleRate = 44100;
	streamSettings.bufferSize = bufferSize;
	streamSettings.numBuffers = 4;
	stream.setup(streamSettings);
	stream.setOutput(output);
	fft.setup(bufferSize);

	ofFileDialogResult result = ofSystemLoadDialog("Please select an audio file (.mp3, .wav, .aiff, .aac");
	if (result.bSuccess) {
		player.load(result.getPath());
		player.play();
	}
	player.connectTo(fft).connectTo(output);

	// Setup video grabber, frame buffer and recorder
	fboOutput.allocate(fboWidth, fboHeight, GL_RGBA);
	recorder.setup(true, false, glm::vec2(fboWidth, fboHeight));
	recorder.setOverWrite(true);
	recorder.setInputPixelFormat(OF_IMAGE_COLOR);

	// Improve output quality:
	// - Use modern H.264 encoder (libx264) instead of legacy mpeg4
	// - Use CRF-based quality control (good quality around 18) and a reasonable preset
	// - Ensure output pixel format is yuv420p which is widely compatible
	// - Optionally bump target bitrate if you prefer a constant bitrate
	recorder.setVideoCodec("libx264");
	// Option A: CRF (recommended) - better quality/size tradeoff
	recorder.addAdditionalOutputArgument("-preset veryfast");
	recorder.addAdditionalOutputArgument("-crf 18");
	recorder.addAdditionalOutputArgument("-pix_fmt yuv420p");

	// Option B: Constant bitrate (uncomment if you prefer a fixed bitrate)
	// recorder.setBitRate(8000); // 8000 kb/s
	// recorder.addAdditionalOutputArgument("-b:v 8000k");
	// recorder.addAdditionalOutputArgument("-pix_fmt yuv420p");

	// start the recorder after configuring it
	// comment/uncomment to toggle recording.
	// There's a better pattern where you can start/stop recording via keypress in the example,
	// but I've spent enough time on this already
	recorder.setOutputPath(ofToDataPath(ofGetTimestampString() + ".mp4", true));
	recorder.startCustomRecord();


	// Other setup code can go here...


	recorder.setOutputPath(ofToDataPath(ofGetTimestampString() + ".mp4", true));
	recorder.startCustomRecord();
	ofBackground(0);
}

//--------------------------------------------------------------
void ofApp::update(){

}

//--------------------------------------------------------------
void ofApp::draw(){

	fboOutput.begin();

	// Drawing code can go here...
	ofBackground(0);
	fft.draw(ofRectangle(0, ofGetHeight() / 2, ofGetWidth(), ofGetHeight() / 2));

	fboOutput.end();

	float scale = ofGetHeight() / 1920.0;
	fboOutput.draw(0, 0, 1080 * scale, 1920 * scale);

	ofPixels px;

	// ofxFastFboReader used to speed this up
	fboReader.readToPixels(fboOutput, px, OF_IMAGE_COLOR);
	if (px.getWidth() > 0 && px.getHeight() > 0) {
		recorder.addFrame(px);
	}

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
