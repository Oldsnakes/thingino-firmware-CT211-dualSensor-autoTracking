/*  VideoWorker.cpp
    ### TW:  modified for dual sensors  9/10/2025
    time lapes MIPI switch selects sensor.
    MIPI switch is controlled by GPIO which is memory mapped.
    send encoded stream pack to channel buffer based on switch position
    grab pack from buffer and feed to streamer
    add pruydnt command line parameters to select mode and time interval.

    RTPS.cpp -- add two more streams
    Config.cpp -- add two more streams
    webui -- add manual sensor select to 
        preview.cgi, 
        config-gpio.cgi, 
        config-stream.cgi, 
        json-gpio.cgi
    sensor init are concurrent for both I2C interface which is not idea and cause artific.
*/
#include "VideoWorker.hpp"

#include "Config.hpp"
#include "IMPEncoder.hpp"
#include "IMPFramesource.hpp"
#include "Logger.hpp"
#include "WorkerUtils.hpp"
#include "globals.hpp"
#include "imp_hal.hpp"
#include "ctrls_hal.hpp"
#include <unistd.h>
#include <fcntl.h>

#undef GDEBUG
#undef GP_DEBUG
#undef SYNC_DEBUG
#undef ALT_DEBUG

#undef MODULE
#define MODULE "VideoWorker"

/* ### TW:  MIPI switch */
#include <sys/mman.h>

#define MIPI_SW_BIT 	    0x00000080
#define GPIO_BASE		    0x10010000
#define GPIO_PortA_PAT0		0x10010040
#define MIPI_SET_REG 		0x10010044
#define MIPI_CLEAR_REG 		0x10010048
#define ALT_OFFSET          4       // 0->4, 1->5
#define NUM_VIDEO_ALT_CHANNELS  3   // 0 is not used to avoid confustion

#if 0
typedef struct {
	uint32_t REG[1024];
} XHAL_HandleTypeDef;

uint32_t* phys_mem;
volatile XHAL_HandleTypeDef *port;

static int fd;
#endif

char 	msg[100];
int32_t v_time_pading = 0;
//extern  int global_MipiMode;
//extern uint32_t global_time_interval;
bool t_trans[NUM_VIDEO_CHANNELS];
int MipiSwitch_pin;
std::string gpio_name = "gpio_sensor_switch";
// uint64_t last_sw_time = 0;

// std::atomic<int> global_Mipi_sw_state;


static struct {
    int64_t sw_time;         // mipi switching time
    uint32_t time_interval;   // for time to switch
    bool set;                   // mipi switch state
} encoder_mipi;

std::shared_ptr<video_stream> alt_stream[NUM_VIDEO_CHANNELS] = {nullptr};
  // follows video channel number, channel 2/3 not used
// #####

VideoWorker::VideoWorker(int chn)
    : encChn(chn)
{
    LOG_DEBUG("VideoWorker created for channel " << encChn);
}

VideoWorker::~VideoWorker()
{
    LOG_DEBUG("VideoWorker destroyed for channel " << encChn);
}

#if 0
/* #### TW:  GPIO MIPI channel sensor switch */
void VideoWorker::MipiSwitch(bool main)
{
    uint32_t    mipi_sw_mask;

    mipi_sw_mask = 0x00000001 << MipiSwitch_pin;
  #ifdef GP_DEBUG
    sprintf(msg,"gpio_mask:%08x",mipi_sw_mask);
    LOG_DEBUG("### " << msg);
  #endif
    if (main) {	// set bit:  FIX sensor 
        port->REG[(MIPI_SET_REG - GPIO_BASE)/4] = mipi_sw_mask;
  #ifdef GP_DEBUG
        sprintf(msg,"PAT0-%p:%08x",&port->REG[(GPIO_PortA_PAT0 - GPIO_BASE)/4],port->REG[(GPIO_PortA_PAT0 - GPIO_BASE)/4]);
        LOG_DEBUG("### set reg - " << msg);
  #endif
    }
    else { // clear bit:  PTZ sensor, we want that be the ch0 so NVR will pick it up
        port->REG[(MIPI_CLEAR_REG - GPIO_BASE)/4] = mipi_sw_mask;
  #ifdef GP_DEBUG
        sprintf(msg,"PAT0:%08x",port->REG[(GPIO_PortA_PAT0 - GPIO_BASE)/4]);
        LOG_DEBUG("### clear reg - " << msg);
  #endif
    }
}
#endif

