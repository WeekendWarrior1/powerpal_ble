from encodings import utf_8
import sys
import asyncio
import requests

from bleak import BleakClient, BleakScanner, BleakError

logger = logging.getLogger(__name__)

pairingCodeChar = '59DA0011-12F4-25A6-7D4F-55961DCE4205'
powerpalUUIDChar ='59DA0009-12F4-25A6-7D4F-55961DCE4205'
powerpalSerialChar = '59DA0010-12F4-25A6-7D4F-55961DCE4205'

def convert_pairing_code(original_pairing_code):
    return int(original_pairing_code).to_bytes(4, byteorder='little')

async def main(address, my_pairing_code):
    while address is None:
        address = input("Your Powerpal MAC address:")
        if (address.count(':') is not 5) or (len(address) is not 17):
            address = None
            print("Incorrect MAC address formatting, should look like -> 12:34:56:78:9A:BC")
    while my_pairing_code is None:
        my_pairing_code = int(input("Your Powerpal pairing code:"))
        if not (0 <= my_pairing_code <= 999999):
            my_pairing_code = None
            print("Pairing Code should be 6 digits...")
    input("Please confirm that you are NOT connected to the Powerpal via Bluetooth using any devices, and hit enter to continue...")

    async with BleakClient(address) as client:
        print(f"Connected: {client.is_connected}")

        paired = await client.pair(2)
        print(f"Paired?: {paired}")

        print(f"Authenticating with pairing_code: {my_pairing_code}, converted: {convert_pairing_code(my_pairing_code)}")
        await client.write_gatt_char(pairingCodeChar, convert_pairing_code(my_pairing_code), response=False)
        print('Auth Success\n')

        await asyncio.sleep(0.5)

        print(f"Attempting to retrieve Powerpal Serial Number...")
        serial = await client.read_gatt_char(powerpalSerialChar)
        formatted_serial = ''.join('{:02x}'.format(x) for x in reversed(serial)).lower()
        print(f"Retrieved Device Serial:   {formatted_serial}")
        print(f"Meaning your meter reading endpoint is:  https://readings.powerpal.net/api/v1/meter_reading/{formatted_serial}\n")

        print(f"Attempting to retrieve apikey (Powerpal UUID)...")
        apikey = await client.read_gatt_char(powerpalUUIDChar)
        joined_apikey = ''.join('{:02x}'.format(x) for x in apikey)
        formatted_apikey = f"{joined_apikey[:8]}-{joined_apikey[8:12]}-{joined_apikey[12:16]}-{joined_apikey[16:20]}-{joined_apikey[20:]}".lower()
        print(f"Retrieved apikey: {formatted_apikey}\n")

        test_endpoint = input(f"Would you like this script to validate your apikey by using it to make a request for your device information from https://readings.powerpal.net/api/v1/device/{formatted_serial}? (y/n)")
        if test_endpoint.lower().startswith('y'):
            url = f"https://readings.powerpal.net/api/v1/device/{formatted_serial}"
            headers = { 'Authorization': formatted_apikey }

            response = requests.request("GET", url, headers=headers, data=payload)
            if (response.status_code < 300):
                print("Success! Looks like your Device Serial and apikey were succesfully retrieved! Here is the device information returned from https://readings.powerpal.net/api/ :")
                print(response.text)
            else: 
                print("Failure!")
                print(f"{response.status_code} - {response.reason} - {response.text}")
        else:
            print(f'You may also confirm your apikey by using curl:\ncurl -H "Authorization: {formatted_apikey}" https://readings.powerpal.net/api/v1/device/{formatted_serial}')

if __name__ == "__main__":
    asyncio.run(main((sys.argv[1] if len(sys.argv) >= 2 else None),(sys.argv[2] if len(sys.argv) == 3 else None)))