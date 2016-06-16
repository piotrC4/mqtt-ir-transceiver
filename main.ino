/*
 * IRremoteESP8266: IRServer - MQTT IR server
 * used library:
 * https://github.com/markszabo/IRremoteESP8266
 * based on:
 * https://github.com/z3t0/Arduino-IRremote/
 * Version 0.2 May, 2016
 */

/*
 * broker -> module:
 *    "_mqtt_prefix_/sender/storeRaw/_store_id_"    msg: "\d+(,\d+)*"
 *                - store raw code in given slot _store_id_
 *    "_mqtt_prefix_/sender/sendStoredRaw"   msg: "\d+"
 *                - transmit raw code from given slot
 *    "_mqtt_prefix_/sender/sendStoredRawSequence"   msg: "\d(,\d+)*"
 *                - transmit raw codes seqnece of given slots
 *    "_mqtt_prefix_/sender/cmd"             msg: "(ls|sysinfo)"
 *                - send system info query response in "esp8266/02/sender/cmd/result"
 *    "_mqtt_prefix_/sender/NC/HDMI"         msg: ".+"
 *                - send NC+ HDMI sequence
 *    "_mqtt_prefix_/sender/NC/EURO"         msg: ".+"
 *                - send NC+ EURO sequence
 *    "_mqtt_prefix_/sender/rawMode"         msg: "(1|ON|true|.*)"
 *                - enable/disable raw mode
 *    "_mqtt_prefix_/sender/type(/\d+(/\d+)) msg: "\d+"
 *                - esp8266/02/sender/type[/bits[/panasonic_address]] - type: NEC, RC_5, RC_6, SAMSUNG, SONY
 *    "_mqtt_prefix_/wipe" msg: ".*"
 *                - wpie config file
 *
 * module -> broker
 *    "_mqtt_prefix_/receiver/_type_/_bits_"               msg: "\d+"
 *                - recived message with given type and n-bits
 *    "_mqtt_prefix_/receiver/_type_/_bits_/_panas_addr_"  msg: "\d+"
 *                - recived message with given type and n-bits  and panasonic addres
 *
 */

#define MQTT_MAX_PACKET_SIZE 1024
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "FS.h"
#include <IRremoteESP8266.h>      // https://github.com/markszabo/IRremoteESP8266 (use local copy)
#include <PubSubClient.h>         // https://github.com/knolleary/pubsubclient (id: 89)
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic (id: 567)
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson (id: 64)


// Slots for RAW data recording
#define SLOTS_NUMBER 20 // Number of slots
#define SLOT_SIZE 300   // Size of single slot
#define SEQ_SIZE 10     // Raw sequnece size
#define DEBUG X


// RAW data storage
unsigned int rawIrData[SLOT_SIZE+1];
unsigned int rawSequence[SEQ_SIZE];

#ifdef DEBUG
 // dev device
#define RECV_PIN 0     // D3 - GPIO0
#define TRANS_PIN 16   // D0 - GPIO16
#define TRIGGER_PIN 12 // D6 - GPIO12
#else
 // production device - ESP01
#define RECV_PIN 0    // D3 - GPIO0 - IR detector/demodulator
#define TRANS_PIN 3   // RX - GPIO3 - IR LED trasmitter
#define TRIGGER_PIN 2 // D4 - GPIO2 - trigger reset (press and hold after boot - 5 seconds)
#endif

#define       SUFFIX_SUBSCRIBE "/sender/#"
#define            SUFFIX_WILL "/status"
#define            SUFFIX_WIPE "/sender/wipe"
#define          SUFFIX_REBOOT "/sender/reboot"
#define         SUFFIX_NC_HDMI "/sender/NC/HDMI"
#define         SUFFIX_NC_EURO "/sender/NC/EURO"
#define             SUFFIX_CMD "/sender/cmd"
#define      SUFFIX_CMD_RESULT "/sender/cmd/result"
#define         SUFFIX_RAWMODE "/sender/rawMode"
#define     SUFFIX_RAWMODE_VAL "/sender/rawMode/val"
#define   SUFFIX_SENDSTOREDRAW "/sender/sendStoredRaw"
#define SUFFIX_SENDSTORERAWSEQ "/sender/sendStoredRawSequence"

char mqtt_server[40];
char mqtt_user[32];
char mqtt_pass[32];
char mqtt_prefix[80];

//flag for saving data
bool shouldSaveConfig = false;

