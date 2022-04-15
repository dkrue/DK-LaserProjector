// Simple I2C for 128x32 oled
#include "SSD1306AsciiAvrI2c.h"

// Laser projector libraries
#include "Laser.h"
#include "Drawing.h"

#include "Bounce2.h"

SSD1306AsciiAvrI2c oled;

Bounce pauseButton = Bounce();  
Bounce encoderButton = Bounce();  

// Create RGB laser instance
Laser laser(9, 5, 6);

// Global rotation positioning
int centerX = 2500;
int centerY = 512;
unsigned int angle = 180;
char direction = 1; // or -1
char directionX = 1; // for centerX, or -1
char directionY = 1;

float scale = 0.5;
char scaleDir = 1; // or -1

long rotationMultiplier1;
long rotationMultiplier2;
byte randomBeamCount;
byte rotationMode;
byte sceneState;

bool laserColor[2][3];
byte laserPWM[3];

int beamPos[30];
char beamBipolar[30];

byte strobePattern[12] = {255, 170, 5, 162, 187, 203, 235, 245, 1, 255, 101, 254}; // binary patterns
byte strobeBitmask = 128; // binary 10000000
byte strobeMode = 0;
byte strobeSpeed = 15; // 15 normal strobe speed (max 100)
byte strobeSubBitmask = 128; // binary 10000000
byte strobeSubMode = 1;
byte strobeSubSpeed = 2;

int frame;
int sceneLen = 20;
bool paused;
unsigned int speed_original = 1; // 1 = normal, 2 = intense?
unsigned int speed = speed_original;
byte gotoScene = 0;  // move through scenes in order for debug mode, else 0
byte scene;
byte intensity = 1; // 1 - smooth (no pwm, no strobe), 2 - normal (pwm), 3 - high (strobe), 4 - intense (sub-strobe)

static int pinA = 2; // Our first hardware interrupt pin is digital pin 2
static int pinB = 3; // Our second hardware interrupt pin is digital pin 3
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile char encoderPos; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile char oldEncPos; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent

byte menuIndex;

void setup()
{  
  // Wait for I2C comms to be ready on Metro Mini 
  delay(2000);

  pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  attachInterrupt(0,PinA,RISING); // set an interrupt on PinA, looking for a rising edge signal and executing the "PinA" Interrupt Service Routine (below)
  attachInterrupt(1,PinB,RISING); // set an interrupt on PinB, looking for a rising edge signal and executing the "PinB" Interrupt Service Routine (below)

  pinMode(8, INPUT_PULLUP);  
  pauseButton.attach(8);
  pauseButton.interval(20);
  pinMode(4, INPUT_PULLUP);  
  encoderButton.attach(4);
  encoderButton.interval(20);
 
  oled.begin(&Adafruit128x32, 0x3C);
  oled.setFont(System5x7); 
  
  laser.init();
  laser.setScale(scale);

  oled.set2X();
  oled.println("DK Laser");
  oled.set1X();

  randomSeed(analogRead(0)); // random seed from analog noise

  oled.println(gotoScene > 0 ? F("Debug mode on") : F("Show mode: Random"));
  oled.println("Press to start");

  do {
      encoderButton.update();
  } while (encoderButton.read() == HIGH);

  oled.clear();
  oled.set2X();
  oled.println("Laser! -~");

  countDown();
  oled.clear();
}

