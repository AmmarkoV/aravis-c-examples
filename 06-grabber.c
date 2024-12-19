/* SPDX-License-Identifier:Unlicense */

/* Aravis header */
#include <arv.h>

/* Standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

// To compile :
//  meson compile -C build
// To Run :
//  build/06-grabber

volatile sig_atomic_t termination_requested = 0;

void sigterm_handler(int signum) {
    termination_requested = 1;
}

struct Image
{
    const unsigned char * pixels;
    unsigned int width;
    unsigned int height;
    unsigned int channels;
    unsigned int bitsperpixel;
    unsigned int image_size;
    unsigned int timestamp;
};


struct Settings
{
    unsigned int delay,maxFramesToGrab;
    unsigned int exposure; // 0 means no setting
    double       gain;
    double       blackLevel;
    double       frameRate;
    char * tickCommand;
};

#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define EPOCH_YEAR_IN_TM_YEAR 1900

unsigned long tickBase = 0;
unsigned long GetTickCountMicroseconds()
{
    struct timespec ts;
    if ( clock_gettime(CLOCK_MONOTONIC,&ts) != 0) {
        return 0;
    }

    if (tickBase==0)
    {
        tickBase = ts.tv_sec*1000000 + ts.tv_nsec/1000;
        return 0;
    }

    return ( ts.tv_sec*1000000 + ts.tv_nsec/1000 ) - tickBase;
}

int writeSettings(const char * filename,struct Settings * settings)
{
    FILE * fp = fopen(filename,"w");
    if (fp!=0)
    {
        fprintf(fp,"{\n\"delay\": %u,\n",settings->delay);
        fprintf(fp,"\"maxFramesToGrab\": %u,\n",settings->maxFramesToGrab);
        fprintf(fp,"\"exposure\": %u,\n",settings->exposure);
        fprintf(fp,"\"blackLevel\": %f,\n",settings->blackLevel);
        fprintf(fp,"\"gain\": %f,\n",settings->gain);
        fprintf(fp,"\"frameRate\": %f,\n",settings->frameRate);
        fprintf(fp,"\"tickCommand\": \"%s\",\n}\n",settings->tickCommand);
        fclose(fp);
        return 1;
    }
    return 0;
}


unsigned int simplePowPPM(unsigned int base,unsigned int exp)
{
    if (exp==0) return 1;
    unsigned int retres=base;
    unsigned int i=0;
    for (i=0; i<exp-1; i++)
    {
        retres*=base;
    }
    return retres;
}


int WritePPM(const char * filename,struct Image * pic)
{
    //fprintf(stderr,"saveRawImageToFile(%s) called\n",filename);
    if (pic==0) {
        return 0;
    }
    if ( (pic->width==0) || (pic->height==0) || (pic->channels==0) || (pic->bitsperpixel==0) )
    {
        fprintf(stderr,"saveRawImageToFile(%s) called with zero dimensions ( %ux%u %u channels %u bpp\n",filename,pic->width, pic->height,pic->channels,pic->bitsperpixel);
        return 0;
    }
    if(pic->pixels==0) {
        fprintf(stderr,"saveRawImageToFile(%s) called for an unallocated (empty) frame , will not write any file output\n",filename);
        return 0;
    }
    if (pic->bitsperpixel>16) {
        fprintf(stderr,"PNM does not support more than 2 bytes per pixel..!\n");
        return 0;
    }

    FILE *fd=0;
    fd = fopen(filename,"wb");

    if (fd!=0)
    {
        unsigned int n;
        if (pic->channels==3) fprintf(fd, "P6\n");
        else if (pic->channels==1) fprintf(fd, "P5\n");
        else
        {
            fprintf(stderr,"Invalid channels arg (%u) for SaveRawImageToFile\n",pic->channels);
            fclose(fd);
            return 1;
        }

        fprintf(fd, "%d %d\n%u\n", pic->width, pic->height, simplePowPPM(2,pic->bitsperpixel)-1);

        float tmp_n = (float) pic->bitsperpixel/ 8;
        tmp_n = tmp_n *  pic->width * pic->height * pic->channels ;
        n = (unsigned int) tmp_n;

        fwrite(pic->pixels, 1, n, fd);
        fflush(fd);
        fclose(fd);
        return 1;
    }
    else
    {
        fprintf(stderr,"SaveRawImageToFile could not open output file %s\n",filename);
        return 0;
    }
    return 0;
}




/*
 * Connect to the first available camera, then acquire 10 buffers.
 */