String clientName; // MQTT client name
bool rawMode = false; // Raw mode receiver status

IRrecv irrecv(RECV_PIN);
IRsend irsend(TRANS_PIN);

WiFiClient wifiClient;

void callback(char* topic, byte* payload, unsigned int length);
void connect_to_MQTT();

//PubSubClient client(mqtt_server, 1883, callback, wifiClient);
PubSubClient client(wifiClient);

/* **************************************************************
 * Convert String to unsigned long
 *
 */
unsigned long StrToUL(String inputString)
{
  unsigned long result = 0;
  for (int i = 0; i < inputString.length(); i++)
  {
    char c = inputString.charAt(i);
    if (c < '0' || c > '9') break;
    result *=10;
    result += (c - '0');
  }
  return result;
}

/* **************************************************************
 * Write IR codes array to slot file
 * - fName - destination file
 * - sourceArray[] - source for data into slot
 * - sourceSize - number of elements in array
 */
bool writeDataFile(const char* fName, unsigned int sourceArray[], int sourceSize) {
  if (SPIFFS.exists(fName)) {
#ifdef DEBUG
    Serial.println("*IR: File exists - removed");
#endif
    SPIFFS.remove(fName);
  }
  File file = SPIFFS.open(fName, "w");
  if (file) {
#ifdef DEBUG
    Serial.print("*IR: Start writing to file: ");
#endif
    for (int i=0;i<sourceSize;i++) {
      if (i>0) {
        file.print("\n");
#ifdef DEBUG
        Serial.print(",");
#endif
      }
#ifdef DEBUG
      Serial.print(sourceArray[i]);
#endif
      file.print(sourceArray[i]);
    }
#ifdef DEBUG
    Serial.println("\n*IR: Writing ok");
#endif
    file.close();
    return true;
  } else {
    return false;
  }
}
/* **************************************************************
 * Read IR codes array from slot file
 * - fName - source file
 * - destinationArray[] - destination for data from slot
 * @returns
 * - -1 - config file not exists
 * - -2 - problem with file opening
 * -  n - number of elements in store
 */
int readDataFile(char * fName, unsigned int destinationArray[]) {
  if (!SPIFFS.exists(fName)) {
    return -1;
  }
  File IRconfigFile=SPIFFS.open(fName,"r");
  if (!IRconfigFile) {
    return -2;
  }

  size_t size = IRconfigFile.size();
#ifdef DEBUG
  if (size>2500) {
    Serial.printf("*IR: Config file %d size (%d)is too large",fName,size);
  }
#endif
  int i =0;
  while(IRconfigFile.available() ) {
    String line = IRconfigFile.readStringUntil('\n');
    destinationArray[i]=(unsigned int)line.toInt();
    i++;
  }
  return i;
}

/* **************************************************************
 * Processing MQTT message
 */
void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;

  char messageBuf[MQTT_MAX_PACKET_SIZE];
  for (unsigned int i = 0; i < length; i++) {
    char tempString[2];
    tempString[0] = (char)payload[i];
    tempString[1] = '\0';
    if (i == 0)
      strcpy(messageBuf, tempString);
    else
      strcat(messageBuf, tempString);
  }

  unsigned int freq=38;
  String msgString = String(messageBuf);
  String topicString = String(topic);

  //unsigned long msgInt = msgString.toInt();
  unsigned long msgInt = StrToUL(msgString);
  #ifdef DEBUG
    Serial.println("*IR: ======= NEW MESSAGE ======");
    Serial.println("*IR: Topic: \"" + topicString+"\"");
    Serial.println("*IR: Message: \""+msgString+"\"");
    Serial.println("*IR: Length: " + String(length,DEC));
  #endif


  String topicSuffix = topicString.substring(strlen(mqtt_prefix));
#ifdef DEBUG
  Serial.printf("*IR: Prefix: %s\n",mqtt_prefix);
  Serial.println("*IR: Extracted suffix:\"" + topicSuffix + "\"");