void loop() {
  // Other todo: make a scene with panning circles around ^^ these are both non rotated scenes
  
  if(gotoScene > 0) {
    scene = gotoScene;
  } else {
    scene = random(100);
  }

  switch (scene)
  {
    case 26:  
      printScene(F("Raindrops"), scene);
      generateRandomSceneValues();
      randomBeamCount+=5;
      for(int d = 0; d < randomBeamCount; d++){
        beamPos[d] = 4096;
        beamBipolar[d] = random(100) + 25;
      }
      laser.setOffset(0,0);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          raindrops(frame, randomBeamCount);
        }
        updateAll(frame);
      }
    break;

    case 25:
      if(intensity < 3) break;
      printScene(F("Random Uno"), scene);
      generateRandomSceneValues();
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        long r = random(2048);
        if(strobeOn(frame)) {
          Drawing::drawDotRotated(random(4096)-1024, r, r*2000L, centerX, centerY, angle % 360);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
      }
    break;

    case 24:
      printScene(F("Sun Stripe"), scene);
      generateRandomSceneValues();
      if(randomBeamCount > 6) {
        randomBeamCount = 6;
      }
      if(randomBeamCount < 4) {
        randomBeamCount = 4;
      }
      sceneState = 0;
      laser.setOffset(0,0);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        // TODO: would love to see sine up and down in addition to down scrolling
        if(strobeOn(frame)) {
          fanFallingSwitched(frame, randomBeamCount);
        }
        updateAll(frame);
      }
    break;

    case 23:
      printScene(F("Spotlights"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      // Set up beam positions in opposing directions
      beamPos[0] = random(1000) + 100;
      beamBipolar[0] = 1;
      beamPos[1] = random(2000) + 1100;
      beamBipolar[1] = -1;
      if(!laserColor[1][0] && !laserColor[1][1] && !laserColor[1][2]) {
        laserColor[1][0] = laserColor[0][0];
        laserColor[1][1] = laserColor[0][1];
        laserColor[1][2] = laserColor[0][2];
      }
      for (frame = 0; frame < sceneLen*200; frame+=speed) {
        if(strobeOn(frame)) {
          spotlightsPainted(frame);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
      }
    break;

    case 22:
      if(intensity < 3) break;
      printScene(F("Random Sky"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*300; frame+=speed) {
        if(strobeOn(frame)) {
          Drawing::drawLineRotated(rotationMultiplier1*100, 0, 2048-rotationMultiplier2*100, 0, centerX, centerY, angle % 360u, true);
        }
        if(frame%20 == 0) {
          angle=random(360);
        }
        updateAll(frame);
      }
    break;

    case 21:
      printScene(F("Sky Wing"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*300; frame+=speed) {
        laser.setColor(laserColor[frame%2][0], laserColor[frame%2][1], laserColor[frame%2][2]);
        if(strobeOn(frame)) {
         Drawing::drawLineRotated( (frame%2==0? 3064-(frame*rotationMultiplier1)%1000 : 1024+(frame*rotationMultiplier2)%1000), 0, (frame%2==0? 4096 : 0), 1200, centerX, centerY, angle % 360u, true);
        } 
        updateAll(frame);
        rotationUpdateLinear(0, frame);
      }
    break;

    case 20:
      printScene(F("Color Mesh"), scene);
      generateRandomSceneValues();
      // Ensure high even number of opposing beams
      randomBeamCount+=10;
      if(randomBeamCount%2>0) {
        randomBeamCount++;
      }
      laser.setOffset(2048,2048);
      // Set up dot positions out of clipping window, in opposing directions
      for(int d = 0; d < randomBeamCount; d++){
        if(d < randomBeamCount/2) {
          beamPos[d] = d * -200;
          beamBipolar[d] = 1;
        } else {
          beamPos[d] = (d-(randomBeamCount/2)) * 200 + 4096;
          beamBipolar[d] = -1;
        }
      }
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          multiplyingBeamsInterlacedColorCollapse(frame);
        }
        updateAll(frame);
        rotationUpdateNonLinear(rotationMode, frame);
      }
    break;

    case 19:
      printScene(F("Sky Tangent"), scene);
      generateRandomSceneValues();
      randomBeamCount+=4;
      laser.setOffset(2048,2048);
      // Set up dots positions out of clipping window
      for(int d = 0; d < randomBeamCount; d++){
        beamPos[d] = d * -300;
        beamBipolar[d] = 1;
      }
      for (frame = 0; frame < sceneLen*150; frame+=speed) {
        if(strobeOn(frame)) {
          multiplyingBeamsTangentalSky(frame);
        }
        updateAll(frame);
        rotationUpdateLinear(rotationMode, frame);
      }
    break;

    case 18:
      printScene(F("Hyper Paint"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          hyperBeamPaintedAltWhite(frame);
        }
        updateAll(frame);
        rotationUpdateLinear(rotationMode, frame);
      }
    break;

    case 17:
      printScene(F("Hyper Duo"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          hyperBeamPainted(frame);
        }
        updateAll(frame);
        rotationUpdateNonLinear(rotationMode, frame);
      }
    break;

    case 16:
      if(intensity < 3) break;
      printScene(F("Random Duo"), scene);
      generateRandomSceneValues();
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        long r = random(2048);
        if(strobeOn(frame)) {
          Drawing::drawDotRotated(random(3064)-1024, r, r*2000L, centerX, centerY, angle % 360);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
        laser.setColor(laserColor[frame%2][0], laserColor[frame%2][1], laserColor[frame%2][2]); 
      }
    break;

    case 15:
      if(intensity > 2) break;
      printScene(F("Spiraling"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*5; frame+=speed) {
        if(strobeOn(frame)) {
          spiralingCircle(frame);
        } else {
          delay(50);
        }
        updateAll(frame);
      }
    break;

    case 14:
      printScene(F("Beam Mesh"), scene);
      generateRandomSceneValues();
      // Ensure high even number of opposing beams
      randomBeamCount+=10;
      if(randomBeamCount%2>0) {
        randomBeamCount++;
      }
      laser.setOffset(2048,2048);
      // Set up dot positions out of clipping window, in opposing directions
      for(int d = 0; d < randomBeamCount; d++){
        if(d < randomBeamCount/2) {
          beamPos[d] = d * -200;
          beamBipolar[d] = 1;
        } else {
          beamPos[d] = (d-(randomBeamCount/2)) * 200 + 4096;
          beamBipolar[d] = -1;
        }
      }
      if(!laserColor[1][0] && !laserColor[1][1] && !laserColor[1][2]) {
        laserColor[1][0] = laserColor[0][0];
        laserColor[1][1] = laserColor[0][1];
        laserColor[1][2] = laserColor[0][2];
      }      
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          multiplyingBeamsOpposing(frame);
        }
        updateAll(frame);
        angle+=speed*speed*direction;
      }
    break;

    case 13:
      printScene(F("Quick Beams"), scene);
      generateRandomSceneValues();
      randomBeamCount+=5;
      laser.setOffset(2048,2048);
      // Set up dot positions out of clipping window
      for(int d = 0; d < randomBeamCount; d++){
        beamPos[d] = d * -400;
        beamBipolar[d] = 1;
      }
      for (frame = 0; frame < sceneLen*500; frame+=speed) {
        if(strobeOn(frame)) {
          multiplyingBeamsSlide(frame);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
      }
    break;

    case 12:
      printScene(F("Curve Beam"), scene);
      generateRandomSceneValues();
      randomBeamCount+=4;
      laser.setOffset(2048,2048);
      // Set up dots positions out of clipping window
      for(int d = 0; d < randomBeamCount; d++){
        beamPos[d] = d * -300;
        beamBipolar[d] = 1;
      }
      for (frame = 0; frame < sceneLen*250; frame+=speed) {
        if(strobeOn(frame)) {
          multiplyingBeamsSwitched(frame);
        }
        updateAll(frame);
        rotationUpdateNonLinear(rotationMode, frame);
      }
    break;

    case 11:
      printScene(F("Hyper Beam"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          hyperBeamMonochrome(frame);
        }
        updateAll(frame);
        rotationUpdateNonLinear(rotationMode, frame);
      }
    break;

    case 10:
      printScene(F("Prism Spin"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      laser.setScale(0.5);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          circlingPrism(frame);
        }
        updateAll(frame);
        rotationUpdateNonLinear(rotationMode, frame);
      }
    break;

    case 9:
      printScene(F("Line Beam"), scene);
      generateRandomSceneValues();
      randomBeamCount+=2;
      laser.setOffset(2048,2048);
      // Set up dot positions out of clipping window
      for(int d = 0; d < randomBeamCount; d++){
        beamPos[d] = d * -300;
        beamBipolar[d] = 1;
      }
      for (frame = 0; frame < sceneLen*250; frame+=speed) {
        if(strobeOn(frame)) {
          multiplyingBeamsSwitched(frame);
        }
        updateAll(frame);
        rotationUpdateLinear(1, frame);
      }
      break;

    case 8:
      if(intensity < 3) break;
      printScene(F("Prism Snag"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        // No rotation doesn't make sense for this scene
        if(rotationMultiplier1 < 2) {
            generateRandomSceneValues();
        }
        if(strobeOn(frame)) {
          randomPrism(frame);
        }
        updateAll(frame);
        rotationUpdateLinear(1, frame);
      }
    break;

    case 7:
      printScene(F("Curve Fan"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*150; frame+=speed) {
        if(strobeOn(frame)) {
          fanWithChasingBeam(frame, randomBeamCount);
        }
        updateAll(frame);
        rotationUpdateNonLinear(rotationMode, frame);
      }
    break;

    case 6:
      if(intensity < 3) break;
      printScene(F("Scroll Fan"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          fanClassicScroll(frame, randomBeamCount);
        }
        updateAll(frame);
        /*if(frame%2 == 0) {
          angle-=speed;
        }*/
        rotationUpdateLinear(1, frame);
      }
    break;

    case 5:
      if(intensity < 3) break;
      printScene(F("Polychroma"), scene);
      generateRandomSceneValues();
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        long r = random(2048);
        if(strobeOn(frame)) {
          Drawing::drawDotRotated(random(3064)-1024, r, r*2000L, centerX, centerY, angle % 360);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
        laser.setColor(random(2), random(2), random(2)); 
      }
    break;

    case 4:
      printScene(F("Rising Sun"), scene);
      generateRandomSceneValues();
      laser.setOffset(0,0);
      for (frame = 0; frame < sceneLen*100; frame++) {
        // TODO: would love to see sine up and down in addition to down scrolling
        if(strobeOn(frame)) {
          fanRisingGrowing(frame);
        }
        updateAll(frame);
      }
    break;

    case 3:
      if(intensity > 3) break;
      printScene(F("Laser Sky"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*350; frame+=speed) {
        Drawing::drawLineRotated(0   , 0, 4048, 0, centerX, centerY, angle % 360u, true);
        updateAll(frame);
        if(frame > 3500 && frame%5 == 0) {
          angle += speed * direction; // freeze rotation for awhile
        }
      }
    break;

    case 2:
      if(intensity < 3) break;
      printScene(F("Random Jot"), scene);
      generateRandomSceneValues();
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          Drawing::drawDotRotated(random(4096)-1024, random(256), 8000, centerX, centerY, angle % 360);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
        if(frame%20 == 0) {
          laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]); 
        } else {
          laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]); 
        }
      }
    break;

    case 1:
      printScene(F("Line Fan"), scene);
      generateRandomSceneValues();
      laser.setOffset(2048,2048);
      for (frame = 0; frame < sceneLen*100; frame+=speed) {
        if(strobeOn(frame)) {
          fanWithChasingBeam(frame, randomBeamCount);
        }
        updateAll(frame);
        rotationUpdateLinear(0, frame);
      }
    break;

    case 0:
      // empty scene - trigger random scene now
    break;
  } // end switch
  
  // Step backwards towards oldest scene
  if(gotoScene > 0) {
    gotoScene--;
  }
} // end main loop


