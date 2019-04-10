

#define DBG_OUTPUT_PORT Serial
#define DEBUG_ESP_WIFI 
#define DEBUG_ESP_PORT 
//---------------------------------------ESP WEBSERVER STUFF---------------------------
#include <ESP8266WiFi.h>
#include <FS.h>   //Include File System Headers
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h> //remove for now, since it doesnt even work
#include <ArduinoJson.h>

// With debug on, uses: Global variables use 46,068 bytes (56%) of dynamic memory, leaving 35,852 bytes for local variables. Maximum is 81,920 bytes
// with debug off,uses: Global variables use 42,484 bytes (51%) of dynamic memory, leaving 39,436 bytes for local variables. Maximum is 81,920 bytes.
// 1.8.9 2.5.0          Global variables use 33572 bytes (40%) of dynamic memory, leaving 48348 bytes for local variables. Maximum is 81920 bytes.
// before progmem:      Global variables use 32528 bytes (39%) of dynamic memory, leaving 49392 bytes for local variables. Maximum is 81920 bytes.
// single progmem:  Global variables use 32512 bytes (39%) of dynamic memory, leaving 49408 bytes for local variables. Maximum is 81920 bytes.
// most progmem: Global variables use 32064 bytes (39%) of dynamic memory, leaving 49856 bytes for local variables. Maximum is 81920 bytes.


//Crashes are caused by Neopixel's strip.show()! 
// use https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods instead
//use NeoEsp8266Uart1800KbpsMethod for uart1 on pin GPIO2/Wemos D4 ? This has some cpu overhead
//use NeoEsp8266Dma800KbpsMethod   for gpio3 (RXD Wemos), so no more serial recv

 


const char* ssid = "BRFK-NNyI";
const char* password = "ingyenwifi";
const char* host = "torch1";

const char* softapssid = "Torch1";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

uint32_t reconnect_interval = 300000;
uint32_t reconnect_last_connected = 0;
uint32_t reconnect_until = 0;
bool reconnect_state = false;
uint32_t prev_free_ram = 0;


ESP8266WebServer server(80);
//holds the current upload
File fsUploadFile;

//TODO for ESP: https://github.com/artf/grapick
//Add gradient picker
//Save gradients to eeprom
//on updates, use query urls to get the resluts of teh color pikaz
//adjustable DIM factors, speed factors?
//Access point mode for standalone ops
//Wifi mode for local connection
//
const int capacity = 3096;
//StaticJsonDocument<capacity> doc; //static is on stack, heap is max 4k so baaaad idea
DynamicJsonDocument doc(4096); //on the heap, so hopefully better 
String jsongradient = "";
uint32_t calibratewhiteness(uint32_t inrgb);

uint32_t last_web_update = 0x7FFFFFFF;
uint32_t web_update_timeout = 60000;

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) {
    return F("application/octet-stream");
  } else if (filename.endsWith(".htm")) {
    return F("text/html");
  } else if (filename.endsWith(".html")) {
    return F("text/html");
  } else if (filename.endsWith(".css")) {
    return F("text/css");
  } else if (filename.endsWith(".js")) {
    return F("application/javascript");
  } else if (filename.endsWith(".png")) {
    return F("image/png");
  } else if (filename.endsWith(".gif")) {
    return F("image/gif");
  } else if (filename.endsWith(".jpg")) {
    return F("image/jpeg");
  } else if (filename.endsWith(".ico")) {
    return F("image/x-icon");
  } else if (filename.endsWith(".xml")) {
    return F("text/xml");
  } else if (filename.endsWith(".pdf")) {
    return F("application/x-pdf");
  } else if (filename.endsWith(".zip")) {
    return F("application/x-zip");
  } else if (filename.endsWith(".gz")) {
    return F("application/x-gzip");
  } else if (filename.endsWith(".json")) {
    return F("application/json");
  }
  return F("text/plain");
}

