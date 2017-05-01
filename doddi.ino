#define PIN            23   // Pin on the Arduino connected to the NeoPixels
#define NUMPIXELS      8   //Number of NeoPixels attached to the Arduino
//#define ACCEL

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
//#include <SerialFlash.h>

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

#ifdef ACCEL
/* Assign a unique ID to this sensor */
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);
#endif

AudioPlaySdWav           playSdWav1;
AudioOutputAnalog        dac1;
AudioConnection          patchCord1(playSdWav1, 0, dac1, 0);

//faces coordinates
static const float PROGMEM gtable[12][3] = {
  {  8.1, 0.4, 5.45 }, //  1
  {  4.1, 9.1,   0.2}, //  2
  {   8.3,  0.6,   -5.4}, //  3
  {  5.1,  -8.2,   -0.5}, //  4
  {  -0.3,  -5.42,  8.4}, //  5
  { -0.45,   5.3,   8.6}, //  6
  {  -5.5,  8.5,  -0.3}, //  7
  {   0.0,  5.2,  -8.9}, //  8
  {   0.2, -5.3,  -8.8}, //  9
  {  -5.1, -8.5,  -0.2}, // 10
  {  -8.8,     0,  5.1}, // 11
  {  -8.8, -0.2 , -5.7}, // 12
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

const uint32_t yellow = pixels.Color(255, 255, 0);
const uint32_t red = pixels.Color(200, 0, 0);
const uint32_t green = pixels.Color(0, 200, 0);
const uint32_t blue = pixels.Color(0, 0, 200);
const uint32_t purple = pixels.Color(150, 0, 150);
const uint32_t off = pixels.Color(0, 0, 0);

const int vib1 = 16;
const int vib2 = 17;
int faceChange = 1;
boolean endgame = false;
int8_t activeF = 1;

//Faces' states structure array
face F[12] = {0};

#ifdef ACCEL
void displaySensorDetails(void)
{
  sensor_t sensor;
  accel.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" m/s^2");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" m/s^2");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" m/s^2");
  Serial.println("------------------------------------");
  Serial.println("");
}
#endif

void setAllPixels(uint32_t color) {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, color);
    pixels.show();
    delay(50);
  }
}

uint8_t splitColor ( uint32_t c, char value )
{
  switch ( value ) {
    case 'r': return (uint8_t)(c >> 16);
    case 'g': return (uint8_t)(c >>  8);
    case 'b': return (uint8_t)(c >>  0);
    default:  return 0;
  }
}

