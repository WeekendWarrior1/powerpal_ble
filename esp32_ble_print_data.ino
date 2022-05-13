/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 * modified by WeekendWarrior1 to specifically connect to a Powerpal
 */

#include "BLEDevice.h"

static char *BLE_address("df:5c:55:00:00:00"); // lowercase only or else will fail to match
// if your pairing pin starts with 0, eg "024024", set the powerpal_pass_key as 24024
static uint32_t powerpal_pass_key = 123123;
static float pulses_per_kw = 1000;
static uint8_t read_every = 1; // minutes (only tested between 1 - 15 minutes)

static float pulse_multiplier = (60.0 / read_every) / pulses_per_kw; //0.075

static BLEUUID SERVICE_POWERPAL_UUID("59DAABCD-12F4-25A6-7D4F-55961DCE4205");
static BLEUUID CHAR_PAIRINGCODE_UUID("59DA0011-12F4-25A6-7D4F-55961DCE4205");  // indicate, notify, read, write
static BLEUUID CHAR_READINGBATCHSIZE_UUID("59DA0013-12F4-25A6-7D4F-55961DCE4205");  // indicate, notify, read, write
static BLEUUID CHAR_MEASUREMENT_UUID("59DA0001-12F4-25A6-7D4F-55961DCE4205");  // notify, read, write
static BLEUUID CHAR_UUID_UUID("59DA0009-12F4-25A6-7D4F-55961DCE4205");  // notify, read, write
static BLEUUID CHAR_SERIAL_NUMBER_UUID("59DA0010-12F4-25A6-7D4F-55961DCE4205");  // notify, read, write

static BLEUUID SERVICE_BATTERY_UUID("0000180f-0000-1000-8000-00805f9b34fb");
static BLEUUID CHAR_BATTERY_READ_UUID("00002a19-0000-1000-8000-00805f9b34fb");  // read, notify

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static boolean authenticated = false;
static boolean paired = false;
static boolean batchset = false;

static BLEAdvertisedDevice *myDevice;
static BLEClient *pClient;

static BLERemoteCharacteristic *pRemoteCharacteristic_pairingcode;
static BLERemoteCharacteristic *pRemoteCharacteristic_readingbatchsize;
static BLERemoteCharacteristic *pRemoteCharacteristic_measurement;
static BLERemoteCharacteristic *pRemoteCharacteristic_battery_read;
static BLERemoteCharacteristic *pRemoteCharacteristic_uuid;
static BLERemoteCharacteristic *pRemoteCharacteristic_serial_number;

/*
    time: '59DA0004-12F4-25A6-7D4F-55961DCE4205',
    ledSensitivity: '59DA0008-12F4-25A6-7D4F-55961DCE4205',
    uuid: '59DA0009-12F4-25A6-7D4F-55961DCE4205',
    serialNumber: '59DA0010-12F4-25A6-7D4F-55961DCE4205',
    pairingCode: '59DA0011-12F4-25A6-7D4F-55961DCE4205',
    measurement: '59DA0001-12F4-25A6-7D4F-55961DCE4205',
    pulse: '59DA0003-12F4-25A6-7D4F-55961DCE4205',
    millisSinceLastPulse: '59DA0012-12F4-25A6-7D4F-55961DCE4205',
    firstRec: '59DA0005-12F4-25A6-7D4F-55961DCE4205',
    measurementAccess: '59DA0002-12F4-25A6-7D4F-55961DCE4205',
    readingBatchSize: '59DA0013-12F4-25A6-7D4F-55961DCE4205',
*/

// uint32_t powerpal_reverse_uint32(uint32_t input) {
//     uint32_t output = 0;
//     output += ((input & 0x000000FF) << 24);
//     output += ((input & 0x0000FF00) << 8);
//     output += ((input & 0x00FF0000) >> 8);
//     output += ((input & 0xFF000000) >> 24);
//     return output;
// }

// needs to return array, not uint32
uint8_t* powerpal_reverse_uint32(uint32_t input) {
    static uint8_t output[] = {
        (input & 0x000000FF),
        ((input & 0x0000FF00) >> 8),
        ((input & 0x00FF0000) >> 16),
        ((input & 0xFF000000) >> 24),
    };
    return output;
}

static void powerpalCommandCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    for (int i = 0; i < length; i++) {
        Serial.print(" ");
        Serial.print(pData[i]);
    }
    Serial.println("");

    // should always be 20, but we're only interested in first 6 (timestamp and pulses)
    if (length >= 6) {
        uint32_t unix_time = pData[0];
        unix_time += (pData[1] << 8);
        unix_time += (pData[2] << 16);
        unix_time += (pData[3] << 24);
        Serial.print("Time: ");
        Serial.print(unix_time);

        
        uint16_t total_pulses = pData[4];
        total_pulses += pData[5] << 8;
        Serial.print(", Pulses: ");
        Serial.print(total_pulses);

        float total_kw = total_pulses * pulse_multiplier;
        Serial.print(", Power: ");
        Serial.print(total_kw);
        Serial.println(" kW");
    }
}