/*  4 channel buffer version */
void VideoWorker::run()
{
    LOG_DEBUG("Start video processing run loop for stream " << encChn);

    uint32_t bps = 0;
    uint32_t fps = 0;
    uint32_t error_count = 0; // Keep track of polling errors
    unsigned long long ms = 0;
    unsigned long long delay_ms = 0;
    bool run_for_jpeg = false;

    uint IMPencChn = encChn;         // call to IMP lib channel number, true encoder number
    uint alt_encChn = encChn;        // alternate sensor second channel offset (paired channel), encChn is stream number instead.
    int64_t current_ts = 0;
    int current_sensor = 0;
    int stream_delay = 0;  
    int64_t time_diff = 0;

    // int32_t v_time_pading = 0;
    int32_t time_frame = 40 * 1000;  // 25 fps
    int32_t pad_time =  time_frame / 20;
    bool auto_skew = true;

    bool first_pack = true;
    H264NALUnit last_nalu;  // use to save last known good pack for the channel
    int ret = 0;
    int pack_count = 0;

#if 0  // old channel scheme
    if (encChn <= 1) {
        alt_encChn = encChn + ALT_OFFSET;  // for alt-channel index
        IMPencChn = encChn;
    } else {
        alt_encChn = encChn - ALT_OFFSET;  // ch 2 not used
        IMPencChn = alt_encChn;
    }
#endif

    // encChn is current stream channel, alt_encChn is paired stream channel on the alternate sensor
    // 0-4, 1-5, 4-0, 5-1
    switch(encChn) {  // main channels:  0/4, second-channels:  1/5.  2/3 reserved for JPEG numbering
        case 0:
            alt_encChn = 4;  // for paired-channel index
            IMPencChn = 0;   // for call IMP
            break;
        case 1:
            alt_encChn = 5;  
            IMPencChn = 1;
            break;
        case 4:
            alt_encChn = 0;  
            IMPencChn = 0;
            break;
        case 5:
            alt_encChn = 1;  
            IMPencChn = 1;
            break;
        default:    // ch 2/3 not used
            break;
    }

    VideoWorker worker(encChn);

    while (global_video[encChn]->running)
    {
        /* bool helper to check if this is the active jpeg channel and a jpeg is requested while 
         * the channel is inactive
         */
       //  run_for_jpeg = (encChn == global_jpeg[0]->streamChn && global_video[encChn]->run_for_jpeg);
        run_for_jpeg = (encChn == global_jpeg[0]->streamChn && global_video[encChn]->run_for_jpeg)
                        | (encChn == global_jpeg[1]->streamChn && global_video[encChn]->run_for_jpeg);

        /* allow change inverval while running */
        encoder_mipi.time_interval = global_time_interval;  // in ms

        /* now we need to verify that 
         * 1. a client is connected (hasDataCallback)
         * 2. a jpeg is requested 
         */
         /*  the alt-channel has its own turn to check anyway. */
        if (global_video[encChn]->hasDataCallback || run_for_jpeg)
        {
            global_video[encChn]->active = true;						
            global_video[encChn]->is_activated.release();	
            /*  packs are dispatched to buffer based on the MIPI sensor condition */
            /*  pack in the buffer also pickup from buffer if available */
            if (IMP_Encoder_PollingStream(IMPencChn, cfg->general.imp_polling_timeout) == 0)   	
            {
                IMPEncoderStream stream;
                    if (IMP_Encoder_GetStream(IMPencChn, &stream, GET_STREAM_BLOCKING) != 0)	
                {
                    LOG_ERROR("IMP_Encoder_GetStream(" << IMPencChn << ") failed");
                    error_count++;
                    continue;
                }
                /*  get timestamp of stream */
                int64_t nal_ts = stream.pack[stream.packCount - 1].timestamp;
                struct timeval encoder_time;

                alt_stream[encChn]->refType = stream.refType;
                for (uint32_t i = 0; i < stream.packCount; ++i)
                {
                    fps++;
                    bps += stream.pack[i].length;
                    if ((global_video[encChn]->hasDataCallback) || (global_video[alt_encChn]->hasDataCallback))  //  not for jpeg
                    {
#if defined(PLATFORM_T31) || defined(PLATFORM_T40) || defined(PLATFORM_T41) || defined(PLATFORM_C100)
                        uint8_t *start = (uint8_t *) stream.virAddr + stream.pack[i].offset;
                        uint8_t *end = start + stream.pack[i].length;
#elif defined(PLATFORM_T10) || defined(PLATFORM_T20) || defined(PLATFORM_T21) \
|| defined(PLATFORM_T23) || defined(PLATFORM_T30)
                        uint8_t *start = (uint8_t *) stream.pack[i].virAddr;
                        uint8_t *end = (uint8_t *) stream.pack[i].virAddr + stream.pack[i].length;
#endif
                        H264NALUnit nalu;
                        // We use start+4 because the encoder inserts 4-byte MPEG
                        //'startcodes' at the beginning of each NAL. Live555 complains
    #ifdef ALT_DEBUG
                        LOG_DEBUG("*--> insert data to nalu - ch:" << encChn);
    #endif
                        nalu.data.insert(nalu.data.end(), start + 4, end);  // get pack data pointers

                        if (global_video[IMPencChn]->idr == false)		// fix idr?
                        {
    #ifdef ALT_DEBUG
                            // LOG_DEBUG("idr false: IMPencChn " << IMPencChn << " packet: " << i);
    #endif
#if defined(PLATFORM_T31) || defined(PLATFORM_T40) || defined(PLATFORM_T41) || defined(PLATFORM_C100)
                            if (stream.pack[i].nalType.h264NalType == 7
                                || stream.pack[i].nalType.h264NalType == 8
                                || stream.pack[i].nalType.h264NalType == 5)
                            {
                                global_video[IMPencChn]->idr = true;				
                            }
                            else if (stream.pack[i].nalType.h265NalType == 32)
                            {
                                global_video[IMPencChn]->idr = true;				
                            }
#elif defined(PLATFORM_T10) || defined(PLATFORM_T20) || defined(PLATFORM_T21) \
|| defined(PLATFORM_T23)
                            if (stream.pack[i].dataType.h264Type == 7
                                || stream.pack[i].dataType.h264Type == 8
                                || stream.pack[i].dataType.h264Type == 5)
                            {
                                global_video[IMPencChn]->idr = true;
                            }
#elif defined(PLATFORM_T30)
                            if (stream.pack[i].dataType.h264Type == 7
                                || stream.pack[i].dataType.h264Type == 8
                                || stream.pack[i].dataType.h264Type == 5)
                            {
                                global_video[IMPencChn]->idr = true;				
                            }
                            else if (stream.pack[i].dataType.h265Type == 32)
                            {
                                global_video[IMPencChn]->idr = true;				
                            }
#endif
                        }  /*  idr false */

                        if (global_video[IMPencChn]->idr == true)					
                        {
    #ifdef ALT_DEBUG
                            LOG_DEBUG("idr TRUE: IMPencChn " << IMPencChn << " packet: " << i << " global_MipiMode: " << global_MipiMode);
    #endif
                            /* ### TW:  check encoder pack time against switching time for dispatch packs */
                            if (global_MipiMode == 3) {  
                                time_diff = stream.pack[i].timestamp - last_sw_time + v_time_pading;
                                time_frame = 1000000 / global_video[IMPencChn]->stream->fps;  // us
                                pad_time =  time_frame / 10;  // us

                                if  (current_sensor != global_Mipi_sw_state) {  // state changed
                                    /* process mipi switching based on time lapes */
                                    nalu.imp_ts = stream.pack[i].timestamp;
        #ifdef SYNC_DEBUG
                                sprintf(msg,"%lld",encoder_mipi.sw_time);
                                LOG_DEBUG("-- ch:" << IMPencChn << "- imp_ts: " << nalu.imp_ts << " sw_time: " << msg);
        #endif
                                    time_diff = nalu.imp_ts - encoder_mipi.sw_time;
                                    if (time_diff > 0)
                                    {   // the pack was encoded after MIPI switched to new sensor - Switch.
                                        current_sensor = encoder_mipi.set;   
                                        if ((encChn == 0) && (time_diff > time_frame/2)) {  // it take too long to switch, reduce pad.
                                            v_time_pading -= pad_time;  // us
        #ifdef SYNC_DEBUG
                                            LOG_DEBUG("<#" << encChn << "#> --- encChn skew =  " << v_time_pading << " ---" );
        #endif
                                        }
                                    } else {  // we have a time skew, increase pad.
                                        if (encChn == 0) {
                                            v_time_pading += pad_time;  // us
        #ifdef SYNC_DEBUG
                                            LOG_DEBUG("<#" << encChn << "#> +++ encChn skew =  " << v_time_pading << " +++");
        #endif
                                        }
                                    }
                                } // state changed
    #if defined(ALT_STREAM_SUPPORT)
        #ifdef SYNC_DEBUG
                                    // LOG_DEBUG("   ch:" << encChn << "Start to fill buffer");
        #endif
                                /* ### write pack data to buffer */
                                if ((encChn == 0) || (encChn == 1)) {  // main-channels 0/1
                                /* ### TW:  0/1 for main-channel, 4/5 for alt-channel, 6 for not */
                                    if (current_sensor == 0) {  // send to main channel buffer
                                        if (global_video[encChn]->hasDataCallback) 
                                        {  // only wirte to buffer if it is active feed
                                            ret = alt_stream[encChn]->msgChannel->write(nalu); 
                #ifdef ALT_DEBUG
                                            LOG_DEBUG("- sensor: " << current_sensor << ", write nalu -> ch:" << encChn << " buffer: " << encChn);
                #endif
                                        }
                                    } 
                                    else 
                                    {  // send to alt-channel buffer
                                        if (global_video[alt_encChn]->hasDataCallback) 
                                        {  // only if it is active feed
                #ifdef ALT_DEBUG
                                            LOG_DEBUG("- sensor: " << current_sensor << ", write nalu -> ch:" << encChn << " buffer: " << alt_encChn);
                #endif
                                            /*  store alt-channel data to queue */
                                            ret = alt_stream[alt_encChn]->msgChannel->write(nalu); // ****
                                        }
                                    }
                                } else 
                                {  // alt-channels  4/5
                                    if (current_sensor == 0) {  // send to alt-channel buffer
                                        if (global_video[alt_encChn]->hasDataCallback) 
                                        {  // only wirte to buffer if it is active feed
                                            ret = alt_stream[alt_encChn]->msgChannel->write(nalu); 
                #ifdef ALT_DEBUG
                                            LOG_DEBUG("- sensor: " << current_sensor << ", write nalu -> ch:" << encChn << " buffer: " << alt_encChn);
                #endif
                                        }
                                    } 
                                    else 
                                    {  // send to main-channel buffer
                                        if (global_video[encChn]->hasDataCallback) 
                                        {  // only if it is active feed
                #ifdef ALT_DEBUG
                                            LOG_DEBUG("- sensor: " << current_sensor << ", write nalu -> ch:" << encChn << " buffer: " << encChn);
                #endif
                                            /*  store data to buffer */
                                            ret = alt_stream[encChn]->msgChannel->write(nalu); // ****
                                        }
                                    }
                                }
                                if (!ret)			
                                {
                                    LOG_ERROR("Buffer " << "channel:" << encChn << ", "
                                                        << "alt_channel:" << alt_encChn << ", "
                                                        << "package:" << i << " of " << alt_stream[encChn]->pack_count
                                                        << ", " << "packageSize:" << nalu.data.size()
                                                        << ".  !sink clogged!");
                                } 
                                else 
                                {
                                    alt_stream[encChn]->pack_count++;
                                    alt_stream[encChn]->seq++;
                                }
//                            }
                                /*  ### get from buffer and write out */
                                if (global_video[encChn]->hasDataCallback)  { 
                                    H264NALUnit *naluptr = &nalu;
                                    bool read_good;
                                    
                                    while ((read_good = alt_stream[encChn]->msgChannel->read(naluptr))) {
                        #ifdef ALT_DEBUG
                                        // LOG_DEBUG("* get from buffer -> ch: " << encChn);
                        #endif
                                        ret = global_video[encChn]->msgChannel->write(nalu);
                        #ifdef ALT_DEBUG
                                        LOG_DEBUG("# write data nalu -> ch: " << encChn);
                        #endif
                                        if (!ret)	//  write failed		
                                        {
                                            LOG_ERROR("video " << "channel:" << encChn << ", "
                                                                << "package:" << i << " of " << global_video[encChn]->pack_count
                                                                << ", " << "packageSize:" << nalu.data.size()
                                                                << ".  !sink clogged!");
                                        }
                                        else
                                        {
                                            std::unique_lock<std::mutex> lock_stream{
                                                global_video[encChn]->onDataCallbackLock};			
                                            if (global_video[encChn]->onDataCallback) {
                                                global_video[encChn]->onDataCallback();
                                            }
                                            alt_stream[encChn]->pack_count--;
                                        }
        #else // no alt-channel
                #if 0
                                    /*  ### TW:  If it is the pack to skip, replicate the last good pack */
                                    /*  looks like it has no improvement to the video quality */
                                        if (first_pack) 
                                        {     // init last_nalu
                                            last_nalu = nalu;
                                            first_pack = false;
                                        } 
                                        else 
                                        {
                                            if (encChn == current_sensor) {
                                                last_nalu = nalu;   // new 
                                            } else {
                                                nalu = last_nalu;   // reuse old
                                            }
                                        }
                #endif
                                        ret = global_video[encChn]->msgChannel->write(nalu);
                    #ifdef ALT_DEBUG
                                        // LOG_DEBUG("### direct write data nalu -> ch:" << encChn);
                    #endif
        #endif // alt-channel     
                                        if (!ret)			
                                        {
                                            LOG_ERROR("video " << "channel:" << encChn << ", "
                                                                << "package:" << i << " of " << stream.packCount
                                                                << ", " << "packageSize:" << nalu.data.size()
                                                                << ".  !sink clogged!");
                                        }
                                        else
                                        {
                                            std::unique_lock<std::mutex> lock_stream{
                                                global_video[encChn]->onDataCallbackLock};			
                                            if (global_video[encChn]->onDataCallback)
                                                global_video[encChn]->onDataCallback();
                                        }
                                    } // while
                    #ifdef ALT_DEBUG
                                        // LOG_DEBUG("#### No more data nalu -> ch:" << encChn);
                    #endif
                                }   // hascallback for buffer write out
                            }   // mode 3
                            else 
                            {  //  accept all packs for none alternate modes (!mode 3)
                                current_sensor = 6;
                                ret = global_video[encChn]->msgChannel->write(nalu);
        #ifdef ALT_DEBUG
                                // LOG_DEBUG("### direct write data nalu -> ch:" << encChn);
        #endif
                                if (!ret)			
                                {
                                    LOG_ERROR("video " << "channel:" << encChn << ", "
                                                        << "package:" << i << " of " << stream.packCount
                                                        << ", " << "packageSize:" << nalu.data.size()
                                                        << ".  !sink clogged!");
                                }
                                else
                                {
                            //LOG_DEBUG(" output to msgChannel: " << encChn);
                                    std::unique_lock<std::mutex> lock_stream{
                                        global_video[encChn]->onDataCallbackLock};			
                                    if (global_video[encChn]->onDataCallback)
                                        global_video[encChn]->onDataCallback();
                                }
                            }  // not mode 3
                        }  /* idr true */
    #if defined(USE_AUDIO_STREAM_REPLICATOR)
                        /* Since the audio stream is permanently in use by the stream replicator, 
                            * and the audio grabber and encoder standby is also controlled by the video threads
                            * we need to wakeup the audio thread 
                        */
                        if (cfg->audio.input_enabled && !global_audio[0]->active && !global_restart)
                        {
                            LOG_DDEBUG("NOTIFY AUDIO " << !global_audio[0]->active << " "
                                                        << cfg->audio.input_enabled);
                            global_audio[0]->should_grab_frames.notify_one();
                        }
    #endif
                    }  /*  hasDataCallback */
                }  /*  packets  */

                IMP_Encoder_ReleaseStream(IMPencChn, &stream);	
                
                /* #### TW:  MIPI switch sensor #### */
                /* use ch 0 to toggle sensor on MIPI interface.  A packet time is about 10 us, a frame time is about 1/30 (33 ms)*/
                if (global_MipiMode == 3) {
                //    if ((phys_mem != NULL) && (phys_mem != MAP_FAILED) && (fd >= 0)) {
                        struct timespec current_time;

                        t_trans[encChn] = false;
                        clock_gettime(CLOCK_MONOTONIC, &current_time);
                        current_ts = (current_time.tv_sec * 1000000) + (current_time.tv_nsec / 1000);  // in us
                        time_diff = current_ts - (last_sw_time + encoder_mipi.time_interval*1000);
//                        if (current_ts > (last_sw_time + encoder_mipi.time_interval*1000)) {
                        if (time_diff > 0) {
                            t_trans[encChn] = true;
                            if (encoder_mipi.set) {  // toggle
                                encoder_mipi.set = false;
                            }
                            else {
                                encoder_mipi.set = true;
                            }
    #ifdef SYNC_DEBUG
                            LOG_DEBUG("<(" << encChn << ")> current_time = " << current_ts << " last_sw_time = "  << last_sw_time 
                                 << " time_diff = " << time_diff << " state: " << encoder_mipi.set);
    #endif
                        }

                        if (t_trans[encChn]) {   // switch GPIO
                            IMP_Encoder_RequestIDR(0);  // I frame only
                            IMP_Encoder_RequestIDR(1);
                            IMP_Encoder_RequestIDR(4);
                            IMP_Encoder_RequestIDR(5);
                            IMP_Encoder_RequestIDR(2);  // jpeg should not need this
                            IMP_Encoder_RequestIDR(3);
                            IMP_Encoder_FlushStream(0);  // disgard packs in video encoder queue
                            IMP_Encoder_FlushStream(1);  
                            IMP_Encoder_FlushStream(4);  
                            IMP_Encoder_FlushStream(5);  
                            IMP_Encoder_FlushStream(2);  // disgard packs in jpeg encoder queue
                            IMP_Encoder_FlushStream(3);  
                            ctrls_hal::setGPIO(MipiSwitch_pin, encoder_mipi.set);
                            // worker.MipiSwitch(encoder_mipi.set);
                            global_Mipi_sw_state = encoder_mipi.set;
                            
                            // convert from <timespec> to <timeval>
    #ifdef SYNC_DEBUG
            //        LOG_DEBUG("<*" << encChn << "*> video_pack_time = " << current_ts << " last_sw_time = "  << last_sw_time);
    #endif

                            /* ########### test alignment */
                            // last_sw_time = current_ts + (global_time_delay-500) * 1000;  // 0 ms, 500 is default, 
                            last_sw_time = current_ts;
                            // LOG_DEBUG("<#" << encChn << "#> current_time = " << current_ts << " last_sw_time = "  << last_sw_time);
                            encoder_mipi.sw_time = current_ts;
                            stream_delay = 0;  // use for debug queue size
    #ifdef SYNC_DEBUG
                            sprintf(msg,"%lld",encoder_mipi.sw_time);
                            LOG_DEBUG("---<*" << encChn << "*> MIPI switch to " << encoder_mipi.set << " @ time: " << msg);
    #endif
                            for (int i=0; i < NUM_VIDEO_CHANNELS; i++) {  // set for other channels, not used
                                t_trans[i] = true;
                            }
                        }
                //    }
                //    else LOG_DEBUG(" *** mem access failed *** " << encChn );
                }  /* mode 3 switch sensor */

                ms = WorkerUtils::tDiffInMs(&global_video[encChn]->stream->stats.ts);		
                if (ms > 1000)  /*  timeout */
                {
                    /* currently we write into osd and stream stats,
                        * osd will be removed and redesigned in future
                    */
                    global_video[encChn]->stream->stats.bps = bps;				
                    global_video[encChn]->stream->osd.stats.bps = bps;
                    global_video[encChn]->stream->stats.fps = fps;
                    global_video[encChn]->stream->osd.stats.fps = fps;

                    fps = 0;
                    bps = 0;
                    gettimeofday(&global_video[encChn]->stream->stats.ts, NULL);		
                    global_video[encChn]->stream->osd.stats.ts = global_video[encChn]		
                                                                        ->stream->stats.ts;
                    /*
                    IMPEncoderCHNStat encChnStats;
                    IMP_Encoder_Query(channel->encChn, &encChnStats);				
                    LOG_DEBUG("ChannelStats::" << channel->encChn <<
                                ", registered:" << encChnStats.registered <<
                                ", leftPics:" << encChnStats.leftPics <<
                                ", leftStreamBytes:" << encChnStats.leftStreamBytes <<
                                ", leftStreamFrames:" << encChnStats.leftStreamFrames <<
                                ", curPacks:" << encChnStats.curPacks <<
                                ", work_done:" << encChnStats.work_done);
                    */
                    if (global_video[encChn]->idr_fix)						
                    {
                        IMP_Encoder_RequestIDR(IMPencChn);						
                        global_video[encChn]->idr_fix--;					
                    }
                }  /* timeout */
            }  /*  pollingstream  */
            else
            {
                error_count++;									
                LOG_DDEBUG("IMP_Encoder_PollingStream("
                            << IMPencChn << ", " << cfg->general.imp_polling_timeout << ") timeout !");
            }
        }  /* hasDataCallback or JPEG */
        else if (global_video[encChn]->onDataCallback == nullptr && !global_restart_video	
                    && !global_video[encChn]->run_for_jpeg)	/*  bad callback, flush them. */				
        {
            LOG_DDEBUG("VIDEO LOCK" << " channel:" << encChn << " hasCallbackIsNull:"
                                    << (global_video[encChn]->onDataCallback == nullptr)	
                                    << " restartVideo:" << global_restart_video
                                    << " runForJpeg:" << global_video[encChn]->run_for_jpeg);	
            // LOG_INFO("ch:  %d -- Video lock --",encChn);
            global_video[encChn]->stream->stats.bps = 0;					
            global_video[encChn]->stream->stats.fps = 0;
            global_video[encChn]->stream->osd.stats.bps = 0;
            global_video[encChn]->stream->osd.stats.fps = 0;

            std::unique_lock<std::mutex> lock_stream{mutex_main};
            global_video[encChn]->active = false;						
            while (global_video[encChn]->onDataCallback == nullptr && !global_restart_video	
                    && !global_video[encChn]->run_for_jpeg)					
                global_video[encChn]->should_grab_frames.wait(lock_stream);

            global_video[encChn]->active = true;						
            global_video[encChn]->is_activated.release();					

            // unlock audio
            global_audio[0]->should_grab_frames.notify_one();

            LOG_DDEBUG("VIDEO UNLOCK" << " channel:" << encChn);
        }  /*  bad callback point, reset */
    }  /*  while loop  */
}  /* run */

