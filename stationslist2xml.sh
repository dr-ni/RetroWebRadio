#!/bin/sh
# Convert a station list into the stations.xml used by roehre.
#
# usage: stationslist2xml.sh [-n stations_per_page] [stationslist.txt] > stations.xml
#
# Input format, one station per line:
#   [Name] http://stream.url/...
# Ignored: empty lines, lines starting with '#', and comment lines
# containing '§' (e.g. "§ Jazz stations").
# Names and URLs are XML-escaped, so '&', '<', '>' are fine.

PER_PAGE=20
while getopts n: opt; do
  case $opt in
    n) PER_PAGE=$OPTARG ;;
    *) echo "usage: $0 [-n stations_per_page] [stationslist.txt]" >&2; exit 1 ;;
  esac
done
shift $((OPTIND - 1))
INFILE=${1:-stationslist.txt}

case $PER_PAGE in
  ''|*[!0-9]*|0) echo "$0: -n needs a positive number" >&2; exit 1 ;;
esac
[ -r "$INFILE" ] || { echo "$0: cannot read $INFILE" >&2; exit 1; }

awk -v per_page="$PER_PAGE" -v infile="$INFILE" '
function esc(s) {
  gsub(/&/, "\\&amp;", s); gsub(/</, "\\&lt;", s); gsub(/>/, "\\&gt;", s)
  return s
}
function trim(s) { sub(/^[ \t\r]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
{
  line = trim($0)
  if (line == "" || line ~ /^#/ || index(line, "§") > 0) next
  if (line !~ /^\[[^]]+\][ \t]+[^ \t]+$/) {
    printf("%s:%d: ignored, expected \"[Name] URL\": %s\n", infile, NR, line) > "/dev/stderr"
    next
  }
  n++
  name[n] = trim(substr(line, 2, index(line, "]") - 2))
  url[n]  = trim(substr(line, index(line, "]") + 1))
}
END {
  if (n == 0) { print infile ": no stations found" > "/dev/stderr"; exit 1 }
  pages = int((n + per_page - 1) / per_page)
  print "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
  print "<global>"
  print "<pages>" pages "</pages>"
  for (p = 1; p <= pages; p++) {
    first = (p - 1) * per_page + 1
    last = first + per_page - 1
    if (last > n) last = n
    print "  <p" p ">"
    print "    <count>" (last - first + 1) "</count>"
    for (i = first; i <= last; i++) {
      s = i - first + 1
      print "    <s" s ">"
      print "      <name>" esc(name[i]) "</name>"
      print "      <url>" esc(url[i]) "</url>"
      print "    </s" s ">"
    }
    print "  </p" p ">"
  }
  print "</global>"
}' "$INFILE"
