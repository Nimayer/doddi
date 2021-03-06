#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
//#include <TeensyThreads.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#include <Audio.h>
#include <SPI.h>
#include <SD.h>

/*
NOTE Finite State Machine structure
off-selection-minigame-off
*/

//#define ACCEL
//#define ACCEL_DEBUG
#define vib 16
#define INTERRUPT_PIN  5
#define PIN            23   // Pin on the Arduino connected to the NeoPixels
#define NUMPIXELS      12   //Number of NeoPixels attached to the Arduino
#define SHAKE_THRESH   1500

void setAllPixels(uint32_t color);
uint8_t splitColor ( uint32_t c, char value );

MPU6050 mpu;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

AudioPlaySdWav           playSdWav1;
AudioOutputAnalog        dac1;
AudioConnection          patchCord1(playSdWav1, 0, dac1, 0);

//faces coordinates
const int facesYZ[12][2] = {
  { 0,   0}, //  0
  { 50,  30}, //  1
  { 40, -30}, //  2
  {-15, -60}, //  3
  {-69,  -5}, //  4
  {-15,  60}, // 5
  { 60, -3},//  6
  { 15, -50},//  7
  {-40, -30}, //  8 
  {-38,  30}, //  9
  { 20, 50}, //  10
  { 0 ,  0},  // 11
};

typedef struct color{
  int r;
  int g;
  int b;
} color;

typedef struct face{
  boolean resPresent;
  int resID;
  uint32_t color;
  boolean isActive;
} face;

const uint32_t yellow = pixels.Color(150, 150, 0);
const uint32_t red = pixels.Color(200, 0, 0);
const uint32_t green = pixels.Color(0, 200, 0);
const uint32_t blue = pixels.Color(0, 0, 200);
const uint32_t purple = pixels.Color(150, 0, 150);
const uint32_t off = pixels.Color(0, 0, 0);

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[128]; // FIFO storage buffer
float final_euler[3];
float final_ypr[3];
VectorInt16 aaWorld;
volatile bool mpuInterrupt = false;

enum states {OFF=0, SELECTION=1, MINIGAME=2, STOP=3};  //States of the FSM
int state = SELECTION;

// game global vars
boolean start = false;
boolean done = false;
boolean gamerunning = false;
boolean endgame = false;
boolean faceChange = false;
uint8_t activeF = 0;
boolean shaken = false;
long lastshake = 0;
face F[12] = {0};

void arrayInit() {
  // Initialize faces array
  memset(&F, 0, 12 * sizeof(face));
  randomSeed(analogRead(A7));
  // Random assignment of resources to faces.
  for(int i = 0; i < 12; i++) {
    F[i].resID = random(4);
  }
  // Set resource color to faces based on resID
  for (int i = 0; i < 12; i++) {
    switch(F[i].resID)
    {
      case 0:
        F[i].color = blue;
        break;
      case 1:
        F[i].color = yellow;
        break;
      case 2:
        F[i].color = green;
        break;
      case 3:
        F[i].color = red;
        break;
    }
  }
}

void maingame() {
  int oldr, oldg, oldb;
  uint32_t oldcolor;
  boolean facesLeft = true;
  switch(state)
  {
    case OFF:
      if(shaken) {
        state = SELECTION; //State change condition
        shaken = false; //Reset shaken to avoid skipping through states
        done = false;
      }
      setAllPixels(off);
      Serial.print("Waiting for start... ");
      break;

    case SELECTION:
      if(shaken) {
        state = MINIGAME; //State change condition
        shaken = false; //Reset shaken to avoid skipping through states
        setAllPixels(off);
        Serial.println("START TO PLAY!!                SOUND FEEDBACK-->intro story");
        Serial.println("Start playing");
        playSdWav1.play("start1.wav");
        delay(50); // wait for library to parse WAV info
      }
      else{
      while(!done) {
        setAllPixels(purple);
        delay(500); //Delay to make game slower
      done=true;
      }
      // Turn on all faces with colors
      // Set faces color to corresponding resources
      for (int i = 0; i < NUMPIXELS; i++) {
        if (F[i].isActive == 0) {
          pixels.setPixelColor(i, F[i].color);
          pixels.show();
          delay(50);
        }
      }
      oldcolor = pixels.getPixelColor(activeF);
      oldr = splitColor(oldcolor, 'r');
      oldg = splitColor(oldcolor, 'g');
      oldb = splitColor(oldcolor, 'b');
      pixels.setPixelColor(activeF, pixels.Color((oldr + 55), (oldg + 55), (oldb + 55)));
      pixels.show();
      }
      break;

    case MINIGAME:
      /* Highlight all the faces by placing them up*/
      //Highlight the upward face
      if (F[activeF].resPresent == false) {
        pixels.setPixelColor(activeF, purple);
        pixels.show();
        F[activeF].resPresent = 1;
      }
      facesLeft = false;
      //facesLeft: there are faces still not active
      for (int i = 0; i < 12; i++) {
        if(!F[i].resPresent) facesLeft = true;
      }
      if (facesLeft == false) state = STOP;
      break;

    case STOP:
      //zeroing faces
      for (int i = 0; i < 12; i++) {
        F[i].resPresent = 0;
      }
      //minigame sound play
      Serial.println("End playing");
      playSdWav1.play("win.wav");
      delay(50); // wait for library to parse WAV info
      state = SELECTION;
      break;
  }
}

void setAllPixels(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, color);
    pixels.show();
    delay(50);
  }
}

