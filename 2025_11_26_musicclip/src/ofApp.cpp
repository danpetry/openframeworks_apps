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

	/*ofFileDialogResult result = ofSystemLoadDialog("Please select an audio file (.mp3, .wav, .aiff, .aac");
	if (result.bSuccess) {
		player.load(result.getPath());
		player.play();
	}*/
	player.load(ofToDataPath("Master rec 0010 [2025-11-26 083611] - Excerpt for IG.wav"));
	player.play();
	player.connectTo(fft).connectTo(output);

	// initialize bands vector used for drawing
	bands.assign(vertDivs, 0.0f);

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
	// Use ofxFft helpers to sample amplitude on a log-frequency axis
	float smoothing = 0.1f; // 0 = no smoothing, 1 = instant
	int numChannels = (int)fft.audioBins.size();
	if (numChannels == 0) return;

	// sampling parameters
	const float sampleRate = 44100.0f; // adjust if you have a different stream sample rate
	const float fMin = 20.0f;
	const float fMax = sampleRate * 0.5f; // nyquist
	const float logMin = std::log(fMin);
	const float logMax = std::log(fMax);

	for (int band = 0; band < vertDivs; ++band) {
		// geometric (log) center frequency for this band
		float frac = (band + 0.5f) / (float)vertDivs;
		float fCenter = std::exp(logMin + frac * (logMax - logMin));

		// sample amplitude from ofxFft via helper (interpolated)
		float sumAmp = 0.0f;
		for (int c = 0; c < numChannels; ++c) {
			// fft is your FftObject; its inner shared_ptr<ofxFft> is `fft.fft`
			sumAmp += fft.fft->getAmplitudeAtFrequency(fCenter, sampleRate);
		}
		float amp = sumAmp / std::max(1, numChannels);

		// convert to dB (amplitude -> dB), clamp and remap to 0..1 for visualization
		amp = std::max(amp, 1e-9f); // avoid log(0)
		float db = 20.0f * std::log10(amp);
		const float dbMin = -90.0f;
		const float dbMax = 0.0f;
		float mapped = ofClamp((db - dbMin) / (dbMax - dbMin), 0.0f, 1.0f);

		// smoothing
		bands[band] = bands[band] * (1.0f - smoothing) + mapped * smoothing;
	}
}

//--------------------------------------------------------------
void ofApp::draw(){

	fboOutput.begin();

	// Drawing code can go here...
	ofBackground(0);
	//fft.draw(ofRectangle(0, ofGetHeight() / 2, ofGetWidth(), ofGetHeight() / 2));
	float pulseAmt = 0.1f;

	for (int i = 0; i < horizDivs; i++) {
		for (int j = 0; j < vertDivs; j++) {
			float x = i * (fboWidth / horizDivs);
			float y = j * (fboHeight / vertDivs);
			ofColor color;
			// use aggregated band value for this vertical division
			int bandIndex = vertDivs - j - 1;
			float binValue = 0.0f;
			if (bandIndex >= 0 && bandIndex < (int)bands.size()) binValue = bands[bandIndex];
			int gain = 1; // keep existing gain behavior (adjust if needed)
			binValue = ofClamp(binValue * gain, 0, 1);
			float w = ofMap(binValue, 0, 1, pulseAmt  * fboWidth / horizDivs, fboWidth / horizDivs);
			float h = ofMap(binValue, 0, 1, pulseAmt * fboWidth / horizDivs, fboHeight / vertDivs);
			//cout << "binIndex: " << binIndex << endl;
			//cout << fft.audioBins[0][binIndex] << endl;
			color.setHsb(255 * binValue, 200, 255);
			ofSetColor(color);
			float xroom = (fboWidth / horizDivs - w) / 2.0f;
			float yroom = (fboHeight / vertDivs - h) / 2.0f;
			x += xroom;
			y += yroom;
			ofDrawRectangle(x, y, w, h);
		}
	}

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
