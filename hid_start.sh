#!/bin/bash
while IFS= read -r line
do
  bdata=( ${line:19:29} )

  if [[ ${bdata[0]} != "a1" ]] || [[ ${bdata[1]} != "01" ]]
  then
    continue
  fi

  output="\x${bdata[2]}\x00\x${bdata[4]}\x${bdata[5]}\x${bdata[6]}\x${bdata[7]}\x${bdata[8]}\x${bdata[9]}"

  printf "%b" "$output" | dd of=/dev/hidg0 2>/dev/null
done < <(unbuffer btmon 2>&1)
