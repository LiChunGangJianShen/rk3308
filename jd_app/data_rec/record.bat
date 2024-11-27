cd %~dp0

adb shell /usr/bin/data_rec record-stop
adb shell /usr/bin/data_rec record-action 60
adb shell sync

adb pull /data/mic-1.wav
adb pull /data/mic-2.wav
adb pull /data/mic-3.wav
adb pull /data/mic-4.wav
adb pull /data/mic-5.wav
adb pull /data/mic-6.wav