void setup(void) {
  /* Hardware initialization */
  pinMode (vib1, OUTPUT);
  pinMode (vib2, OUTPUT);
  AudioMemory(100);
  Serial.begin(9600);
  SPI.begin();
  while(!(SD.begin(10))) {
    Serial.println("Unable to access the SD card");
    delay(500);
  }
  pixels.begin();
  // turn off all the pixels
  pixels.show();
  #ifdef ACCEL
  if (!accel.begin())
  {
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    while (1);
  }
  displaySensorDetails();
  #endif

  /* Data structure initialization */
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

void loop(void)
{
  /* Game start */
  boolean start;
  do{
    Serial.print("Waiting for start... ");
    Serial.println(start);
    start = shake(50, 150, 1);
  }while(!start);
  Serial.println("START TO PLAY!!                SOUND FEEDBACK-->intro story");
  Serial.println("Start playing");
  playSdWav1.play("start1.wav");
  delay(50); // wait for library to parse WAV info
  setAllPixels(purple);
  delay(500); //Delay to make game slower

  //TODO: Add main game loop
  #ifdef ACCEL
  /* Get a new sensor event */
  sensors_event_t event;
  accel.getEvent(&event);
  #endif
  /* Display the results (acceleration is measured in m/s^2) */
  //  Serial.print("X: "); Serial.print(event.acceleration.x); Serial.print("  ");
  //  Serial.print("Y: "); Serial.print(event.acceleration.y); Serial.print("  ");
  //  Serial.print("Z: "); Serial.print(event.acceleration.z); Serial.print("  ");Serial.println("m/s^2 ");
  //  delay(10);

  // Set faces color to corresponding resources
  for (int i = 0; i < NUMPIXELS; i++) {
    if (F[i].isActive == 0) {
      pixels.setPixelColor(i, F[i].color);
    }
  }
  pixels.show();

  boolean endgame = shake(50, 120, 1);
  while (endgame) {
    Serial.print("End Game:           enter animation");
    Serial.println(endgame);
    Serial.println("Starting minigames ");
    playSdWav1.play("start1.wav");   //starting minigame sound
    delay(50); // wait for library to parse WAV info
    // "animation"
    for (int i = 0; i < 3; i++) {
      setAllPixels(purple);
      setAllPixels(off);
    }
    game();
    endgame = shake(50, 170, 1);   //coment this out to stay inside the loop
    //minigame sound play
    Serial.println("End playing");
    //playSdWav1.play("win.wav");
    //delay(50); // wait for library to parse WAV info
  }
  #ifdef ACCEL
  activeF = getFace();
  #endif
  Serial.print("Face: ");
  Serial.print(activeF + 1);
  Serial.print("    ");
  Serial.print("Shake: ");
  boolean activ = shake(50, 80, 0);
  Serial.print(activ);
  Serial.println("    ");
  // Face highlighting
  if (F[activeF].isActive == 0) {
    // simple blink
    pixels.setPixelColor(activeF, F[activeF].color);
    pixels.show();
    delay(100);
    pixels.setPixelColor(activeF, off);
    pixels.show();
    Serial.print("resource = ");
    Serial.print(F[activeF].resID);
    //TODO: print resource status from color intensity
  }
  //vibration for choosing
  if (faceChange != activeF)
  {
    Serial.println("the face has changed");
    faceChange = activeF;
    digitalWrite(vib1, HIGH);
    digitalWrite(vib2, HIGH);
    delay(150);
    digitalWrite(vib1, LOW);
    digitalWrite(vib2, LOW);
  }
  //Face activation sequence
  while (activ) {
    //tell me what face was activated
    Serial.print("Face nr ");
    Serial.print(activeF + 1);
    Serial.println(" has been activated");
    //change color of the activated face to purple
    pixels.setPixelColor(activeF, purple);
    pixels.show();
    F[activeF].isActive = 1;
    //face activation sound
    switch(F[activeF].resID)
    {
      case 0:
        playSdWav1.play("s1.wav");
        break;
      case 1:
        playSdWav1.play("s2.wav");
        break;
      case 2:
        playSdWav1.play("s3.wav");
        break;
      case 3:
        playSdWav1.play("s4.wav");
        break;
    }
    delay(50); // wait for library to parse WAV info
    activ = shake(50, 100, 0);
    //delay(1000);
  }
}

uint32_t getFace(void) {
  int32_t  dX, dY, dZ, d, dMin = 999999;
  int16_t  fX, fY, fZ;
  uint8_t  i, iMin = 0;
  sensors_event_t event;
  #ifdef ACCEL
  accel.getEvent(&event);
  #endif

  for ( i = 0; i < 12; i++) { // For each face...
    fX = pgm_read_word(&gtable[i][0]); // Read face X/Y/Z
    fY = pgm_read_word(&gtable[i][1]); // from PROGMEM
    fZ = pgm_read_word(&gtable[i][2]);
    dX = event.acceleration.x - fX; // Delta between accelerometer & face
    dY = event.acceleration.y - fY;
    dZ = event.acceleration.z - fZ;
    d  = dX * dX + dY * dY + dZ * dZ; // Distance^2
    // Check if this face is the closest match so far.  Because
    // we're comparing RELATIVE distances, sqrt() can be avoided.
    if (d < dMin) { // New closest match?
      dMin = d;    // Save closest distance^2
      iMin = i;    // Save index of closest match
    }
  }
  return iMin; // Index of closest matching face
}

//shake detection
#ifdef ACCEL
boolean shake(uint32_t ms, uint32_t timeout, int small) {
  int d = (small)? 80 : 210; //small or big shake
  uint32_t startTime, prevTime, currentTime;
  int32_t  prevX, prevY, prevZ;
  int32_t  dX, dY, dZ;

  // Get initial orientation and time
  sensors_event_t event;
  #ifdef ACCEL
  accel.getEvent(&event);
  #endif
  prevX    = event.acceleration.x;
  prevY    = event.acceleration.y;
  prevZ    = event.acceleration.z;
  prevTime = startTime = millis();

  // Then make repeated readings until orientation settles down.
  // A normal roll should only take a second or two...if things do not
  // stabilize before timeout, probably being moved or carried.
  while (((currentTime = millis()) - startTime) < timeout) {
    if ((currentTime - prevTime) >= ms) return false; // Stable!
    sensors_event_t event;
    #ifdef ACCEL
    accel.getEvent(&event);
    #endif
    dX = event.acceleration.x - prevX; // X/Y/Z delta from last stable position
    dY = event.acceleration.y - prevY;
    dZ = event.acceleration.z - prevZ;
    // Compare distance.  sqrt() can be avoided by squaring distance
    // to be compared; about 100 units on X+Y+Z axes ((100^2)*3 = 30K)
    if ((dX * dX + dY * dY + dZ * dZ) >= d) { // Big change?
      prevX    = event.acceleration.x;    // Save new position
      prevY    = event.acceleration.y;
      prevZ    = event.acceleration.z;
      prevTime = millis(); // Reset timer
    }
  }
  return true;
}
#endif
#ifndef ACCEL
// Fake shake detection
boolean shake(uint32_t ms, uint32_t timeout, int small) {
  int time = millis();
  while(Serial.read()== -1 && (millis() - time < timeout))
  {
    delay(10);
  }
  if(millis() - time < timeout) return true;
  else return false;
}
#endif

void game() {
  setAllPixels(off);
  boolean facesLeft = true;
  int x = 0;
  while (facesLeft) {
    //get the upward face
    activeF = getFace();
    //check if there are not active faces left
    for (int i = 0; i < 12; i++) {
      x = x + F[i].resPresent;
    }
    if (x == 12) {
      facesLeft = false;
    }
    else {
      facesLeft = true;
      x = 0;
    }
    //Highlight face
    if (F[activeF].resPresent == 0) {
      uint32_t oldcolor = pixels.getPixelColor(activeF);
      int oldr = splitColor(oldcolor, 'r');
      int oldg = splitColor(oldcolor, 'g');
      int oldb = splitColor(oldcolor, 'b');
      pixels.setPixelColor(activeF, pixels.Color((oldr + 50), (oldg + 50), (oldb + 50)));
      pixels.show();
      F[activeF].resPresent = 1;
    }
  }
  //zeroing faces
  for (int i = 0; i < 12; i++) {
    F[i].resPresent = 0;
  }
  facesLeft = true;
  //minigame sound play
  Serial.println("End playing");
  playSdWav1.play("win.wav");
  delay(50); // wait for library to parse WAV info
}
