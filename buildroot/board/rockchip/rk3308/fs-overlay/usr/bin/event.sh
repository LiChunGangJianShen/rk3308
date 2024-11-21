do_volup() {
    item=6
    curr=`amixer -c2  cget name='playback_softvol' | grep ": value" | awk -F ',' '{print $NF}'`
    if [ $curr -ge 99 ] && [ "x$1" = "xlongpress" ] ; then
        return;
    fi
    wav="/opt/audio/volup.wav"
    if [ $curr -ge 99 ] ; then
        wav="/opt/audio/voledge.wav"
    fi
    val=`expr $curr + $item`
    if [ $val -ge 99 ] ; then
        val=99
    fi
    amixer -q -c2 cset name='playback_softvol' $val
    aplay $wav 2> /dev/null &
}
do_voldown() {
    item=6
    curr=`amixer -c2  cget name='playback_softvol' | grep ": value" | awk -F ',' '{print $NF}'`
    if [ $curr -le 0 ] && [ "x$1" = "xlongpress" ] ; then
        return;
    fi
    val=`expr $curr - $item`
    wav="/opt/audio/voldown.wav"
    if [ $val -le 0 ] ; then
        val=0
        # wav="/opt/audio/voledge.wav"
    fi
    amixer -q -c2 cset name='playback_softvol' $val
    aplay $wav 2> /dev/null &
}

do_setvol() {
    amixer -q -c2 cset name='playback_softvol' $1
}
do_getvol() {
    curr=`amixer -c2  cget name='playback_softvol' | grep ": value" | awk -F ',' '{print $NF}'`
    echo "vol:$curr"
    return $curr
}
do_usb() {
    if [ $1 = connect ] ; then
        aplay /opt/audio/$1.wav 2> /dev/null &
        # hexdump /dev/hidg0 2> /dev/null &
    elif [ $1 = disconnect ] ; then
        aplay /opt/audio/$1.wav 2> /dev/null &
        # killall hexdump 2> /dev/null
    fi
}

event=$1

if [ $event = volup ] ; then
    do_volup $2
elif [ $event = voldown ] ; then
    do_voldown $2
elif [ $event = pickup ] ; then
    do_getvol
    curr=$?
    dev="default"
    if [ $curr -ge 48 ]; then
        dev=tonevol
    fi
    aplay -D$dev /opt/audio/pickup.wav 2> /dev/null
elif [ $event = hangup ] ; then
    do_getvol
    curr=$?
    dev="default"
    if [ $curr -ge 48 ]; then
        dev=tonevol
    fi
    aplay -D$dev /opt/audio/hangup.wav 2> /dev/null
elif [ $event = setvol ] ; then
    do_setvol $2
elif [ $event = getvol ] ; then
    do_getvol
elif [ $event = usb ] ; then
    do_usb $2
elif [ $event = mute ] ; then
    do_getvol
    curr=$?
    dev="default"
    if [ $curr -ge 48 ]; then
        dev=tonevol
    fi
    aplay -D$dev /opt/audio/mute.wav 2> /dev/null &
elif [ $event = unmute ] ; then
    do_getvol
    curr=$?
    dev="default"
    if [ $curr -ge 48 ]; then
        dev=tonevol
    fi
    aplay -D$dev /opt/audio/unmute.wav 2> /dev/null &
elif [ $event = "usb_connect_volume" ]; then
    sleep 2
    do_getvol
    curr=$?
    if [ $curr -lt 71 ]; then
        n=0
        if [ $curr -lt 25 ]; then
            n=8
        elif [ $curr -lt 35 ]; then
            n=7
        elif [ $curr -lt 43 ]; then
            n=6
        elif [ $curr -lt 50 ]; then
            n=5
        elif [ $curr -lt 56 ]; then
            n=4
        elif [ $curr -lt 61 ]; then
            n=3
        elif [ $curr -lt 66 ]; then
            n=2
        elif [ $curr -lt 71 ]; then
            n=1
        fi
        do_setvol 71
        # echo "vol sync n:$n"
        i=0
        while true
        do
            if [ $i -ge $n ]; then
                break
            fi
            mosquitto_pub -t event -m "sync_up"
            i=$(( i + 1))
        done
    fi
fi
