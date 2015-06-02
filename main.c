#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#define SDL_AUDIO_BUFFER_SIZE 1024
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

int quit = 0;

int main(int argc, char* argv[])
{
    AVFormatContext *pFormatCtx;
    int i, videoindex;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    SDL_Event event;

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx,argv[1],NULL,NULL)!=0)
    {
        printf("Couldn't open input stream.\n");
        return -1;
    }

    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        printf("Couldn't find stream information.\n");
        return -1;
    }


    /*
     * Find Video Video Stream
     */
    videoindex = -1;

    for(i=0; i<pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
                && videoindex < 0) {
            videoindex = i;
        }
    }

    if(videoindex==-1) {
        printf("Didn't find a video stream %d.\n", videoindex);
        return -1;
    }


    /*
     * Open Video Codec
     */
    pCodecCtx = pFormatCtx->streams[videoindex]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

    if(pCodec==NULL){
        printf("Codec not found.\n");
        return -1;
    }

    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        printf("Could not open codec.\n");
        return -1;
    }

    AVFrame	*pFrame, *pFrameRGB;
    pFrame = avcodec_alloc_frame();
    pFrameRGB = avcodec_alloc_frame();

    uint8_t *out_buffer = (uint8_t *)av_malloc(
            avpicture_get_size(PIX_FMT_RGB24,
                pCodecCtx->width,
                pCodecCtx->height));

    avpicture_fill((AVPicture *)pFrameRGB,
            out_buffer,
            PIX_FMT_RGB24,
            pCodecCtx->width,
            pCodecCtx->height);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }


    /*
     * Initialize SDL
     */
    int screen_w=0, screen_h=0;
    SDL_Window *screen;
    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;
    screen = SDL_CreateWindow("Simplest Video Player",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              screen_w,
                              screen_h,
                              SDL_WINDOW_OPENGL);

    if(!screen) {
        printf("SDL: could not create window - exiting:%s\n",SDL_GetError());
        return -1;
    }

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRenderer,
                                SDL_PIXELFORMAT_RGB24,
                                SDL_TEXTUREACCESS_STREAMING,
                                pCodecCtx->width,
                                pCodecCtx->height);

    SDL_Rect sdlRect;
    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = screen_w;
    sdlRect.h = screen_h;

    int ret, got_picture;
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    // Output file information
    printf("File Information\n");
    av_dump_format(pFormatCtx,0,argv[1],0);

    struct SwsContext *img_convert_ctx;
    img_convert_ctx = sws_getContext(pCodecCtx->width,
                                     pCodecCtx->height,
                                     pCodecCtx->pix_fmt,
                                     pCodecCtx->width,
                                     pCodecCtx->height,
                                     PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);

    int m = 0;
    while(1)
    {
        if (av_read_frame(pFormatCtx, packet) < 0){
            break;
        }

        if(packet->stream_index == videoindex)
        {
            ret = avcodec_decode_video2(pCodecCtx, pFrame,
                    &got_picture, packet);

            if(ret < 0) {
                printf("Decode Error.\n");
                return -1;
            }

            if(got_picture){

                sws_scale(img_convert_ctx,
                          (const uint8_t* const*)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGB->data, pFrameRGB->linesize);

               // if (m < 10)
               //     SaveFrame(pFrameRGB, pFrame->width, pFrame->height, m);

                SDL_UpdateTexture(sdlTexture, &sdlRect,
                        pFrameRGB->data[0], pFrameRGB->linesize[0]);
                SDL_RenderClear(sdlRenderer);
                SDL_RenderCopy(sdlRenderer, sdlTexture, &sdlRect, &sdlRect);

                SDL_RenderPresent(sdlRenderer);

            }
        }

        SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            SDL_Quit();
            exit(0);
            break;
        default:
            break;
        }
    }
    sws_freeContext(img_convert_ctx);

    SDL_Quit();

    av_free_packet(packet);
    av_free(out_buffer);
    av_free(pFrameRGB);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int  y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width * 3, pFile);

  // Close file
  fclose(pFile);
}