bool handleFileRead(String path) {
  DBG_OUTPUT_PORT.print(F("handleFileRead: ")); DBG_OUTPUT_PORT.println(path);
  if (path.endsWith("/")) {
    path += "index.htm";
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  Serial.println(F("File does not exist"));
  return false;
}

void handleFileUpload() {
  if (server.uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    DBG_OUTPUT_PORT.print(F("handleFileUpload Name: ")); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    DBG_OUTPUT_PORT.print(F("handleFileUpload Size: ")); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void handleFileDelete() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.print(F("handleFileDelete: ")); DBG_OUTPUT_PORT.println(path);
  if (path == "/") {
    return server.send(500, F("text/plain"), "BAD PATH");
  }
  if (!SPIFFS.exists(path)) {
    return server.send(404, F("text/plain"), "FileNotFound");
  }
  SPIFFS.remove(path);
  server.send(200, F("text/plain"), "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0) {
    return server.send(500, F("text/plain"), "BAD ARGS");
  }
  String path = server.arg(0);
  DBG_OUTPUT_PORT.print(F("handleFileCreate: ")); DBG_OUTPUT_PORT.println(path);
  if (path == "/") {
    return server.send(500, F("text/plain"), "BAD PATH");
  }
  if (SPIFFS.exists(path)) {
    return server.send(500, F("text/plain"), "FILE EXISTS");
  }
  File file = SPIFFS.open(path, "w");
  if (file) {
    file.close();
  } else {
    return server.send(500, F("text/plain"), F("CREATE FAILED"));
  }
  server.send(200, F("text/plain"), "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, F("text/plain"), "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.print(F("handleFileList: ")); DBG_OUTPUT_PORT.println( path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") {
      output += ',';
    }
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, F("text/json"), output);
}


//-------------------------------------NEOPIXEL STUFF-------------------------------------
//Info: // R152 G79 B21 MATCHES W64!!!!!!!
//Design params: 5-10W average power draw

//Battery operation stability:
//@ 5V: r,g,b are 10mA each, w is 20mA
//@ idle of arduino is 20mA
//@ 4V: r,g,b are 10mA each, w is 20mA
//@ 3.6V:  r,g,b are 10mA each, w is 20mA
//@3.4 r,g,b are 10mA each, w is 15mA

//So for 60 LEDS, the max power draw is 50mA*60 = 3A (nearly 15W)
//Target power is approx 5W
//Conclusion: NO POINT IN 5V boost, operation down to 3.4V is totally fine
//Battery considerations
//Each lipo is 2000mAh, so 2 hours of operation per cell. is 4 hours enough? NO, we need 4 cells maybe.

#include "RGBWWPalette.h"
#include "FastLED.h"
#include <NeoPixelBus.h>
//#include <Adafruit_NeoPixel.h>
const uint8_t Width  = 8;
const uint8_t Height = 8;
const uint8_t CentreX =  (Width / 2) - 1;
const uint8_t CentreY = (Height / 2) - 1;

#define NUM_LEDS     64
#define BRIGHTNESS    25
#define DBG 0
#define PIN 4
#define DIMFACTOR 1.4 // height/6 is good
int delaytime = 20;
int heatness = 178;
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);
NeoPixelBus<NeoGrbwFeature,NeoEsp8266Dma800KbpsMethod> strip(64);
uint32_t deltat = 0;
//CRGB leds[NUM_LEDS];

RgbwColor supertest(0,0,0,0);

//RgbwColor((color >> 16) & 0xff,(color >> 8) & 0xff,(color >> 0) & 0xff,(color >> 24) & 0xff);
/*
RgbwColor * fromBGRW(uint32_t incolor){
	uint8_t b = incolor & 0xff;
	uint8_t g = (incolor >>8) & 0xff;
	uint8_t r = (incolor >>16) & 0xff;
	uint8_t w = (incolor >>24) & 0xff;
	return RgbwColor(r,g,b,w);
}*/
/*
uint32_t toBGRW(const RgbwColor& incolor){
	uint32_t result = 0;
	result = (incolor.W);
	result = (result << 8) + incolor.R;
	result = (result << 8) + incolor.G;
	result = (result << 8) + incolor.B;
	return result;
}*/

uint32_t uint32color(uint32_t r, uint32_t g, uint32_t b, uint32_t w){
	return ( ((r&0xff)<<16 ) + ((g&0xff)<<8 ) + ((b&0xff)<<0 ) + ((w&0xff)<<24 ) );
}

// control parameters for the noise array
uint32_t x;
uint32_t y;
uint32_t z;
uint32_t scale_x;
uint32_t scale_y;

// storage for the noise data
// adjust the size to suit your setup
uint8_t noise[8][8];

// heatmap data with the size matrix width * height
uint8_t heat[64];

// the color palette
CRGBPalette16 Pal;

//------------------------------------------HANDLE GET REQUESTS-----------------------------------
PalettePoint WebGradient[16]; //80 bytes
void handlesetGradients() {
  jsongradient = server.arg("setGradient");
  Serial.printf("Setgradients received at %ld: ", millis());
  server.send(200, "text/plain", "");
  #if DBG
	Serial.println(jsongradient);
  #endif
  return;
  DeserializationError err = deserializeJson(doc, jsongradient);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return;
  }
  int active = doc["active"];
  char objectname[20];
  //strcpy(objectname,"grad");
  //strcat(objectname, itoa(active));
  sprintf(objectname, "grad%d", active);
  for (int handle = 0; handle < doc[objectname].size(); handle++) {
    //WebGradient[handle].pos = doc[objectname][handle]["pos"];
    uint8_t r = doc[objectname][handle]["r"];
    uint8_t g = doc[objectname][handle]["g"];
    uint8_t b = doc[objectname][handle]["b"];
    int   pos = doc[objectname][handle]["pos"];
    pos = (pos * 256) / 100;
    uint8_t w = 0;
    #if DBG
		Serial.printf("Gradient %s handle %d R%d G%d B%d W%d pos%d\n", objectname, handle, r, g, b, w, pos);
	#endif

    uint32_t calibcolor = calibratewhiteness(uint32color(r, g, b, w));
    WebGradient[handle].wrgb =  calibcolor;
    //WebGradient[handle].wrgb =  strip.Color(r,g,b,w);
    WebGradient[handle].pos = pos;
  }
  sprintf(objectname, "grad%dspeed", active);
  int gradspeed = doc[objectname]; //[-13;+12]
  delaytime = 20 - gradspeed ;
  //fixed time to update is 3.5ms
  //delay goes from 11.5ms (85hz) to 35ms (30hz)

  sprintf(objectname, "grad%dheat", active);
  int gradheat = doc[objectname];////[-13;+12]
  heatness = min(255, 207 + 4 * gradheat);
  //heatness sensibly should go from 255 to 150



  //TODO PARSE JSON
  last_web_update = millis();
}