void updateAll(unsigned int keyframe) {
  pauseButton.update();
  encoderButton.update();

  // Process pause button
  if(pauseButton.read() == LOW) {
    if(speed > 0 && !paused) {
      speed = 0;
    } else if(paused) {
      speed = speed_original;
    }
  } else {
    paused = speed == 0;
  }

  // Process encoder rotation and button press
  if(oldEncPos != encoderPos) {
    if(encoderButton.read() == LOW) {
      menuIndex+=encoderPos-oldEncPos;
      menuIndex%=5;
    } else {
      switch(menuIndex) {
        case 0:
          if(encoderPos-oldEncPos > 0) {
            gotoScene =  scene + 2;
            if(gotoScene > 27) {
              gotoScene = 2; // skip scene 0
            }
          } else {
            gotoScene = scene;
            if(scene == 1) {
              gotoScene = 27; // max scene index + 1
            }
          }
          frame = 32000; // jump to scene now
        break;
        case 1:
          intensity+=encoderPos-oldEncPos;
          intensity%=5;
        break;
        case 2:
          strobeSpeed+=(encoderPos-oldEncPos)*3;
          if(strobeSpeed >= 100) {
            strobeSpeed = 3;
          } else if(strobeSpeed < 3) {
            strobeSpeed = 99;
          }
          //strobeSpeed%=50;
        break;       
        case 3:
          speed+=encoderPos-oldEncPos;
          speed%=3+1;
        break;
        case 4:
         sceneLen+=encoderPos-oldEncPos;
          if(sceneLen > 30) {
            sceneLen = 1;
          } else if(sceneLen < 1) {
            sceneLen = 20;
          }
        case 5:
          generateRandomSceneValues();
        break;                     
      }
    }
    printMenu();
  }

  oldEncPos = encoderPos;

  scalingUpdateLinear(keyframe);
  centerUpdateLinear();
}

