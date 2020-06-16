#include <iostream>
#include <cstdio>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
using namespace std;

static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, char *filename)
{
    FILE *f;
    int i;
    f = fopen(filename,"w");
    if(nullptr == f)
    {
        cout<<"fopen error\n"<<endl;
        return ;
    }
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

int ffmpeg_help()
{
    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        cout<<"ERROR could not allocate memory for Format Context"<<endl;
        return -1;
    }
    if(avformat_open_input(&pFormatContext, "../resources/yuan.mp4", nullptr, nullptr) != 0)
    {
        cout<<"error"<<endl;
        return -1;
    }
    cout<<"Format:"<<pFormatContext->iformat->long_name<<endl;
    //printf("duration: %lld\n", pFormatContext->duration);

    if(avformat_find_stream_info(pFormatContext, nullptr) < 0)
    {
        cout<<"avformat_find_stream_info error!\n"<<endl;
        return -1;
    }

    int videoIndex = -1;
    int audioIndex = -1;
    for(int i = 0; i < pFormatContext->nb_streams; i++)
    {
        if(pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
            break;
        }
//        else if(pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO)
//        {
//            audioIndex = i;
//            cout<<"channels:"<<pLocalCodecParameters->channels<<endl;
//        }
    }

    if( -1 == videoIndex)
    {
        cout<<"videoIndex error!\n"<<endl;
        return -1;
    }

    //获取video的编码器
    AVCodecParameters *pCodecParameters = pFormatContext->streams[videoIndex]->codecpar;
    AVCodec *pCodec = avcodec_find_decoder(pCodecParameters->codec_id);
    if(nullptr == pCodec)
    {
        cout<<"avcodec_find_decoder error!\n"<<endl;
        return -1;
    }

    AVCodecContext  *pCodecContext = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecContext, pCodecParameters);
    if(avcodec_open2(pCodecContext, pCodec, nullptr))
    {
        cout<<"avcodec_open2 error!\n"<<endl;
        return -1;
    }
    AVFrame *pFrame = av_frame_alloc();
    AVPacket *pPacket = av_packet_alloc();

    while(av_read_frame(pFormatContext, pPacket) >= 0)
    {
        if(pPacket->stream_index == videoIndex)
        {
            int response = avcodec_send_packet(pCodecContext, pPacket);
            if(response < 0)
            {
                cout<<"avcodec_send_packet error\n"<<endl;
                break;
            }
            response = avcodec_receive_frame(pCodecContext, pFrame);
            if(response < 0)
            {
                cout<<"avcodec_receive_frame error\n"<<endl;
                break;
            }
            cout<<"type:"<<av_get_picture_type_char(pFrame->pict_type)<<endl;
            cout<<"frame_number: "<<pCodecContext->frame_number<<endl;
            cout<<"pts: "<<pFrame->pts<<endl;
            cout<<"pkt_dts: "<<pFrame->pkt_dts<<endl;
            cout<<"key_frame: "<<pFrame->key_frame<<endl;
            cout<<"coded_picture_number: "<<pFrame->coded_picture_number<<endl;
            cout<<"display_picture_number: "<<pFrame->display_picture_number<<endl;

            char frame_filename[1024];
            snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", pCodecContext->frame_number);
            save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
            break;
        }
        av_packet_unref(pPacket);
    }

    avformat_close_input(&pFormatContext);
    av_packet_free(&pPacket);
    av_frame_free(&pFrame);
    avcodec_free_context(&pCodecContext);

}

int remux(char* in_filename, char* out_filename)
{
    AVFormatContext *input_format_context = nullptr;
    AVFormatContext *output_format_context = nullptr;
    AVPacket packet;

    if(0 != avformat_open_input(&input_format_context, in_filename, nullptr, nullptr))
    {
        std::cout<<"remuxing avformat_open_input error\n"<<std::endl;
        return -1;
    }
    if(avformat_find_stream_info(input_format_context, nullptr) < 0)
    {
        std::cout<<"remuxing avformat_find_stream_info error\n"<<std::endl;
        return -1;
    }

    avformat_alloc_output_context2(&output_format_context, nullptr, nullptr, out_filename);
    if(nullptr == output_format_context)
    {
        std::cout<<"output_format_context is null\n"<<std::endl;
        return -1;
    }

    int number_streams = input_format_context->nb_streams;
    for(int i = 0; i < number_streams; i++)
    {
        AVStream *out_stream = nullptr;
        AVStream *in_stream = nullptr;
        in_stream = input_format_context->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if(!in_stream &&
           in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
           in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
           in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            continue;
        }

        out_stream = avformat_new_stream(output_format_context, nullptr);
        if(nullptr == out_stream) {
            std::cout << "avformat_new_stream error\n" << std::endl;
            return -1;
        }
        avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    }

    //print format
    av_dump_format(output_format_context, 0, out_filename, 1);
    if(!(output_format_context->oformat->flags & AVFMT_NOFILE))
    {
        avio_open(&output_format_context->pb, out_filename, AVIO_FLAG_WRITE);
    }
    avformat_write_header(output_format_context, nullptr);
    while(true)
    {
        AVStream *in_stream = nullptr;
        AVStream *out_stream = nullptr;
        av_read_frame(input_format_context, &packet);
        in_stream = input_format_context->streams[packet.stream_index];
        out_stream = output_format_context->streams[packet.stream_index];
        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        packet.pos = -1;
        av_interleaved_write_frame(output_format_context, &packet);
        av_packet_unref(&packet);
    }
    av_write_trailer(output_format_context);
    end:
    avformat_close_input(&input_format_context);
    /* close output */
    if (output_format_context && !(output_format_context->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_format_context->pb);
    avformat_free_context(output_format_context);
    return 0;
}


int main()
{
//    ffmpeg_help();
    remux("../resources/yuan.mp4", "../resources/yuan.ts");
    return 0;
}