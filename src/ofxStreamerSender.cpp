//
//  ofxX264.cpp
//  x264Example
//
//  Created by Jonas Jongejan on 17/04/13.
//
//

#include "ofxStreamerSender.h"





ofxStreamerSender::ofxStreamerSender(){
    streaming = false;
}



void ofxStreamerSender::setup(int _width, int _height, string destination_ip, int destination_port ,string _preset){
    width = _width,
    height = _height;
    preset = _preset;
    
    frameNum = 0;
    
    av_register_all();
    avformat_network_init();
    
    
    //Create scale context for converting from rgb to yuv
    imgctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                            width, height, PIX_FMT_YUV420P,
                            SWS_FAST_BILINEAR, NULL, NULL, NULL);
    
    
    //Allocate the input picture
    x264_picture_alloc(&picture_in, X264_CSP_I420, width, height);
    //Allocate the output picture
    picture_out = (x264_picture_t*) malloc(sizeof(x264_picture_t));

    
    
    int bitrate = 4000;
    
    //Initialize the h264 encoder
    x264_param_t param;
    x264_param_default_preset(&param, preset.c_str(), "zerolatency");
    param.i_frame_reference = 1;
    
    param.i_threads = 1;
    param.b_sliced_threads = 5;
    param.i_slice_max_size = 8192;
    param.i_width = width;
    param.i_height = height;
    param.i_fps_num = 30;
    param.i_fps_den = 1;
    param.i_sync_lookahead = 0;
    
    param.i_bframe = 0;
    // Intra refres:
    param.i_keyint_max = 30;
    param.b_intra_refresh = 1;
    //Rate control:
    param.rc.i_rc_method = X264_RC_CRF;
//    param.rc.f_rf_constant = 25;
//    param.rc.f_rf_constant_max = 35;
    param.rc.i_lookahead = 0;
    param.rc.i_bitrate = bitrate;
    param.rc.i_vbv_max_bitrate = bitrate;
    param.rc.i_vbv_buffer_size = bitrate/30;
    
    //For streaming:
    param.b_repeat_headers = 1;
    param.b_annexb = 1;
    
    

    
    x264_param_apply_profile(&param, "baseline");
    
    encoder = x264_encoder_open(&param);
    
    url = "udp://"+destination_ip+":"+ofToString(destination_port);
    rtsp_url = "udp://"+destination_ip+":"+ofToString(destination_port);

    // initalize the AV context
    avctx = avformat_alloc_context();
    //avformat_alloc_output_context2(&avctx, 0, "sdp", rtsp_url.c_str());
    if (!avctx)
    {
        ofLog(OF_LOG_FATAL_ERROR, "Couldn't initalize AVFormat output context");
        exit(0);
    }
    snprintf(avctx->filename, sizeof(avctx->filename), "%s", rtsp_url.c_str());
    
    // get the output format container
    AVOutputFormat * fmt = av_guess_format("mpegts", NULL, NULL);
    if (!fmt)
    {
        ofLog(OF_LOG_FATAL_ERROR, "Unsuitable output format");
        exit(0);
    }
    avctx->oformat = fmt;
    
    
    // try to open the UDP stream
//    snprintf(filename, sizeof(filename), "udp://%s:%d", destination_ip.c_str(), destination_port);
    //if (avio_open(&avctx->pb, rtsp_url.c_str(), AVIO_FLAG_WRITE) < 0)
    //if (avio_open2(&avctx->pb, avctx->filename, AVIO_FLAG_WRITE, NULL, NULL) < 0)
    if (avio_open(&avctx->pb, avctx->filename, AVIO_FLAG_WRITE) < 0)
    {
        ofLog(OF_LOG_FATAL_ERROR, "Couldn't open UDP output stream %s",avctx->filename);
        exit(0);
    }
    
    // add an H.264 stream
    stream = avformat_new_stream(avctx, NULL);
    if (!stream)
    {
        ofLog(OF_LOG_FATAL_ERROR, "Couldn't allocate H.264 stream");
        exit(0);
    }
    
    // initalize codec
    AVCodecContext* c = stream->codec;
    c->codec_id = CODEC_ID_H264;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->bit_rate = bitrate;
    c->width = width;
    c->height = height;
    c->time_base.den = 30;
    c->time_base.num = 1;
    //c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    // write the header
    AVDictionary* opts = 0;
    av_dict_set(&opts, "rtsp_transport", "udp_multicast", 0);
    avformat_write_header(avctx, opts ? &opts : 0);
    av_dump_format(avctx, 0, avctx->filename, 1);
    //avformat_write_header(avctx, nil);
    
    
