#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RestClient.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <FS.h>
#define LEDUP 14

LiquidCrystal_I2C lcd(0x27, 16, 2);

static const unsigned long STRUCT_MAGIC = 123456789;
float VERSION = 0.1;
int _timeFREQUENCY  = 0;
int _timeREFRESHSCREEN = 0;
int _timeREFRESHTEMP = 1;
/* Les parametres wifi */
struct WifiParams {
  unsigned long magic;
  char WIFISSID[32];
  char WIFIPWD[64];
  char WIFIHOST[10];
  char SERVERHOST[16];
  char SERVERLOGIN[10];
  char SERVERPWD[10];
  int FREQUENCY;
  float VERSION;
};
int NOSSID  = 0;
String temp_string ="";
WifiParams ms;

/* Les autres parametres */
#define ONE_WIRE_BUS1 2 /* connect to GPIO */
OneWire ourWire1(ONE_WIRE_BUS1);
DallasTemperature sensor1(&ourWire1);
float temp            = 0;
ESP8266WebServer server(80);

/* Gestion de la eeprom */
void readEEPROM() {
  // Lit la mémoire EEPROM
  EEPROM.begin(512);
  EEPROM.get(0, ms);
  EEPROM.commit();
  EEPROM.end();
  
  // Détection d'une mémoire non initialisée
  byte erreur = ms.magic != STRUCT_MAGIC;
  // Valeurs par défaut struct_version == 0
  if (erreur) {
    Serial.print("EEPROM Magic erreur : " );
    Serial.println(ms.magic);
    clearEEPROM();
    readEEPROM();
  }
  erreur = ms.VERSION != VERSION;
   if (erreur) {
    Serial.print("EEPROM VERSION : " );
    Serial.print(ms.VERSION);
    Serial.print(" unless VERSION : " );
    Serial.println(VERSION);
    clearEEPROM();
    readEEPROM();
  }
  _timeFREQUENCY  = -ms.FREQUENCY;
}

void clearEEPROM() {
  strcpy(ms.WIFISSID, "A848_N");
  strcpy(ms.WIFIPWD, "thomjess");
  strcpy(ms.WIFIHOST, "ESP8266");
  strcpy(ms.SERVERHOST, "192.168.4.2");
  strcpy(ms.SERVERLOGIN, "ESP8266");
  strcpy(ms.SERVERPWD, "");
  ms.VERSION=VERSION;
  ms.FREQUENCY=0;
  saveEEPROM();
}

void saveEEPROM() {
  // Met à jour le nombre magic et le numéro de version avant l'écriture
  ms.magic = STRUCT_MAGIC;
  EEPROM.begin(512);
  EEPROM.put(0, ms);
  EEPROM.end();
}

void WiFiConnectSTA(int wait=0) {
  WiFi.mode(WIFI_STA);
  delay(1000);
  if(WiFi.status() != WL_CONNECTED ){
    Serial.print("Connecting to ");
    Serial.println(ms.WIFISSID);
    int i=-1;
    while ( WiFi.status() != WL_CONNECTED && i < wait ) {
      WiFi.begin(ms.WIFISSID, ms.WIFIPWD);
      int j=0;
      while ( WiFi.status() != WL_CONNECTED && j < 20 ) {
        digitalWrite(LEDUP,LOW);
        delay ( 500 );
        Serial.print ( "." );
        digitalWrite(LEDUP,HIGH);
        if (wait) i++;
        j++;
      }
    }
  }
  if(WiFi.status() == WL_CONNECTED){
     Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }
  server.send ( 404, "text/plain", message );
}

void handleGetTemperature(){
  server.send(200, "application/json", "{\"temperature\":\"" + temp_string + "\"}");
}

