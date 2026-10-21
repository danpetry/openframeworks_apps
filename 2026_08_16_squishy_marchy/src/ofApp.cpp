#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetBackgroundColor(2,7,6);
	ofEnableAlphaBlending(); // needed for the trail-fade overlay in draw() below

	// The default ESC-quits-app behavior routes through the same window-close
	// shutdown path that's crashing on exit (see keyPressed() and exit()
	// below for the actual fix); disable it so ESC only does the one thing.
	ofSetEscapeQuitsApp(false);

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

	// Remember whichever file gets picked, so bShowFilePicker=false can reload
	// it next run instead of prompting again.
	std::string lastAudioFilePath = ofToDataPath("lastAudioFile.txt", true);
	bool bNeedPicker = bShowFilePicker;
	if (!bNeedPicker && !ofFile::doesFileExist(lastAudioFilePath, false)) {
		ofLogWarning("ofApp") << "bShowFilePicker is false but no remembered file yet -- showing the picker once.";
		bNeedPicker = true;
	}

	std::string audioPath;
	if (bNeedPicker) {
		ofFileDialogResult result = ofSystemLoadDialog("Please select an audio file (.mp3, .wav, .aiff, .aac");
		if (result.bSuccess) {
			audioPath = result.getPath();
			ofBuffer buf;
			buf.set(audioPath);
			ofBufferToFile(lastAudioFilePath, buf, false);
		}
	} else {
		audioPath = ofTrim(ofBufferFromFile(lastAudioFilePath, false).getText());
	}

	if (!audioPath.empty()) {
		player.load(audioPath);
		player.setLoop(true);
		player.play();
	}
	player.connectTo(fft).connectTo(output);

	// Band extraction: one smoothed 0..1 value per band, for draw() to react to.
	bands.assign(numBands, 0.0f);
	bandMin.assign(numBands, 1.0f);
	bandMax.assign(numBands, 0.0f);
	bandEnabled.assign(numBands, true);

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

	if (bRecordEnabled) {
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

		recorder.setOutputPath(ofToDataPath(ofGetTimestampString() + ".mp4", true));
		// Not started here -- see update(), which starts it once real audio
		// is actually flowing, so the video and audio start in sync.
	}

	// Other setup code can go here...
}

