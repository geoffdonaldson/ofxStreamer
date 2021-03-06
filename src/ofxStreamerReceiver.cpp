//
//  ofxStreamerReceiver.c
//  x264Example
//
//  Created by Johan Bichel Lindegaard on 4/18/13.
//
//


#include "ofxStreamerReceiver.h"

using namespace std;

void av_log_shd_callback(void* ptr, int level, const char* fmt, va_list vl)
{
    static int print_prefix=1;
    AVClass* avc= ptr ? *(AVClass**)ptr : NULL;
    //if(level>av_log_get_level())
    //    return;
#undef fprintf
    if(print_prefix && avc) {
        fprintf(stdout, "[%s @ %p]", avc->item_name(ptr), avc);
    }
#define fprintf please_use_av_log
    
    print_prefix= strstr(fmt, "\n") != NULL;
    
    vfprintf(stdout, fmt, vl);
    cout << "HEllo ErrROORO" << endl;
}


ofxStreamerReceiver::ofxStreamerReceiver(){
    bHavePixelsChanged = false;
    allocated = false;
    connected = false;
    newFrame = false;
    paused = false;
}

bool ofxStreamerReceiver::setup(int _port, string _host) {
    
    port = _port; host = _host;
    url = host;//  ":" + ofToString(port);
    ofLog(OF_LOG_NOTICE, "Opening stream at " + url);
    
    bHavePixelsChanged = false;
    allocated = false;
    connected = false;
    newFrame = false;
    paused = false;

    startThread(false,true);
    
    dead = false;
    
    lastFrame = new ofImage();
    lastFrame->allocate(1, 1, OF_IMAGE_COLOR);

    av_log_set_level(48);
    //av_log_set_callback(av_log_shd_callback);
    
    return true;
}