void handleConfig(){
  String html ="<html>"
"<head>"
"<title>Temperature (config)</title>"
"<style type='text/css'>body{width:500px;margin:auto}.d0{background-color:#ceb5de}.p0{color:grey;font-size:2em}.p2{width:200px;text-align:right;display:inline-block}.p3{border-radius:25px;background-color:#83fdb2;width:150px;margin-top:5px;box-shadow:10px 10px 5px grey}.bs{cursor:pointer;text-align:center;margin:auto}"
".p4{width:100px;height:100px;background:#fafafa;box-shadow:2px 2px 8px #aaa;font:bold 13px Arial;border-radius:50%;display:flex;justify-content:center;align-items:center}input{width:150px}.br{border-radius:25px;padding:10px;margin:10px}#cS{position:fixed;bottom:0;background-color:#f5a9a9;width:450px;display:none}</style>"
"</head>"
"<body>"
"<div class='br' id='cS'>&nbsp;</div>"
"<div class='d0 br'>"
"<div class='p0'>Configuration du serveur</div>"
"<div class='p1'>"
"<div class='p2'>IP : </div>"
"<input type='text' name='SERVERHOST' id='SERVERHOST' value='";
html = html + ms.SERVERHOST;
html = html+"' maxlength='16'>"
"<div class='p2'>Frequence : </div>"
"<input type='text' name='FREQUENCY' id='FREQUENCY' value='";
html = html + ms.FREQUENCY;
html = html+"' maxlength='16'>seconde(s)"
"</div>"
"<div class='bs p3' onclick='sset()'>Valider</div>"
"</div>"
"<div class='d0 br'>"
"<div class='p0'>Configuration du wifi</div>"
"<div class='p1'>"
"<div class='p2'>Hostname : </div>"
"<input type='text' name='WIFIHOST' id='WIFIHOST' value='";
html = html + ms.WIFIHOST;
html = html+"' maxlength='9'>.local"
"</div>"
"<div class='p1'>"
"<div class='p2'>SSID : </div>"
"<input type='text' name='WIFISSID' id='WIFISSID' value='";
html = html +ms.WIFISSID ;
html += "' maxlength='32'>"
"</div>"
"<div class='p1'><div class='p2'>Password : </div>"
"<input type='password' name='WIFIPWD' id='WIFIPWD' value='";
html = html +ms.WIFIPWD ;
html +=  "' maxlength='64'>"
"</div>"
"<div class='bs p3' onclick='cset()'>Valider</div>"
"</div>"
"<div class='d0 br'>"
"<div class='p0'>Configuration usine</div>"
"<div class='bs p3' onclick='crst()'>Default configuration</div><br>"
"<div class='bs p3' onclick='restart()'>Restart</div>"
"</div>"
"<a href='/index.html'>Page d'accueil</a>"
"<script type='text/javascript'>/*<![CDATA[*/var cS=document.getElementById('cS');function restart(){send('/rest/reload')}function crst(){send('/rest/reset')}"
"function cset(){send('/rest/configset?WIFIHOST='+document.getElementById('WIFIHOST').value+'&WIFISSID='+document.getElementById('WIFISSID').value+'&WIFIPWD='+document.getElementById('WIFIPWD').value)}"
"function sset(){send('/rest/configset?SERVERHOST='+document.getElementById('SERVERHOST').value+'&FREQUENCY='+document.getElementById('FREQUENCY').value)}"
"function send(uri) {"
"    if (x && x.readyState != 0) {"
"     return;"
"   }"
"   var x = new XMLHttpRequest();"
"   x.onreadystatechange = function() {"
"     if (x.readyState == 4){"
"       if( x.status == 200) {"
"         var resp = x.responseText;"
"         var obj = JSON.parse(resp);"
"         if (typeof obj.message != 'undefined') {"
"           cS.innerHTML = obj.message;"
"           cS.style.display='block';"
"           setTimeout(function(){cS.style.display='none';}, 3000);"
"         }"
"         if (typeof obj.pt != 'undefined') {"
"           sT(parseInt(obj.pt));"
"         }"
"       }"
"       if (x.status== 404) {"
"         cS.innerHTML = 'Page not found';"
"         cS.style.display='block';"
"         setTimeout(function(){cS.style.display='none';}, 3000);"
"       }"
"     }"
"   };"
"   x.timeout = 4000;"
"   x.ontimeout = function () { "
"     cS.innerHTML = 'Prise not reachable !';"
"     cS.style.display='block';"
"     setTimeout(function(){cS.style.display='none';}, 3000); "
"     };"
"   x.open('GET', uri, true);"
"   x.send();"
" }"
"/*]]>*/</script>"
"</body>"
"</html>";

  server.send(200, "text/html", html);
}

