#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofSetBackgroundColor(ofColor::black);
	srand((unsigned)time(nullptr));
	balls.clear();
	for(int i=0;i<NUM_BALLS;i++){
		Ball b;
		b.radius = ofRandom(25, 75);
		b.pos = glm::vec3(ofRandom(b.radius, ofGetWidth()-b.radius), ofRandom(b.radius, ofGetHeight()-b.radius), ofRandom(zMin+ b.radius, zMax - b.radius));
		float speed = ofRandom(200, 500);
		float angle = ofRandom(0, TWO_PI);
		float zangle = ofRandom(-1,1);
		b.vel = glm::vec3(cos(angle), sin(angle), zangle) * speed;
		b.color = ofColor::fromHsb(ofRandom(0,255), 255, 255);
		balls.push_back(b);
	}

	// setup glow texture (radial alpha)
	int texSize = 4;
	ofPixels pix;
	pix.allocate(texSize, texSize, OF_PIXELS_RGBA);
	for(int y=0;y<texSize;y++){
		for(int x=0;x<texSize;x++){
			float nx = (x + 0.5f) / texSize * 2.0f - 1.0f;
			float ny = (y + 0.5f) / texSize * 2.0f - 1.0f;
			float d = sqrt(nx*nx + ny*ny);
			float a = 0.0f;
			if(d < 1.0f) {
				// falloff, smoothstep-like
				float t = 1.0f - d;
				a = pow(t, 2.0f);
			}
			unsigned char alpha = (unsigned char)ofClamp(a * 255.0f, 0.0f, 255.0f);
			pix.setColor(x, y, ofColor(255, 255, 255, alpha));
		}
	}
	glowTex.loadData(pix);

	// setup light and camera
	light.setPointLight();
	light.setDiffuseColor(ofFloatColor(1.0, 1.0, 1.0));
	light.setSpecularColor(ofFloatColor(1.0, 1.0, 1.0));
	// place the light near the camera so most front-facing surfaces are lit
	light.setPosition(ofGetWidth() * .25, ofGetHeight() * .75, 200);
	lightOn = true;

	cam.setNearClip(0.1f);
	cam.setFarClip(2000.0f);
	// position the camera so that world x/y coordinates align with window coordinates
	// this makes collisions against ofGetWidth()/ofGetHeight() correct
	cam.setPosition(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f, 800.0f);
	cam.lookAt(glm::vec3(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f, 0.0f));
}

//--------------------------------------------------------------
void ofApp::update(){
	float dt = ofGetLastFrameTime();
	// update positions
	for(auto &b : balls){
		b.pos += b.vel * dt;
		// wall collisions (x/y)
		if(b.pos.x - b.radius < 0){ b.pos.x = b.radius; b.vel.x *= -1; }
		if(b.pos.x + b.radius > ofGetWidth()){ b.pos.x = ofGetWidth() - b.radius; b.vel.x *= -1; }
		if(b.pos.y - b.radius < 0){ b.pos.y = b.radius; b.vel.y *= -1; }
		if(b.pos.y + b.radius > ofGetHeight()){ b.pos.y = ofGetHeight() - b.radius; b.vel.y *= -1; }
		// z bounds
		if(b.pos.z - b.radius < zMin){ b.pos.z = zMin + b.radius; b.vel.z *= -1; }
		if(b.pos.z + b.radius > zMax){ b.pos.z = zMax - b.radius; b.vel.z *= -1; }
	}

	// simple pairwise collision response in 3D
	for(size_t i=0;i<balls.size();++i){
		for(size_t j=i+1;j<balls.size();++j){
			Ball &a = balls[i];
			Ball &b = balls[j];
			glm::vec3 diff = b.pos - a.pos;
			float dist = glm::length(diff);
			float minDist = a.radius + b.radius;
			if(dist > 0 && dist < minDist){
				// normalize
				glm::vec3 n = diff / dist;
				// push them apart
				float overlap = 0.5f * (minDist - dist);
				a.pos -= n * overlap;
				b.pos += n * overlap;
				// relative velocity
				glm::vec3 relVel = b.vel - a.vel;
				float velAlongNormal = glm::dot(relVel, n);
				if(velAlongNormal > 0) continue; // they are separating
				// compute restitution (elastic)
				float e = 0.9f;
				float m1 = a.mass();
				float m2 = b.mass();
				float j = -(1 + e) * velAlongNormal / (1.0f/m1 + 1.0f/m2);
				glm::vec3 impulse = (j * n);
				a.vel -= impulse * (1.0f / m1);
				b.vel += impulse * (1.0f / m2);
			}
		}
	}

	// optionally move light here if you want dynamic lighting
}

//--------------------------------------------------------------
void ofApp::draw(){
	// clear color and depth buffers each frame and enable depth testing
	ofClear(0, 0, 0, 255);
	ofEnableDepthTest();

	cam.begin();

	// enable lighting so bounding planes are affected by it
	if(lightOn) {
		ofEnableLighting();
		light.enable();
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	} else {
		ofDisableLighting();
	}

	// draw spheres (with lighting already enabled)
	for(auto &b : balls){
		ofSetColor(b.color);
		ofDrawSphere(b.pos, b.radius);
	}

	// cleanup lighting state
	if(lightOn) {
		glDisable(GL_COLOR_MATERIAL);
		light.disable();
		ofDisableLighting();
	}

	cam.end();

	// disable depth testing for 2D billboard pass
	ofDisableDepthTest();

	// draw additive billboard glows in screen space
	ofEnableBlendMode(OF_BLENDMODE_ADD);
	for(auto &b : balls){
		// project to screen
		auto screenPos = cam.worldToScreen(b.pos);
		// simple off-screen cull
		if(screenPos.x < -200 || screenPos.x > ofGetWidth() + 200 || screenPos.y < -200 || screenPos.y > ofGetHeight() + 200) continue;
		// approximate screen radius by projecting a point at +radius on x axis
		auto screenEdge = cam.worldToScreen(b.pos + glm::vec3(b.radius, 0.0f, 0.0f));
		float screenR = glm::distance(glm::vec2(screenEdge.x, screenEdge.y), glm::vec2(screenPos.x, screenPos.y));
		float size = (screenR * 2.0f) * glowScale;
		ofSetColor(b.color, 200);
		glowTex.draw(screenPos.x - size * 0.5f, screenPos.y - size * 0.5f, size, size);
	}
	ofDisableBlendMode();

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if(key == 'r'){
		setup();
	}
	if(key == 'l'){
		// toggle light
		lightOn = !lightOn;
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
	// light is now static; do not move it with the mouse
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
	// reposition camera when window size changes so bounds stay correct
	cam.setPosition(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f, cam.getPosition().z);
	cam.lookAt(glm::vec3(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f, 0.0f));
	// reposition light relative to new window center
	light.setPosition(ofGetWidth()/2, ofGetHeight()/2, 600);
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
