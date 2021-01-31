#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <SD.h>
#include "RTClib.h"

const byte DNS_PORT = 53;
const String HOSTNAME = "CovCheck";

IPAddress apIP(172, 20, 0, 1);
IPAddress netMsk(255, 255, 255, 0);

DNSServer dnsServer;
ESP8266WebServer server(80);
RTC_DS1307 rtc;

/////////////////////// Basics
boolean sdCardWorking;
boolean rtcWorking;
String filename = "log.txt";
File file;
int id = 0;
String serverUri;
String response;
String customResponse;
String customName;
String customLogFileEntry;
////////////////////// Form data
bool symptoms;
bool contact;
String firstName;
String lastName;
String address;
String phoneNumber;

String getTime(){
  String s;
  DateTime now = rtc.now();
  s = now.second();
  s += ":";
  s += now.minute();
  s += ":";
  s += now.hour();
  s += " - ";  
  s += now.day();
  s += ".";  
  s += now.month();
  s += ".";  
  s += now.year();
  return s;
}

void handleRoot() {
  Serial.print("Answering with root to: ");
  Serial.println(server.uri());
  if(sdCardWorking && rtcWorking){
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  sendFile("/form.html");
  }else{
  handleError();   
  }
}

void evaluateData(){
  Serial.println("Evaluating recieved data..."); 
  symptoms = true;
  contact = true;
  firstName = "";
  lastName = "";
  address = "";
  phoneNumber = "";
    if(server.argName(0)=="symptoms"){
      if(server.arg(0) == "false"){
        symptoms = false;
    }
    }

    
    if(server.argName(1)=="contact"){
      if(server.arg(1) == "false"){
        contact = false;
      }
    }
    
    if(server.argName(2)=="firstName"){
    firstName = server.arg(2);  
    }
    
    if(server.argName(3)=="lastName"){
    lastName = server.arg(3); 
    }
    
    if(server.argName(4)=="address"){
    address = server.arg(4); 
    }
    
    if(server.argName(5)=="phoneNumber"){
    phoneNumber = server.arg(5); 
    }

    if(!contact && !symptoms){
  Serial.print("  Saving data...");
  customLogFileEntry = id;
  customLogFileEntry += ";";
  customLogFileEntry += getTime();
  customLogFileEntry += ";";
  customLogFileEntry += firstName;
  customLogFileEntry += ";";
  customLogFileEntry += lastName;
  customLogFileEntry += ";";
  customLogFileEntry += address;
  customLogFileEntry += ";";
  customLogFileEntry += phoneNumber;
  file = SD.open(filename, FILE_WRITE );
  file.println(customLogFileEntry);
  file.close();
  Serial.println("Done");
  Serial.print("  Sending response...");
  customResponse = response;
  customName = firstName;
  customName += " ";
  customName += lastName;
  customResponse.replace("%name",customName);
  customResponse.replace("%id",String(id));
  server.send(200, "text/html", customResponse);
  id++;
  Serial.println("Done");
  }else{  
  Serial.print("  Sending warning...");
  sendFile("/warning.html"); 
  Serial.println("Done"); 
  }
  Serial.println("Done");
}
  
void handleNotFound() {
  if(!captivePortal()){
  serverUri = server.uri();
  if (!sendFile( serverUri)) { //check if it is not a ressource
  Serial.print("Not Found: ");
  Serial.println( serverUri);
  handleError();  
  }
  }
}

boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(WiFi.hostname()) + ".local")) {
    Serial.println("Request redirected...");
    server.sendHeader("Location", String("http://") + ipToString(server.client().localIP()), true);
    server.send ( 302, "text/plain", "");
    server.client().stop();
    return true;
  }
  return false;
}

boolean isIp(String s) {
  for (int i = 0; i < s.length(); i++) {
    int c = s.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

String ipToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 3; i++) {
    s += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  s += String(((ip >> 8 * 3)) & 0xFF);
  return s;
}

void handleError(){
  if (!sendFile("/error.html")) { //check if error.html is available
    Serial.println("Sending 404 manually");
    server.send(404,"text/plain","404 Error - Page not Found");
    }
}

bool sendFile(String path) { 
  Serial.print("Reading file: " + path + " from internal memory");
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, "text/html");
    file.close();
    Serial.println(String ("  --  \"") + path + ("\" was sent successfully") );
    return true;
  }
  Serial.println(" --  Error: file does not exist");   // If the file doesn't exist, return false
  return false;
}


//====================================================================================
//                                    SETUP
//====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.print("Configuring WiFi Accespoint - ");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(HOSTNAME);
  Serial.println("Done");


  Serial.print("Starting SD-Card - ");
  if (SD.begin(SS)) {
    file = SD.open(filename, FILE_WRITE );
    file.println("ID;TIME;FIRSTNAME;LASTNAME;ADDRESS;PHONENUMBER");
    file.close();
    Serial.println("Success");
    sdCardWorking = true;
  }else{
    Serial.println("Initialization failed");
    sdCardWorking = false;
  }




  Serial.print("Starting Captive Portal and Web Server - ");
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  server.on("/", handleRoot);
  server.on("/generate_204", handleRoot); //android page
  server.on("/fwlink", handleRoot);       //windows page
  server.on("/evaluation", evaluateData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Done");



  Serial.print("Starting SPIFFS - ");
  SPIFFS.begin();
  Serial.println("Done");



  Serial.print("Starting RTC - ");
  if (rtc.begin()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("Success");
    rtcWorking = true;
  }else{
    Serial.println("Initialization failed");
    rtcWorking = false;
  }


  
  file = SPIFFS.open("/response.html","r");
  if (file) {
    response = file.readString();
    file.close();    
  }   
}


//====================================================================================
//                                    LOOP
//====================================================================================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