#endif

  unsigned int  rawData_s1[35] = {250,950, 200,2000, 200,1200, 200,2850, 200,1350, 200,1350, 200,1050, 200,2150, 200,12950, 200,950, 200,1200, 200,800, 200,1200, 200,1600, 200,1200, 200,800, 200,800, 200};  // UNKNOWN C0092718
  unsigned int  rawData_s2[37] = {100,5850, 200,950, 200,2000, 200,1200, 200,2850, 200,1350, 200,1350, 200,1050, 200,2150, 200,12950, 200,950, 200,2300, 200,1900, 200,1200, 200,1650, 200,1200, 200,800, 200,800, 200};  // UNKNOWN 4AE2F613
  unsigned int  rawData_11[35] = {250,950, 200,2000, 200,1250, 200,2850, 200,1350, 200,1350, 200,1050, 200,2150, 200,12950, 200,950, 200,800, 200,800, 200,1200, 200,800, 200,2450, 200,800, 200,800, 200};  // UNKNOWN 290BC97A
  unsigned int  rawData_12[35] = {250,950, 200,2000, 200,1200, 200,2850, 200,1350, 200,1350, 200,1050, 200,2150, 200,12950, 200,950, 200,1900, 200,1900, 200,1200, 200,800, 200,2450, 200,800, 200,800, 200};  // UNKNOWN 8FEB0411
  unsigned int  rawData_91[35] = {250,950, 200,2000, 200,1200, 200,2850, 200,1350, 200,1350, 200,1100, 200,2150, 200,12950, 200,950, 200,1750, 200,800, 200,1200, 200,1200, 200,1100, 200,800, 200,800, 200};  // UNKNOWN 19B50A9
  unsigned int  rawData_92[35] = {250,950, 200,2000, 200,1250, 200,2850, 200,1350, 200,1350, 200,1100, 200,2150, 200,12950, 200,950, 200,2850, 200,1900, 200,1200, 200,1200, 200,1100, 200,800, 200,800, 200};  // UNKNOWN 442CD28F

#ifdef DEBUG
  Serial.println("*IR: Payload String: \"" + msgString + "\"");
