#!/usr/bin/env python3
import hid, time

VID, PID = 0x2047, 0x0965
RID = 0x3F
MAX_REPORT = 64
MAX_DATA = MAX_REPORT - 2  # 62 bájt
BAUD = 9600                # ~100 µs/bit -> 9600

def pick_paths():
    devs = [d for d in hid.enumerate() if d.get('vendor_id')==VID and d.get('product_id')==PID]
    if not devs:
        raise SystemExit("Nincs eszköz 2047:0965 VID:PID-en.")
    by_if = {d.get('interface_number'): d['path'] for d in devs}
    console = by_if.get(1) or (devs[1]['path'] if len(devs)>1 else devs[0]['path'])
    data    = by_if.get(0) or (devs[0]['path'] if len(devs)>1 else None)
    if not data:
        raise SystemExit("Nem találtam adat (ifnum=0) HID interfészt.")
    return console, data

def open_path(path):
    h = hid.device(); h.open_path(path); h.set_nonblocking(False); return h

def send_report(dev, payload: bytes):
    if len(payload) > MAX_DATA: raise ValueError("Payload max 62 B.")
    buf = bytes([RID, len(payload)]) + payload + b'\x00'*(MAX_DATA-len(payload))
    n = dev.write(buf)
    if n != len(buf): raise IOError(f"Short write {n}/{len(buf)}")

def read_report(dev, timeout_ms=1000):
    raw = dev.read(MAX_REPORT, timeout_ms)
    if not raw: return None, b''
    b = bytes(raw); rid, size = b[0], b[1]
    return rid, b[2:2+size]

def hb(cons, cmd: str, tmo=500):   # nincs sorvége!
    send_report(cons, cmd.encode('ascii'))
    time.sleep(0.02)
    _, p = read_report(cons, tmo)
    return (p.decode('ascii','ignore') if p else "")

def main():
    cons_path, data_path = pick_paths()
    cons = open_path(cons_path)
    data = open_path(data_path)
    try:
        # 1) RAW UART mód
        print("MODE(set):", hb(cons, "HB MODE RAW UART") or "<no text>")
        # 2) Baud 9600
        print("UARTBAUD(set):", hb(cons, f"HB UARTBAUD {BAUD}") or "<no text>")
        # 3) Fogadás
        print("RX indul (Ctrl+C kilép)…")
        while True:
            rid, p = read_report(data, 1000)
            if p:
                try:
                    print(p.decode('latin-1'), end='', flush=True)
                except:
                    print("RX hex:", p.hex())
    except KeyboardInterrupt:
        pass
    finally:
        try: data.close(); cons.close()
        except: pass

if __name__ == "__main__":
    main()
