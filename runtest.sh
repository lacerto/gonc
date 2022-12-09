#!/bin/sh

echo -e "\nSource\n"
# -c %y BusyBox stat syntax for showing modification time
find test/src -type f -exec echo {} \; -exec stat -c %y {} \;

echo -e "\nDestination\n"
find test/dest -type f -exec echo {} \; -exec stat -c %y {} \;

echo -e "\n"

./gonc -d test/src test/dest

echo -e "\nSource\n"
find test/src -type f -exec echo {} \; -exec stat -c %y {} \;

echo -e "\nDestination\n"
find test/dest -type f -exec echo {} \; -exec stat -c %y {} \;