//--------------------------------------------------------------
void ofApp::update(){
	// Start recording on the first frame real audio has actually been
	// processed, not in setup() -- this update() call and this frame's draw()
	// always run back to back, so bands[] is already valid by the time draw()
	// makes its first addFrame() call: video frame 0 is audio-in-sync.
	if (bRecordEnabled && !bRecordingStarted && fft.bIsProcessed) {
		recorder.startCustomRecord();
		bRecordingStarted = true;
	}

	fft.updateBands(bands, sampleRate, freqMin, dbMin, dbMax, bandAttacks, bandReleases);

	// Force disabled bands to silence here, before anything downstream (bar
	// chart, stats overlay, a derived sketch) reads bands[]. This also means
	// a disabled band's attack/release state resets to 0, so re-enabling it
	// fades back in via `bandAttack` rather than resuming its old value.
	for (int i = 0; i < numBands; ++i) {
		if (!bandEnabled[i]) bands[i] = 0.0f;
	}

	// Track each band's observed range and periodically print it, so you
	// know the actual dynamic range of this sample before designing around it.
	for (int i = 0; i < numBands; ++i) {
		// Only update the min if the band is non-zero, so an initial silence doesn't make the min 0 and squash the range
		if (bands[i] != 0) {
			bandMin[i] = std::min(bandMin[i], bands[i]);
		}
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

	myLine.clear();

	static float phase = 0;
	phase += 0.01f;
	int minX = fboWidth * 0.2f;
	int maxX = fboWidth * 0.8f;
	// int minY = fboHeight * 0.2f;
	// int maxY = fboHeight * 0.8f;
	int minY = ofMap(bands[1], bandMin[1] + (bandMax[1] - bandMin[1]) * band1threshold, bandMax[1], fboHeight * 0.3, fboHeight * 0.1f, true);
	int maxY = ofMap(bands[1], bandMin[1] + (bandMax[1] - bandMin[1]) * band1threshold, bandMax[1], fboHeight * 0.7f, fboHeight * 0.9f, true);

	float noiseAmount = ofMap(bands[3], bandMin[3]  + (bandMax[3] - bandMin[3]) * band3threshold, bandMax[3], 0, noiseScale, true);
	bandReleases[3] = 0.1f; // band 3 is the one we control with mouse X

	for (float i = 0; i <= 2 * PI + 0.19; i += 2 * PI * resolution) {
			float x = ofMap(sin(4 * i + phase + ofRandom(-noiseAmount, noiseAmount)), -1, 1, minX, maxX);
			float y = ofMap(sin(5 * i), -1, 1, minY, maxY);
			myLine.curveTo(x, y);
		}
		//myLine.close();
	// static int frameCount = 0;
	// 	frameCount++;
	// 	if (frameCount % 10 == 0) {

	// 	myLine.curveTo(ofMap(bands[1], bandMin[1], bandMax[1], 0, fboWidth), ofMap(bands[3], bandMin[3], bandMax[3], 0, fboHeight));
    // }
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
	ofColor traceColor = ofColor::fromHsb(96, 140, 255, 255);
	ofColor haloColor = ofColor::fromHsb(96, haloS, 255, haloA);
	
// for (int i = 0; i < numBands; i++) {
// 		float barWidth = (float)fboWidth / numBands;
// 		float barHeight = bands[i] * fboHeight;
// 		ofSetColor(255 * bands[i]);
// 		ofDrawRectangle(i * barWidth, fboHeight - barHeight, barWidth, barHeight);
// 	}
	// --- Your sketch goes here. ---------------------------------------------
	// use bands two and four of five ie bands[1] and bands[3]
	//draw central line
	

	//draw a halo around the line
	ofSetColor(haloColor);
	ofSetLineWidth(10);
	myLine.draw();
	ofSetColor(traceColor); // reset to opaque white for your drawing code below
	ofSetLineWidth(2);
	myLine.draw();
	
	// -------------------------------------------------------------------------

	// Finish drawing into current FBO
	fboOutput.end();

	// Present, scaled to fit the window, without upscaling past native FBO size.
	float scale = std::min(1.0f, ofGetHeight() / (float)fboHeight);
	fboOutput.draw(0, 0, fboWidth * scale, fboHeight * scale);

	// Debug overlay: band-count/selection controls, drawn straight to the
	// window (not into fboOutput above), so it's live on screen but never in
	// a recording.
	if (bShowStatsOverlay) {
		std::stringstream ss;
		ss << "numBands: " << numBands << "  ('=' / '-' to change)\n";
		ss << "selected band: " << selectedBand << (bandEnabled[selectedBand] ? " [on]" : " [off]")
		   << "  (Left/Right to move, Space to toggle)\n";
		ofSetColor(255);
		ofDrawBitmapStringHighlight(ss.str(), 12, 20);
	}
	std::stringstream ss;
	// ss << "haloS: " << haloS << "  (mouse X)\n";
	// ss << "haloA: " << haloA << "  (mouse Y)\n";
	ofSetColor(255);
	ofDrawBitmapStringHighlight(ss.str(), 12, 80);

	if (bRecordingStarted) {
		// ofxFastFboReader used to speed this up; recordPixels is a persistent
		// buffer reused every frame instead of reallocating.
		fboReader.readToPixels(fboOutput, recordPixels, OF_IMAGE_COLOR);
		if (recordPixels.getWidth() > 0 && recordPixels.getHeight() > 0) {
			recorder.addFrame(recordPixels);
		}
	}

	// Ping-pong: make the just-rendered FBO become "last" for the next frame
	// and reuse the other FBO as the render target next frame.
	std::swap(fboOutput, lastFboOutput);
}

//--------------------------------------------------------------
void ofApp::exit(){
	// Stop the audio stream before any members get destroyed. Without this,
	// the audio callback thread can still be alive and calling into
	// player/fft/output while the main thread tears them down on shutdown --
	// that's what was causing the access violation in
	// ofxSoundObject::disconnect() on exit.
	stream.stop();
	stream.close();
}

//--------------------------------------------------------------
void ofApp::setNumBands(int n) {
	n = ofClamp(n, numBandsMin, numBandsMax);
	if (n == numBands) return;

	// resize() keeps existing values for indices that still exist and
	// fills any newly-added ones with the given default.
	bands.resize(n, 0.0f);
	bandMin.resize(n, 1.0f);
	bandMax.resize(n, 0.0f);
	bandEnabled.resize(n, true);

	numBands = n;
	selectedBand = ofClamp(selectedBand, 0, numBands - 1);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if (key == '=' || key == '+') {
		setNumBands(numBands + 1);
	} else if (key == '-' || key == '_') {
		setNumBands(numBands - 1);
	} else if (key == OF_KEY_LEFT) {
		selectedBand = (selectedBand - 1 + numBands) % numBands;
	} else if (key == OF_KEY_RIGHT) {
		selectedBand = (selectedBand + 1) % numBands;
	} else if (key == ' ') {
		bandEnabled[selectedBand] = !bandEnabled[selectedBand];
	} else if (key == OF_KEY_ESC) {
		// Closing the GL window's own close button routes through
		// ofApp::exit() and then C++ destructors for player/fft/output,
		// which crashes on exit -- root cause not fully pinned down (the
		// stream.stop()/close() in exit() and reordering those members
		// didn't resolve it). Abrupt termination skips that destructor
		// chain entirely, which is why Ctrl+C in the console window closes
		// cleanly where the display window's close button doesn't. This
		// does the same thing from a single keystroke in the display window.
		if (bRecordingStarted) {
			recorder.stop(); // finalize the in-progress .mp4 before exiting
		}
		std::exit(0);
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
	// haloS = ofMap(x, 0, ofGetHeight(), 0, 255, true);
	// haloA = ofMap(y, 0, ofGetHeight(), 0, 255, true);
	// bandReleases[3] = ofMap(x, 0, ofGetWidth(), 0.0f, 1.0f, true);
	// band3threshold = ofMap(y, 0, ofGetHeight(), 0.0f, 1.0f, true);

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
