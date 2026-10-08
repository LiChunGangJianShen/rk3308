#!/bin/sh

# if [ ! -e "$/data/acodec_config.xml" ]; then
	# echo "use default acodec_config.xml"
	# cp -f /usr/share/acodec_config.xml /data/
# fi

# CONFIG_FILE=/data/acodec_config.xml
CONFIG_FILE=/usr/share/acodec_config.xml

ACODEC_CARD=0

MIC1_SWITCH=$(grep 'mic1_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC2_SWITCH=$(grep 'mic2_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC3_SWITCH=$(grep 'mic3_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC4_SWITCH=$(grep 'mic4_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC5_SWITCH=$(grep 'mic5_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC6_SWITCH=$(grep 'mic6_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC7_SWITCH=$(grep 'mic7_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC8_SWITCH=$(grep 'mic8_switch' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

MIC1_VOLUME=$(grep 'mic1_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC2_VOLUME=$(grep 'mic2_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC3_VOLUME=$(grep 'mic3_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC4_VOLUME=$(grep 'mic4_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC5_VOLUME=$(grep 'mic5_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC6_VOLUME=$(grep 'mic6_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC7_VOLUME=$(grep 'mic7_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
MIC8_VOLUME=$(grep 'mic8_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

# ADC_ALC_LEFT_0=$(grep 'adc_alc_left_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_LEFT_1=$(grep 'adc_alc_left_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_LEFT_2=$(grep 'adc_alc_left_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_LEFT_3=$(grep 'adc_alc_left_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_RIGHT_0=$(grep 'adc_alc_right_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_RIGHT_1=$(grep 'adc_alc_right_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_RIGHT_2=$(grep 'adc_alc_right_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ADC_ALC_RIGHT_3=$(grep 'adc_alc_right_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

# ALC_AGC_SWITCH_LEFT_0=$(grep 'alc_agc_switch_left_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_LEFT_1=$(grep 'alc_agc_switch_left_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_LEFT_2=$(grep 'alc_agc_switch_left_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_LEFT_3=$(grep 'alc_agc_switch_left_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_RIGHT_0=$(grep 'alc_agc_switch_right_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_RIGHT_1=$(grep 'alc_agc_switch_right_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_RIGHT_2=$(grep 'alc_agc_switch_right_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_SWITCH_RIGHT_3=$(grep 'alc_agc_switch_right_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

# ALC_AGC_LEFT_0=$(grep 'alc_agc_left_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_LEFT_1=$(grep 'alc_agc_left_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_LEFT_2=$(grep 'alc_agc_left_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_LEFT_3=$(grep 'alc_agc_left_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_RIGHT_0=$(grep 'alc_agc_right_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_RIGHT_1=$(grep 'alc_agc_right_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_RIGHT_2=$(grep 'alc_agc_right_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
# ALC_AGC_RIGHT_3=$(grep 'alc_agc_right_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

ADC_HPF0=$(grep 'adc_hpf_0' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
ADC_HPF1=$(grep 'adc_hpf_1' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
ADC_HPF2=$(grep 'adc_hpf_2' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
ADC_HPF3=$(grep 'adc_hpf_3' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')


LINEOUT_HPOUT_ENABLE=$(grep 'lineout_hpout_enable' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

HPMIX_LEFT_VOLUME=$(grep 'hpmix_left_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
HPMIX_RIGHT_VOLUME=$(grep 'hpmix_right_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
HPOUT_LEFT_VOLUME=$(grep 'hpout_left_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
HPOUT_RIGHT_VOLUME=$(grep 'hpout_right_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
LINEOUT_LEFT_VOLUME=$(grep 'lineout_left_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')
LINEOUT_RIGHT_VOLUME=$(grep 'lineout_right_volume' $CONFIG_FILE | awk -F ">" '{print $2}' | awk -F "<" '{print $1}')

echo "====  start acodec config  ===="

# echo "config acodec adc mic switch"
amixer -c $ACODEC_CARD cset name='ADC MIC Group 0 Left Switch'  $MIC1_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 0 Right Switch' $MIC2_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 1 Left Switch'  $MIC3_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 1 Right Switch' $MIC4_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 2 Left Switch'  $MIC5_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 2 Right Switch' $MIC6_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 3 Left Switch'  $MIC7_SWITCH >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 3 Right Switch' $MIC8_SWITCH >> /dev/null

# echo "config acodec adc mic volume"
amixer -c $ACODEC_CARD cset name='ADC MIC Group 0 Left Volume'   $MIC1_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 0 Right Volume'  $MIC2_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 1 Left Volume'   $MIC3_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 1 Right Volume'  $MIC4_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 2 Left Volume'   $MIC5_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 2 Right Volume'  $MIC6_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 3 Left Volume'   $MIC7_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='ADC MIC Group 3 Right Volume'  $MIC8_VOLUME >> /dev/null

# echo "config acodec adc alc 0dB"
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 0 Left Volume'  $ADC_ALC_LEFT_0 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 1 Left Volume'  $ADC_ALC_LEFT_1 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 2 Left Volume'  $ADC_ALC_LEFT_2 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 3 Left Volume'  $ADC_ALC_LEFT_3 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 0 Right Volume' $ADC_ALC_RIGHT_0 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 1 Right Volume' $ADC_ALC_RIGHT_1 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 2 Right Volume' $ADC_ALC_RIGHT_2 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ADC ALC Group 3 Right Volume' $ADC_ALC_RIGHT_3 >> /dev/null

# echo "close alc agc"
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 0 Left Switch' $ALC_AGC_SWITCH_LEFT_0 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 1 Left Switch' $ALC_AGC_SWITCH_LEFT_1 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 2 Left Switch' $ALC_AGC_SWITCH_LEFT_2 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 3 Left Switch' $ALC_AGC_SWITCH_LEFT_3 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 0 Right Switch' $ALC_AGC_SWITCH_RIGHT_0 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 1 Right Switch' $ALC_AGC_SWITCH_RIGHT_1 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 2 Right Switch' $ALC_AGC_SWITCH_RIGHT_2 >> /dev/null
# amixer -c $ACODEC_CARD cset name='ALC AGC Group 3 Right Switch' $ALC_AGC_SWITCH_RIGHT_3 >> /dev/null

# echo "close all adc hpf cut-off"
amixer -c $ACODEC_CARD cset name='ADC Group 0 HPF Cut-off' $ADC_HPF0 > /dev/null
amixer -c $ACODEC_CARD cset name='ADC Group 1 HPF Cut-off' $ADC_HPF1 > /dev/null
amixer -c $ACODEC_CARD cset name='ADC Group 2 HPF Cut-off' $ADC_HPF2 > /dev/null
amixer -c $ACODEC_CARD cset name='ADC Group 3 HPF Cut-off' $ADC_HPF3 > /dev/null

# echo "enable hpout and lineout output"
# echo "dac path: $LINEOUT_HPOUT_ENABLE"
# echo $LINEOUT_HPOUT_ENABLE > /sys/devices/platform/ff560000.acodec/rk3308-acodec-dev/dac_output >> /dev/null

# echo "config hpmix volume"
amixer -c $ACODEC_CARD cset name='DAC HPMIX Left Volume'  $HPMIX_LEFT_VOLUME  >> /dev/null
amixer -c $ACODEC_CARD cset name='DAC HPMIX Right Volume' $HPMIX_RIGHT_VOLUME >> /dev/null
# echo "config lineout volume"
amixer -c $ACODEC_CARD cset name='DAC LINEOUT Left Volume' $LINEOUT_LEFT_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='DAC LINEOUT Right Volume' $LINEOUT_RIGHT_VOLUME >> /dev/null
# echo "config hpout volume"
amixer -c $ACODEC_CARD cset name='DAC HPOUT Left Volume' $HPOUT_LEFT_VOLUME >> /dev/null
amixer -c $ACODEC_CARD cset name='DAC HPOUT Right Volume' $HPOUT_RIGHT_VOLUME >> /dev/null

echo "====  end acodec config  ===="
