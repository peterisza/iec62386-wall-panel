#!/usr/bin/env python3
from dali.gear.general import QueryActualLevel
from dali.address import Short, Broadcast
from dali.frame import ForwardFrame, BackwardFrame
from dali.driver.hasseb import SyncHassebDALIUSBDriver
import time

import pprint as pp

driver = SyncHassebDALIUSBDriver()

# NE hívd meg: driver.enableSniffing()  ← ezt kihagyjuk!
driver.enableSniffing()
print("Várom a 24 bites event kereteket (Ctrl+C kilépés)")

while True:
    frame = driver.receive()   # ez most normál master módban működik
    if frame:
        if isinstance(frame, ForwardFrame) and len(frame) == 24:
            # 24 bites DALI-2 parancs vagy event
            addr = frame.address_byte
            cmd  = frame.command_bytes
            print(f"→ 24bit CMD: {frame}  | Addr: {addr:02X}  Cmd: {cmd.hex()}")
        
        elif isinstance(frame, BackwardFrame) and len(frame) == 8:
            print(f"← 8bit válasz: {frame}  ({frame.value})")
        
        else:
            print(f"Egyéb keret: {frame}")

    time.sleep(0.001)