bool strobeOn(unsigned int keyframe) {
  bool laserOn;
  
  if(intensity < 3) return true;

  if(strobePattern[strobeMode] & strobeBitmask) {
    laserOn = true;
  } else {
    delayMicroseconds(8000);
    laserOn = false;
  }

  // Bitshift bitmask by one every 15 keyframes for next strobe state
  if(keyframe%(strobeSpeed/speed) == 0) {
    strobeBitmask >>= 1;
    if(strobeBitmask == 0) {
      strobeBitmask = 128;
    }

    // Random generation of new strobe pattern
    if(random(100-strobeSpeed/2) == 0) {
      strobeMode = random(20); 
      if(strobeMode >= 12) {
        strobeMode = 0; // if past strobePattern array length reset to full on
      }
      oled.setCursor(72,2);
      for(byte mask=128; mask>0; mask >>= 1) {
        oled.print(strobePattern[strobeMode] & mask ? F("1") : F("0"));
      }
    }
  }

  // Sub-strobing within main strobing for higher intensities
  if(intensity > 3) {
    if((strobePattern[strobeSubMode] & strobeSubBitmask) && laserOn) {
      laserOn = true;
    } else {
      delayMicroseconds(4000); // half of full strobe
      laserOn = false;
    }

    if(keyframe%(strobeSubSpeed+1) == 0) {
      strobeSubBitmask >>= 1;
      strobeSubSpeed++;
      strobeSubSpeed%=20;
      if(strobeSubBitmask == 0) {
        strobeSubBitmask = 128;
      }

      // Random generation of new sub-strobe pattern
      if(random(200-strobeSubSpeed/2) == 0) {
        strobeSubMode = random(30); 
        if(strobeSubMode >= 12) {
          strobeSubMode = 0; // if past strobePattern array length reset to full on
        }
      }
    }
  }
  return laserOn;
}

void printScene(__FlashStringHelper * name, int scene) {
  oled.clear();
  oled.set2X();
  oled.println(name);
  oled.set1X();
}

void generateRandomSceneValues() {
  oled.setCursor(0,2);
  oled.print(scene < 10 ? F("#0") : F("#"));
  oled.print(scene);
  oled.print(F(" "));

  do {
      // For color continuity only one of the colors is regenerated sometimes
    byte i = random(3);
    if(i < 2) {
      laserColor[i][0] = random(2);
      laserColor[i][1] = random(2);
      laserColor[i][2] = random(2);
    }
  } while(!laserColor[0][0] && !laserColor[0][1] && !laserColor[0][2]); // require a primary color
  laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);

  for(byte i=0; i<2; i++) {
    oled.print(laserColor[i][0] ? F("R") : F("r"));
    oled.print(laserColor[i][1] ? F("G") : F("g"));
    oled.print(laserColor[i][2] ? F("B ") : F("b "));
  }

  for(byte mask=128; mask>0; mask >>= 1) {
    oled.print(strobePattern[strobeMode] & mask ? F("1") : F("0"));
  }
  oled.println();

  rotationMultiplier1 = random(5*speed)+1;
  rotationMultiplier2 = random(5*speed)+1;
  rotationMode = random(2);
  randomBeamCount = random(8)+1;  
  if(randomBeamCount >= 8 && rotationMultiplier1 < 12) { // generate lots of beams unless going extra fast
    randomBeamCount = random(10)+8;
    rotationMultiplier1 *= 2;
  }

  oled.print(F("r:"));
  oled.print(rotationMultiplier1);
  oled.print(F("/"));
  oled.print(rotationMultiplier2); 
  //oled.print(F(" beams:"));
  //oled.print(randomBeamCount);

  oled.print(F(" pwm:"));
  if(intensity < 2 || random(intensity * 2) > 0) {
    laserPWM[0] = 255;
    laserPWM[1] = 255;
    laserPWM[2] = 255;
    oled.println(F("off"));
  } else {
    laserPWM[0] = random(255);
    laserPWM[1] = random(255);
    laserPWM[2] = random(255);

    oled.print(laserPWM[0] / 10);
    oled.print(F("/"));
    oled.print(laserPWM[1] / 10);
    oled.print(F("/"));
    oled.println(laserPWM[2] / 10);
  }
  laser.setPWM(laserPWM[0], laserPWM[1], laserPWM[2]);


  //oled.print(F(" rotMd:"));
  //oled.println(rotationMode);  
}

void printMenu() {
    oled.setCursor(0,0);
    oled.set2X();
    oled.print(F("           "));
    oled.setCursor(0,0);
    switch(menuIndex) {
      case 0:
        oled.print(F("Scene "));
        oled.print(scene);
      break;
      case 1:
        oled.print(F("Intensity "));
        oled.print(intensity);
      break;
      case 2:
        oled.print(F("Strobe "));
        oled.print(strobeSpeed);      
      break;
      case 3:
        oled.print(F("Speed "));
        oled.print(speed);      
      break;
      case 4:
        oled.print(F("Length "));
        oled.print(sceneLen);
      break;
      case 5:
        oled.print(F("Color Spin")); 
      break;          
    }
    oled.set1X();
}


// --------------Scaling modes -------------------

void scalingUpdateLinear(unsigned int keyframe) {
  scale += 0.001f * speed * scaleDir;

  if(scale > 1.0f) {
    scaleDir *= -1;
  }
  if(scale < 0.5f) {
    scaleDir *= -1;
  }
  
  laser.setScale(scale);
}

void centerUpdateLinear() {
  centerX += 2 * speed * directionX;

  if(centerX > 3000) {
    directionX *= -1;
  }
  if(centerX < 2000) {
    directionX *= -1;
  }

  centerY += 5  * speed * directionY;

  if(centerY > 1400) {
    directionY *= -1;
  }
  if(centerY < -1200) {
    directionY *= -1;
  }
}


