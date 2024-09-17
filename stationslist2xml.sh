#!/bin/bash
cat stationslist.txt | sed -r '/(^\s*#)|(^\s*$)/d' > tmpstl
mv tmpstl stationslist.txt 
INFILE=stationslist.txt
OUTFILE=stations.xml

STATIONS_PER_PAGE=20

NUM_STATIONS=`wc -l $INFILE | cut -d ' ' -f 1`
NUM_PAGES=`echo "(${NUM_STATIONS})/$STATIONS_PER_PAGE" | bc`

#echo "Got ${NUM_STATIONS} stations, ${NUM_PAGES} Pages"

echo '<?xml version="1.0" encoding="utf-8"?>'
echo '<global>'
echo '<pages>'${NUM_PAGES}'</pages>'
# STATION_IDX=1
PAGE_IDX=1     # Index OF page (starting with 1)
IDX_IN_PAGE=1  # Station index IN page (starting with 1)

while read ST; do
  #echo "st=$ST"
  Comment=""
  [[ "${ST}" == "" ]] && read ST
  Comment=$(echo ${ST} | awk -F '§' '{print $2}')
  #echo "cm $Comment"
  [[ "$Comment" == "" ]] || read ST
  # Check whether to start new page
  if test $IDX_IN_PAGE == "1"; then
    echo '  <p'$PAGE_IDX'>'
    if test $NUM_STATIONS -lt $STATIONS_PER_PAGE;  then
      echo '    <count>'${NUM_STATIONS}'</count>'
    else
      echo '    <count>'$STATIONS_PER_PAGE'</count>'
    fi
  fi
  URL=$(echo ${ST} |  awk -F '] ' '{print $2}')
  STA=$(echo ${ST} |  awk -F '] ' '{print $1}' | tr -d '[')
  echo '    <s'$IDX_IN_PAGE'>'
  echo '      <name>'${STA}'</name>'
  echo '      <url>'${URL}'</url>'
  echo '    </s'$IDX_IN_PAGE'>'
  IDX_IN_PAGE=`echo "$IDX_IN_PAGE + 1" | bc`
  NUM_STATIONS=`echo "$NUM_STATIONS - 1" | bc`
  
  if test ${NUM_STATIONS} == 0; then
    echo '  </p'$PAGE_IDX'>'
  else 
    if test ${IDX_IN_PAGE} == `echo "$STATIONS_PER_PAGE +1" | bc`; then
      echo '  </p'$PAGE_IDX'>'
      PAGE_IDX=`echo "$PAGE_IDX + 1" | bc`
      IDX_IN_PAGE=1
    fi
  fi
#echo $NUM_STATIONS
done < $INFILE
echo '</global>'
