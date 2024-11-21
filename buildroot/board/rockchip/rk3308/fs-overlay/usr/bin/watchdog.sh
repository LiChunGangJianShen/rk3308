num=`ps -A | grep event_poll | grep -v grep | wc -l`
if [ "0" = $num ]; then
    /etc/init.d/S60event_poll restart
fi
num=`ps -A | grep event_manager | grep -v grep | wc -l`
if [ "0" = $num ]; then
    /etc/init.d/S61event_manager restart
fi
num=`ps -A | grep audio_engine | grep -v grep | wc -l`
if [ "0" = $num ]; then
    /etc/init.d/S46audio_engine restart
fi
