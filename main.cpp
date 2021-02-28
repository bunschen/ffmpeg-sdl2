// 3-FFmpeg-SDL视频播放器.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL.h"
}

using namespace std;

char Video_FileName[] = "/Users/rlchen/CLionProjects/sdl_test/Titanic.ts";	// 播放的文件名


//初始化 FFmpeg   ///
AVFormatContext* v_pFormatCtx = NULL;

AVStream* video_st = NULL, * audio_st = NULL;
AVCodec* video_dec = NULL, * audio_dec = NULL;
AVCodecContext* video_dec_ctx = NULL, * audio_dec_ctx = NULL;

unsigned char* frame_out_buffer;
AVFrame* p_frame = NULL, * p_frame_yuv = NULL;
AVPacket* p_pkt = NULL;

struct SwsContext* video_sws_ctx;
int st_index[AVMEDIA_TYPE_NB];
int video_frame_cout = 0, audio_frame_cout = 0;


char frame_type[][3] = {"N", "I", "P", "B", "S" ,"SI", "SP", "BI"};

int ffmpeg_init(void)
{
    //avformat_network_init();

    // 1. 打开文件，分配AVFormatContext	结构体上下文
    v_pFormatCtx = avformat_alloc_context();	// 初始化上下文
    if (avformat_open_input(&v_pFormatCtx, Video_FileName, NULL, NULL) < 0) {
        cout << "打开 " << Video_FileName << " 文件失败!!!" << endl;
        return -1;
    }
    // 2. 查找文件中的流信息
    if (avformat_find_stream_info(v_pFormatCtx, NULL) < 0) {
        cout << "avformat_alloc_context Faild!!!" << endl;
        return -1;
    }

    // 3. 打印流信息
    av_dump_format(v_pFormatCtx, 0, Video_FileName, 0);

    memset(st_index, -1, sizeof(st_index));

    // 4. 找到视频流 与 音频流的 index索引
    for (unsigned int i = 0; i < v_pFormatCtx->nb_streams; i++)
    {
        AVMediaType type = v_pFormatCtx->streams[i]->codecpar->codec_type;
        if (type >= 0 && st_index[type] == -1) {
            st_index[type] = av_find_best_stream(v_pFormatCtx, type, -1, -1, NULL, 0);
            cout << "找到 st_index[" << type << "] = " << st_index[type] << endl;
        }
    }

    // 5. 检查是否存在流信息
    for (unsigned int i = 0; ; i++) {
        if (i < v_pFormatCtx->nb_streams && st_index[i] >= 0) {
            break;
        }
        else {
            cout << "未找到流信息" << endl;
        }
    }

    // 6. 找到视频流的 codec, 初始化并打开解码器
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        // 获得视频流
        video_st = v_pFormatCtx->streams[st_index[AVMEDIA_TYPE_VIDEO]];
        // 找到视频解码器
        video_dec = avcodec_find_decoder(video_st->codecpar->codec_id);
        // 初始化视频解码器上下文
        video_dec_ctx = avcodec_alloc_context3(video_dec);
        // 拷贝解码器信息到上下文中
        avcodec_parameters_to_context(video_dec_ctx, video_st->codecpar);
        // 打开视频解码器
        if (avcodec_open2(video_dec_ctx, video_dec, NULL) < 0) {
            cout << "Open Codec Faild\n";
        }
        cout << "打开视频解码器： " << video_dec->name << " (" << video_dec->long_name << ")\n";
    }

    // 7. 找到音频流的 codec, 初始化并打开解码器
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        // 获得音频流
        audio_st = v_pFormatCtx->streams[st_index[AVMEDIA_TYPE_AUDIO]];
        // 找到音频解码器
        audio_dec = avcodec_find_decoder(audio_st->codecpar->codec_id);
        // 初始化音频解码器上下文
        audio_dec_ctx = avcodec_alloc_context3(audio_dec);
        // 拷贝解码器信息到上下文中
        avcodec_parameters_to_context(audio_dec_ctx, audio_st->codecpar);
        // 打开音频解码器
        if (avcodec_open2(audio_dec_ctx, audio_dec, NULL) < 0) {
            cout << "Open Codec Faild\n";
        }
        cout << "打开音频解码器： " << audio_dec->name << " (" << audio_dec->long_name << ")\n";
    }

    // 8. 分配AVFrame 和 AVPacket 内存
    p_frame = av_frame_alloc();	// 初始化 AVFrame 帧内存

    // 申请 视频 buffer
    p_frame_yuv = av_frame_alloc();
    frame_out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, video_dec_ctx->width, video_dec_ctx->height, 1));
    av_image_fill_arrays(p_frame_yuv->data, p_frame_yuv->linesize, frame_out_buffer,
                         AV_PIX_FMT_YUV420P, video_dec_ctx->width, video_dec_ctx->height, 1);

    video_sws_ctx = sws_getContext(video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
                                   video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    // 初始化 AVPacket 包内存
    p_pkt = av_packet_alloc();
    av_init_packet(p_pkt);
    p_pkt->data = NULL;
    p_pkt->size = 0;

    return 0;
}


