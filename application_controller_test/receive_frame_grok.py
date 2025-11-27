import hid  # HID eszközökhöz
import logging
from dali.driver.hasseb import SyncHassebDALIUSBDriver
from dali.address import Broadcast
from dali.gear.general import Off, QueryActualLevel  # Példa parancsok

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