// check the Serial Monitor for fps rate
void show_fps() {
  EVERY_N_MILLIS(1000) {
    Serial.println(deltat);
  }
}

// this finds the right index within a serpentine matrixuint8_t x, uint8_t y) __attribute__((always_inline)); //this forces true inlineness
inline uint16_t XY( uint8_t x, uint8_t y) {
  uint16_t i;
  if ( y & 0x01) {
    uint8_t reverseX = (Width - 1) - x;
    i = (y * Width) + reverseX;
  } else {
    i = (y * Width) + x;
  }
  return i;
}

//this is a


PalettePoint WarmTorch4p[4] = { //0 is black, 255 is full white
  { 0, 0, 0, 0,   0}, //BGRW,L
  { 0, 4, 255, 10,   100},
  { 20, 100, 255, 100,   200},
  { 255, 255, 255, 255,    255}
};
#define WARMTORCH4P 4

PalettePoint WarmTorchFlash7p[7] = { //0 is black, 255 is full white
  { 0, 0, 0, 0,   0}, //BGRW,L
  { 0, 4, 80, 10,   30},
  { 100, 0, 50, 10,   35},
  { 0, 4, 100, 10,   40},
  { 0, 4, 255, 10,   100},
  { 20, 100, 255, 100,   200},
  { 255, 255, 255, 255,    255}
};
#define WARMTORCHFLASH7P 7






uint32_t calibratewhiteness(uint32_t inrgb) { // example in color is (128,200,50,0)
  // R152 G79 B21 MATCHES W64!!!!!!!
  ColorBGRW in = {inrgb};
  ColorBGRW ww64 = {uint32color(152, 79, 21, 64)};
  ColorBGRW nc = {0};

  //find max divisor
  uint16_t maxw = min(min((in.r * ww64.w) / ww64.r, (in.g * ww64.w) / ww64.g), min((in.b * ww64.w) / ww64.b, 255 - in.w));
  nc.w = min(255, in.w +  maxw);
  nc.r = (in.r - ((maxw * ww64.r) / ww64.w));
  nc.g = (in.g - ((maxw * ww64.g) / ww64.w));
  nc.b = (in.b - ((maxw * ww64.b) / ww64.w));

#if DBG
  Serial.print(F("Calibrating color "));
  printColorWRGB(in.wrgb);
  Serial.print(F("Using warmwhite64 "));
  printColorWRGB(ww64.wrgb);
  Serial.print (F("maxw= "));
  Serial.println(maxw);
  Serial.print(F("Result=           "));
  printColorWRGB(nc.wrgb);
#endif
  return nc.wrgb;
}

