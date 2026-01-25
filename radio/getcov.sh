#!/bin/bash
/home/radio/xremotemouse/xremotemouse t
while :
do
rm -f /home/radio/img.*
str=\"$(mpc current |  awk -F ': ' '{print $2}' | sed 's/ - /\" \"/g' | sed 's/ \/ /\" \"/g' )\"
str1=$(mpc current |  awk -F ': ' '{print $2}' | awk -F  ' - ' '{print $1}' | awk -F  ' \/ ' '{print $1}')
str2=$(mpc current |  awk -F ': ' '{print $2}' | awk -F  ' - ' '{print $2}')
[ -z "$str2" ] && str2=$(mpc current |  awk -F ': ' '{print $2}' | awk -F  ' \/ ' '{print $2}')
echo $str
convert -pointsize 20 -fill white /home/radio/radio/master-orig.png -gravity north -annotate +0+20 "${str1:0:40}" -pointsize 20 -gravity south -annotate +0+5 "${str2:0:40}" /home/radio/radio/master.png
eval "sudo -u radio sacad $str 200 img.jpg"
sleep 20
if [ -f /home/radio/img.jpg ]
then
convert /home/radio/img.jpg -resize 210x210 /home/radio/img.png
convert -gravity center /home/radio/radio/master-black.png /home/radio/img.png -composite /home/radio/radio/master.png
#/home/radio/xremotemouse/xremotemouse
fi
sleep 30
mate-screensaver-command -p
done