int main (int argc, char **argv)
{
// Set up SIGTERM signal handler
    struct sigaction action;
    action.sa_handler = sigterm_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, NULL);

    guint64 n_completed_buffers=0, n_failures=0, n_underruns=0;

    char dir[512]= {0};
    snprintf(dir,512,".");

    unsigned int i=0;
    unsigned int ARV_VIEWER_N_BUFFERS=10;
    struct Settings settings= {0};
    struct Image dataAsImage= {0};
    settings.maxFramesToGrab = 10;
    char forceDims = 0;
    char refreshDimsOnEachFrame = 1;

    for (i=0; i<argc; i++)
    {
        if (strcmp(argv[i],"-o")==0)          {
            snprintf(dir,512,"%s",argv[i+1]);
            char makedircmd[1025]= {0};
            snprintf(makedircmd,1024,"mkdir -p %s",dir);
            int z = system(makedircmd);
            if (z==0)
            {
                fprintf(stderr,"Output Path set to \"%s\" \n",dir);
            }
            else
            {
                fprintf(stderr,"Failed setting output Path to \"%s\" \n",dir);
            }
        } else if (strcmp(argv[i],"--norefresh")==0) {
            refreshDimsOnEachFrame=0;
        } else if (strcmp(argv[i],"--size")==0)      {
            forceDims=1;
            dataAsImage.width=atoi(argv[i+1]);
            dataAsImage.height=atoi(argv[i+2]);
            fprintf(stderr,"Camera size set to %u x %u pixels \n",dataAsImage.width,dataAsImage.height);
        } else if (strcmp(argv[i],"--delay")==0)     {
            settings.delay=atoi(argv[i+1]);
            fprintf(stderr,"Delay set to %u seconds \n",settings.delay);
        } else if (strcmp(argv[i],"--tick")==0)     {
            settings.tickCommand=argv[i+1];
            fprintf(stderr,"Setting tick command to %s \n",settings.tickCommand);
        } else if (strcmp(argv[i],"--buffers")==0)   {
            ARV_VIEWER_N_BUFFERS=atoi(argv[i+1]);
            fprintf(stderr,"ARV_VIEWER_N_BUFFERS = %u \n",ARV_VIEWER_N_BUFFERS);
        } else if (strcmp(argv[i],"--exposure")==0)  {
            settings.exposure=atoi(argv[i+1]);
            fprintf(stderr,"Exposure will be set to %u μsec \n",settings.exposure);
        } else if (strcmp(argv[i],"--gain")==0)      {
            settings.gain=atof(argv[i+1]);
            fprintf(stderr,"Gain will be set to %f \n",settings.gain);
        } else if (strcmp(argv[i],"--fps")==0)      {
            settings.frameRate=atof(argv[i+1]);
            fprintf(stderr,"Framerate will be set to %f Hz \n",settings.frameRate);
        } else if (strcmp(argv[i],"--blacklevel")==0) {
            settings.blackLevel=atof(argv[i+1]);
            fprintf(stderr,"Black Level will be set to %f μsec \n",settings.blackLevel);
        } else if (strcmp(argv[i],"--maxFrames")==0) {
            settings.maxFramesToGrab=atoi(argv[i+1]);
            fprintf(stderr,"Setting frame grab to %u \n",settings.maxFramesToGrab);
        }



    }

    ArvCamera *camera;
    GError *error = NULL;

    /* Connect to the first available camera */
    camera = arv_camera_new (NULL, &error);

    if (ARV_IS_CAMERA (camera)) {
        ArvStream *stream = NULL;

        printf ("Found camera '%s'\n", arv_camera_get_model_name (camera, NULL));

        arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);

        if (error == NULL)
            /* Create the stream object without callback */
            stream = arv_camera_create_stream (camera, NULL, NULL, &error);

        if (ARV_IS_STREAM (stream))
        {
            int i;
            size_t payload;

            /* Retrieve the payload size for buffer creation */
            payload = arv_camera_get_payload (camera, &error);
            if (error == NULL) {
                /* Insert some buffers in the stream buffer pool */
                for (i = 0; i < ARV_VIEWER_N_BUFFERS; i++)
                    arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));
            }


            arv_stream_set_emit_signals (stream, TRUE);
            arv_stream_create_buffers(stream, ARV_VIEWER_N_BUFFERS, NULL, NULL, NULL);


            if (error == NULL)
                /* Start the acquisition */
                arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, NULL);
            arv_camera_start_acquisition (camera, &error);

            if (settings.exposure!=0)
            {
                arv_camera_set_exposure_time(camera, settings.exposure, NULL);
            }
            if (settings.gain!=0.0)
            {
                arv_camera_set_gain (camera, settings.gain, NULL);
            }
            if (settings.blackLevel!=0.0)
            {
                arv_camera_set_black_level(camera, settings.blackLevel, NULL);
            }
            if (settings.frameRate!=0.0)
            {
                arv_camera_set_frame_rate (camera, settings.frameRate, NULL);
            }




            if (error == NULL)
            {
                if ( (!refreshDimsOnEachFrame)&& (!forceDims) )
                {   //Poll dims so that we know them in advance if we dont want to get them from each buffer, and we dont want to force a specific dimension
                    int minvalue=0,maxvalue=0;
                    arv_camera_get_width_bounds(camera,&minvalue,&maxvalue,NULL);
                    dataAsImage.width  = (unsigned int) maxvalue;
                    arv_camera_get_height_bounds(camera,&minvalue,&maxvalue,NULL);
                    dataAsImage.height  = (unsigned int) maxvalue;
                }

                if ( (!refreshDimsOnEachFrame) || (forceDims) )
                {   //Attempt to setup region if there is no autorefresh of dimensions, or we want to force a specific dimension
                    arv_camera_set_region(camera,0,0,dataAsImage.width,dataAsImage.height,NULL); //Use full sensor area
                }

                const void *data;
                char filename[1025]= {0};
                unsigned int frameNumber = 0;
                unsigned int brokenFrameNumber = 0;
                ArvBuffer *buffer;

                snprintf(filename,1024,"%s/info.json",dir);
                writeSettings(filename,&settings);

                unsigned long startTime = GetTickCountMicroseconds();

                unsigned long startGrab, endGrab;
                unsigned long microsecondsGrab;
                unsigned long timeToSleepToWaitFor1Frame = 0;

                if (settings.frameRate!=0.0)
                {
                    fprintf(stderr,"Waiting initial period to buffer at least one frame..\n");
                    timeToSleepToWaitFor1Frame = 1000000 / settings.frameRate;
                    usleep(timeToSleepToWaitFor1Frame);
                }




                while  (!termination_requested && frameNumber<settings.maxFramesToGrab)
                {
                    startGrab = GetTickCountMicroseconds();
                    buffer = arv_stream_pop_buffer (stream);
                    if (ARV_IS_BUFFER(buffer))
                    {
                        if (refreshDimsOnEachFrame)
                        {
                            dataAsImage.width        = arv_buffer_get_image_width (buffer);
                            dataAsImage.height       = arv_buffer_get_image_height (buffer);
                        }

                        if ((dataAsImage.width!=0) && (dataAsImage.height!=0))
                        {
                            size_t size;
                            data = arv_buffer_get_image_data(buffer,&size);
                            //printf ("Size =  %lu\n",size);
                            dataAsImage.pixels       = data;

                            dataAsImage.channels     = 1;
                            dataAsImage.bitsperpixel = 8;
                            dataAsImage.image_size   = dataAsImage.width  * dataAsImage.height * dataAsImage.channels;
                            dataAsImage.timestamp    = i;

                            /* Display some informations about the retrieved buffer */
                            //printf ("Acquired %d×%d buffer\n",dataAsImage.width,dataAsImage.height);
                            unsigned long endTime = GetTickCountMicroseconds();


                            arv_stream_get_statistics (stream,&n_completed_buffers,&n_failures,&n_underruns);
                            float frameRate = arv_camera_get_frame_rate (camera, NULL);
                            printf("\r %u Frames Grabbed (%u dropped) - @ %0.2f FPS (set %0.2f) ",frameNumber,brokenFrameNumber,(float) frameNumber / ((endTime-startTime)/1000000), frameRate );
                            printf("Ok %lu/Fail %lu/Under %lu    \r",n_completed_buffers,n_failures,n_underruns);

                            snprintf(filename,1024,"%s/colorFrame_0_%05u.pnm",dir,frameNumber);
                            WritePPM(filename,&dataAsImage);
                            frameNumber = frameNumber+1;

                            if (settings.tickCommand!=0)
                            {
                                system(settings.tickCommand);
                            }

                        } else
                        {
                            brokenFrameNumber = brokenFrameNumber + 1;
                        }

                        /* Don't destroy the buffer, but put it back into the buffer pool */
                        arv_stream_push_buffer (stream, buffer);
                    } else
                    {
                        usleep(timeToSleepToWaitFor1Frame);
                    }

                    endGrab = GetTickCountMicroseconds();

                    if (settings.frameRate!=0.0)
                    {   //Enforce framrates to prevent buffer underrun
                        microsecondsGrab = endGrab - startGrab;
                        // Calculate the time to usleep to achieve target framerate
                        unsigned long targetMicroseconds = 1000000 / settings.frameRate;
                        if (microsecondsGrab < targetMicroseconds)
                        {
                            usleep(targetMicroseconds - microsecondsGrab);
                        }
                    }//We have a framerate set
                } //While loop
            } // No initialization error

            if (error == NULL)
                /* Stop the acquisition */
                arv_stream_set_emit_signals (stream, FALSE);
            arv_camera_stop_acquisition (camera, &error);

            /* Destroy the stream object */
            g_clear_object (&stream);
        }

        /* Destroy the camera instance */
        g_clear_object (&camera);
    }

    if (error != NULL) {
        /* En error happened, display the correspdonding message */
        printf ("Error: %s\n", error->message);
        return EXIT_FAILURE;
    }

    printf("\n\nDone\n");
    printf("Summary : Ok %lu/Fail %lu/Under %lu    \r",n_completed_buffers,n_failures,n_underruns);

    if (settings.exposure!=0)
    {
        printf("Exposure time was %u",settings.exposure);
        printf("This is equivalent to %0.2f FPS",(float) 1000000.0/settings.exposure);
    }
    return EXIT_SUCCESS;
}

