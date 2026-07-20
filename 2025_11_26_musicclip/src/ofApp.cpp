#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofColor bgColor;

	// Default: pure black (unchanged)
	//bgColor.setHsb(0, 0, 0);
	//bgColor.setHsb(140, 201, 33); // Deep teal charcoal
	bgColor.setHsb(184, 222, 38); // Dark indigo / night
	//bgColor.setHsb(160, 30, 21);	// Neutral charcoal (very dark desaturated)
	// bgColor.setHsb(150, 20, 18);	// Slightly warmed black
	ofSetBackgroundColor(bgColor);
	ofEnableAlphaBlending(); // enable once

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
	// allocate the "previous frame" FBO for feedback/ping-pong
	lastFboOutput.allocate(fboWidth, fboHeight, GL_RGBA);
	// avoid edge sampling/alpha artifacts. Do this by clearing and sampling the FBOs consistently and setting texture wrap/filtering.
	fboOutput.getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	lastFboOutput.getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	fboOutput.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);
	lastFboOutput.getTexture().setTextureMinMagFilter(GL_NEAREST, GL_NEAREST);

	// clear both fbos to the background color to avoid garbage on first frames
	ofFloatColor bg = ofGetBackgroundColor();
	fboOutput.begin();
	ofClear(bg.r, bg.g, bg.b, bg.a);
	fboOutput.end();

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


	recorder.setOutputPath(ofToDataPath(ofGetTimestampString() + ".mp4", true));
	recorder.startCustomRecord();
	//ofBackground(0);
}

//--------------------------------------------------------------
void ofApp::update(){
	// Use ofxFft helpers to sample amplitude on a log-frequency axis
	float smoothing = 0.5f; // 0 = no smoothing, 1 = instant
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

	// Feedback/fade settings:
	// feedbackAmount: how much of the previous frame is drawn on top of itself (0..1)
	// - 1.0 -> previous frame fully opaque (very long trails)
	// - 0.0 -> previous frame not drawn at all (no feedback)
	// tweak this value to taste
	//const float feedbackAmount = 0.95;

	// Begin rendering into the "current" FBO
	fboOutput.begin();

	ofFloatColor bg = ofGetBackgroundColor();
	ofSetColor(bg);
	ofDrawRectangle(0, 0, fboWidth, fboHeight);

	// Draw the previous frame into the current FBO with an alpha to create fading trails.
	//int alpha255 = (int)ofClamp(feedbackAmount * 255.0f, 0.0f, 255.0f);
	//ofSetColor(255, 255, 255, alpha255);
	ofSetColor(255);
	lastFboOutput.draw(0, 0, fboWidth, fboHeight);

	//// draw translucent background-colored overlay using ofFloatColor
	const float overlayAlpha = 0.0135f; // small per-frame fade (tweak 0.02..0.08)
	//ofFloatColor bg = ofGetBackgroundColor();
	bg.a = overlayAlpha;
	ofSetColor(bg);
	ofDrawRectangle(0, 0, fboWidth, fboHeight);
	ofSetColor(255); // reset color to opaque white for subsequent draws

	// Now draw the current frame content on top of the feedback
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

			// MOODY / SINISTER PALETTE MAPPING
			// Hue: bias towards teal -> deep purple (roughly 150..235 in 0..255 space)
			// Add small positional offset for variety
			float baseHue = ofMap(binValue, 0.0f, 1.0f, 150.0f, 235.0f);
			float hueOffset = ((float)i / std::max(1, horizDivs) - 0.5f) * 24.0f + ((float)j / std::max(1, vertDivs) - 0.5f) * -12.0f;
			float hue = ofClamp(baseHue + hueOffset, 0.0f, 255.0f);
			// Saturation: mid-high but not full, so colors feel rich but not garish
			float saturation = ofClamp(ofMap(binValue, 0.0f, 1.0f, 90.0f, 210.0f), 0.0f, 255.0f);
			// Brightness: keep generally dark for moody feel, peaks brighten more
			float brightness = ofClamp(ofMap(binValue, 0.0f, 1.0f, 80.0f, 190.0f), 0.0f, 255.0f);
			// Alpha: low-energy cells are more transparent, peaks stronger
			float alpha = ofClamp(ofMap(binValue, 0.0f, 1.0f, 150.0f, 240.0f), 0.0f, 255.0f);
			color.setHsb(hue, saturation, brightness, alpha);

			ofSetColor(color);
			float xroom = (fboWidth / horizDivs - w) / 2.0f;
			float yroom = (fboHeight / vertDivs - h) / 2.0f;
			x += xroom;
			y += yroom;
			ofDrawRectangle(x, y, w, h);
		}
	}

	// Finish drawing into current FBO
	fboOutput.end();

	// Present and record the freshly rendered frame (before swapping)
	// Restore original scale behaviour: scale by window height relative to 1920,
	// but do not upscale beyond native FBO size.
	float scale = ofGetHeight() / 1920.0f;
	scale = std::min(1.0f, scale);
	float drawW = fboWidth * scale;
	float drawH = fboHeight * scale;
	float drawX = 0.0f;
	float drawY = 0.0f;
	fboOutput.draw(drawX, drawY, drawW, drawH);

	ofPixels px;
	// ofxFastFboReader used to speed this up
	fboReader.readToPixels(fboOutput, px, OF_IMAGE_COLOR);
	if (px.getWidth() > 0 && px.getHeight() > 0) {
		recorder.addFrame(px);
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