void handleSaveConfig() {
  Serial.println("Saving param ... ");
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    //SSID
    if(!strcmp(server.argName (i).c_str(), "WIFISSID")) {
      Serial.println("Saving SSID ... ");
      strcpy(ms.WIFISSID, server.arg ( i ).c_str());
      Serial.print("SSID ");
      Serial.println(server.arg (i));
    }
    //WIFIPWD
    if(!strcmp(server.argName (i).c_str(), "WIFIPWD")) {
      Serial.println("Saving PWD ... ");
      strcpy(ms.WIFIPWD, server.arg ( i ).c_str());
      Serial.print("WIFIPWD ");
      Serial.println(server.arg (i));
    }
    //WIFIHOST
    if(!strcmp(server.argName (i).c_str(), "WIFIHOST")) {
      Serial.println("Saving WIFIHOST ... ");
      strcpy(ms.WIFIHOST, server.arg ( i ).c_str());
      Serial.print("WIFIHOST ");
      Serial.println(server.arg (i));
    }
    //SERVERHOST
    if(!strcmp(server.argName (i).c_str(), "SERVERHOST")) {
      Serial.println("Saving SERVERHOST ... ");
      strcpy(ms.SERVERHOST, server.arg ( i ).c_str());
      Serial.print("SERVERHOST ");
      Serial.println(server.arg (i));
    }
    //SERVERLOGIN
    if(!strcmp(server.argName (i).c_str(), "SERVERLOGIN")) {
      Serial.println("Saving SERVERLOGIN ... ");
      strcpy(ms.SERVERLOGIN, server.arg ( i ).c_str());
      Serial.print("SERVERLOGIN ");
      Serial.println(server.arg (i));
    }
    //SERVERPWD
    if(!strcmp(server.argName (i).c_str(), "SERVERPWD")) {
      Serial.println("Saving SERVERPWD ... ");
      strcpy(ms.SERVERPWD, server.arg ( i ).c_str());
      Serial.print("SERVERPWD ");
      Serial.println(server.arg (i));
    }
    //FREQUENCY
    if(!strcmp(server.argName (i).c_str(), "FREQUENCY")) {
      Serial.println("Saving FREQUENCY ... ");
      ms.FREQUENCY = server.arg ( i ).toInt();
      Serial.print("FREQUENCY ");
      Serial.println(server.arg (i));
    }
  }
  saveEEPROM();
  Serial.println("Params saved !");
  server.send(200, "application/json", "{\"message\":\"Params saved !\"}");
  delay(100);
}

void handleReset(){
  clearEEPROM();
  handleRestart();
}

void updateTemp(){
  Serial.print("Trying to get temperature value ");
  do {
    sensor1.requestTemperatures(); 
    temp = sensor1.getTempCByIndex(0);
  } while (temp == 85.0 || temp == (-127.0));
  Serial.println( temp);
  
  temp_string=temp;
}
void updateScreen(){
  lcd.setCursor(5,1);
  lcd.print(temp_string + " C            ");
}
void SendTemperature(){
  String response ="";
  if(strlen(ms.SERVERHOST) > 0){
    RestClient client = RestClient(ms.SERVERHOST);
    int statusCode = client.post("/rest/post", ("{\"temperature\":\"" + temp_string + "\",\"host\":\"" + ms.WIFIHOST + "\"}").c_str (), &response);
    Serial.print("Status code from server: ");
    Serial.println(statusCode);
    Serial.print("Response body from server: ");
    Serial.println(response);
  }else {
    Serial.println("No Server defined !");
  }
}
void handleLed(){
  int led_state = digitalRead(14);
  digitalWrite(14, !led_state);
  if(led_state){
    server.send(200, "application/json", "{\"message\":\"Led has changed state (now DOWN)! \"}");
  }else{
    server.send(200, "application/json", "{\"message\":\"Led has changed state (now UP)! \"}");
  }
}

