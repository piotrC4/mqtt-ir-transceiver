#include "globals.h"

unsigned int rawIrData[SLOT_SIZE+1];
unsigned int rawSequence[SEQ_SIZE];

unsigned int rawIR1[SLOT_SIZE+1];
unsigned int rawIR2[SLOT_SIZE+1];

int rawIR1size, rawIR2size;

char mqtt_server[40];
char mqtt_port[5];
char mqtt_user[32];
char mqtt_pass[32];
char mqtt_prefix[80];
char mqtt_secure[1];
bool mqtt_secure_b;
int mqtt_port_i;

bool buttonState = 1 - BUTTON_ACTIVE_LEVEL; // State of control button
bool MQTTMode = true;
bool autoSendMode = false;
bool shouldSaveConfig = false; //flag for saving data
String clientName; // MQTT client name
bool rawMode = false; // Raw mode receiver status

unsigned long lastTSAutoStart;
unsigned long lastTSMQTTReconect;
unsigned long autoStartFreq = 300000; // Frequency of autostart
bool autoStartSecond = false;

#ifdef DEBUG
const bool useDebug = true;
#else
const bool useDebug = false;
#endif

// ------------------------------------------------
// Global objects

IRrecv irrecv(RECV_PIN);
IRsend irsend(TRANS_PIN);

WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient;
 EEpromDataStruct EEpromData;
