# mqtt-ir-transceiver

ESP8266 based gateway between MQTT and IR. Use with PlatformIO. Works with ESP-01 (debug mode have to be disabled in globals.h)

## Features

* Receiving of IR transmission and publish it as MQTT messages
* Receive MQTT messages and send IR signal (multiple formats supported - NEC, RC5, LG, SONY, Global Cache)
* Storing raw IR messages on flash and transmitting via IR  

##Used librariers

* IRremoteESP8266 - https://github.com/markszabo/IRremoteESP8266/
* ArduinoJson - https://github.com/bblanchon/ArduinoJson
* PubSubClient - https://github.com/knolleary/pubsubclient
* WiFiManager - https://github.com/tzapu/WiFiManager

##Installation

###GPIO connections:
<table>
  <tr>
  <th>GPIO WEMOS</th>
  <th>GPIO ESP01</th>
  <th>Usage</th>
  </tr>
  <tr>
  <td>13</td>
  <td>0</td>
  <td>IR receiver</td>
  </tr>
  <tr>
  <td>14</td>
  <td>3 (Uart RX)</td>
  <td>IR LED - connected via simple transistor amplifier</td>
  </tr>
  <tr>
  <td>15 (to +3,3V)</td>
  <td>2 (to GND)</td>
  <td>Button - used for reset configuration</td>
  </tr>
  <tr>
  <td>2 (Weomos buildin)</td>
  <td>not used</td>
  <td>LED</td>
  </tr>
</table>

###Compilation and firmware uploading

TODO

##Usage

### Configuration

TODO

### Controller → Device communication
<table>
  <tr>
    <th>Property</th>
    <th>Message format</th>
    <th>Description</th>
    <th>Example</th>
  </tr>
  <tr>
    <td>_mqtt_prefix_/sender/storeRaw/_store_id_</td>
    <td>\d+(,\d+)</td>
    <td>store raw codes sequence in slot no. _store_id_</td>
    <td>Topic: "_mqtt_prefix_/sender/storeRaw/10" <br/> Message: "32,43,54,65,32"</td>
  </tr>
  <tr>
    <td>_mqtt_prefix_/sender/sendStoredRaw</td>
    <td>\d+</td>
    <td>Transmit via IR RAW code from provided slot</td>
    <td>Topic: "_mqtt_prefix_/sender/sendStoredRaw" <br/> Message: "1"</td>
  </tr>
  <tr>
    <td>_mqtt_prefix_/sender/sendStoredRawSequence</td>
    <td>\d+(,\d+)*</td>
    <td>Transmit via IR sequence of RAW codes from provided slots</td>
    <td>Topic: "_mqtt_prefix_/sender/sendStoredRawSequence" <br/> Message: "1,2,3"</td>
  </tr>
  <tr>
    <td>_mqtt_prefix_/sender/cmd</td>
    <td>(ls|sysinfo)</td>
    <td>Execute on device command, replay in topic _mqtt_prefix_/sender/cmd/result</td>
    <td>Topic: "_mqtt_prefix_/sender/cmd"<br/> Message: "sysinfo"</td>
  </tr>
  <tr>
    <td>_mqtt_prefix_/sender/rawMode</td>
    <td>(1|ON|true|.*)</td>
    <td>Turn on/off reporting to controller received by device IR raw codes</td>
    <td>Topic: "_mqtt_prefix_/sender/rawMode"<br/>Message: "1"</td>
  </tr>
  <tr>
    <td>_mqtt_prefix_/wipe</td>
    <td>.*</td>
    <td>Wipe configuration for next boot</td>
    <td>Topic: "_mqtt_prefix_/wipe"<br/>Message: "1"</td>
  </tr>
  <tr>    
    <td>_mqtt_prefix_/sender/(RC_5|RC_6|NEC|SAMSUNG|SONY|LG)/(\d+)</td>
    <td>\d+</td>
    <td>Send IR signal based on type</td>
    <td>Topic: "esp8266/02sender/RC_5/12"<br/>Message: "3294"</td>
  </tr>
  <tr>  
    <td>_mqtt_prefix_/sender/sendGC</td>
    <td>\d+(,\d+)</td>
    <td>Send Global Cache code</td>
    <td>Topic: "_mqtt_prefix_/sender/sendGC" <br/> Message: "32000,43,54,65,32,...."</td>
  </tr>
</table>


### Device → Controller communication

<table>
  <tr>
    <th>Property</th>
    <th>Message format</th>
    <th>Direction</th>
    <th>Example</th>
  </tr>
    <tr>
    <td>_mqtt_prefix_/sender/cmd/result</td>
    <td>.*</td>
    <td>Result of command</td>
    <td></td>
  </tr>
  <tr>
    <td>_mqtt_prefix_/receiver/_type_/_bits_/_panas_addr_</td>
    <td>\d+(,\d+)*</td>
    <td>Send to controller received IR code</td>
    <td>Topic: "_mqtt_prefix_/receiver/RC_5/12"<br/>Message: "3294"</td>
  </tr>
</table>