void handleRestart(){
  server.send(200, "application/json", "{\"message\":\"Restart card ! \"}");
  ESP.restart();
}

void handleName(){
  lcd.setCursor(0,1);
  lcd.print(ms.WIFIHOST);
  lcd.print("                ");
  server.send(200, "application/json", "{\"message\":\"Name display for 5 secondes \"}");
  delay(4*1000);
  lcd.setCursor(0,1);
  lcd.print("Temp:");
  updateTemp();
  updateScreen();
}

void setup() {
  lcd.begin();
  lcd.backlight();
  lcd.scrollDisplayRight();
  lcd.clear();
  
  Serial.begin(115200);
  Serial.println("Startup");
  // lecture des parametres wifi
  readEEPROM();

  // Lecture de la memoire SPIFFS
  if (!SPIFFS.begin()){
    Serial.println("SPIFFS Mount failed");
  } else {
    Serial.println("SPIFFS Mount succesfull");
  }
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
  }
      
  Serial.println("Init GPIO ... ");
  pinMode(LEDUP,OUTPUT);
 
  //eteindre la led
  digitalWrite(LEDUP,LOW);

  // Activation de la carte wifi
  Serial.println("Connecting ... ");
  lcd.clear();
  lcd.print(ms.WIFIHOST);
  lcd.setCursor(0,1);
  lcd.print("Connecting...");
  WiFi.disconnect();
  if(strlen(ms.WIFISSID) > 1){
    Serial.println("Station MODE");
    WiFiConnectSTA(10);
  }
  
  if(WiFi.status() != WL_CONNECTED ){
    NOSSID=1;
    WiFi.softAP("ESP8266Temperature", "");
    Serial.println("SoftAP MODE");
  }

  /* La page index */
  lcd.clear();
 if(NOSSID){
    lcd.print("L");
    lcd.print("192.168.4.1");
    server.on ( "/", handleConfig );
  } else {
    lcd.print("@");
    lcd.print(WiFi.localIP());
    server.serveStatic("/", SPIFFS, "/index.html");
  }
  server.serveStatic("/index.html", SPIFFS, "/index.html");
  /* La page conf */
  server.on ( "/rest/configset", handleSaveConfig );
  server.on ( "/rest/reset", handleReset );
  server.on ( "/rest/reload", handleRestart);
  server.on ( "/rest/temperature", handleGetTemperature);
  server.on ( "/config", handleConfig );
  server.on ( "/rest/led", handleLed );
  server.on ( "/rest/name", handleName );
  server.onNotFound ( handleNotFound );
  server.begin();
  sensor1.begin();
  
  Serial.print("Init temperature value ");
  do {
    sensor1.requestTemperatures(); 
    temp = sensor1.getTempCByIndex(0);
    Serial.print(".");
  } while (temp == 85.0 || temp == (-127.0));
  Serial.print("");
  
  Serial.println("Module UP");
  lcd.setCursor(0,1);
  lcd.print("Temp:");
  updateTemp();
  updateScreen();
}

void loop() {
  server.handleClient();
   if ((millis()/1000 - _timeREFRESHTEMP) >1){
    _timeREFRESHTEMP  = round(millis()/1000);
    updateTemp();
  }
  if (ms.FREQUENCY && (millis()/1000 - _timeFREQUENCY) > ms.FREQUENCY){
    _timeFREQUENCY  = round(millis()/1000);
    SendTemperature();
  }
  if ((millis()/1000 - _timeREFRESHSCREEN) >1){
    _timeREFRESHSCREEN  = round(millis()/1000);
    updateScreen();
  }
  
} 