// ------------- Rotation modes ------------------

void rotationUpdateNonLinear(int mode, unsigned int keyframe) {
    
    switch (mode)
    { 

      // Rotation mode: Smooth complete circle reversal - uneven 5L/10L multipliers are awesome
      case 0:
        if(keyframe%360 > 180) {
          long p = SIN(keyframe%180) * rotationMultiplier1 * speed;
          angle -= TO_INT(p);
        } else {
          long p = SIN(keyframe%180) * rotationMultiplier2 * speed;
          angle += TO_INT(p);
        }
      break;

      // Rotation mode: 3/4 circle bounce back
      case 1:
        if(keyframe%180 > 90) {
          long p = SIN(keyframe%180) * rotationMultiplier1 * speed;
          angle -= TO_INT(p);
        } else {
          long p = SIN(keyframe%180) * rotationMultiplier2 * speed;
          angle += TO_INT(p);
        }
      break;

      // Rotation mode: Not sure what this sin/cos combo is doing but I love it (with 6L/3L multiplier)
      case 2:
        if(keyframe%360 > 90) {
          long p = COS(keyframe%180) * rotationMultiplier1 * speed;
          angle += TO_INT(p);
        } else {
          long p = SIN(keyframe%180) * rotationMultiplier2 * speed;
          angle += TO_INT(p);
        }
      break;
    }
    
    // If angle overflowed negative, smooth out angle to avoid small glitch at -0
    if(angle > 64800u) {
      angle -= 64800u;
    }
}

void rotationUpdateLinear(int mode, unsigned int keyframe) {

  switch(mode) {
    
    // Rotation mode: Linear slow using mod op - higher rotationMultipler1 will be slower
    case 0:
      if(keyframe%rotationMultiplier1 == 0) {
        angle += speed * direction;
      }
    break;

    // Rotation mode: Linear fast
    case 1:
        angle += speed * direction * rotationMultiplier1;
    break;    
  }

  // Random rotation reversal, more likely when rotationMultiplier2 is small
  // Q: Should this be a different mode?
  if(keyframe%(rotationMultiplier2+1) == 0) {
    if(random(200/intensity) == 0) {
      direction *= -1;
    }
  }

  // If angle overflowed negative, smooth out angle to avoid small glitch at -0
  if(angle > 64800u) {
    angle -= 64800u;
  }
}


// ------------- Animation keyframes ---------------


void fanWithChasingBeam(unsigned int keyframe, unsigned int beamCount) {
    unsigned int beamWidth = 4096u / beamCount;
    //laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);

    // Chasing beam within bounds of rendered fan width
    // Note: (keyframe/beamCount)*50u*speed % scrolls chasing beam faster inversely related to beamCount
    Drawing::drawDotRotated(((keyframe/beamCount)*25u*(speed+1)*rotationMultiplier2)%(4096 - beamWidth / 2), 0, 5000, centerX, centerY, angle % 360);

    //laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);

    for (int i = 0; i < beamCount; i++) {
      Drawing::drawLineRotated(i * beamWidth, (keyframe*speed)%(300*(rotationMultiplier1+1)), i * beamWidth + beamWidth / 2, (keyframe*speed)%(300*(rotationMultiplier1+1)), centerX, centerY, angle % 360, true);
    }
}

void hyperBeamMonochrome(unsigned int keyframe) {
    unsigned int beamWidth = 2048u;

    // Draw each line with bright edges - maximum of 2 hyper beams
    // Swinging effect that raises Y over the course of the scene is cool
    for (int i = 0; i < 2; i++) {
      Drawing::drawDotRotated(i * beamWidth, keyframe%4096, 2500, centerX, centerY, angle % 360);
      Drawing::drawLineRotated(i * beamWidth ,keyframe%4096, i * beamWidth + beamWidth / 4, keyframe%4096, centerX, centerY, angle % 360, false);
      delayMicroseconds(2500);
      laser.off();

      // Draw sky again for increased definition
      Drawing::drawLineRotated(i * beamWidth ,keyframe%4096, i * beamWidth + beamWidth / 4, keyframe%4096, centerX, centerY, angle % 360, true);
    }
}

void hyperBeamPainted(unsigned int keyframe) {
    unsigned int beamWidth = 2048u;

    // Draw each line with bright edges - maximum of 2 hyper beams
    // Swinging effect that raises Y over the course of the scene is cool
    for (int i = 0; i < 2; i++) {
      laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
      Drawing::drawDotRotated(i * beamWidth, keyframe%4096, 1200, centerX, centerY, angle % 360);

      laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
      Drawing::drawLineRotated(i * beamWidth ,keyframe%4096, i * beamWidth + beamWidth / 4, keyframe%4096, centerX, centerY, angle % 360, true);
      
      laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
      Drawing::drawDotRotated(i * beamWidth + beamWidth / 4, keyframe%4096, 1200, centerX, centerY, angle % 360);

      // Draw sky again for increased definition
      laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
      Drawing::drawLineRotated(i * beamWidth ,keyframe%4096, i * beamWidth + beamWidth / 4, keyframe%4096, centerX, centerY, angle % 360, true);
    }
}