//    // RTSP Streamer
//    
//    rtsp_url = "rtp://"+destination_ip+":"+ofToString(destination_port);
//
//    // RTSP Output Setup
//    int ret = avformat_alloc_output_context2(&rtspctx, NULL, "rtsp", rtsp_url.c_str());
//    if (ret < 0) {
//        ofLog(OF_LOG_FATAL_ERROR, "avformat_alloc_output_context2 error : %d", ret);
//        exit(0);
//    }
//    
//    rtspctx->oformat = fmt;
//    
//    ret = avio_open2(&rtspctx->pb, rtspctx->filename, AVIO_FLAG_WRITE, NULL, NULL);
//    if (ret != 0) {
//        ofLog(OF_LOG_FATAL_ERROR, "avio_open2 error : %d", ret);
//        exit(0);
//    }
//    
//    AVStream* vstream = avformat_new_stream(rtspctx, NULL);
//    if (!vstream)
//        return;
//    vstream->duration = 0;
//    vstream->time_base.den = 30;
//    vstream->time_base.num = 1;
//    vstream->avg_frame_rate = vstream->time_base;
//    
//    AVCodecContext* vcodec = vstream->codec;
//    vcodec->codec_id = CODEC_ID_H264;
//    vcodec->codec_type = AVMEDIA_TYPE_VIDEO;
//    vcodec->bit_rate = bitrate;
//    vcodec->width = width;
//    vcodec->height = height;
//    vcodec->time_base.den = 30;
//    vcodec->time_base.num = 1;
//    vcodec->flags |= CODEC_FLAG_GLOBAL_HEADER;
//    
//    AVDictionary* opts = 0;
////    if (_sendOption == RTSP_SEND_TCP)
////        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
////    
////    if (_sendOption == RTSP_SEND_UDP)
//        av_dict_set(&opts, "rtsp_transport", "udp", 0);
//    
//    ret = avformat_write_header(rtspctx, opts ? &opts : 0);
//    if (ret != 0) {
//        ofLog(OF_LOG_FATAL_ERROR, "avformat_write_header error : %d", ret);
//        char errbuf[128];
//        const char *errbuf_ptr = errbuf;
//        if (av_strerror(ret, errbuf, sizeof(errbuf)) < 0)
//            errbuf_ptr = strerror(AVUNERROR(ret));
//            ofLog(OF_LOG_FATAL_ERROR, "Could not write header (incorrect codec parameters ?): %s", errbuf_ptr);
//        ret = AVERROR(EINVAL);
//        exit(0);
//    }
//    av_dump_format(rtspctx, 0, rtspctx->filename, 1);
//    cout << endl;
    
    
    streaming = true;
}


bool ofxStreamerSender::encodeFrame(ofImage image){
    if((image.type != OF_IMAGE_COLOR) && (image.type != OF_IMAGE_COLOR_ALPHA)){
        cout<<"Only implemented OF_IMAGE_COLOR & OF_IMAGE_COLOR_ALPHA type images in encodeFrame"<<endl;
        cout << image.type << endl;
        return false;
    }
    
    unsigned char * data = image.getPixelsRef().getPixels();
    int length = image.getWidth() * image.getHeight() * image.getPixelsRef().getNumChannels();
    
    return encodeFrame(data, length, (image.type == OF_IMAGE_COLOR) ? OF_PIXELS_RGB : OF_PIXELS_RGBA);
}

bool ofxStreamerSender::encodeFrame(unsigned char *data, int data_length, ofPixelFormat pix_format){
    if(streaming){
        encodedFrameSize = 0;
        
        
        int stride = width * ((pix_format == OF_PIXELS_RGB) ? 3 : 4);
        
        //Convert to YUV format
        sws_scale(imgctx, (const uint8_t* const*) &data, &stride, 0, height, picture_in.img.plane, picture_in.img.i_stride);
        
        
        //Encode h264 frame
        x264_nal_t* nals;
        int num_nals;
        
        int frame_size = x264_encoder_encode(encoder, &nals, &num_nals, &picture_in, picture_out);
        if (frame_size > 0)
        {
            encodedFrameData = (unsigned char*)nals[0].p_payload ;
            encodedFrameSize = frame_size ;
            return true;
        }
    }

    return false;
}


bool ofxStreamerSender::sendFrame(){
    if(streaming){
        if(encodedFrameSize == 0){
            ofLog(OF_LOG_WARNING, "No encoded frame to send, make sure to call encodeFrame");
            return false;
        }
        
        
        AVCodecContext *codecContext = stream->codec;
        
        // initalize a packet
        AVPacket p;
        av_init_packet(&p);
        p.data = encodedFrameData;
        p.size = encodedFrameSize;
        p.stream_index = codecContext->frame_number;
        
        
           //p.pts = int64_t(0x8000000000000000);
          //p.dts = int64_t(0x8000000000000000);
        
        /*    if(codecContext->coded_frame->key_frame)
         p.flags |= AV_PKT_FLAG_KEY;
         */
        
        frameNum++;
        
        float timeDiff = ofGetElapsedTimeMillis() - lastSendTime;
        
        frameRate += ((1.0/(timeDiff/1000.0)) - frameRate)*0.8;
        
        bitrate = 8 * encodedFrameSize * frameRate / 1000.0;
        
        
        lastSendTime = ofGetElapsedTimeMillis();
        
        // send it out
        return av_write_frame(avctx, &p);
        //return av_write_frame(avctx, &p);
    }
    return false;
}


x264_picture_t* ofxStreamerSender::getPictureRef(){
    return picture_out;
}