#endif
  if (topicSuffix==SUFFIX_RAWMODE_VAL || topicSuffix==SUFFIX_CMD_RESULT) {
#ifdef DEBUG
    Serial.println("*IR: Ignore own response");
#endif
    // Ignore own responses
    return;
  } else if (topicSuffix==SUFFIX_NC_HDMI) { // *9
    irsend.sendRaw(rawData_s1, 35, freq);
    irsend.sendRaw(rawData_s2, 37, freq);
    irsend.sendRaw(rawData_91, 35, freq);
    irsend.sendRaw(rawData_92, 35, freq);
#ifdef DEBUG
    Serial.println("*IR: Send NC+ HDMI: *9");
#endif
  } else if (topicSuffix==SUFFIX_NC_EURO) { // *1
    irsend.sendRaw(rawData_s1, 35, freq);
    irsend.sendRaw(rawData_s2, 37, freq);
    irsend.sendRaw(rawData_11, 35, freq);
    irsend.sendRaw(rawData_12, 35, freq);
#ifdef DEBUG
    Serial.println("*IR: Send NC+ EURO: *1");
#endif
  }else if (topicSuffix==SUFFIX_REBOOT) {
#ifdef DEBUG
    Serial.println("*IR: reboot");
#endif
    ESP.restart();
  }else if (topicSuffix==SUFFIX_WIPE) {
    if (SPIFFS.exists("/config.json")) {
      SPIFFS.remove("/config.json");
    }
#ifdef DEBUG
    Serial.println("*IR: Wipe config");
#endif
  } else if (topicSuffix==SUFFIX_CMD) {
#ifdef DEBUG
    Serial.println("*IR: execute command");
#endif
    String replay;

    if (msgString=="ls") {
      replay="";
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        replay+=dir.fileName();
        replay+="=";
        File f = dir.openFile("r");
        replay+= f.size();
        replay+=";";
      }
      FSInfo fs_info;
      SPIFFS.info(fs_info);
      replay+="Total bytes=";
      replay+=fs_info.totalBytes;
      replay+=";";
      replay+="Used bytes=";
      replay+=fs_info.usedBytes;
    } else if (msgString =="sysinfo") {
      uint32_t realSize = ESP.getFlashChipRealSize();
      uint32_t ideSize = ESP.getFlashChipSize();
      FlashMode_t ideMode = ESP.getFlashChipMode();
      replay = "Chip id:"+String(ESP.getChipId(), HEX);
      replay+= ";Flash id:"+String(ESP.getFlashChipId(),HEX);
      replay+= ";Flash real size:"+String(realSize);
      replay+= ";Flash ide size:"+String(ideSize);
      replay+= ";Flash ide mode:"+String ((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
      if(ideSize != realSize) {
        replay+="Flash Chip configuration:wrong";
      } else {
        replay+="Flash Chip configuration:ok";
      }
    } else {
      replay = "command unknown";
    }
    String topicCmdResult=String(mqtt_prefix)+ SUFFIX_CMD_RESULT;
    client.publish((char *)topicCmdResult.c_str(), (char *)replay.c_str());
#ifdef DEBUG
    Serial.println("*IR: Publish on: \""+ topicCmdResult +"\":"+ replay);
#endif

  } else if (topicSuffix==SUFFIX_RAWMODE) {
      String topicRawModeVal=String(mqtt_prefix)+ SUFFIX_RAWMODE_VAL;
#ifdef DEBUG
      Serial.println("*IR: Publish rawmode status on: \""+topicRawModeVal+"\"" );
#endif
    if (msgString=="1" || msgString=="ON" || msgString=="true") {
      client.publish(topicRawModeVal.c_str(),"true");
        rawMode=true;
    } else {
      client.publish(topicRawModeVal.c_str(),"false");
      rawMode=false;
    }

  } else if (topicSuffix==SUFFIX_SENDSTOREDRAW) {
#ifdef DEBUG
    Serial.println("*IR: raw send request from slot:"+msgString);
#endif
    int slotNo=msgString.toInt();
    if (slotNo>0 && slotNo<=SLOTS_NUMBER) {
      char fName[20];
      sprintf(fName,"/ir/%d.dat",slotNo);
      int size = readDataFile(fName, rawIrData);
      if (size>0) {
        Serial.println("*IR: transmitting");
        unsigned int freq=38;
        irsend.sendRaw(rawIrData, size, freq);
      }
#ifdef DEBUG
    } else {
      Serial.println("*IR: wrong slot");
#endif
    }
  } else if (topicSuffix==SUFFIX_SENDSTORERAWSEQ) {
#ifdef DEBUG
    Serial.println("*IR: Sending sequence: "+msgString);
#endif
    unsigned int msgLen = msgString.length();
    String allowedChars = String("0123456789,");
    for (int i=0;i< msgLen;i++) {
      if (allowedChars.indexOf(msgString[i])==-1) {
        return;
      }
    }
    // Coma at begin or end is not allowed
    if (msgString[0]==',' || msgString[msgLen-1]==',') {
      return;
    }
    // We have proper slot number and proper message - so we can load it into store slot
    int commIdx=0;
    int commIdxPrev=0;
    int elementIdx=0;
    // Parse message context
    do {
      commIdx=msgString.indexOf(',',commIdxPrev);
      if (commIdx>-1) {
        // Not last element
        String tmpString=msgString.substring(commIdxPrev,commIdx);
        commIdxPrev=commIdx+1;
        // store in array
        if (elementIdx>SEQ_SIZE) {
          return;
        }
        rawSequence[elementIdx]=tmpString.toInt();
      } else {
        // Last element
        String tmpString=msgString.substring(commIdxPrev);
        if (elementIdx>SEQ_SIZE) {
          return;
        }
        rawSequence[elementIdx]=tmpString.toInt();
      }
      elementIdx++;
    } while (commIdx>-1);
    // Sending of ir codes sequnece
    for (int i=0;i<elementIdx;i++)
    {
      int slotNo=rawSequence[i];
      if (slotNo>0 && slotNo<=SLOTS_NUMBER) {
        char fName[20];
        sprintf(fName,"/ir/%d.dat",slotNo);
#ifdef DEBUG
        Serial.printf("*IR: Read file %s\n",fName);
#endif
        int size = readDataFile(fName, rawIrData);
        if (size>0) {
#ifdef DEBUG
          Serial.print("*IR: File content:");
          for (int j=0;j<size;j++)
          {
            if (j>0) Serial.print(",");
            Serial.printf("%u",rawIrData[j]);
          }
          Serial.println(" ");
          Serial.println("*IR: Transmitting");
#endif
          unsigned int freq=38;
          irsend.sendRaw(rawIrData, size, freq);
        }
      }
    }
    // --------------------------------------------------------------------
  } else {
    // structure
    // - _mqtt_prefix_/sender/typ[/bits[/panasonic_address]]
    // - _mqtt_prefix_/sender/storeRaw/slot_ID
    int endOfBits;
    String irTypStr = "";
    String irBitsStr = "";
    int irBitsInt=-1;
    String irPanasAddrStr = "";

    int endOfTyp = topicSuffix.indexOf("/",8);
    if (endOfTyp == -1)
    {
      // One element - only irTyp
      irTypStr  = topicSuffix.substring(8);
    } else {
      // irTyp exists i cos dalej
      irTypStr  = topicSuffix.substring(8, endOfTyp);
      endOfBits = topicSuffix.indexOf("/",endOfTyp+1);
      if (endOfBits== -1)
      {
        // irBits jest na koncy
        irBitsStr = topicSuffix.substring(endOfTyp+1);
      } else {
        // irBits i cos dalej
        irBitsStr = topicSuffix.substring(endOfTyp+1, endOfBits);
        irPanasAddrStr = topicSuffix.substring(endOfBits+1);
      }
      irBitsInt = irBitsStr.toInt();
    }

#ifdef DEBUG
    Serial.println("*IR: TypStr=" + irTypStr);
    Serial.println("*IR: irBitrStr="+ irBitsStr);
    Serial.println("*IR: irPanasAddrStr="+irPanasAddrStr);
#endif
    if (irTypStr=="storeRaw") {
      // Store Raw
      #ifdef DEBUG
      Serial.println("*IR: Start storeRaw");
#endif
      if (irBitsInt>0 && irBitsInt<=SLOTS_NUMBER) {
        unsigned int msgLen = msgString.length();
        String allowedChars = String("0123456789,");
        for (int i=0;i< msgLen;i++) {
          if (allowedChars.indexOf(msgString[i])==-1) {
            return;
          }
        }
        // Coma at begin or end is not allowed
        if (msgString[0]==',' || msgString[msgLen-1]==',') {
          return;
        }
        // We have proper slot number and proper message - so we can load it into store slot
        int commIdx=0;
        int commIdxPrev=0;
        int elementIdx=0;
        // Parse message context
#ifdef DEBUG
        Serial.println("*IR: Start parsing message");
#endif
        do {
          commIdx=msgString.indexOf(',',commIdxPrev);
          if (commIdx>-1) {
            // Not last element
            String tmpString=msgString.substring(commIdxPrev,commIdx);
            commIdxPrev=commIdx+1;
            // store in array
            rawIrData[elementIdx]=tmpString.toInt();
          } else {
            // Last element
            String tmpString=msgString.substring(commIdxPrev);
            rawIrData[elementIdx]=tmpString.toInt();
          }
          elementIdx++;
        } while (commIdx>-1);
        char fName[20];
        sprintf(fName,"/ir/%d.dat",irBitsInt);
#ifdef DEBUG
        Serial.println("*IR: Parsing finished");
        Serial.printf("*IR: Write to file: %s\n",fName);
        Serial.printf("*IR: Elements to write: %d\n",elementIdx);
#endif
        writeDataFile(fName, rawIrData, elementIdx);
      }
    }else if (irTypStr=="NEC") {
#ifdef DEBUG
      Serial.print("*IR: Send NEC:");
      Serial.println(msgInt);
#endif
      irsend.sendNEC(msgInt, irBitsInt);
    } else if (irTypStr=="RC5") {
#ifdef DEBUG
      Serial.print("*IR: Send RC5:");
      Serial.print(msgInt);
      Serial.print(" (");
      Serial.print(irBitsInt);
      Serial.println("-bits)");
#endif
      irsend.sendRC5(msgInt, irBitsInt);
    } else if (irTypStr=="RC6") {
#ifdef DEBUG
      Serial.print("*IR: Send RC6:");
      Serial.print(msgInt);
      Serial.print(" (");
      Serial.print(irBitsInt);
      Serial.println("-bits)");
#endif
      irsend.sendRC6(msgInt, irBitsInt);
    } else if (irTypStr=="LG") {
#ifdef DEBUG
      Serial.print("*IR: Send LG:");
      Serial.print(msgInt);
      Serial.print(" (");
      Serial.print(irBitsInt);
      Serial.println("-bits)");
#endif
      irsend.sendLG(msgInt, irBitsInt);
    } else if (irTypStr=="SONY") {
#ifdef DEBUG
      Serial.print("*IR: Send SONY:");
      Serial.print(msgInt);
      Serial.print(" (");
      Serial.print(irBitsInt);
      Serial.println("-bits)");
#endif
      irsend.sendSony(msgInt, irBitsInt);
    } else if (irTypStr=="SAMSUNG") {
#ifdef DEBUG
      Serial.print("*IR: Send SAMSUNG:");
      Serial.print(msgInt);
      Serial.print(" (");
      Serial.print(irBitsInt);
      Serial.println("-bits)");
#endif
      irsend.sendSAMSUNG(msgInt, irBitsInt);
    }
  }

}