uint8_t splitColor ( uint32_t c, char value ) {
  switch ( value ) {
    case 'r': return (uint8_t)(c >> 16);
    case 'g': return (uint8_t)(c >>  8);
    case 'b': return (uint8_t)(c >>  0);
    default:  return 0;
  }
}

void dmpDataReady() {
    mpuInterrupt = true;
}

void setup() {
  /* Hardware initialization */
  pinMode(vib, OUTPUT);
  pinMode(INTERRUPT_PIN, INPUT);
  pixels.begin();
  pixels.setBrightness(64);
  pixels.show();  // turn off all the pixels
  Serial.begin(9600);
  SPI.begin();
  while(!(SD.begin(10))) {
    Serial.println("Unable to access the SD card");
    delay(500);
  }
  Wire.begin();
  Wire.setClock(400000);
  AudioMemory(100);
  accelinit();
  /* Data structure initialization */
  arrayInit();
}

void loop() {
  // The main loop is dedicated to the accelerometer
  VectorInt16 aa;         //accel sensor measurements
  VectorInt16 aaReal;     // [x, y, z]    gravity-free accel sensor measurements
  VectorFloat gravity;    //gravity vector
  Quaternion q;
  float euler[3];
  float ypr[3];
  if (!dmpReady) return;
    while (!mpuInterrupt){ //&& fifoCount < packetSize) { // This condition is causing hang
        // The game code is executed while the accelerometer is not working
        maingame();
    }
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();
    //Serial.print("MPU status: ");
    //Serial.println(mpuIntStatus);
    fifoCount = mpu.getFIFOCount();
    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));
    // otherwise, check for DMP data ready interrupt
    } else if (mpuIntStatus & 0x02) {
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();
        mpu.getFIFOBytes(fifoBuffer, packetSize);
        fifoCount -= packetSize;
        /* Orientation reading */
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetEuler(euler, &q);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        /* Acceleration reading */
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
        for(int i=0; i<3; i++)
        {
          final_euler[i] = euler[i] * 180/M_PI;
          final_ypr[i] = ypr[i] * 180/M_PI;
        }
        #ifdef ACCEL_DEBUG
        Serial.print("ypr\t");
        Serial.print(ypr[0] * 180/M_PI);
        Serial.print("\t");
        Serial.print(ypr[1] * 180/M_PI);
        Serial.print("\t");
        Serial.println(ypr[2] * 180/M_PI);
        Serial.print("euler z  ");
        Serial.println(euler[2] * 180/M_PI);
        Serial.print("aworld\t");
        Serial.print(aaWorld.x);
        Serial.print("\t");
        Serial.print(aaWorld.y);
        Serial.print("\t");
        Serial.println(aaWorld.z);
        #endif
        int newF = getFace();
        if(newF != activeF) {
          activeF = newF;
          Serial.print("Active face: ");
          Serial.println(activeF);
          digitalWrite(vib, HIGH);
          delay(30);
          digitalWrite(vib, LOW);
        }
        if(millis() - lastshake > 1000) {
          shaken = getShake();
          lastshake = millis();
          if(shaken) printf("Shake: true");
        }
    }
}

void accelinit() {
  mpu.initialize();
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
  devStatus = mpu.dmpInitialize();
  // supply your own gyro offsets here, scaled for min sensitivity
  mpu.setXGyroOffset(220);
  mpu.setYGyroOffset(76);
  mpu.setZGyroOffset(-85);
  mpu.setZAccelOffset(1788); // 1688 factory default for my test chip
  // make sure it worked (returns 0 if so)
  if (devStatus == 0) {
      mpu.setDMPEnabled(true);
      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
      mpuIntStatus = mpu.getIntStatus();
      dmpReady = true;
      packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
      // ERROR!
      // 1 = initial memory load failed
      // 2 = DMP configuration updates failed
      // (if it's going to break, usually the code will be 1)
      Serial.print(F("DMP Initialization failed (code "));
      Serial.print(devStatus);
      Serial.println(F(")"));
    }
}

int getFace() {
  int d, iMin = 0, dMin = 999999;
  float fY, fZ, dY, dZ;

  for (int i=0; i < 12; i++) { // For each face...
    fY = (float)facesYZ[i][0]; // Read y and z coordinates
    fZ = (float)facesYZ[i][1];
    dY = final_ypr[1] - fY; // Delta between accelerometer & face
    dZ = final_ypr[2] - fZ; // Delta between accelerometer & face
    d  = dY * dY + dZ * dZ; // Distance^2
    // Check if this face is the closest match so far.  Because
    // we're comparing RELATIVE distances, sqrt() can be avoided.
    if (d < dMin) { // New closest match?
      dMin = d;    // Save closest distance^2
      iMin = i;    // Save index of closest match
    }
  }
  if((iMin == 0) || (iMin == 11)) { //Top and bottom face disambiguation
    int eulerZ = (final_euler[2] > 0) ? final_euler[2] : -final_euler[2];
    iMin = (eulerZ > 160) ? 0 : 11;
  }
  return iMin; // Index of closest matching face*/
}

boolean getShake() {
  int accelx = (aaWorld.x > 0) ? aaWorld.x : -aaWorld.x;
  int accely = (aaWorld.y > 0) ? aaWorld.y : -aaWorld.y;
  int accelz = (aaWorld.z > 0) ? aaWorld.z : -aaWorld.z;
  if(accelx > SHAKE_THRESH || accely > SHAKE_THRESH || accelz > SHAKE_THRESH)
    return true;
  else return false;
}
