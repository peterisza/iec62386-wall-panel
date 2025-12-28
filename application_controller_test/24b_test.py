import hid  # HID eszközökhöz
import logging
from dali.driver.hasseb import SyncHassebDALIUSBDriver
from dali.address import Broadcast
from dali.gear.general import Off, QueryActualLevel  # Példa parancsok
import time

# Logging beállítása debugoláshoz
logging.basicConfig(level=logging.INFO)

# Hasseb eszköz keresése USB-n
HASSEB_VENDOR_ID = 0x04cc
HASSEB_PRODUCT_ID = 0x0802

devices = hid.enumerate(HASSEB_VENDOR_ID, HASSEB_PRODUCT_ID)
if not devices:
    print("Nem található Hasseb USB eszköz!")
    exit(1)

device_path = devices[0]['path']
print(f"Eszköz megtalálva: {device_path}")

# Driver inicializálása (szinkron mód, mert egyszerűbb kezdőknek)
driver = SyncHassebDALIUSBDriver()  # Vagy SyncHassebDALIUSBDriver, ha a verzióban így van
driver.enableSniffing()
# Hasseb USB protokoll 24-bitre (Mode 4):
# Report ID (1) + Command (4) + 3 adatbájt + padding
#usb_packet = [0x01, 0x04, b2, b1, b0, 0x00, 0x00, 0x00]
packet = 1200 + 0x8000

def setParam(param, value):
    if param != 0xA000:
        value = value >> 2
    packet = param + value
    usb_packet = [0x07, 0x02, 16, 0, 0, 0, (packet >> 8) & 0xFF, packet & 0xFF]
    driver.device.write(usb_packet)

setParam(0x8000, 50)
setParam(0x9000, 2000)
setParam(0xA000, 34)

exit(1)

# Példa: Lekérdezés küldése és válasz fogadása (fogadás)
cmd_query = QueryActualLevel(Broadcast())
response = driver.send(cmd_query)
if response:
    print(f"Válasz érkezett: {response.value}")
else:
    print("Nincs válasz a lekérdezésre.")

# Ha folyamatosan akarsz fogadni (de Hasseb-bel korlátozott), loop-ban küldhetsz lekérdezéseket
# while True:
#     response = driver.send(cmd_query)
#     if response:
#         print(f"Fogadott: {response.value}")
#     time.sleep(1)