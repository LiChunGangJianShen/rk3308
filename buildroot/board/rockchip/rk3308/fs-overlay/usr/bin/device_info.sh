cmd=$1

if [ "$cmd"x = ""x ]; then
    echo "Usage: $0 <cmd>"
    exit 1
fi

if [ $cmd = sn ] ; then
    result=`sn_read sn`
    sn="KSA1020406000000"
    version=`cat /etc/version | awk -F' ' '{print $1F}' | sed -n '1p' | awk -F'=' '{print $2F}'`
    if [ -n "$result" ]; then
        sn=$result
    fi
    sn="${sn}${version}"
    echo "$sn"
elif [ $cmd = uac_name ] ; then
    result=`sn_read uac_name`
    if [ -n "$result" ]; then
        echo "$result"
    else
        echo "SoundMatrix A10"
    fi
elif [ $cmd = chip ] ; then
    result=`chipid_read`
    echo "$result"
fi
