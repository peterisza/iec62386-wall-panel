# melyik hidraw melyik interface?
for n in /dev/hidraw*; do
  echo "---- $n ----"
  udevadm info -a -n $n | egrep -m1 'idVendor|idProduct|bInterfaceNumber'
done
# jegyezd fel: a 2047:0965 VID:PID közül amelyiknek bInterfaceNumber==1 → KONZOL (HB parancsok)
# amelyiknek bInterfaceNumber==0 → ADAT (RAW UART bájtok)

# dumpold a riport-deszkriptort a konzol (iface=1) eszközre:
sudo usbhid-dump -d 2047:0965 -i 1 -e descriptor
