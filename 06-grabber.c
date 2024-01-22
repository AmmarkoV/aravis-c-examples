/* SPDX-License-Identifier:Unlicense */

/* Aravis header */
#include <arv.h>

/* Standard headers */
#include <stdlib.h>
#include <stdio.h>

// To compile :
//  meson compile -C build
// To Run : 
//  build/06-grabber

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



#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define EPOCH_YEAR_IN_TM_YEAR 1900

unsigned long tickBase = 0;
unsigned long GetTickCountMicroseconds()
{
   struct timespec ts;
   if ( clock_gettime(CLOCK_MONOTONIC,&ts) != 0) { return 0; }

   if (tickBase==0)
   {
     tickBase = ts.tv_sec*1000000 + ts.tv_nsec/1000;
     return 0;
   }

   return ( ts.tv_sec*1000000 + ts.tv_nsec/1000 ) - tickBase;
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
    if (pic==0) { return 0; }
    if ( (pic->width==0) || (pic->height==0) || (pic->channels==0) || (pic->bitsperpixel==0) )
        {
          fprintf(stderr,"saveRawImageToFile(%s) called with zero dimensions ( %ux%u %u channels %u bpp\n",filename,pic->width , pic->height,pic->channels,pic->bitsperpixel);
          return 0;
        }
    if(pic->pixels==0) { fprintf(stderr,"saveRawImageToFile(%s) called for an unallocated (empty) frame , will not write any file output\n",filename); return 0; }
    if (pic->bitsperpixel>16) { fprintf(stderr,"PNM does not support more than 2 bytes per pixel..!\n"); return 0; }

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

        fprintf(fd, "%d %d\n%u\n", pic->width, pic->height , simplePowPPM(2 ,pic->bitsperpixel)-1);

        float tmp_n = (float) pic->bitsperpixel/ 8;
        tmp_n = tmp_n *  pic->width * pic->height * pic->channels ;
        n = (unsigned int) tmp_n;

        fwrite(pic->pixels, 1 , n , fd);
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
int
main (int argc, char **argv)
{
  char dir[512]={0};
  snprintf(dir,512,".");
  unsigned int delay=0,maxFramesToGrab=10;
  unsigned int i=0;
  for (i=0; i<argc; i++)
  {
   if (strcmp(argv[i],"-o")==0)          {
                                           snprintf(dir,512,"%s",argv[i+1]);
                                           char makedircmd[512]={0};
                                           snprintf(makedircmd,512,"mkdir -p %s",dir); 
                                           int z = system(makedircmd);
                                           fprintf(stderr,"Output Path set to %s \n",dir);
                                         } else
   if (strcmp(argv[i],"--delay")==0)     {
                                           delay=atoi(argv[i+1]);
                                           fprintf(stderr,"Delay set to %u seconds \n",delay);
                                         } else
   if (strcmp(argv[i],"--maxFrames")==0) {
                                           maxFramesToGrab=atoi(argv[i+1]);
                                           fprintf(stderr,"Setting frame grab to %u \n",maxFramesToGrab);
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

		if (ARV_IS_STREAM (stream)) {
			int i;
			size_t payload;

			/* Retrieve the payload size for buffer creation */
			payload = arv_camera_get_payload (camera, &error);
			if (error == NULL) {
				/* Insert some buffers in the stream buffer pool */
				for (i = 0; i < 2; i++)
					arv_stream_push_buffer (stream, arv_buffer_new (payload, NULL));
			}

			if (error == NULL)
				/* Start the acquisition */
				arv_camera_start_acquisition (camera, &error);

			if (error == NULL) 
            {
                const void *data;
                struct Image dataAsImage={0};
                char filename[1024]={0};
                unsigned int frameNumber = 0;
                unsigned int brokenFrameNumber = 0;

				ArvBuffer *buffer;

                unsigned long startTime = GetTickCountMicroseconds();
				/* Retrieve 10 buffers */
				while  (frameNumber<maxFramesToGrab) 
                { 
					buffer = arv_stream_pop_buffer (stream);
					if (ARV_IS_BUFFER (buffer)) 
                    { 
                        dataAsImage.width        = arv_buffer_get_image_width (buffer);
                        dataAsImage.height       = arv_buffer_get_image_height (buffer); 

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
                         printf("\r %u Frames Grabbed (%u dropped) - @ %0.2f FPS  \r",frameNumber,brokenFrameNumber,(float) (endTime-startTime)/(i*1000));

                         snprintf(filename,512,"%s/colorFrame_0_%05u.pnm",dir,i);
                         WritePPM(filename,&dataAsImage); 
                         frameNumber = frameNumber+1;
                        } else
                        {
                          brokenFrameNumber = brokenFrameNumber + 1;
                        }                     

						/* Don't destroy the buffer, but put it back into the buffer pool */
						arv_stream_push_buffer (stream, buffer);
					}
				}
			}

			if (error == NULL)
				/* Stop the acquisition */
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

	return EXIT_SUCCESS;
}

