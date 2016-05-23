# mqtt-ir-transceiver

Gateway between MQTT and IR signal

##Usage

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
    <td>Topic: "esp8266/02/sender/RC_5/12"<br/>Message: "3294"</td>
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
