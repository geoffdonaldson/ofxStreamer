//
//  ofxStreamerReceiver.h
//  x264Example
//
//  Created by Johan Bichel Lindegaard on 4/18/13.
//
//

#pragma once

#define 	FF_INPUT_BUFFER_PADDING_SIZE   16
#define     INBUF_SIZE 1024

#include "ofMain.h"

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

class ofxStreamerReceiver : public ofBaseVideoPlayer , ofThread {
    
public:
    ofxStreamerReceiver();
    bool            connected;
    bool            allocated;
    bool            bHavePixelsChanged;
    int             width;
    int             height;
    int             port;
    string          host;
    string          url;
    
    bool            loadMovie(string name){return false;}
    void            play(){}
    void            setPaused(bool p);
    void            stop(){}
    
    bool            setup(int port, std::string host="udp://@");
    void            update();
    bool            isFrameNew();
    void            draw(float x, float y);
    void            draw(float x, float y, float w, float h);
    void            draw(const ofPoint &p);
    void            draw(const ofRectangle &r);
    void            close();
    ofTexture *     getTexture(){return NULL;}
    ofTexture &     getTextureReference();
    unsigned char * getPixels();
    ofPixelsRef     getPixelsRef();
    float           getHeight();
    float           getWidth();
    bool            isConnected();
    long            frameNum;
    int             bitrate;
    float           frameRate;
    
    bool isPaused(){return false;}
    bool isLoaded(){return false;}
    bool isPlaying(){return false;}
    bool setPixelFormat(ofPixelFormat pixelFormat){return false;}
    ofPixelFormat getPixelFormat(){return OF_PIXELS_RGB;}
    
private:
    
    AVFormatContext*    context;
    AVCodecContext*     ccontext;
    SwsContext*         img_convert_ctx;
    AVFormatContext*    oc;
    AVFrame*            pic;
    AVFrame*            picrgb;
    uint8_t*            picture_buf2;
    uint8_t*            picture_buf;
    AVPacket            packet;
    AVStream*           stream;
    int                 video_stream_index;
    long long           lastReceiveTime;
    
    ofImage *           lastFrame;
    int                 encodedFrameSize;
    
    void                threadedFunction();
    
    bool                newFrame;
    bool                paused;
    
    unsigned char *     pixelData;
    
    //  pthread_mutex_t mutex;
    ofMutex             mutex;
    Poco::Condition     isConnectedCondition;
};







