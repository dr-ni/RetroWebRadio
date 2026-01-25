#!/bin/bash
echo
/home/radio/radio/lpoti > /tmp/lpoti.log 2>&1 &
sleep 1
if pgrep -x "lpoti"
then
    echo
    echo "Running"
else
    echo
    echo "Stopped"
	echo "retry" >> /tmp/lpoti.log
        killall lpoti
        /home/radio/radio/lpoti >> /tmp/lpoti.log 2>&1 &
fi
sleep 1
/home/radio/radio/mpoti > /tmp/mpoti.log 2>&1 &
sleep 1
if pgrep -x "mpoti"
then
    echo
    echo "Running"
else
    echo
    echo "Stopped"
        echo "retry" >> /tmp/mpoti.log
        killall mpoti
        /home/radio/radio/mpoti >> /tmp/mpoti.log 2>&1 &
fi
sleep 1
/home/radio/radio/rpoti > /tmp/rpoti.log 2>&1 &
sleep 1
if pgrep -x "rpoti"
then
    echo
    echo "Running"
else
    echo
    echo "Stopped"
        echo "retry" >> /tmp/rpoti.log
        killall rpoti
        /home/radio/radio/rpoti >> /tmp/rpoti.log 2>&1 &
fi
echo "potid started"

mpc enable only 1 > /dev/null 2>&1 &
mpc volume 50 > /dev/null 2>&1 &
mpc play > /dev/null 2>&1 &
echo "radio started"

