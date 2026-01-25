#!/bin/bash

#deps:
# wget
# xml2
# lynx

devel_id=ia9p4XYXmOPEtXzL
name=$1
anzahl=10
# option check
if [ $# -lt 1 ]
then
echo "Usage: ./$0 name"
exit 1
fi
#echo "http://api.shoutcast.com/legacy/stationsearch?k=${devel_id}&search=${name}&limit=0,${anzahl}"
wget -qO list.xml "http://api.shoutcast.com/legacy/stationsearch?k=${devel_id}&search=${name}&limit=0,${anzahl}"
xml2 < list.xml | grep id= | sed 's/.*=//' >idlist.txt


# Eingabedatei
INPUTDATEI=idlist.txt
# Anzahl der Zeilen der Eingabedatei
NZEIL=$(wc -l ${INPUTDATEI} | awk ' // { print $1; } ')
# echo "$NZEIL lines"
[ $NZEIL -eq 0 ] && echo "none"
# jede Zeile der Eingabedatei abarbeiten
for LaufZeile in $(seq 1 ${NZEIL})
 do
  # Zeile aus Datei einlesen
  string1='lynx -dump yp.shoutcast.com/sbin/tunein-station.m3u?id='
  string2=$(sed -n "${LaufZeile}p" ${INPUTDATEI})
  $string1$string2  | grep -v "^#\|^$" | sort -r | tail -n 1
done
