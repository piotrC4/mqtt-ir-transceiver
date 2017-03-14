#include "globals.h"

/* **************************************************************
 * Processing MQTT message
 */
void MQTTcallback(char* topic, byte* payload, unsigned int length)
{
  int i = 0;

  char messageBuf[MQTT_MAX_PACKET_SIZE];
  for (unsigned int i = 0; i < length; i++)
  {
    char tempString[2];
    tempString[0] = (char)payload[i];
    tempString[1] = '\0';
    if (i == 0)
      strcpy(messageBuf, tempString);
    else
      strcat(messageBuf, tempString);
  }

  String msgString = String(messageBuf);
  String topicString = String(topic);

  unsigned long msgInt = StrToUL(msgString);
  sendToDebug("*IR: ======= NEW MESSAGE ======\n");
  sendToDebug(String("*IR: Topic: \"")+topicString+"\"\n");
  sendToDebug(String("*IR: Message: \"")+msgString+"\"\n");
  sendToDebug(String("*IR: Length: ")+String(length,DEC)+"\n");

  String topicSuffix = topicString.substring(strlen(mqtt_prefix));
  sendToDebug(String("*IR: Prefix: ")+mqtt_prefix+"\n");
  sendToDebug(String("*IR: Extracted suffix:\"") + topicSuffix + "\"\n");

  sendToDebug(String("*IR: Payload String: \"") + msgString + "\"\n");
  if (topicSuffix==SUFFIX_RAWMODE_VAL || topicSuffix==SUFFIX_CMD_RESULT)
  {
    sendToDebug("*IR: Ignore own response\n");
    // Ignore own responses
    return;
  }
  else if (topicSuffix==SUFFIX_REBOOT)
  {
    sendToDebug("*IR: reboot\n");
    ESP.restart();
  }
  else if (topicSuffix==SUFFIX_WIPE)
  {
    if (SPIFFS.exists("/config.json"))
    {
      SPIFFS.remove("/config.json");
    }
    sendToDebug("*IR: Wipe config\n");
  }
  else if (topicSuffix==SUFFIX_CMD)
  {
    sendToDebug("*IR: execute command\n");
    String replay;

    if (msgString=="ls")
    {
      replay="";
      Dir dir = SPIFFS.openDir("/");
      while (dir.next())
      {
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
    }
    else if (msgString =="sysinfo")
    {
      uint32_t realSize = ESP.getFlashChipRealSize();
      uint32_t ideSize = ESP.getFlashChipSize();
      FlashMode_t ideMode = ESP.getFlashChipMode();
      replay = "Chip id:"+String(ESP.getChipId(), HEX);
      replay+= ";Flash id:"+String(ESP.getFlashChipId(),HEX);
      replay+= ";Flash real size:"+String(realSize);
      replay+= ";Flash ide size:"+String(ideSize);
      replay+= ";Flash ide mode:"+String ((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
      if(ideSize != realSize)
      {
        replay+="Flash Chip configuration:wrong";
      }
      else
      {
        replay+="Flash Chip configuration:ok";
      }
    }
    else
    {
      replay = "command unknown";
    }
    String topicCmdResult=String(mqtt_prefix)+ SUFFIX_CMD_RESULT;
    mqttClient.publish((char *)topicCmdResult.c_str(), (char *)replay.c_str());
    sendToDebug(String("*IR: Publish on: \"")+ topicCmdResult +"\":"+ replay+"\n");
  }
  else if (topicSuffix==SUFFIX_OTA)
  {
    t_httpUpdate_return ret = ESPhttpUpdate.update(msgString);
    delay(500);

    switch(ret) {
        case HTTP_UPDATE_FAILED:
            sendToDebug(String("*IR: HTTP_UPDATE_FAILD Error (")
              + ESPhttpUpdate.getLastError() + "): "+ ESPhttpUpdate.getLastErrorString().c_str()+"\n");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            sendToDebug("*IR: HTTP_UPDATE_NO_UPDATES\n");
            break;
        case HTTP_UPDATE_OK:
            sendToDebug("*IR: HTTP_UPDATE_OK\n");
            break;
    }
    delay(500);

  }
  else if (topicSuffix==SUFFIX_RAWMODE)
  {
      String topicRawModeVal=String(mqtt_prefix)+ SUFFIX_RAWMODE_VAL;
      sendToDebug(String("*IR: Publish rawmode status on: \"")+topicRawModeVal+"\"\n" );
    if (msgString=="1" || msgString=="ON" || msgString=="true")
    {
      mqttClient.publish(topicRawModeVal.c_str(),"true");
      rawMode=true;
    }
    else
    {
      mqttClient.publish(topicRawModeVal.c_str(),"false");
      rawMode=false;
    }

  }
  else if (topicSuffix==SUFFIX_AUTOSENDMODE)
  {
    String topicAutoSendModeVal=String(mqtt_prefix)+SUFFIX_AUTOSENDMODE_VAL;
    if (msgString=="1" || msgString=="ON" || msgString=="true")
    {
      sendToDebug("*IR: AutoSend enabled\n");
      mqttClient.publish(topicAutoSendModeVal.c_str(),"true");
      EEpromData.autoSendMode = true;
    } else {
      sendToDebug("*IR: AutoSend disabled\n");
      mqttClient.publish(topicAutoSendModeVal.c_str(),"false");
      EEpromData.autoSendMode = false;
    }
    EEPROM.put(0, EEpromData);
    EEPROM.commit();
  }
  else if (topicSuffix==SUFFIX_SENDSTOREDRAW)
  {
    sendToDebug(String("*IR: raw send request from slot:")+msgString+"\n");
    int slotNo=msgString.toInt();
    if (slotNo>0 && slotNo<=SLOTS_NUMBER)
    {
      char fName[20];
      sprintf(fName,"/ir/%d.dat",slotNo);
      int size = readDataFile(fName, rawIrData);
      if (size>0)
      {
        sendToDebug("*IR: transmitting raw data from slot\n");
        irsend.sendRaw(rawIrData, size-1, rawIrData[size-1]);
      }
    }
    else
    {
      sendToDebug("*IR: wrong slot\n");
    }
  }
  else if (topicSuffix==SUFFIX_SENDSTORERAWSEQ)
  {
    sendToDebug(String("*IR: Sending sequence: ")+msgString);
    unsigned int msgLen = msgString.length();
    String allowedChars = String("0123456789,");
    for (int i=0;i< msgLen;i++)
    {
      if (allowedChars.indexOf(msgString[i])==-1)
      {
        return;
      }
    }
    // Coma at begin or end is not allowed
    if (msgString[0]==',' || msgString[msgLen-1]==',')
    {
      return;
    }
    // We have proper slot number and proper message - so we can load it into store slot
    int commIdx=0;
    int commIdxPrev=0;
    int elementIdx=0;
    // Parse message context
    do
    {
      commIdx=msgString.indexOf(',',commIdxPrev);
      if (commIdx>-1)
      {
        // Not last element
        String tmpString=msgString.substring(commIdxPrev,commIdx);
        commIdxPrev=commIdx+1;
        // store in array
        if (elementIdx>SEQ_SIZE)
        {
          return;
        }
        rawSequence[elementIdx]=tmpString.toInt();
      }
      else
      {
        // Last element
        String tmpString=msgString.substring(commIdxPrev);
        if (elementIdx>SEQ_SIZE)
        {
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
      if (slotNo>0 && slotNo<=SLOTS_NUMBER)
      {
        char fName[20];
        sprintf(fName,"/ir/%d.dat",slotNo);
        sendToDebug(String("*IR: Read file %s\n")+fName);
        int size = readDataFile(fName, rawIrData);
        if (size>0)
        {
          sendToDebug("*IR: File content:");
          for (int j=0;j<size;j++)
          {
            if (j>0)
              sendToDebug(",");
            sendToDebug(String(rawIrData[j]));
          }
          sendToDebug("\n*IR: Transmitting raw data sequence\n");
          irsend.sendRaw(rawIrData, size-1, rawIrData[size-1]);
        }
      }
    }
    // --------------------------------------------------------------------
  }
  else
  {
    // structure
    // - _mqtt_prefix_/sender/typ[/bits[/panasonic_address]]
    // - _mqtt_prefix_/sender/storeRaw/slot_ID
    // - _mqtt_prefix_/sender/sendGC
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
    }
    else
    {
      // irTyp exists i cos dalej
      irTypStr  = topicSuffix.substring(8, endOfTyp);
      endOfBits = topicSuffix.indexOf("/",endOfTyp+1);
      if (endOfBits== -1)
      {
        // irBits jest na koncy
        irBitsStr = topicSuffix.substring(endOfTyp+1);
      }
      else
      {
        // irBits i cos dalej
        irBitsStr = topicSuffix.substring(endOfTyp+1, endOfBits);
        irPanasAddrStr = topicSuffix.substring(endOfBits+1);
      }
      irBitsInt = irBitsStr.toInt();
    }

    sendToDebug(String("*IR: TypStr=") + irTypStr+"\n");
    sendToDebug(String("*IR: irBitrStr=")+ irBitsStr+"\n");
    sendToDebug(String("*IR: irPanasAddrStr=")+irPanasAddrStr+"\n");

    if (irTypStr=="storeRaw" || irTypStr == "sendGC" || irTypStr == "sendRAW")
    {
      unsigned int msgLen = msgString.length();
      String allowedChars = String("0123456789,");
      for (int i=0;i< msgLen;i++)
      {
        if (allowedChars.indexOf(msgString[i])==-1)
        {
          return;
        }
      }
      // Coma at begin or end is not allowed
      if (msgString[0]==',' || msgString[msgLen-1]==',')
      {
        return;
      }
      // We have proper slot number and proper message - so we can load it into store slot
      int commIdx=0;
      int commIdxPrev=0;
      int elementIdx=0;
      // Parse message context
      sendToDebug("*IR: Start parsing message\n");
      do
      {
        commIdx=msgString.indexOf(',',commIdxPrev);
        if (commIdx>-1)
        {
          // Not last element
          String tmpString=msgString.substring(commIdxPrev,commIdx);
          commIdxPrev=commIdx+1;
          // store in array
          rawIrData[elementIdx]=tmpString.toInt();
        }
        else
        {
          // Last element
          String tmpString=msgString.substring(commIdxPrev);
          rawIrData[elementIdx]=tmpString.toInt();
        }
        elementIdx++;
      } while (commIdx>-1);
      if (irTypStr=="storeRaw" && irBitsInt>0 && irBitsInt<=SLOTS_NUMBER)
      {
        // Store Raw
        sendToDebug("*IR: Start storeRaw\n");
        char fName[20];
        sprintf(fName,"/ir/%d.dat",irBitsInt);
        sendToDebug("*IR: Parsing finished\n");
        sendToDebug(String("*IR: Write to file: ")+fName+"\n");
        sendToDebug(String("*IR: Elements to write: ")+elementIdx+"\n");
        writeDataFile(fName, rawIrData, elementIdx);
        sendToDebug("*IR: File written\n");

        if (irBitsInt == 1 or irBitsInt ==2)
        {
          sendToDebug("*IR: read files for default player\n");
          loadDefaultIR();
        }
      }
      else if (irTypStr == "sendGC")
      {
        // Send GC
        sendToDebug("*IR: Send GC\n");
        sendToDebug(String("*IR: Elements to send: ")+elementIdx+"\n");
        irsend.sendGC(rawIrData,elementIdx);
        sendToDebug("*IR: GC send done.\n");
      }
      else if (irTypStr == "sendRAW")
      {
        sendToDebug("*IR: Send RAW from MQTT\n");
        sendToDebug(String("*IR: No of elements to send: ")+(elementIdx-1)+", frequency="+rawIrData[elementIdx-1]+"kHz\n");
        irsend.sendRaw(rawIrData,elementIdx-1,rawIrData[elementIdx-1]);
        sendToDebug("*IR: RAW send done.\n");
      }
    }
    else if (irTypStr=="NEC")
    {
      sendToDebug(String("*IR: Send NEC:")+msgInt);
      irsend.sendNEC(msgInt, irBitsInt);
    }
    else if (irTypStr=="RC5")
    {
      sendToDebug(String("*IR: Send RC5:")+msgInt+" (bits: "+irBitsInt+")\n");
      irsend.sendRC5(msgInt, irBitsInt);
    }
    else if (irTypStr=="RC6")
    {
      sendToDebug(String("*IR: Send RC6:")+msgInt+" (bits: "+irBitsInt+")\n");
      irsend.sendRC6(msgInt, irBitsInt);
    }
    else if (irTypStr=="LG")
    {
      sendToDebug(String("*IR: Send LG:")+msgInt+" (bits: "+irBitsInt+")\n");
      irsend.sendLG(msgInt, irBitsInt);
    }
    else if (irTypStr=="SONY")
    {
      sendToDebug(String("*IR: Send Sony:")+msgInt+" (bits: "+irBitsInt+")\n");
      irsend.sendSony(msgInt, irBitsInt);
    }
    else if (irTypStr=="SAMSUNG")
    {
      sendToDebug(String("*IR: Send Samsung:")+msgInt+" (bits: "+irBitsInt+")\n");
      irsend.sendSAMSUNG(msgInt, irBitsInt);
    }
  }

}


/************************************************
 *  connect to MQTT broker
 */
void connect_to_MQTT()
{

  char myTopic[100];
  if (mqtt_secure_b)
  {
    sendToDebug("*IR: connecting to TLS server:");
    mqttClient.setClient(wifiClientSecure);
  } else {
    sendToDebug("*IR: connecting to nonTLS server:");
    mqttClient.setClient(wifiClient);
  }
  sendToDebug(String(" ")+ mqtt_server+ ":"+ mqtt_port_i +" as " + clientName +"\n");
  mqttClient.setServer(mqtt_server, mqtt_port_i);
  mqttClient.setCallback(MQTTcallback);
  int conn_counter = 2;

  while (conn_counter > 0)
  {
    sendToDebug(String("*IR: MQTT user:")+ mqtt_user + "\n");
    sendToDebug("*IR: MQTT pass: ********\n");
    String topicWill = String(mqtt_prefix)+ SUFFIX_WILL;
    if (mqttClient.connect((char*) clientName.c_str(), (char*)mqtt_user, (char *)mqtt_pass, topicWill.c_str(), 2, true, "false"))
    {
      mqttClient.publish(topicWill.c_str(), "true", true);
      sendToDebug("*IR: Connected to MQTT broker\n");
      sprintf(myTopic, "%s/info/client", mqtt_prefix);
      mqttClient.publish((char*)myTopic, (char*) clientName.c_str());
      IPAddress myIp = WiFi.localIP();
      char myIpString[24];
      sprintf(myIpString, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
      sprintf(myTopic, "%s/info/ip", mqtt_prefix);
      mqttClient.publish((char*)myTopic, (char*) myIpString);
      sprintf(myTopic, "%s/info/type", mqtt_prefix);
      mqttClient.publish((char*)myTopic,"IR server");
      sprintf(myTopic, "%s/info/version", mqtt_prefix);
      mqttClient.publish((char*)myTopic,VERSION);
      String topicSubscribe = String (mqtt_prefix)+ SUFFIX_SUBSCRIBE;
      sendToDebug(String("*IR: Topic is: ") + topicSubscribe.c_str()+"\n");
      if (mqttClient.subscribe(topicSubscribe.c_str()))
      {
        sendToDebug("*IR: Successfully subscribed\n");
        delay(1);
      }

      conn_counter = -1;
    }
    else
    {
      sendToDebug(String("*IR: MQTT connect failed, rc=") + mqttClient.state() + " try again in 5 seconds\n");
      // Wait 5 seconds before retrying
      for (int i = 0; i<10; i++)
      {
        #ifdef LED_PIN
        digitalWrite(LED_PIN, 1-digitalRead(LED_PIN));
        #endif
        delay(500);
      }
      #ifdef LED_PIN
      digitalWrite(LED_PIN, HIGH);
      #endif
      conn_counter = conn_counter-1;
    }
  }
  if (conn_counter==0)
  {
    MQTTMode=false;
    sendToDebug("*IR: Entering non MQTT mode\n");
  } else {
    MQTTMode=true;
    sendToDebug("*IR: Entering MQTT mode\n");
  }
}
