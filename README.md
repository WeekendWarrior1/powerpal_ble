_This repository is not affiliated with Powerpal._

# powerpal_ble
Collection of code, tools and documentation for data retrieval over BLE from your Powerpal

## Using the Arduino sketch
This sketch simply prints the timestamp, pulses and energy usage of the updates sent by the Powerpal (the update interval can also be configured).

It's mainly useful to demonstrate that the Powerpal is connectable over BLE by a third party device, without the need for the proprietary Powerpal Pro wifi gateway (which I believe also uses an ESP32).

Of interest in the sketch is the security configuration required to pair/handshake with the Powerpal, which may limit what BLE technologies, libraries and languages you can use to connect (not every library seems to support secure BLE connections)

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