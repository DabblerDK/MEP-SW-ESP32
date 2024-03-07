#!/bin/bash

cd "$(dirname "$1")"
rm update.wwws
for file in www_*; do
  echo "/$file" >> update.wwws
  cat "$file" >> update.wwws
  echo >> update.wwws
done

read -p "Press any key to continue . . ."