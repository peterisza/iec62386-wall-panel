# test3.py - Egyszerű 16 bites DALI keret küldése
import time
import dali.frame
import dali.command
from dali.driver.hasseb import SyncHassebDALIUSBDriver

def send16(dev, addr, data):
    """Küld egy 16 bites DALI keretet (addr, data)."""
    frame = dali.frame.ForwardFrame(16, [addr & 0xFF, data & 0xFF])
    cmd = dali.command.from_frame(frame)
    dev.send(cmd)
    print(f"Küldve: addr=0x{addr:02X}, data=0x{data:02X}")

def send24(dev, addr, data, data2):
    """Küld egy 24 bites DALI keretet (addr, data)."""
    frame = dali.frame.ForwardFrame(24, [addr & 0xFF, data & 0xFF, data2 & 0xFF])
    cmd = dali.command.from_frame(frame)
    dev.send(cmd)
    print(f"Küldve: addr=0x{addr:02X}, data=0x{data:02X}, data2=0x{data2:02X}")

def main():
    dev = SyncHassebDALIUSBDriver()

    # Itt állíthatod be a keret tartalmát
    # Formátum: [addr, data], mindkettő 8 bites (0x00-0xFF)
    ADDR = 0x52  # Példa: 0xA5
    DATA = 0xFA  # Példa: 0x00
    
    print(f"16 bites DALI keret küldése: addr=0x{ADDR:02X}, data=0x{DATA:02X}")
    send24(dev, 0xFA, 0x55, 0x52)
    
    print("Kész!")

if __name__ == "__main__":
    main()
