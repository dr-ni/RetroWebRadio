#!/bin/sh
INFILE=stations.txt
OUTFILE=stations.xml

NUM_STATIONS=`wc -l $INFILE | cut -d ' ' -f 1`
NUM_PAGES=`echo "(${NUM_STATIONS} + 7)/8" | bc`

# echo "Got ${NUM_STATIONS} stations, ${NUM_PAGES} Pages"

echo '<?xml version="1.0" encoding="utf-8"?>'
echo '<global>'
echo '<pages>'${NUM_PAGES}'</pages>'

# STATION_IDX=1
PAGE_IDX=1     # Index OF page (starting with 1)
IDX_IN_PAGE=1  # Station index IN page (starting with 1)

while read ST; do
# Check whether to start new page
  if test $IDX_IN_PAGE = "1"; then
    echo '  <p'$PAGE_IDX'>'
    if test $NUM_STATIONS -lt 8;  then
      echo '    <count>'${NUM_STATIONS}'</count>'
    else
      echo '    <count>8</count>'
    fi
  fi

#  ID=`wget -O - http://www.radio-browser.info/webservice/xml/stations/byname/${ST} | xml2 | grep "id=" | head -n 1 | cut -d '=' -f 2` 2>> /dev/null 
#  URL=`wget -O - "http://www.radio-browser.info/webservice/v2/m3u/url/${ID}" | grep -v '^#'` 2>> /dev/null
  URL=`./getstationbyname.sh ${ST}` 2>> /dev/null
  echo '    <s'$IDX_IN_PAGE'>'
  echo '      <name>'${ST}'</name>'
  echo '      <url>'${URL}'</url>'
  echo '    </s'$IDX_IN_PAGE'>'

  IDX_IN_PAGE=`echo "$IDX_IN_PAGE + 1" | bc`
  NUM_STATIONS=`echo "$NUM_STATIONS - 1" | bc`

  if test ${NUM_STATIONS} = 0; then
    echo '  </p'$PAGE_IDX'>'
  else 
    if test ${IDX_IN_PAGE} = 9; then
      echo '  </p'$PAGE_IDX'>'
      PAGE_IDX=`echo "$PAGE_IDX + 1" | bc`
      IDX_IN_PAGE=1
    fi
  fi


done < $INFILE

echo '</global>'