void holdcolor(uint32_t color, int holdtime) {
  for (int i = 0; i < NUM_LEDS; i++) {
	
    strip.SetPixelColor(i, RgbwColor((color >> 16) & 0xff,(color >> 8) & 0xff,(color >> 0) & 0xff,(color >> 24) & 0xff));
  }
  strip.Show();
  delay(holdtime);
}
void testcolors() {
  holdcolor(uint32color(0, 0, 0, 0), 3000 );
  holdcolor(uint32color(255, 0, 0, 0), 3000 );
  holdcolor(uint32color(0, 255, 0, 0), 3000 );
  holdcolor(uint32color(0, 0, 255, 0), 3000 );
  holdcolor(uint32color(0, 0, 0, 255), 3000 );
  holdcolor(uint32color(255, 255, 255, 0), 3000 );
  holdcolor(uint32color(255, 255, 255, 255), 3000 );
  holdcolor(uint32color(128, 128, 128, 128), 3000 );
}

//-------------------------------------------------SETUP-------------------------------------------------
void setup() {
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);
  jsongradient.reserve(1024);

  Serial.begin(115200); //serial.begin before strip.begin!
#if DBG
  for (int level = 0; level < 256; level++) {
    //Serial.print(level);
    //Serial.print("level gives color: ");
    //printColorWRGB(getFromPalette(WarmTorch4p, 4, level));
  }
#endif
  strip.Begin();  //serial.begin before strip.begin!
  //strip.setBrightness(BRIGHTNESS);
  //testcolors();
  calibratewhiteness(0);

  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }

  //SoftAP init:


  //WIFI INIT
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.mode(WIFI_AP);
    //WiFi.begin(ssid, password);
  }

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
    attempts++;
    if (attempts > 10) break;
  }

  Serial.print(F("Setting soft-AP configuration ... "));
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print(F("Setting soft-AP ... "));
  Serial.println(WiFi.softAP(softapssid) ? "Ready" : "Failed!");

  Serial.print(F("Soft-AP IP address = "));
  Serial.println(WiFi.softAPIP());

  WiFi.setAutoReconnect(false);
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print(F("Connected! IP address: "));
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  //MDNS.begin(host);
  DBG_OUTPUT_PORT.print(F("Open http://"));
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.println(F(".local/edit to see the file browser"));


  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/edit.htm")) {
      server.send(404, F("text/plain"), "FileNotFound");
    }
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "FileNotFound");
    }
  });

  server.on("/setGradients", handlesetGradients);

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");
  for (uint8_t i = 0; i < 4; i++) {
    WebGradient[i].wrgb = WarmTorch4p[i].wrgb;
    WebGradient[i].wrgb = WarmTorch4p[i].pos;
  }
}

//-----------------------------------------------LOOP-----------------------------------------------------
void loop() {
  Fire2018();
  server.handleClient();
  //show_fps();

  EVERY_N_MILLIS(5000){
	if (WiFi.status() == WL_CONNECTED){
		reconnect_last_connected = millis();
	}else{
		if (reconnect_last_connected + reconnect_interval < millis()){
			Serial.printf("Disconnected at %ds, enabling autoreconnect\n",millis()/1000);
			reconnect_last_connected = millis();
			reconnect_until = millis() + 10000;
			WiFi.setAutoReconnect(true);
			reconnect_state = true;
		}
		if ((reconnect_until < millis()) && reconnect_state){
			Serial.printf("%ds Disabling autoreconnect\n",millis()/1000);
			WiFi.setAutoReconnect(false);
			reconnect_state = false;
		}
      }
    }
	EVERY_N_MILLIS(4000){
		Serial.printf("ESP.getFreeHeap()=%d  ESP.getHeapFragmentation()=%d ESP.getMaxFreeBlockSize()=%d\n",ESP.getFreeHeap(),ESP.getHeapFragmentation(),ESP.getMaxFreeBlockSize());
		savesettings();
	}
}
//--------------------------------------------DRAW FIRE---------------------------------------------------
// here we go
/*
  https://gist.github.com/StefanPetrick/1ba4584e534ba99ca259c1103754e4c5
  FromFastLED Fire 2018 by Stefan Petrick
  https://www.youtube.com/watch?v=SWMu-a9pbyk

*/