void hyperBeamPaintedAltWhite(unsigned int keyframe) {
    unsigned int beamWidth = 2048u;

    // Draw each line with bright white edges - maximum of 2 hyper beams
    // Swinging effect that raises Y over the course of the scene is cool
    for (int i = 0; i < 2; i++) {
      laser.setColor(true, true, true);
      Drawing::drawDotRotated(i * beamWidth, keyframe%4096, 100, centerX, centerY, angle % 360);

      // Switch fill color depending on direction
      if(direction > 0) {
        laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
      } else {
        laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
      }
      Drawing::drawLineRotated(i * beamWidth ,keyframe%4096, i * beamWidth + beamWidth / 4, keyframe%4096, centerX, centerY, angle % 360, true);
      Drawing::drawLineRotated(i * beamWidth ,keyframe%4096, i * beamWidth + beamWidth / 4, keyframe%4096, centerX, centerY, angle % 360, true);      

      laser.setColor(true, true, true);
      Drawing::drawDotRotated(i * beamWidth + beamWidth / 4, keyframe%4096, 100, centerX, centerY, angle % 360);
    }
}

void spotlightsPainted(unsigned int keyframe) {

    for (int i = 0; i < 2; i++) {
      laser.setColor(true, true, true);
      Drawing::drawDotRotated(beamPos[i], 0, 0, centerX, centerY, angle % 360);

      laser.setColor(laserColor[i][0], laserColor[i][1], laserColor[i][2]);
      Drawing::drawLineRotated(beamPos[i], 0, beamPos[i] + 1000, 0, centerX, centerY, angle % 360, true);
      
      laser.setColor(true, true, true);
      Drawing::drawDotRotated(beamPos[i] + 1000, 0, 0, centerX, centerY, angle % 360);

      // Draw sky again for increased definition
      laser.setColor(laserColor[i][0], laserColor[i][1], laserColor[i][2]);
      Drawing::drawLineRotated(beamPos[i], 0, beamPos[i] + 1000, 0, centerX, centerY, angle % 360, true);

      beamPos[i] += beamBipolar[i] * 28u * speed;

      if(beamPos[i] > 4200 || beamPos[i] < 100) {
        beamBipolar[i] *= -1;
      }
    }
}

void circlingPrism(unsigned int keyframe) {

    laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);

    Drawing::drawLineRotated(1024, 1024, 2048, 3064, centerX, centerY, angle % 360, false);
    delayMicroseconds(5000);
    laser.off();
    
    laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);

    Drawing::drawLineRotated(2048, 3064, 3064, 1024, centerX, centerY, angle % 360, false);
    delayMicroseconds(5000);
    laser.off();

    laser.setColor(laserColor[1][2], laserColor[1][0], laserColor[1][1]); // mix it up for third color

    Drawing::drawLineRotated(3064, 1024, 1024, 1024, centerX, centerY, angle % 360, false);
    delayMicroseconds(5000);
    laser.off();

}


void randomPrism(unsigned int keyframe) {

    Drawing::drawLineRotated(0, 0, 1024, 2048, centerX, centerY, angle % 360, false);
    delayMicroseconds(5000);
    laser.off();
    
    Drawing::drawLineRotated(1024, 2048, 2048, 0, centerX, centerY, angle % 360, false);
    delayMicroseconds(5000);
    laser.off();

    Drawing::drawLineRotated(2048, 0, 0, 0, centerX, centerY, angle % 360, false);
    delayMicroseconds(5000);
    laser.off();

  if(keyframe%(rotationMultiplier1+1) == 0) {
    if(random(20) == 0) {
        laser.setOffset(random(rotationMultiplier1*500)+512, random(rotationMultiplier2*500)+256);
    }
  }
  if(random(10)==0) {
      laser.setColor(laserColor[keyframe%2][0], laserColor[keyframe%2][1], laserColor[keyframe%2][2]);
  }
}

void raindrops(unsigned int keyframe, unsigned int beamCount) {
    unsigned int beamWidth = 4096u / beamCount;

    for (int i = 0; i < beamCount; i++) {
        laser.setColor(laserColor[i%2][0], laserColor[i%2][1], laserColor[i%2][2]);
        if(laserColor[i%2][0] || laserColor[i%2][1] || laserColor[i%2][2]) {
          laser.drawline(i * beamWidth, 4096 - ((4096 - beamPos[i]) / rotationMultiplier1), i * beamWidth, beamPos[i]);
        }
        beamPos[i] -= beamBipolar[i] * speed;

        if(beamPos[i] <= 125) {
          beamPos[i] = 4096;
          //beamBipolar[i] = random(100) + 25; // there's something nice about each line not being totally random every time
        }
    }
}

void fanRisingGrowing(unsigned int keyframe) {
    
    //  Rising fan with growing beams - using full 4096 window height sticks momentarily at top
    laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
    laser.drawline(0,   (keyframe*10*(speed+1))%(500*(rotationMultiplier1+2)),250 +(keyframe%600+(keyframe/8)), (keyframe*10*(speed+1))%(500*(rotationMultiplier1+2)));
    
    laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
    laser.drawline(1000,(keyframe*10*(speed+1))%(500*(rotationMultiplier2+2)),1250+(keyframe%600+(keyframe/8)),(keyframe*10*(speed+1))%(500*(rotationMultiplier2+2)));

    laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
    laser.drawline(2000,(keyframe*10*(speed+1))%(500*(rotationMultiplier1+2)),2250+(keyframe%600+(keyframe/8)),(keyframe*10*(speed+1))%(500*(rotationMultiplier1+2)));

    laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
    laser.drawline(3000,(keyframe*10*(speed+1))%(500*(rotationMultiplier2+2)),3250+(keyframe%600+(keyframe/8)),(keyframe*10*(speed+1))%(500*(rotationMultiplier2+2)));
    // (keyframe*20)%4000: Scroll fan vertically 20 times faster than loop
    // keyframe%600+keyframe/8: Grow fan width at loop rate, but also grow wider throughout the loop
}

