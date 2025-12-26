#ifndef __WAV_FILE_H_
#define __WAV_FILE_H_

#ifdef __cplusplus
extern "C" {
#endif

struct my_riff
{
    unsigned int riff_chunk_id;
    unsigned int riff_chunk_size;
    unsigned int riff_format;
};

struct my_fmt
{
    unsigned int fmt_chunk_id;
    unsigned int fmt_chunk_size;
    unsigned short fmt_audio_format;
    unsigned short fmt_num_channels;
    unsigned int fmt_sample_rate;
    unsigned int fmt_byte_rate;
    unsigned short fmt_block_align;
    unsigned short fmt_bits_per_sample;
};

struct my_data
{
    unsigned int data_chunk_id;
    unsigned int data_chunk_size; 
};


struct my_wave_file_headers{
    struct my_riff st_riff;
    struct my_fmt st_fmt;
    struct my_data st_data;
};

void wav_start_write(FILE* fp, struct my_wave_file_headers *fileheader, int bitofsample, int sample_chn, int sample_rate);
void wav_stop_write(FILE* fp, struct my_wave_file_headers *fileheader, int data_size);
void parse_wav_file_para(const char *path, int *ch, int *fmt, int *rate);

#ifdef __cplusplus
}
#endif
#endif