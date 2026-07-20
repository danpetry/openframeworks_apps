#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetBackgroundColor(ofColor::black);
	ofEnableAlphaBlending(); // needed for the trail-fade overlay in draw() below

	// Set up audio stream
	ofSetLogLevel(OF_LOG_VERBOSE);
	int bufferSize = 512;
	ofSoundStreamSettings streamSettings;
	// Pin the API instead of leaving it unspecified: with no API set, RtAudio
	// probes every installed driver (ASIO/WASAPI/DS) to find one that works,
	// which on this machine wakes third-party ASIO-hooked apps (e.g.
	// Sonarworks) as a side effect. WASAPI is the standard low-latency
	// Windows default and skips ASIO device enumeration entirely.
	streamSettings.setApi(ofSoundDevice::MS_WASAPI);
	streamSettings.numInputChannels = 2;
	streamSettings.numOutputChannels = 2;
	streamSettings.sampleRate = static_cast<size_t>(sampleRate);
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

	// Band extraction: one smoothed 0..1 value per band, for draw() to react to.
	bands.assign(numBands, 0.0f);
	bandMin.assign(numBands, 1.0f);
	bandMax.assign(numBands, 0.0f);

	// Setup video grabber, frame buffer and recorder
	fboOutput.allocate(fboWidth, fboHeight, GL_RGBA);
	// allocate the "previous frame" FBO for feedback/ping-pong
	lastFboOutput.allocate(fboWidth, fboHeight, GL_RGBA);
	// avoid edge sampling artifacts on both FBOs used in the ping-pong
	for (ofFbo* fbo : { &fboOutput, &lastFboOutput }) {
		fbo->getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
		fbo->getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
	}

	// Clear lastFboOutput so the very first frame's ping-pong read (in draw())
	// doesn't show garbage; fboOutput itself doesn't need pre-clearing since
	// draw() unconditionally opaque-fills it before anything else, every frame.
	ofFloatColor bg = ofGetBackgroundColor();
	lastFboOutput.begin();
	ofClear(bg.r, bg.g, bg.b, bg.a);
	lastFboOutput.end();

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
}

//--------------------------------------------------------------
void ofApp::update(){
	fft.updateBands(bands, sampleRate, freqMin, dbMin, dbMax, bandSmoothing);

	// Track each band's observed range and periodically print it, so you
	// know the actual dynamic range of this sample before designing around it.
	for (int i = 0; i < numBands; ++i) {
		bandMin[i] = std::min(bandMin[i], bands[i]);
		bandMax[i] = std::max(bandMax[i], bands[i]);
	}

	if (ofGetElapsedTimef() - lastStatsLogTime > statsLogInterval) {
		lastStatsLogTime = ofGetElapsedTimef();
		std::stringstream ss;
		for (int i = 0; i < numBands; ++i) {
			ss << i << ":[" << ofToString(bandMin[i], 2) << "-" << ofToString(bandMax[i], 2) << "] ";
		}
		ofLogNotice("bands") << ss.str();
	}
}

//--------------------------------------------------------------
void ofApp::draw(){

	// Begin rendering into the "current" FBO
	fboOutput.begin();

	// Opaque fill first: lastFboOutput may have partial alpha (from the fade
	// overlay below), so this avoids stale/garbage pixels showing through.
	ofFloatColor bg = ofGetBackgroundColor();
	ofSetColor(bg);
	ofDrawRectangle(0, 0, fboWidth, fboHeight);

	// Draw the previous frame back in...
	ofSetColor(255);
	lastFboOutput.draw(0, 0, fboWidth, fboHeight);

	// ...then fade it slightly with a translucent background-colored overlay,
	// so trails decay over time instead of accumulating forever.
	bg.a = trailFadeAlpha;
	ofSetColor(bg);
	ofDrawRectangle(0, 0, fboWidth, fboHeight);
	ofSetColor(255); // reset to opaque white for your drawing code below

	// --- Your sketch goes here. ---------------------------------------------
	// This bar chart is a minimal prototype -- read bands[] and draw something
	// with it, however simple -- not a final look. Replace it once you know
	// (from the console log in update()) what this sample's values actually do.
	// `bands[i]` is a smoothed 0..1 amplitude for frequency band i (low -> high).
	for (int i = 0; i < numBands; i++) {
		float barWidth = (float)fboWidth / numBands;
		float barHeight = bands[i] * fboHeight;
		ofSetColor(255 * bands[i]);
		ofDrawRectangle(i * barWidth, fboHeight - barHeight, barWidth, barHeight);
	}
	// -------------------------------------------------------------------------

	// Finish drawing into current FBO
	fboOutput.end();

	// Present, scaled to fit the window, without upscaling past native FBO size.
	float scale = std::min(1.0f, ofGetHeight() / (float)fboHeight);
	fboOutput.draw(0, 0, fboWidth * scale, fboHeight * scale);

	// ofxFastFboReader used to speed this up; recordPixels is a persistent
	// buffer reused every frame instead of reallocating.
	fboReader.readToPixels(fboOutput, recordPixels, OF_IMAGE_COLOR);
	if (recordPixels.getWidth() > 0 && recordPixels.getHeight() > 0) {
		recorder.addFrame(recordPixels);
	}

	// Ping-pong: make the just-rendered FBO become "last" for the next frame
	// and reuse the other FBO as the render target next frame.
	std::swap(fboOutput, lastFboOutput);
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