void Fire2018() {
  uint32_t starttime = micros();
  // get one noise value out of a moving noise space
  uint16_t ctrl1 = inoise16(11 * millis(), 0, 0);
  // get another one
  uint16_t ctrl2 = inoise16(13 * millis(), 100000, 100000);
  // average of both to get a more unpredictable curve
  uint16_t  ctrl = ((ctrl1 + ctrl2) / 2);

  // this factor defines the general speed of the heatmap movement
  // high value = high speed
  uint8_t speed = 27;

  // here we define the impact of the wind
  // high factor = a lot of movement to the sides
  x = 3 * ctrl * speed;

  // this is the speed of the upstream itself
  // high factor = fast movement
  y = 15 * millis() * speed;

  // just for ever changing patterns we move through z as well
  z = 3 * millis() * speed ;

  // ...and dynamically scale the complete heatmap for some changes in the
  // size of the heatspots.
  // The speed of change is influenced by the factors in the calculation of ctrl1 & 2 above.
  // The divisor sets the impact of the size-scaling.
  scale_x = ctrl1 / 2;
  scale_y = ctrl2 / 2;

  // Calculate the noise array based on the control parameters.
  uint8_t layer = 0;
  for (uint8_t i = 0; i < Width; i++) {
    uint32_t ioffset = scale_x * (i - CentreX);
    for (uint8_t j = 0; j < Height; j++) {
      uint32_t joffset = scale_y * (j - CentreY);
      uint16_t data = ((inoise16(x + ioffset, y + joffset, z)) + 1);
      noise[i][j] = data >> 8;
    }
  }
  yield();

  // Draw the first (lowest) line - seed the fire.
  // It could be random pixels or anything else as well.
  for (uint8_t x = 0; x < Width; x++) {
    // draw
    //leds[XY(x, Height-1)] = ColorFromPalette( Pal, noise[x][0]);
	uint32_t color = getFromPalette(WebGradient, 16, noise[x][0]);
    strip.SetPixelColor(XY(x, Height - 1), RgbwColor((color >> 16) & 0xff,(color >> 8) & 0xff,(color >> 0) & 0xff,(color >> 24) & 0xff));
    // and fill the lowest line of the heatmap, too
    heat[XY(x, Height - 1)] = noise[x][0];
  }

  // Copy the heatmap one line up for the scrolling.
  for (uint8_t y = 0; y < Height - 1; y++) {
    for (uint8_t x = 0; x < Width; x++) {
      heat[XY(x, y)] = heat[XY(x, y + 1)];
    }
  }
  yield();
  // Scale the heatmap values down based on the independent scrolling noise array.
  for (uint8_t y = 0; y < Height - 1; y++) {
    for (uint8_t x = 0; x < Width; x++) {

      // get data from the calculated noise field
      uint8_t dim = noise[x][y];

      // This number is critical
      // If it´s to low (like 1.1) the fire dosn´t go up far enough.
      // If it´s to high (like 3) the fire goes up too high.
      // It depends on the framerate which number is best.
      // If the number is not right you loose the uplifting fire clouds
      // which seperate themself while rising up.
      //dim = dim / DIMFACTOR;
      dim = scale8(dim, heatness);
      dim = 255 - dim;

      //dim = 255 - (dim/decay)

      // here happens the scaling of the heatmap
      heat[XY(x, y)] = scale8(heat[XY(x, y)] , dim);
    }
  }
  yield();
  // Now just map the colors based on the heatmap.
  for (uint8_t y = 0; y < Height - 1; y++) {
    for (uint8_t x = 0; x < Width; x++) {
      //leds[XY(x, y)] = ColorFromPalette( Pal, heat[XY(x, y)]);
      uint32_t color = getFromPalette(WebGradient, 16, heat[XY(x, y)]);
      strip.SetPixelColor(XY(x, y), RgbwColor((color >> 16) & 0xff,(color >> 8) & 0xff,(color >> 0) & 0xff,(color >> 24) & 0xff));
    }
  }
  // Done. Bring it on!
  uint32_t showtime = micros(); //showtime should be 1.2us*32*NUM_LEDS = 2.3ms

  //shift all the pixels forward, as we are missing 4 pixels out of 64 (only 60 pixels per meter of led strip!)
  for (uint8_t i = 0; i < 60; i++) {
    strip.SetPixelColor(i, strip.GetPixelColor(i + 4));
  }
  yield();
  strip.Show();

  deltat = micros() - starttime; //takes about 10ms to update :/
  yield();
  // I hate this delay but with 8 bit scaling there is no way arround.
  // If the framerate gets too high the frame by frame scaling doesn´s work anymore.
  // Basically it does but it´s impossible to see then...
  uint32_t totalcurrent = 0;
  for (uint8_t y = 0; y < Height ; y++) {
    for (uint8_t x = 0; x < Width; x++) {
      //Serial.print(heat[XY(x, y)]);
	  RgbwColor rgbwc = strip.GetPixelColor(XY(x, y));
      uint32_t c = uint32color(rgbwc.R,rgbwc.G,rgbwc.B,rgbwc.W);
      uint8_t h = heat[XY(x, y)];
#if DBG
      Serial.print("Heat=" );
      Serial.print(h);
      Serial.print(" x=");
      Serial.print(x);
      Serial.print(" y=");
      Serial.print(y);
      Serial.print(' ');
      printColorWRGB(c);
#endif
      //Serial.println("");

      totalcurrent += c & 0xff;
      totalcurrent += (c & 0xff00) >> 8;
      totalcurrent += (c & 0xff0000) >> 16;
      totalcurrent += (c & 0xff000000) >> 24; //count twice as white is 20ma per led, where others are only 10ma per led
      totalcurrent += (c & 0xff000000) >> 24;
      // Serial.print('\t');
    }
    //Serial.println("");
  }
  EVERY_N_MILLIS(3000) {


    totalcurrent = totalcurrent / 25;//each 255 brightness is 10mA
    totalcurrent = (totalcurrent * BRIGHTNESS) / 255; //scale with hard coded brightness

    //Serial.print("Average brightness = (max 255)");
    Serial.print(totalcurrent);
    Serial.print("mA dt_us=");
    Serial.println(deltat);
  }


  // If you change the framerate here you need to adjust the
  // y speed and the dim divisor, too.
  delay(delaytime);
  yield();
  if (ESP.getFreeHeap() != prev_free_ram){
	  
	Serial.printf("Free=%d  Frag=%d\n",ESP.getFreeHeap(),ESP.getHeapFragmentation());
	prev_free_ram = ESP.getFreeHeap();
	Serial.printf("Stations connected to soft-AP = %d\n", WiFi.softAPgetStationNum());
  }
  //void * m = malloc(32); //MALLOC BOMB for testing
  //memset(m,0x41 ,32);//'A'
  //Serial.print("Allocated at: 0x");Serial.println((uint32_t)m, HEX);
  if (ESP.getFreeHeap()<16000){
	  dumpRAM(0x14000/2, 0x14000); //dump second half of ram
	  while(1);
  }
}