/**********************************************
 * Convert MAC to String
 */
String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

//callback notifying us of the need to save config
void saveConfigCallback () {
#ifdef DEBUG
  Serial.println("*IR: Should save config");
#endif
  shouldSaveConfig = true;
}


/***************************************************
 * Setup
 */
void setup(void){


  // delay for reset button
  delay(5000);
#ifdef DEBUG
  Serial.begin(115200);
//  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
#endif
pinMode(TRIGGER_PIN, INPUT);
if (SPIFFS.begin()) {
#ifdef DEBUG
    Serial.println("*IR: mounted file system");
#endif
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
#ifdef DEBUG
      Serial.println("*IR: reading config file");
#endif
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
#ifdef DEBUG
        Serial.print("*IR: opened config file");
#endif
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
#ifdef DEBUG
        json.printTo(Serial);
        Serial.println(" ");
#endif
        if (json.success()) {
#ifdef DEBUG
          Serial.println("*IR: parsed json");
#endif
          if (json.containsKey("mqtt_server"))
            strcpy(mqtt_server, json["mqtt_server"]);
          if (json.containsKey("mqtt_user"))
            strcpy(mqtt_user, json["mqtt_user"]);
          if (json.containsKey("mqtt_pass"))
            strcpy(mqtt_pass, json["mqtt_pass"]);
          if (json.containsKey("mqtt_prefix"))
            strcpy(mqtt_prefix, json["mqtt_prefix"]);
#ifdef DEBUG
        } else {
          Serial.println("*IR: failed to load json config");
#endif
        }
      }
    }
