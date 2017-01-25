
#include "globals.h"

/* **************************************************************
 * Convert String to unsigned long
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
bool writeDataFile(const char* fName, unsigned int sourceArray[], int sourceSize)
{
  if (SPIFFS.exists(fName))
  {
    sendToDebug("*IR: File exists - removeing\n");
    SPIFFS.remove(fName);
    sendToDebug("*IR: File remove\n");
  }
  File file = SPIFFS.open(fName, "w");
  if (file)
  {
    sendToDebug("*IR: Start writing to file: ");
    for (int i=0;i<sourceSize;i++)
    {
      if (i>0)
      {
        file.print("\n");
        sendToDebug(",");
      }
      sendToDebug(String(sourceArray[i]));
      file.print(sourceArray[i]);
    }
    sendToDebug("\n*IR: Writing ok");
    file.close();
    sendToDebug("*IR: Writing ok\n");
    return true;
  }
  else
  {
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
int readDataFile(char * fName, unsigned int destinationArray[])
{
  if (!SPIFFS.exists(fName))
  {
    sendToDebug(String("*IR: File not found: ")+fName+"\n");

    return -1;
  }
  File IRconfigFile=SPIFFS.open(fName,"r");
  if (!IRconfigFile)
  {
    sendToDebug(String("*IR: Unable to read file: ")+fName+"\n");
    return -2;
  }

  size_t size = IRconfigFile.size();
  if (size>2500)
  {
    sendToDebug(String("*IR: Config file ")+fName+" size ("+size+") is too large.\n");
  }
  int i =0;
  sendToDebug(String("*IR: Start reading file: ")+fName+" size: "+size+"\n");
  while(IRconfigFile.available() )
  {
    String line = IRconfigFile.readStringUntil('\n');
    destinationArray[i]=(unsigned int)line.toInt();
    i++;
  }
  return i;
}


/**********************************************
 * Convert MAC to String
 */
String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

/**********************************************
 * callback for wifimanager to notifying us of the need to save config
 */
void saveConfigCallback ()
{
  sendToDebug("*IR: Should save config\n");
  shouldSaveConfig = true;
}

/***************************************************
 * Load default IR data
 */

void loadDefaultIR()
{
  sendToDebug("*IR: loading IR raw codes\n");
  char fName[20];
  sprintf(fName,"/ir/1.dat");
  rawIR1size = readDataFile(fName, rawIR1);
  sprintf(fName,"/ir/2.dat");
  rawIR2size = readDataFile(fName, rawIR2);
}

/***************************************************************
 *  Interpreter of IR encoding ID
 */
void  getIrEncoding (decode_results *results, char * result_encoding)
{
  switch (results->decode_type)
  {
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
    case PANASONIC:    strncpy(result_encoding,"PANASONIC\0",11);    break ;
  }
}
