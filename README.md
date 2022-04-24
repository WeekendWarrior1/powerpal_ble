_This repository is not affiliated with Powerpal._

# powerpal_ble
Collection of code, tools and documentation for data retrieval over BLE from your Powerpal

## Using the Arduino sketch
This sketch simply prints the timestamp, pulses and energy usage of the updates sent by the Powerpal (the update interval can also be configured).

It's mainly useful to demonstrate that the Powerpal is connectable over BLE by a third party device, without the need for the proprietary Powerpal Pro wifi gateway (which I believe also uses an ESP32).

#### Requirements:
- An ESP32
- A configured Powerpal
- Powerpal device information:
  - BLE MAC address (can be found on device sticker, by running sketch, or by using an app like nRF Connect)
  - Connection pairing pin (6 digits you input when setting up your device, also can be found printed in Powerpal info pack, or inside the Powerpal application)
  - Your Smart meter pulse rate (eg. 1000 pulses = 1kW/h)


#### Fill in your details at top of esp32_ble_print_data.ino and upload:
```c++
static char *BLE_address("df:5c:55:00:00:00"); // lowercase only or else will fail to match
// if your pairing pin starts with 0, eg "024024", set the powerpal_pass_key as 24024
static uint32_t powerpal_pass_key = 123123;
static float pulses_per_kw = 1000;
static uint8_t read_every = 1; // minutes (only tested between 1 - 15 minutes)
```

#### Serial Monitor output:
![Serial Monitor Example Output](assets/arduino_serial_monitor_output.png)

## Connecting to a Powerpal over BLE

#### Important BLE services
```js
SERVICE_POWERPAL_UUID: '59DAABCD-12F4-25A6-7D4F-55961DCE4205'
    Characteristics:
        // Once subscribed to notifications, sends pulses with a timestamp every ${readingBatchSize}
        measurement: '59DA0001-12F4-25A6-7D4F-55961DCE4205' // notify, read, write

        // Use to trigger notifications of historic measurements between 2 dates
        measurementAccess: '59DA0002-12F4-25A6-7D4F-55961DCE4205' // indicate, write

        // Once subscribed to notifications, sends a notification (timestamp)for every pulse. This seems to be used by the application to display instantaneous power usage. This will likely chew through battery
        pulse: '59DA0003-12F4-25A6-7D4F-55961DCE4205' // notify, read

        // Used to set the time of the Powerpal. The Powerpal seems to have a pretty good RTC and you will likely not have to set this after the powerpal has been configured in it's app
        time: '59DA0004-12F4-25A6-7D4F-55961DCE4205' // indicate, notify, read, write

        // Seems to retrieve the timestamp of the first and last datapoints stored in the Powerpal
        firstRec: '59DA0005-12F4-25A6-7D4F-55961DCE4205' // read, write

        // Used to configure the sensitivity of the pulse reading sensor (Can also be done within the app)
        ledSensitivity: '59DA0008-12F4-25A6-7D4F-55961DCE4205' // indicate, notify, read, write

        // Read or change Powerpal hardware UUID
        uuid: '59DA0009-12F4-25A6-7D4F-55961DCE4205' // indicate, notify, read, write

        // Read or change Powerpal Serial Number
        serialNumber: '59DA0010-12F4-25A6-7D4F-55961DCE4205' // indicate, notify, read, write

        // needs to be written to with your powerpal pairing key before other services are accessible
        pairingCode: '59DA0011-12F4-25A6-7D4F-55961DCE4205' // indicate, notify, read, write

        // Seems to be used to calculate instantaneous power usage in app
        millisSinceLastPulse: '59DA0012-12F4-25A6-7D4F-55961DCE4205' // read

        // Can be written to to change Powerpal ${measurement} notification interval
        readingBatchSize: '59DA0013-12F4-25A6-7D4F-55961DCE4205' // indicate, notify, read, write
```
#### Connecting to a Powerpal over BLE

The Powerpal has simple authentication requirements allowing most devices and libraries to connect and pair without issue:

![Powerpal sent authreq](assets/powerpal_authreq.png)

After connecting, to be able to read, write or subscribe to notifications of any of the Powerpal Service characteristics your pairingCode must be written to the `pairingCode` characteristic, `59DA0011-12F4-25A6-7D4F-55961DCE4205`.
Your pairingCode needs to be converted to hex and then have its bytes reversed, eg:
```c++
uint32_t powerpal_pass_key = 123123;
// in hex 01E0F3, so
uint32_t powerpal_pass_key_hex = 0x01E0F3;
// esp32 arduino BLE library needs to write data as an array of uint8_t's, so
uint8_t powerpal_pass_key_array[] = {0x00, 0x01, 0xE0, 0xF3};
// Powerpal wants this array in reversed byte order, so:
uint8_t powerpal_pass_key_array_reversed[] = {0xF3, 0xE0, 0x01, 0x00};

// now this can be written to the pairing code characteristic:
pRemoteCharacteristic_pairingcode->writeValue(powerpal_pass_key_array_reversed, sizeof(powerpal_pass_key_array_reversed), false);
```

Authentication is now complete, so time to configure the `readingBatchSize` (which also needs to be converted to hex and then have its bytes reversed):
```c++
// set update interval to every 1 minute
uint8_t newBatchReadingSize[] = {0x01, 0x00, 0x00, 0x00};
pRemoteCharacteristic_readingbatchsize->writeValue(newBatchReadingSize, sizeof(newBatchReadingSize), false);
```
> :warning: **If reducing the readingBatchSize**: If you have reduced the readingBatchSize, eg from 15m to 1m, at the time of the next update you will receive all the historic pulse updates that have been collected during the previous interval configuration. This may be an issue if you are writing this data to Home Assistant, which currently doesn't support recieving historic datapoints. Luckily all the updates include a timestamp, so you can likely filter to only accept updates that are within +-5 seconds of the current time.

Subscribe to `measurement` notifications:
```c++
pRemoteCharacteristic_measurement->registerForNotify(powerpalCommandCallback);
```

Parse incoming `measurement` notifications:
```c++
// incoming data
// pData = [112 7 98 98 4 0 208 101 196 189 209 1 7 63 123 158 108 62 160 115]
// first 4 bytes (0-3) are a unix time stamp, again with reversed byte order
uint32_t unix_time = pData[0];
unix_time += (pData[1] << 8);
unix_time += (pData[2] << 16);
unix_time += (pData[3] << 24);

// next 2 bytes (4+5) are the pulses within the time interval window, with reversed byte order
uint16_t total_pulses = pData[4];
total_pulses += pData[5] << 8;

```