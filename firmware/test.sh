CONS=/dev/hidraw0    # <-- IDE tedd a HELYES (2047:0965, iface=1) hidraw-ot

send_hb() {
  local cmd=$1          # pl. 'HB VERSION'
  local c="${cmd}"$'\r' # CR a végére
  # hosszmérés byte-alapon, LC_ALL=C hogy a shell bájtot számoljon
  LC_ALL=C printf ''    # biztos ami biztos
  local clen=${#c}
  if [ $clen -gt 63 ]; then
    echo "Parancs túl hosszú: $clen > 63" >&2; return 1
  fi
  {
    printf '\x3F'                       # ReportID
    printf '%s' "$c"                    # parancs + CR
    head -c $((63 - clen)) < /dev/zero  # null padding, hogy pont 63 legyen a payload
  } | dd of="$CONS" bs=64 count=1 status=none || return $?

  # Válasz olvasása (64B), első bájt a ReportID → vágd le
  dd if="$CONS" bs=64 count=1 status=none \
  | tail -c +2 | tr -d '\0'
}

send_hb 'HB VERSION'
send_hb 'HB MODE RAW UART'
send_hb 'HB UARTBAUD 250000'
# opcionális mentés:
send_hb 'HB SAVECONFIG ACTIVE'