void fanFallingSwitched(unsigned int keyframe, unsigned int beamCount) {   
    unsigned int beamWidth = 4096u / beamCount;

    for (int i = 0; i < beamCount; i++) {
      if(sceneState == i) { // highlighted chasing segment
          laser.setColor(true, true, true);
          laser.drawline(i * beamWidth, 4000-((keyframe*20*(speed+1))%(400*(rotationMultiplier1+2))), i * beamWidth + beamWidth / 2, 4000-((keyframe*20*(speed+1))%(400*(rotationMultiplier1+2))));
      } else {
          laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
          laser.drawline(i * beamWidth, 4000-((keyframe*20*(speed+1))%(400*(rotationMultiplier1+2))), i * beamWidth, 4000-((keyframe*20*(speed+1))%(400*(rotationMultiplier1+2))));
          laser.drawline(i * beamWidth + beamWidth / 2, 4000-((keyframe*20*(speed+1))%(400*(rotationMultiplier1+2))), i * beamWidth + beamWidth / 2, 4000-((keyframe*20*(speed+1))%(400*(rotationMultiplier1+2))));
      }
    
    }

    if(keyframe%10 == 0) {
      sceneState++;
      if(sceneState >= beamCount) {
        sceneState = 0;
      }
    }
}

void multiplyingBeamsSwitched(unsigned int keyframe) {
    // Beams start out of clip window and move linearly past other side
    for(int i = 0; i < randomBeamCount; i++) {
        beamPos[i] += beamBipolar[i] * 36u * speed;
        if(beamPos[i] > 0 && beamPos[i] < 4096) {
          Drawing::drawDotRotated(beamPos[i], 0, 200, centerX, centerY, angle % 360);
        } else {
          delayMicroseconds(500); // for more even timing, leaving off has an intense whipped effect
        }
    }
    // Last dot past clipping window, reverse direction and switch color
    if(beamPos[randomBeamCount-1] > 5000) {
        for(int i = 0; i < randomBeamCount; i++) {
          beamBipolar[i] = -1;
        }
        laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
    }
    if(beamPos[randomBeamCount-1] < -4096) {
        for(int i = 0; i < randomBeamCount; i++) {
          beamBipolar[i] = 1;
        }
        laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
    }
}

void multiplyingBeamsSlide(unsigned int keyframe) {
    // Beams start out of clip window and move linearly past other side
    for(int i = 0; i < randomBeamCount; i++) {
        // Slide dots into the center of the frame
        beamPos[i] += beamBipolar[i] * ((beamPos[i] > 3000 || beamPos[i] < 1000) ? 64 : 24) * speed;
        if(beamPos[i] > 0 && beamPos[i] < 4096) {
          Drawing::drawDotRotated(beamPos[i], 0, 200, centerX, centerY, angle % 360);
        } else {
          //delayMicroseconds(500); // for more even timing, leaving off has an intense whipped effect
        }
    }
    // Last dot past clipping window, reverse direction
    if(beamPos[randomBeamCount-1] > 6000) {
        for(int i = 0; i < randomBeamCount; i++) {
          beamBipolar[i] = -1;
        }
    }
    if(beamPos[randomBeamCount-1] < -5000) {
        for(int i = 0; i < randomBeamCount; i++) {
          beamBipolar[i] = 1;
        }
    }
}

void multiplyingBeamsOpposing(unsigned int keyframe) {
    // Beams start out of clip window and move linearly past other side
    for(int i = 0; i < randomBeamCount; i++) {
        beamPos[i] += beamBipolar[i] * 36 * speed;
        if(beamPos[i] > 0 && beamPos[i] < 4096) {
          if(i < (randomBeamCount/2)) {
            laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
          } else {
            laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
          }
          Drawing::drawDotRotated(beamPos[i], 0, 200, centerX, centerY, angle % 360);
        } else {
          delayMicroseconds(500); // for more even timing, leaving off has an intense whipped effect
        }
    }
    // Last dot past clipping window, reverse direction
    if(beamPos[(randomBeamCount/2)-1] > 4500) {
        for(int i = 0; i < randomBeamCount/2; i++) {
          beamBipolar[i] = -1;
        }
    }
    if(beamPos[(randomBeamCount/2)-1] < -2048) {
        for(int i = 0; i < randomBeamCount/2; i++) {
          beamBipolar[i] = 1;
        }
    }
    if(beamPos[randomBeamCount-1] > 4500) {
        for(int i = randomBeamCount/2; i < randomBeamCount; i++) {
          beamBipolar[i] = -1;
        }
    }
    if(beamPos[randomBeamCount-1] < -2048) {
        for(int i = randomBeamCount/2; i < randomBeamCount; i++) {
          beamBipolar[i] = 1;
        }
    }
}

void multiplyingBeamsInterlacedColorCollapse(unsigned int keyframe) {
    // Beams start out of clip window and move linearly to center / each other with alternating colors
    for(int i = 0; i < randomBeamCount; i++) {
        beamPos[i] += beamBipolar[i] * 36 * speed;
        if((i < randomBeamCount/2-1 && beamPos[i] > 0 && beamPos[i] < 2048) ||
           (i > randomBeamCount/2-1 && beamPos[i] > 2048 && beamPos[i] < 4096)) {
          laser.setColor(laserColor[i%2][0], laserColor[i%2][1], laserColor[i%2][2]); 
          Drawing::drawDotRotated(beamPos[i], 0, 200, centerX, centerY, angle % 360);
        } else {
          delayMicroseconds(500); // for more even timing, leaving off has an intense whipped effect
        }
    }
    // Last dot past clipping window ro center point, reverse direction
    if(beamPos[(randomBeamCount/2)-1] > 4500) {
        for(int i = 0; i < randomBeamCount/2; i++) {
          beamBipolar[i] = -1;
        }
    }
    if(beamPos[(randomBeamCount/2)-1] < -2048) {
        for(int i = 0; i < randomBeamCount/2; i++) {
          beamBipolar[i] = 1;
        }
    }
    if(beamPos[randomBeamCount-1] > 4500) {
        for(int i = randomBeamCount/2; i < randomBeamCount; i++) {
          beamBipolar[i] = -1;
        }
    }
    if(beamPos[randomBeamCount-1] < -2048) {
        for(int i = randomBeamCount/2; i < randomBeamCount; i++) {
          beamBipolar[i] = 1;
        }
    }
}

