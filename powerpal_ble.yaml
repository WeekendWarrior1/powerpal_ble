esphome:
  name: powerpalble

esp32:
  board: wemos_d1_mini32
  framework:
    type: arduino

external_components:
  - source: github://WeekendWarrior1/esphome@powerpal_ble
    # requires ble_client because I had to add some small features to authenticate properly
    components: [ ble_client, powerpal_ble ]

api:

ota:

wifi:
  ssid: "YOUR_WIFI"
  password: "super_secret"

  ap:
    ssid: "powerpalble Fallback Hotspot"
    password: "super_secret"

# optional requirement to enable powerpal cloud uploading
#http_request:
#  id: powerpal_cloud_uploader

# optional requirement used with daily energy sensor
time:
  - platform: homeassistant
    id: homeassistant_time

esp32_ble_tracker:

ble_client:
  - mac_address: DF:5C:55:01:02:03
    id: powerpal

sensor:
  - platform: powerpal_ble
    ble_client_id: powerpal
    power:
      name: "Powerpal Power"
    daily_energy:
      name: "Powerpal Daily Energy"
    energy:
      name: "Powerpal Total Energy"
    battery_level:
      name: "Powerpal Battery"
    pairing_code: 123123
    notification_interval: 1
    pulses_per_kwh: 1000
    time_id: homeassistant_time # daily energy still works without a time_id, but recommended to include one to properly handle daylight savings, etc.
#    http_request_id: powerpal_cloud_uploader
#    cost_per_kwh: 0.1872 #dollars per kWh
#    powerpal_device_id: 0000abcd #optional, component will retrieve from your Powerpal if not set
#    powerpal_apikey: 4a89e298-b17b-43e7-a0c1-fcd1412e98ef #optional, component will retrieve from your Powerpal if not set
