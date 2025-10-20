#!/usr/bin/env python3

import hid, time

VID, PID = 0x2047, 0x0965
RID, PAYLOAD = 0x3F, 63  # ReportID és payload méret a descriptorból

def open_iface(iface):
    for d in hid.enumerate(VID, PID):
        if d.get('interface_number') == iface:
            h = hid.device(); h.open_path(d['path']); return h
    raise SystemExit(f"iface {iface} nem található")

cfg  = open_iface(1)  # konzol (HB parancsok)
data = open_iface(0)  # adat (RAW UART)

def send_cmd(cmd_ascii: bytes, timeout=0.5) -> bytes:
    # cmd_ascii: pl. b'HB VERSION\r'  (CR kell, CRLF is jó)
    if len(cmd_ascii) > PAYLOAD:
        raise ValueError("parancs túl hosszú a 63B payloadhoz")
    pkt = bytes([RID]) + cmd_ascii + b"\x00"*(PAYLOAD - len(cmd_ascii))
    print(len(pkt), pkt)
    cfg.write(pkt)  # Output report, pontosan 64 bájt (ID+63)
    # válasz
    cfg.set_nonblocking(True); t0=time.time(); buf=b""
    while time.time()-t0 < timeout:
        r = cfg.read(64)
        if r and r[0] == RID:
            buf += bytes(r[1:])  # levágjuk az ID-t
            if b"\n" in buf: break
        else:
            time.sleep(0.01)
    return buf.rstrip(b"\x00")

# -- konzol parancsok --
print(send_cmd(b'HB VERSION\r').decode('utf-8', 'ignore'))
#print(send_cmd(b'HB MODE RAW UART\r').decode('utf-8', 'ignore'))
#print(send_cmd(b'HB UARTBAUD 9600\r').decode('utf-8', 'ignore'))
# opcionális mentés:
# print(send_cmd(b'HB SAVECONFIG ACTIVE\r').decode('utf-8','ignore'))

# -- UART olvasás (adat iface) --
print("UART bejövő adat (Ctrl+C):")
data.set_nonblocking(True)
try:
    while True:
        r = data.read(64)
        if r and r[0] == RID:
            bs = bytes(r[1:]).rstrip(b"\x00")
            if bs:
                print(bs.decode('utf-8', 'ignore'), end="", flush=True)
        else:
            time.sleep(0.01)
except KeyboardInterrupt:
    pass