SDL_Window* s_window;
SDL_Renderer* s_renderer;
SDL_Texture* s_texture;
SDL_Rect s_rect;
int screen_w = 0, screen_h = 0;

int sdl_init(void)
{
    // 8. 初始化 SDL， 支持音频、视频、定时器
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        cout << "初始化SDL 失败 - " << SDL_GetError() << endl;
        return -1;
    }

    screen_w = video_dec_ctx->width;
    screen_h = video_dec_ctx->height;

    // 9. 创建SDL 窗口(显示位置由系统决定，窗口大小为视频大小)
    s_window = SDL_CreateWindow("FFmpeg 视频解码器", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                screen_w, screen_h, SDL_WINDOW_OPENGL);
    if (!s_window) {
        cout << "SDL 窗口创建失败 - " << SDL_GetError() << endl;
    }

    // 10. 初始化 SDL_Renderer 渲染器, 用于渲染 Texture 到 SDL_Windows 中
    s_renderer = SDL_CreateRenderer(s_window, -1, 0);

    // 11. 初始化SDL_Texture， 用于显示YUV数据
    // SDL_PIXELFORMAT_IYUV: 格式为 Y + U + V
    // SDL_PIXELFORMAT_YV12: 格式为 Y + V + U
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, screen_w, screen_h);

    s_rect.x = 0;
    s_rect.y = 0;
    s_rect.w = screen_w;
    s_rect.h = screen_h;

    SDL_Event event;
    if (SDL_PollEvent(&event)) {
        printf("event is %#x\n", event.type); // test code
        if (SDL_QUIT == event.type) {
            cout << "SDL2 quit" << endl;
        }
    }

    return 0;
}