static void powerpalBatteryCallback(
    BLERemoteCharacteristic *pBLERemoteCharacteristic,
    uint8_t *pData,
    size_t length,
    bool isNotify)
{
    Serial.print("Notify callback for battery characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("Battery: ");
    for (int i = 0; i < length; i++) {
        Serial.print(" ");
        Serial.print(pData[i]);
    }
    Serial.println("%");
}

class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        connected = true;
        Serial.println("onConnect");

        Serial.print("MTU:");
        Serial.println(pclient->getMTU());
        // pclient->getMTU();
    }

    void onDisconnect(BLEClient *pclient)
    {
        connected = false;
        authenticated = false;
        paired = false;
        batchset = false;
        Serial.println("onDisconnect");
    }
};

class MySecurityCallback : public BLESecurityCallbacks
{
    /**
     * @brief Its request from peer device to input authentication pin code displayed on peer device.
     * It requires that our device is capable to input 6-digits code by end user
     * @return Return 6-digits integer value from input device
     */
    uint32_t onPassKeyRequest()
    {
        Serial.println("onPassKeyRequest");
        return powerpal_pass_key;
    }

    bool onConfirmPIN(uint32_t pin)
    {
        Serial.print("onConfirmPIN: ");
        Serial.println(pin);
        return true;
    }

    /**
     * @brief Provide us 6-digits code to perform authentication.
     * It requires that our device is capable to display this code to end user
     * @param
     */
    void onPassKeyNotify(uint32_t pass_key)
    {
        Serial.print("onPassKeyNotify: ");
        Serial.println(pass_key);
    }

    /**
     * @brief Here we can make decision if we want to let negotiate authorization with peer device or not
     * return Return true if we accept this peer device request
     */
    bool onSecurityRequest()
    {
        Serial.println("onSecurityRequest");
        return true; //?
    }

    /**
     * Provide us information when authentication process is completed
     */
    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl)
    {
        Serial.println("onAuthenticationComplete");
        if (auth_cmpl.success)
        {
            Serial.println("auth_cmpl.success");
            paired = true;
        }
        else
        {
            Serial.println("auth_cmpl.failed");
            Serial.print("fail_reason: ");
            Serial.println(auth_cmpl.fail_reason);
        }
    }
};

