#include "globals.h"


void sendToDebug(String message)
{
   if (useDebug)
   {
     Serial.print(message);
   }
}