void ofxStreamerReceiver::threadedFunction(){
    context = avformat_alloc_context();
    ccontext = avcodec_alloc_context3(NULL);
    
    av_register_all();
    avformat_network_init();
    
    if(avformat_open_input(&context, url.c_str(),NULL,NULL) != 0){
        ofLog(OF_LOG_ERROR, "Could not open input.");
        connected = false;
        return;
    }
    context->max_delay = 0;
    cout << "AVInputFormat: " << context->iformat->name << endl;
    
    if(avformat_find_stream_info(context,NULL) < 0){
        ofLog(OF_LOG_ERROR, "Stream information not found.");
         connected = false;
        return;
    }
    
    //search video stream
    for(int i =0;i<context->nb_streams;i++){
        if(context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            video_stream_index = i;
    }
    
    av_init_packet(&packet);
    
    oc = avformat_alloc_context();
    
    stream = NULL;
    frameNum = 0;
    
    //start reading packets from stream and write them to file
    av_read_play(context);
    
    AVCodec *codec = NULL;
    codec = avcodec_find_decoder(CODEC_ID_H264);
    if (!codec) {
        ofLog(OF_LOG_FATAL_ERROR, "Codec not found.");
        exit(1);
    }
    
    avcodec_get_context_defaults3(ccontext, codec);
    avcodec_copy_context(ccontext,context->streams[video_stream_index]->codec);
    
    if (avcodec_open2(ccontext, codec, NULL) < 0) {
        ofLog(OF_LOG_FATAL_ERROR, "Could not open codec.");
        exit(1);
    }
    
    width = ccontext->width;
    height = ccontext->height;
    
    
    pixelData = (unsigned char*)malloc(sizeof(unsigned char)*width*height*3);
    
    for(int i=0;i<width*height*3;i++){
        pixelData[i] = 0;
    }
    
    img_convert_ctx = sws_getContext(width, height, ccontext->pix_fmt, width, height,
                                     PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
    
    int size = avpicture_get_size(PIX_FMT_YUV420P, width, height);
    picture_buf = (uint8_t*)(av_malloc(size));
    pic = avcodec_alloc_frame();
    picrgb = avcodec_alloc_frame();
    int size2 = avpicture_get_size(PIX_FMT_RGB24, width, height);
    picture_buf2 = (uint8_t*)(av_malloc(size2));
    avpicture_fill((AVPicture *
                    ) pic, picture_buf, PIX_FMT_YUV420P, width, height);
    avpicture_fill((AVPicture *) picrgb, picture_buf2, PIX_FMT_RGB24, width, height);
    
    
    connected = true;
    
    
    while(isThreadRunning()){
        
        if(paused) continue;
        
        if(&packet){
            av_free_packet(&packet);
            av_init_packet(&packet);
        }
        
        int readStatus = av_read_frame(context,&packet);
        
        if (readStatus == 0) {
                        
            if(packet.stream_index == video_stream_index){ //packet is video
                
                if(stream == NULL) {
                    // create stream
                    stream = avformat_new_stream(oc,context->streams[video_stream_index]->codec->codec);
                    avcodec_copy_context(stream->codec,context->streams[video_stream_index]->codec);
                    stream->sample_aspect_ratio = context->streams[video_stream_index]->codec->sample_aspect_ratio;
                }
                packet.stream_index = stream->id;
                encodedFrameSize = packet.size;
                
                // decode
                int frameFinished = 0;
                int result = avcodec_decode_video2(ccontext, pic, &frameFinished, &packet);
                
                if(result > 0 && frameFinished == 1) {
                    mutex.lock();

                    sws_scale(img_convert_ctx, pic->data, pic->linesize, 0, ccontext->height, picrgb->data, picrgb->linesize);
                    
                    newFrame = true;
                    
                    mutex.unlock();

                } else {
                    //cout<<"No frame decoded result is:"<<ofToString(result) << "\n" << endl;
                }
                
            }
        } else
        {
            setDead(true);
            newFrame = false;
            cout << "EOF or error statuscode is: " << ofToString(readStatus) << "\n" << endl;
        }
        
    }
    
}

void ofxStreamerReceiver::update() {
    
    if (!connected) return;
    if (picrgb->data[0] == NULL) return;
    
    if(mutex.tryLock()){
        if(newFrame){
            if(!allocated){
                lastFrame = new ofImage();
                lastFrame->allocate(width, height, OF_IMAGE_COLOR);
                allocated = true;
            }
            
            // save frame to image
            lastFrame->setFromPixels(picrgb->data[0], width, height, OF_IMAGE_COLOR);
            
            float timeDiff = ofGetElapsedTimeMillis() - lastReceiveTime;
            frameRate += ((1.0/(timeDiff/1000.0)) - frameRate)*0.8;
            bitrate = 8 * encodedFrameSize * frameRate / 1000.0;
            lastReceiveTime = ofGetElapsedTimeMillis();
            frameNum++;
            
            bHavePixelsChanged = true;
            newFrame=false;
        }
        mutex.unlock();
    } else {
      //  cout<<"Could not lock"<<endl;
    }
}

void ofxStreamerReceiver::setPaused(bool p)
{
    paused = p;
}

void ofxStreamerReceiver::draw(const ofPoint &p) {
    draw(p.x, p.y);
}

void ofxStreamerReceiver::draw(float x, float y) {
    draw(x, y, getWidth(), getHeight());
}

void ofxStreamerReceiver::draw(const ofRectangle &r) {
    draw(r.x, r.y, r.width, r.height);
}

void ofxStreamerReceiver::draw(float x, float y, float w, float h) {
    bHavePixelsChanged = false;
    if(allocated){
        lastFrame->draw(x,y,w,h);
    } else {
    }
}

unsigned char * ofxStreamerReceiver::getPixels() {
    if(allocated){
        bHavePixelsChanged = false;
        return lastFrame->getPixels();
    }
    return nil;
}

ofPixelsRef ofxStreamerReceiver::getPixelsRef() {
    bHavePixelsChanged = false;
    return lastFrame->getPixelsRef();
}

ofTexture & ofxStreamerReceiver::getTextureReference() {
    bHavePixelsChanged = false;
    return lastFrame->getTextureReference();
}

bool ofxStreamerReceiver::isFrameNew() {
    return (bHavePixelsChanged && allocated);
}

bool ofxStreamerReceiver::isConnected() {
    return (connected && newFrame);
}

void ofxStreamerReceiver::close() {
    
    waitForThread(true);
    delete lastFrame;
    
    if(&packet){
        av_free_packet(&packet);
    }    
    if (pixelData != NULL) {
        free(pixelData);
    }
    
    av_free(pic);
    av_free(picrgb);
    av_free(picture_buf);
    av_free(picture_buf2);
    av_read_pause(context);
    sws_freeContext(img_convert_ctx);
    
    if (context)
        avformat_free_context(context);
    if(oc)
        avformat_free_context(oc);
    if(ccontext)
        avcodec_close(ccontext);
    
    allocated = false;
}

float ofxStreamerReceiver::getWidth() {
    return width;
}

float ofxStreamerReceiver::getHeight() {
    return height;
}
