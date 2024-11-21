#include <stdio.h>
#include <string.h>
#include "wav_file.h"

#define RIFF    0x52494646  //"RIFF"
#define FMT     0x666D7420  //"fmt "  注意fmt后有一个空格
#define DATA    0x64617461  //"data"
#define WAVE    0x57415645  //"WAVE"


static int g_bits_per_sample = 0;
static int g_channels = 0;
static int g_sample_rate = 0;

static int my_host_to_net32(int num)
{
    int ret = 0;

    // printf("num = 0x%x\n", num);

    // printf("(((0xff << 0) & num) << 24) | (((0xff << 8) & num) << 8) | (((0xff << 16) & num) >> 8) | (((0xff << 24) & num) >> 24) = 0x%x\n",
    // ((((0xff << 0) & num) << 24) | (((0xff << 8) & num) << 8) | (((0xff << 16) & num) >> 8) | (((0xff << 24) & num) >> 24)));

    ret = ((((0xff << 0) & num) << 24) |
         (((0xff << 8) & num) << 8) | 
         (((0xff << 16) & num) >> 8) | 
         (((0xff << 24) & num) >> 24));

    return ret;
}

void wav_init_header(struct my_wave_file_headers *fileheader)
{
    unsigned int data_size = 0;

    //RIFF
    fileheader->st_riff.riff_chunk_id = my_host_to_net32(RIFF);
    fileheader->st_riff.riff_chunk_size = data_size + sizeof(struct my_wave_file_headers) - 24;
    fileheader->st_riff.riff_format = my_host_to_net32(WAVE);

    //fmt
    fileheader->st_fmt.fmt_chunk_id = my_host_to_net32(FMT);
    fileheader->st_fmt.fmt_chunk_size = 16;
    fileheader->st_fmt.fmt_audio_format = 1;  //PCM_FORMAT
    fileheader->st_fmt.fmt_num_channels = g_channels;
    fileheader->st_fmt.fmt_sample_rate = g_sample_rate;
    fileheader->st_fmt.fmt_bits_per_sample = g_bits_per_sample;
    fileheader->st_fmt.fmt_block_align = fileheader->st_fmt.fmt_num_channels*fileheader->st_fmt.fmt_bits_per_sample/8;
    fileheader->st_fmt.fmt_byte_rate = fileheader->st_fmt.fmt_sample_rate*fileheader->st_fmt.fmt_bits_per_sample*fileheader->st_fmt.fmt_num_channels/8;

    //data
    fileheader->st_data.data_chunk_id = my_host_to_net32(DATA);
    fileheader->st_data.data_chunk_size = 0;
}

void wav_start_write(FILE* fp, struct my_wave_file_headers *fileheader, int bitofsample, int sample_chn, int sample_rate)
{
    g_bits_per_sample = bitofsample;
    g_channels = sample_chn;
    g_sample_rate = sample_rate;
    wav_init_header(fileheader);
    fwrite(fileheader, 1, sizeof(struct my_wave_file_headers), fp);
}

void wav_stop_write(FILE* fp, struct my_wave_file_headers *fileheader, int data_size)
{
    //整个文件的长度减去ID和SIZE的长度， 可以用数据的长度+文件头的长度-文件头中ID和SIZE的长度
    unsigned long rcsize = data_size + sizeof(struct my_wave_file_headers) - 24;

    fileheader->st_riff.riff_chunk_size = rcsize;
    fileheader->st_data.data_chunk_size = data_size;
    fseek(fp, 0, SEEK_SET);
    fwrite(fileheader, 1, sizeof(struct my_wave_file_headers), fp);
}

void parse_wav_file_para(const char *path, int *ch, int *fmt, int *rate)
{
    if(!path){
        printf("invalid path\n");
        return;
    }

    FILE *fp = NULL;
    fp = fopen(path, "r");
    if(!fp){
        printf("open %s fail\n", path);
        return;
    }

    struct my_wave_file_headers fileheader;
    char buff_h[64] = {0};
    int ret = 0;

    memset(&fileheader, 0, sizeof(struct my_wave_file_headers));
    ret = fread(buff_h, sizeof(struct my_wave_file_headers), 1, fp);
    if(ret != sizeof(struct my_wave_file_headers)){
        printf("read wave file head faile\n");
    }
    else{
        printf("chn:%d, fmt:%d, rate:%d\n", 
            fileheader.st_fmt.fmt_num_channels, fileheader.st_fmt.fmt_bits_per_sample,fileheader.st_fmt.fmt_sample_rate);

        *ch = fileheader.st_fmt.fmt_num_channels;
        *fmt = fileheader.st_fmt.fmt_bits_per_sample;
        *rate = fileheader.st_fmt.fmt_sample_rate;
    }
    
    if(fp){
        fclose(fp);
        fp = NULL;
    }
}