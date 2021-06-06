//Arduino
#include "Arduino.h"


//OTA
#include <ArduinoOTA.h>

#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>


/******************************************************************************************************************//**
 * briefing
 * WiFi Config
 *********************************************************************************************************************/	
#define Wifissid "Tausche Internet gegen Bier"
#define Wifipassword "69899240720270747889"
#define Wifihostname "Kanalarbeiter"


/******************************************************************************************************************//**
 * briefing
 * OtaPasswort is for security. You have to set in in "platformio.ini" -> upload_flags = --auth=xxxxxxx
 *********************************************************************************************************************/	
#define OtaPassword  "45214561sf"


/******************************************************************************************************************//**
 * briefing
 * Set LED
 *********************************************************************************************************************/	
#define LedCh 2
#define LedPin 4
#define LedFreq  5000
#define Ledresolution 8


/******************************************************************************************************************//**
 * WIFI & OTA
 *********************************************************************************************************************/	
void SetupOTA (void) {
  
  WiFi.setHostname(Wifihostname);
  //WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(Wifissid, Wifipassword);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname(Wifihostname);
  ArduinoOTA.setPassword(OtaPassword);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
     // Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
    //  Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
   //   Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}


/******************************************************************************************************************//**
 * Webserver
 *********************************************************************************************************************/	
WebServer server(80);

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto hiRes = esp32cam::Resolution::find(800, 600);

void
handleBmp()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }

  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  if (!frame->toBmp()) {
    Serial.println("CONVERT FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CONVERT OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/bmp");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void
serveJpg()
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void
handleJpgLo()
{
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void
handleJpgHi()
{
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void
handleJpg()
{
  server.sendHeader("Location", "/cam-hi.jpg");
  server.send(302, "", "");
}

void
handleMjpeg()
{
  if (server.arg("LED") !="")       { ledcWrite(LedCh, ((server.arg("LED")).toInt()));}
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }

  Serial.println("STREAM BEGIN");
  WiFiClient client = server.client();
  auto startTime = millis();
  int res = esp32cam::Camera.streamMjpeg(client);
  if (res <= 0) {
    Serial.printf("STREAM ERROR %d\n", res);
    return;
  }
  auto duration = millis() - startTime;
  Serial.printf("STREAM END %dfrm %0.2ffps\n", res, 1000.0 * res / duration);
  
}




/******************************************************************************************************************//**
 * Arduino Setup
 *********************************************************************************************************************/	
void setup() {
  SetupOTA();

  Serial.begin(115200);
  Serial.println();

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }


  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam.bmp");
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam.mjpeg");

  server.on("/cam.bmp", handleBmp);
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam.jpg", handleJpg);
  server.on("/live", handleMjpeg);

  server.begin();

  //LED
  // configure LED PWM functionalitites
  ledcSetup(LedCh, LedFreq, Ledresolution);
  
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(LedPin, LedCh);
}


/******************************************************************************************************************//**
 * Arduino loop
 *********************************************************************************************************************/	

void loop() {

  ArduinoOTA.handle();
  server.handleClient();

 // ledcWrite(LedCh, 50);

}                    




 