void multiplyingBeamsTangentalSky(unsigned int keyframe) {
    // Beams start out of clip window and move linearly past other side
    for(int i = 0; i < randomBeamCount; i++) {
        beamPos[i] += beamBipolar[i] * 36 * speed;
        if(beamPos[i] > 0 && beamPos[i] < 4096) {
          Drawing::drawDotRotated(beamPos[i], 0, 200, centerX, centerY, angle % 360);
        } else {
          delayMicroseconds(500); // for more even timing, leaving off has an intense whipped effect
        }
    }
    // Tangental sky 1
    Drawing::drawLineRotated(beamPos[0], 0, 4096, 4096, centerX, centerY, angle % 360, true);

    // Last dot past clipping window, reverse direction
    if(beamPos[randomBeamCount-1] > 5000) {
        for(int i = 0; i < randomBeamCount; i++) {
          beamBipolar[i] = -1;
        }
        laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
    }
    if(beamPos[randomBeamCount-1] < -4096) {
        for(int i = 0; i < randomBeamCount; i++) {
          beamBipolar[i] = 1;
        }
        laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
    }

    // Tangental sky 2 - connected to middle of fan
    Drawing::drawLineRotated(beamPos[randomBeamCount-1], 2048, beamPos[randomBeamCount/2], 0, centerX, centerY, angle % 360, true);
}

// This was supposed to be multiplying beams set out evenly from origin (not currently used)
void fightingBeams(unsigned int keyframe) {
  for(unsigned int i = 0; i < 10; i++) {
    // Draw beam at origin if its not its turn yet
    if(i < 4) {
      Drawing::drawDotRotated(keyframe/200u > i ? (keyframe*200u%4096u) / (i+1) : 0, 0, 200, centerX, centerY, angle % 360);
    } else { // losing battle mode!
      Drawing::drawDotRotated(4096 - (keyframe/200u > i ? (keyframe*200u%4096u) : 0), 0, 200, centerX, centerY, angle % 360);
    }
  }
  // Draw outer beams opposite for symmetry
  laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
  Drawing::drawLineRotated(0, 0, 4096, 0, centerX, centerY, angle % 360, true);
  laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
}

void fanClassicScroll(unsigned int keyframe, unsigned int beamCount) {
    unsigned int beamWidth = 4096u / beamCount;

    for (int i = 0; i < beamCount; i++) {
      Drawing::drawLineRotated(i * beamWidth, 0, i * beamWidth + beamWidth, 0, centerX, centerY, angle % 360, false);

      delayMicroseconds(keyframe*80u%30000u);
    }
    // Turn off laser after last beam
    laser.off();
}

void spiralingCircle(unsigned int keyframe) {
  int scale = (keyframe % 12) + 6;
  laser.sendto(SIN(0)/scale, COS(0)/scale);
  laser.on();
  for (unsigned int r = (direction > 0 ? 5 : 355); r <= 360; r += 5*direction)
  {    
    laser.sendto(SIN(r)/scale, COS(r)/scale);
    // Chasing beam if((keyframe * 5u * speed)%360 == r) {
    delayMicroseconds((keyframe * (4000u/(speed+1))%10000u));
  }
  laser.off();
  // Random reversal
  if(random(5/(speed+1)) == 0) {
    direction *= -1;
    if(direction > 0) {
      laser.setColor(laserColor[0][0], laserColor[0][1], laserColor[0][2]);
    } else {
      laser.setColor(laserColor[1][0], laserColor[1][1], laserColor[1][2]);
    }
  }
}

// TODO: every third line off as chasing
void geometricCircle(unsigned int keyframe) {
    const int scale = 12;
    laser.sendto(SIN(0)/scale, COS(0)/scale);
    laser.on();
    for (int r = 5;r<=360;r+=360/randomBeamCount)
    { 
      laser.on();   
      laser.sendto(SIN(r)/scale, COS(r)/scale);
      laser.off();
    }
}

// Rotary encoder interrupts
void PinA(){
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; // read all eight pin values then strip away all but pinA and pinB's values
  if(reading == B00001100 && aFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos --; //decrement the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00000100) bFlag = 1; //signal that we're expecting pinB to signal the transition to detent from free rotation
  sei(); //restart interrupts
}

void PinB(){
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; //read all eight pin values then strip away all but pinA and pinB's values
  if (reading == B00001100 && bFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos ++; //increment the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00001000) aFlag = 1; //signal that we're expecting pinA to signal the transition to detent from free rotation
  sei(); //restart interrupts
}


// draw a circle using sin/cos
void circle() {
  const int scale = 12;
  laser.sendto(SIN(0)/scale, COS(0)/scale);
  laser.on();
  for (int r = 5;r<=360;r+=5)
  {    
    laser.sendto(SIN(r)/scale, COS(r)/scale);
  }
  laser.off();
}

// Draw circle and count down from 3 to 1
void countDown() {
  laser.setScale(1);
  laser.setOffset(2048,2048);
  int center = Drawing::advance('3');
  for (char j = '3';j>'0';j--) {
    float scale = 0.0;
    float step = 0.01;
    for (int i = 0;i<40;i++) {
      laser.setScale(1);
      circle();
      laser.setScale(scale);
      Drawing::drawLetter(j, -center/3, -center*2/3 + 100);   
      scale += step;
      step += 0.002;
    }
  }
}
  