#ifdef DEBUG
  } else {
    Serial.println("*IR: failed to mount FS");
#endif
  }
#ifdef DEBUG
  Serial.println("*IR: Start setup");
#endif
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt password", mqtt_pass, 32);
  WiFiManagerParameter custom_mqtt_prefix("prefix", "mqtt prefix", mqtt_prefix, 80);
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setTimeout(180);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_prefix);

  if ( digitalRead(TRIGGER_PIN) == LOW || (!SPIFFS.exists("/config.json")) ) {
    wifiManager.resetSettings();
  }
  #ifndef DEBUG
    wifiManager.setDebugOutput(false);
  #endif
  char mySSID[17];
  char myPASS[7];
  sprintf(mySSID,"IRTRANS-%06X", ESP.getChipId());
  sprintf(myPASS,"%06X", ESP.getChipId());
  if (!wifiManager.autoConnect(mySSID, myPASS) ) {
#ifdef DEBUG
    Serial.println("*IR: failed to connect and hit timeout");
#endif
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(mqtt_prefix, custom_mqtt_prefix.getValue());

  irsend.begin();
  irrecv.enableIRIn();  // Start the receiver

  //save the custom parameters to FS
  if (shouldSaveConfig) {
#ifdef DEBUG
    Serial.println("*IR: saving config");
#endif
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["mqtt_prefix"] = mqtt_prefix;

    File configFile = SPIFFS.open("/config.json", "w");
    if (configFile) {
#ifdef DEBUG
      Serial.print("*IR: ");
      json.printTo(Serial);
#endif
      json.printTo(configFile);
      configFile.close();
#ifdef DEBUG
    } else {
      Serial.println("*IR: failed to open config file for writing");
#endif
    }
  }

#ifdef DEBUG
  Serial.print("*IR: Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("*IR: IP address: ");
  Serial.println(WiFi.localIP());
#endif

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  connect_to_MQTT();

}

/************************************************
 *  connect to MQTT broker
 */
void connect_to_MQTT() {
#ifdef DEBUG
  Serial.print("*IR: Connecting to ");
  Serial.print(mqtt_server);
  Serial.print(" as ");
  Serial.println(clientName);
#endif
  char myTopic[100];

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  int is_conn = 0;
  while (is_conn == 0) {
#ifdef DEBUG
    Serial.print("*IR: MQTT user:");
    Serial.println(mqtt_user);
    Serial.println("*IR: MQTT pass: ********");
#endif
    String topicWill = String(mqtt_prefix)+ SUFFIX_WILL;
    if (client.connect((char*) clientName.c_str(), (char*)mqtt_user, (char *)mqtt_pass, topicWill.c_str(), 2, true, "false")) {
      client.publish(topicWill.c_str(), "true", true);
#ifdef DEBUG
      Serial.println("*IR: Connected to MQTT broker");
#endif
      sprintf(myTopic, "%s/info/client", mqtt_prefix);
      client.publish((char*)myTopic, (char*) clientName.c_str());
      IPAddress myIp = WiFi.localIP();
      char myIpString[24];
      sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
      sprintf(myTopic, "%s/info/ip", mqtt_prefix);
      client.publish((char*)myTopic, (char*) myIpString);
      sprintf(myTopic, "%s/info/type", mqtt_prefix);
      client.publish((char*)myTopic,"IR server");
      String topicSubscribe = String (mqtt_prefix)+ SUFFIX_SUBSCRIBE;
#ifdef DEBUG
      Serial.print("*IR: Topic is: ");
      Serial.println(topicSubscribe.c_str());
#endif
      if (client.subscribe(topicSubscribe.c_str())){
#ifdef DEBUG
        Serial.println("*IR: Successfully subscribed");
#endif
        delay(1);
      }

      is_conn = 1;
    }
    else {
#ifdef DEBUG
      Serial.print("*IR: MQTT connect failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/***************************************************************
 *  Interpreter of IR encoding ID
 */
void  encoding (decode_results *results, char * result_encoding)
{
  switch (results->decode_type) {
    default:
    case UNKNOWN:      strncpy(result_encoding,"UNKNOWN\0",8);       break ;
    case NEC:          strncpy(result_encoding,"NEC\0",4);           break ;
    case SONY:         strncpy(result_encoding,"SONY\0",5);          break ;
    case RC5:          strncpy(result_encoding,"RC5\0",4);           break ;
    case RC6:          strncpy(result_encoding,"RC6\0",4);           break ;
    case DISH:         strncpy(result_encoding,"DISH\0",5);          break ;
    case SHARP:        strncpy(result_encoding,"SHARP\0",6);         break ;
    case JVC:          strncpy(result_encoding,"JVC\0",4);           break ;
    case SANYO:        strncpy(result_encoding,"SANYO\0",6);         break ;
    case MITSUBISHI:   strncpy(result_encoding,"MITSUBISHI\0",11);   break ;
    case SAMSUNG:      strncpy(result_encoding,"SAMSUNG\0",8);       break ;
    case LG:           strncpy(result_encoding,"LG\0",3);            break ;
    case WHYNTER:      strncpy(result_encoding,"WHYNTER\0",8);       break ;
    case AIWA_RC_T501: strncpy(result_encoding,"AIWA_RC_T501\0",13); break ;
    case PANASONIC:    strncpy(result_encoding,"PANASONIC\0",11);    break ;
  }
}

/****************************************************************
 * Main loop
 */
void loop(void){
  client.loop();

  if (! client.connected()) {
#ifdef DEBUG
    Serial.println("*IR: Not connected to MQTT....");
    delay(5000);
#endif
    connect_to_MQTT();
  }
  decode_results  results;        // Somewhere to store the results
  if (irrecv.decode(&results)) {  // Grab an IR code
    char myTopic[100];
    char myTmp[50];
    char myValue[500];
    encoding (&results, myTmp);
    if (results.decode_type == PANASONIC) { //Panasonic has address
      // structure "prefix/typ/bits[/panasonic_address]"
      sprintf(myTopic, "%s/receiver/%s/%d/%d", mqtt_prefix, myTmp, results.bits, results.panasonicAddress );
    } else {
      sprintf(myTopic, "%s/receiver/%s/%d", mqtt_prefix, myTmp, results.bits );
    }
    if (results.decode_type != UNKNOWN) {
      // any other has code and bits
      sprintf(myValue, "%d", results.value);
      client.publish((char*) myTopic, (char*) myValue );
    } else if (rawMode==true) {
      // RAW MODE
      String myString;
      for (int i = 1;  i < results.rawlen;  i++) {
        myString+= (results.rawbuf[i] * USECPERTICK);
        if ( i < results.rawlen-1 ) myString+=","; // ',' not needed on last one
      }
      myString.toCharArray(myValue,500);
      sprintf(myTopic, "%s/receiver/raw", mqtt_prefix );
      client.publish( (char*) myTopic, (char*) myValue );
    }
    irrecv.resume();              // Prepare for the next value
  }
}