bool connectToServer()
{
    Serial.print("Forming a connection to ");
    Serial.println(BLE_address);

    // security
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_BLE_SEC_ENCRYPT_MITM);
    pSecurity->setCapability(ESP_IO_CAP_KBDISP);

    // https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_security_server/tutorial/Gatt_Security_Server_Example_Walkthrough.md
    // esp_ble_io_cap_t iocap = ESP_IO_CAP_KBDISP;//set the IO capability to Keyboard,display
    // esp_ble_io_cap_t iocap = ESP_IO_CAP_IO;//set the IO capability to Keyboard,display
    // esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND; //bonding with peer device after authentication
    // uint8_t key_size = 16;      //the key size should be 7~16 bytes
    // uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    // uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    // esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    // esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    // esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    // esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    // esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    BLEDevice::setSecurityCallbacks(new MySecurityCallback());

    pClient = BLEDevice::createClient();
    Serial.println(" - Created client");
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remote BLE Server.
    pClient->connect(myDevice);
    Serial.println(" - Connected to server");

    // service battery
    Serial.println("Attempting to get battery service...");
    BLERemoteService *pRemoteService_battery = pClient->getService(SERVICE_BATTERY_UUID);
    if (pRemoteService_battery == nullptr)
    {
        Serial.print("Failed to find our battery service UUID: ");
        Serial.println(SERVICE_BATTERY_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    pRemoteCharacteristic_battery_read = pRemoteService_battery->getCharacteristic(CHAR_BATTERY_READ_UUID);
    if (pRemoteCharacteristic_battery_read == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(CHAR_BATTERY_READ_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    else
    {
        // Serial.println("Attempting to read battery level...");
        // uint32_t battery = pRemoteCharacteristic_battery_read->readUInt32();
        // Serial.print("Battery Level: ");
        // Serial.println(battery);

        // if (pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic_battery_read->registerForNotify(powerpalBatteryCallback);
    }

    // powerpal service
    Serial.println("Attempting to get powerpal service...");
    BLERemoteService *pRemoteService_powerpal = pClient->getService(SERVICE_POWERPAL_UUID);
    if (pRemoteService_powerpal == nullptr)
    {
        Serial.print("Failed to find our powerpal service UUID: ");
        Serial.println(SERVICE_POWERPAL_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    // pairingcode
    pRemoteCharacteristic_pairingcode = pRemoteService_powerpal->getCharacteristic(CHAR_PAIRINGCODE_UUID);
    if (pRemoteCharacteristic_pairingcode == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(CHAR_PAIRINGCODE_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    // readingbatchsize
    pRemoteCharacteristic_readingbatchsize = pRemoteService_powerpal->getCharacteristic(CHAR_READINGBATCHSIZE_UUID);
    if (pRemoteCharacteristic_readingbatchsize == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(CHAR_READINGBATCHSIZE_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    // measurement
    pRemoteCharacteristic_measurement = pRemoteService_powerpal->getCharacteristic(CHAR_MEASUREMENT_UUID);
    if (pRemoteCharacteristic_measurement == nullptr)
    {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(CHAR_MEASUREMENT_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    //uuid (cloud api key)
    pRemoteCharacteristic_uuid = pRemoteService_battery->getCharacteristic(CHAR_UUID_UUID);
    if (pRemoteCharacteristic_uuid == nullptr)
    {
        Serial.print("Failed to find our uuid UUID: ");
        Serial.println(CHAR_UUID_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    //serial number (cloud device id)
    pRemoteCharacteristic_serial_number = pRemoteService_battery->getCharacteristic(CHAR_SERIAL_NUMBER_UUID);
    if (pRemoteCharacteristic_serial_number == nullptr)
    {
        Serial.print("Failed to find our serial number UUID: ");
        Serial.println(CHAR_SERIAL_NUMBER_UUID.toString().c_str());
        pClient->disconnect();
        return false;
    }

    connected = true;
    return true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        if (advertisedDevice.getAddress().toString() == BLE_address)
        {
            Serial.println("Attempting to connect to device...");

            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;

            Serial.println("Found our device");
        }
        else
        {
            Serial.println("Skipping, isn't correct device");
        }
    } // onResult
};    // MyAdvertisedDeviceCallbacks

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting Arduino BLE Client application...");

    BLEDevice::init("");

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
} // End of setup.

// This is the Arduino main loop function.
void loop()
{

    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be true.
    if (doConnect == true)
    {
        if (connectToServer())
        {
            Serial.println("We are now connected to the BLE Server.");
        }
        else
        {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
        }
        doConnect = false;
    }

    if (connected)
    {
        if (paired && !authenticated) {
            // connected but not authenticated, so do that now (need to send pairing code)
            // Serial.println(rev_powerpal_pass_key, HEX);
            Serial.println("Sending pairingcode to char...");
            uint8_t* rev_powerpal_pass_key = powerpal_reverse_uint32(powerpal_pass_key);
            pRemoteCharacteristic_pairingcode->writeValue(rev_powerpal_pass_key, sizeof(rev_powerpal_pass_key), false);
            
            // notify for pulse updates
            pRemoteCharacteristic_measurement->registerForNotify(powerpalCommandCallback);
            authenticated = true;
        }
        if (paired && authenticated && !batchset)  {
            uint32_t batchReadingSize = pRemoteCharacteristic_readingbatchsize->readUInt32();
            // std::string batchReadString =  pRemoteCharacteristic_readingbatchsize->readValue();
            Serial.print("Reading measurements every: ");
            Serial.print(batchReadingSize);
            Serial.println(" minute(s)");
            // Serial.println(batchReadString.c_str());
            if (batchReadingSize != 1) {
                Serial.println("Setting to every 1 minute");
                uint8_t newBatchReadingSize[] = {read_every, 0x00, 0x00, 0x00};
                // uint8_t* newBatchReadingSize = powerpal_reverse_uint32(read_every);
                pRemoteCharacteristic_readingbatchsize->writeValue(newBatchReadingSize, sizeof(newBatchReadingSize), false);
            }
            batchset = true;

            const char* hexmap[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"};

            uint8_t* raw_api_key = pRemoteCharacteristic_uuid->readRawData();
            uint8_t length = 16;
            std::string api_key;
            for (int i = 0; i < length; i++) {
            if ( i == 4 || i == 6 || i == 8 || i == 10 ) {
                api_key.append("-");
            }
                api_key.append(hexmap[(raw_api_key[i] & 0xF0) >> 4]);
                api_key.append(hexmap[raw_api_key[i] & 0x0F]);
            }
            Serial.print("Powerpal Cloud API Key: ");
            Serial.println(api_key.c_str());

            uint8_t* raw_device_id = pRemoteCharacteristic_uuid->readRawData();
            length = 4;
            std::string device_id;
            for (int i = length-1; i >= 0; i--) {
                device_id.append(hexmap[(raw_device_id[i] & 0xF0) >> 4]);
                device_id.append(hexmap[raw_device_id[i] & 0x0F]);
            }
            Serial.print("Powerpal Device ID: ");
            Serial.println(device_id.c_str());

            Serial.println("You can confirm these decoded results on a computer with curl installed by running: ");
            Serial.print("curl -H \"Authorization: ");
            Serial.print(api_key.c_str());
            Serial.print("\" https://readings.powerpal.net/api/v1/device/");
            Serial.println(device_id.c_str());
        }
    }
    else if (doScan)
    {
        BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
    }

    delay(1000); // Delay a second between loops.
} // End of loop