/*  Entry point, call run to loop.  */
void *VideoWorker::thread_entry(void *arg)
{ 
    StartHelper *sh = static_cast<StartHelper *>(arg);
    int encChn = sh->encChn;
    
    // global_MipiMode = sh->MipiMode;
    global_time_interval = sh->Time_interval;
    LOG_INFO("Camera Mode = " << global_MipiMode);
    LOG_DEBUG("Start stream_grabber thread for stream " << encChn);

    int ret;
    uint alt_encChn = encChn + ALT_OFFSET;

#if 0
    if ((encChn == 0)  || (encChn == 1)) {  // setup encoder streams
        global_video[encChn]->imp_framesource = IMPFramesource::createNew(global_video[encChn]->stream, &cfg->sensor, encChn);
    #ifdef GDEBUG
        LOG_DEBUG(" * ch: " << encChn << ", name: " << global_video[encChn]->name);
        LOG_DEBUG("   --  width: " << global_video[encChn]->stream->width << ", height: " << global_video[encChn]->stream->height );
    #endif
        global_video[encChn]->imp_encoder = IMPEncoder::createNew(global_video[encChn]->stream, encChn, encChn, global_video[encChn]->name);
        global_video[encChn]->imp_framesource->enable();
        global_video[encChn]->run_for_jpeg = false;
    } //xxxx
#endif

 /*  one Group only support one resolution, and can allow 2 encode channel to be registered into.
 *  encoder channel contains encoder attribute and ratecontroller attribute
 *  encoder should choose encoder protocol, and then set the attribute to the encoder
 *  registration of a channel to an uncreated Group will cause failure
 *  One channel can be registed to only one group, if it has already been registered to another Group, it will return failure;
 *  If one Group is already used by a Channel, so it can not be used again by another one unless this Group's Channel is first destroyed, otherwise it wil return failure.
*/

 /* video channels 
 * (encChn) (chnNr)                (encChn)      (cfg->)   (stream)   
 * video   framesource    group    encoder       sensor    format   
 *  ch 0:  framesource 0, group 0, encoder ch 0, sensor 0, H264,     
 *  ch 1:  framesource 1, group 1, encoder ch 1, sensor 1, H264,    
 *  ch 2:  framesource 0, group 0, encoder ch 2, sensor 0, JPEG,    
 *  ch 3:  framesource 1, group 1, encoder ch 3, sensor 1, JPEG,    
 *  ch 4:  framesource 3, group 2, encoder ch 4, sensor 0, H264,     
 *  ch 5:  framesource 4, group 3, encoder ch 5, sensor 1, H264,     
 */
    // note:  IMPFramesource(stream, sensor, chnNr);
    // note:  IMPEncoder(stream, encChn, encGrp, name);
    switch(encChn) {
        case 0:
            global_video[encChn]->imp_framesource = IMPFramesource::createNew(global_video[encChn]->stream, &cfg->sensor, 0);
            global_video[encChn]->imp_encoder = IMPEncoder::createNew(global_video[encChn]->stream, encChn, 0, global_video[encChn]->name);
            break;
        case 1:
            global_video[encChn]->imp_framesource = IMPFramesource::createNew(global_video[encChn]->stream, &cfg->sensor, 1);
            global_video[encChn]->imp_encoder = IMPEncoder::createNew(global_video[encChn]->stream, encChn, 1, global_video[encChn]->name);
            break;
        case 4:
            global_video[encChn]->imp_framesource = IMPFramesource::createNew(global_video[encChn]->stream, &cfg->sensor1, 3);
            global_video[encChn]->imp_encoder = IMPEncoder::createNew(global_video[encChn]->stream, encChn, 2, global_video[encChn]->name);
            break;
        case 5:
            global_video[encChn]->imp_framesource = IMPFramesource::createNew(global_video[encChn]->stream, &cfg->sensor1, 4);
            global_video[encChn]->imp_encoder = IMPEncoder::createNew(global_video[encChn]->stream, encChn, 3, global_video[encChn]->name);
            break;
        default:
            break;
    }
    global_video[encChn]->imp_framesource->enable();
    global_video[encChn]->run_for_jpeg = false;
    // inform main that initialization is complete
    sh->has_started.release();

    ret = IMP_Encoder_StartRecvPic(encChn);
        LOG_DEBUG_OR_ERROR(ret, "IMP_Encoder_StartRecvPic(" << encChn << ")");

        if (ret != 0) {
            return 0;
        }
        /* 'active' indicates, the thread is activly polling and grabbing images
        * 'running' describes the runlevel of the thread, if this value is set to false
        *           the thread exits and cleanup all ressources 
        */
        global_video[encChn]->active = true;
        global_video[encChn]->running = true;
        /*  init for alternate channel */
    #if defined(ALT_STREAM_SUPPORT)
        if ((encChn == 0) || (encChn == 1)) {
            LOG_DEBUG("entry:  Dual channel  " << encChn << " -> " << alt_encChn);	
            /*  share encoder */			
            global_video[alt_encChn]->imp_framesource = global_video[encChn]->imp_framesource;
            global_video[alt_encChn]->imp_encoder = global_video[encChn]->imp_encoder;
            global_video[alt_encChn]->imp_framesource->enable();
            LOG_DEBUG(" * ch: " << alt_encChn << ", name: " << global_video[alt_encChn]->name);
            LOG_DEBUG("   --  width: " << global_video[alt_encChn]->stream->width << ", height: " << global_video[alt_encChn]->stream->height );
            global_video[alt_encChn]->run_for_jpeg = false;
            global_video[alt_encChn]->active = true;
            global_video[alt_encChn]->running = true;
        }
    #endif

        MipiSwitch_pin = ctrls_hal::getGPIO_Pin_byName(gpio_name);
        LOG_DEBUG(gpio_name << " -  MipiSwitch_pin = " << MipiSwitch_pin);
#if 0
    // ### change to use ctrls_hal GPIO ###
	/* ### TW:  setup access GPIO for MIPI switch only needed setup once, with first stream */
    /* TODO:  maybe should move this to the main.cpp and pass down <port> pointer */
    if (encChn == 0) {
        //  GPIO mmio registers access setup
        int fd = open("/dev/mem", O_RDWR|O_SYNC);

        if (fd < 0) {
            perror("error: failed to open /dev/mem");
            return 0;
        }
        phys_mem = static_cast<uint32_t*> (mmap(NULL, 0x10020000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE));  
        if (phys_mem == MAP_FAILED) {
            perror("error: mmap failed");
            return 0;
        }
        /*  The offset in mmap does not work for mmio, use <port> to set at start of the offset */
        port = (XHAL_HandleTypeDef *) (phys_mem + (GPIO_BASE / 4));  
        /*  MIPI mode, 0:  manual, 1: FIX-sensor, 2: PTZ-sensor, 3:  MIPI sensor switching */
        if (global_MipiMode == 1) {
            port->REG[(MIPI_SET_REG - GPIO_BASE)/4] = MIPI_SW_BIT;  // set to FIX sensor
        }
        else if (global_MipiMode == 2) {
            port->REG[(MIPI_CLEAR_REG - GPIO_BASE)/4] = MIPI_SW_BIT;  // set to PTZ sensor
        }
    }
    /* ### */
#endif
    encoder_mipi.time_interval = global_time_interval;
    LOG_INFO("time interval init = " << encoder_mipi.time_interval);
    // setup buffers for channels(dummy parameters)
    // alt_stream[encChn] = std::make_shared<video_stream>(encChn-ALT_OFFSET, &cfg->stream0, "stream3");
    alt_stream[encChn] = std::make_shared<video_stream>(encChn, &cfg->stream0, "stream");

    VideoWorker worker(encChn);
    worker.run();

    /* end of frame session */
    if ((encChn == 0)  || (encChn == 1)) {
        ret = IMP_Encoder_StopRecvPic(encChn);
        LOG_DEBUG_OR_ERROR(ret, "IMP_Encoder_StopRecvPic(" << encChn << ")");
    }

    if (global_video[encChn]->imp_framesource)
    {
        global_video[encChn]->imp_framesource->disable();

        if (global_video[encChn]->imp_encoder)
        {
            global_video[encChn]->imp_encoder->deinit();
            delete global_video[encChn]->imp_encoder;
            global_video[encChn]->imp_encoder = nullptr;
        }
    }
//	close(fd);  // all done
    return 0;
}