int decode_and_play() {
    int ret = 0;

    //FILE* audio_dst_filename;
    //fopen_s(&audio_dst_filename, "audio.aac", "wb");

    // 读取一帧数据
    while (av_read_frame(v_pFormatCtx, p_pkt) >= 0) {
        if (p_pkt->stream_index == st_index[AVMEDIA_TYPE_VIDEO]) {
            // 解码视频帧
            ret = avcodec_send_packet(video_dec_ctx, p_pkt);

            while (ret >= 0) {
                // 接收解码后的数据
                ret = avcodec_receive_frame(video_dec_ctx, p_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    ret = 0;
                    break;
                }
                // 复制解码后的YUV数据到 p_frame_yuv 中
                sws_scale(video_sws_ctx, (const unsigned char* const*)p_frame->data, p_frame->linesize, 0,
                          video_dec_ctx->height, p_frame_yuv->data, p_frame_yuv->linesize);

                // 更新 YUV数据到 Texture 中
                SDL_UpdateTexture(s_texture, &s_rect, p_frame_yuv->data[0], p_frame_yuv->linesize[0]);

                // 清除 Rendder
                SDL_RenderClear(s_renderer);

                // 复制Texture 到 renderer渲染器中
                SDL_RenderCopy(s_renderer, s_texture, NULL, &s_rect);

                // 显示
                SDL_RenderPresent(s_renderer);

                cout << "视频：\t第 " << video_frame_cout++ << " 帧  \tpkt_size = " << p_frame->pkt_size << "  \t" << frame_type[p_frame->pict_type] << "帧 " << p_frame->pict_type << endl;
            }

        }
        else if (p_pkt->stream_index == st_index[AVMEDIA_TYPE_AUDIO]) {
            // 解码音频帧
            ret = avcodec_send_packet(audio_dec_ctx, p_pkt);
            while (ret >= 0) {
                ret = avcodec_receive_frame(audio_dec_ctx, p_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                {
                    ret = 0;
                    break;
                }
                //ret = (int)fwrite(p_frame->extended_data[0], 1, p_frame->nb_samples * av_get_bytes_per_sample(audio_dec_ctx->sample_fmt), audio_dst_filename);

                cout << "音频：\t第 " << audio_frame_cout++ << " 帧   \tpkt_size = " << p_frame->pkt_size << "  \t\t\taudio_size   = " << p_frame->nb_samples * av_get_bytes_per_sample(audio_dec_ctx->sample_fmt) << endl;
            }
        }
        av_frame_unref(p_frame);
        av_packet_unref(p_pkt);
        SDL_Delay(30);
    }
    cout << "读取数据完毕" << endl;

    // 读取文件结束，开始播放 解码器中未播放的数据
    while (1) {
        // 接收解码后的数据
        ret = avcodec_receive_frame(video_dec_ctx, p_frame);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
        {
            ret = 0;
            break;
        }
        // 复制解码后的YUV数据到 p_frame_yuv 中
        sws_scale(video_sws_ctx, (const unsigned char* const*)p_frame->data, p_frame->linesize, 0,
                  video_dec_ctx->height, p_frame_yuv->data, p_frame_yuv->linesize);

        // 更新 YUV数据到 Texture 中
        SDL_UpdateTexture(s_texture, &s_rect, p_frame_yuv->data[0], p_frame_yuv->linesize[0]);

        // 清除 Rendder
        SDL_RenderClear(s_renderer);

        // 复制Texture 到 renderer渲染器中
        SDL_RenderCopy(s_renderer, s_texture, NULL, &s_rect);

        // 显示
        SDL_RenderPresent(s_renderer);

        // Delay 30ms
        SDL_Delay(30);
    }

    //fclose(audio_dst_filename);
    return 0;
}


void sdl_clear(void)
{
    SDL_Quit();

    if (s_texture)
        SDL_DestroyTexture(s_texture);
    if (s_renderer)
        SDL_DestroyRenderer(s_renderer);
    if (s_window)
        SDL_DestroyWindow(s_window);
}

void ffmpeg_clear(void)
{
    if (video_sws_ctx)
        sws_freeContext(video_sws_ctx);
    if (frame_out_buffer)
        av_free(frame_out_buffer);
    if (p_frame_yuv)
        av_frame_free(&p_frame_yuv);	// 释放 frame 内存
    if (p_frame)
        av_frame_free(&p_frame);	// 释放 frame 内存
    if (p_pkt)
        av_packet_free(&p_pkt);	// 释放 packet 内存

    if (audio_dec_ctx) {
        avcodec_close(audio_dec_ctx);			// 关闭解码器
        avcodec_free_context(&audio_dec_ctx);	// 释放解码器上下文
    }

    if (video_dec_ctx) {
        avcodec_close(video_dec_ctx);			// 关闭解码器
        avcodec_free_context(&video_dec_ctx);	// 释放解码器上下文
    }
    if (v_pFormatCtx) {
        avformat_close_input(&v_pFormatCtx);	// 关闭文件
        avformat_free_context(v_pFormatCtx); // 释主 avformat 上下文
    }
}


int main(int argc, char* argv[])
{
    // 初始化 FFmpeg
    if (ffmpeg_init() != 0) {
        goto ffmpeg_faild;
    }

    // 初始化 SDL
    if (sdl_init() != 0) {
        goto sdl_faild;
    }

    // 解码 并 播放
    decode_and_play();

    cout << "播放完毕" << endl;
sdl_faild:
    sdl_clear();	// 释放 sdl 相关内存

ffmpeg_faild:
    ffmpeg_clear();	// 释放 ffmpeg 相关内存

    return 0;
}