void savesettings(){
	if ((last_web_update + web_update_timeout) < millis()) {
		Serial.print(F("Updating files from web change: "));
		Serial.println(millis());
		last_web_update = 0x7FFFFFFF; //so we dont keep saving shit
		char fn[40];
		sprintf(fn, "/saved_%09d.js", millis());
		File outf = SPIFFS.open(fn, "w");
		if (!outf) {
			Serial.printf("Failed to open file %s\n", fn);
			return;
		}
		if (outf.print(jsongradient)) Serial.printf("File %s written sucessfully\n", fn);
		else Serial.printf("File %s write failed\n", fn);
		outf.close();
	}
}
void dumpRAM(uint32_t start, uint32_t end){
	//https://github.com/esp8266/esp8266-wiki/wiki/Memory-Map
	//3FFE8000h 	dram0 	14000h 	RAM 	RW 	User data RAM. Available to applications.
	//Start from bottom up?
	#define RAMSTART 0x3FFE8000
	#define RAMSIZE 0x14000
	//uint32_t * ram_ptr = RAMSTART;
	char chbuf [20];
	for (uint32_t i = RAMSTART + start; i< (RAMSTART+end); i=i+16){
		Serial.print(i,HEX);
		Serial.print(": ");
		uint32_t chbptr = 0;
		for (uint32_t j = 0; j < 4;j++){
			uint32_t nowptr = i+4*j;
			uint32_t ramval = (*(RwReg*)(nowptr));
			//Serial.print(ramval,HEX);
			Serial.printf("0x%08x",ramval);
//			Serial.print(*(nowptr),HEX);
			Serial.print("\t");
			for (uint32_t k = 0; k <4 ;k++){
				uint32_t charval = (ramval >> k*8) &0xff;
				if (charval >=32 && charval <=126){
					chbuf[chbptr] = charval;
				}else{
					chbuf[chbptr] = ' ';
				}
				chbptr++;
			}
		}
		chbuf[chbptr] = 0;
		Serial.println(chbuf);
		if (i%128 == 0) yield();
	}
	
}
	
	

	