#define SAVE_IMAGES false // true to continuously save images
#define SAVE_LOCATION1 "/mnt/SAAS/images/" //Save locations for FITS files
#define MOD_SAVE 30
#define TIMESTAMP_LENGTH       19
#define PRINT_TO_FILE true // Default for whether print statements are sent to screen or file.

#define MAX_THREADS            10
#define MAX_SAVE_THREADS       4
#define SLEEP_CAMERA_CONNECT   1    // waits for errors while connecting to camera
#define SLEEP_KILL             2    // waits when killing all threads
#define DEFAULT_CALIB_CENTER_X       648    // the default calibrated screen center for HUD display
#define DEFAULT_CALIB_CENTER_Y       483    // the default calibrated screen center for HUD display
#define NUM_CIRCLE_SEGMENTS   30    // the number of line segments to use for circles
#define NUM_XPIXELS         1296    // number of X pixels of sensor
#define NUM_YPIXELS         966     // number of Y pixels of sensor

// Camera Defaults, overwritten by program_settings.txt
#define DEFAULT_CAMERA_EXPOSURE 10000
#define DEFAULT_CAMERA_AGAIN    400
#define DEFAULT_CAMERA_PGAIN    -3
#define DEFAULT_CAMERA_BLEVEL   0

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>    /* for multithreading */
#include <signal.h>     /* for signal() */
#include <unistd.h>     /* for sleep()  */
#include <stdint.h>     /* for uint_16 */
#include <inttypes.h>   /* for fscanf uint types */
// openGL libraries
#include <GL/gl.h>
#include <GL/glut.h>

#include "compression.hpp"

// imperx camera libraries
#include <PvSampleUtils.h>
#include <PvDevice.h>
#include <PvPipeline.h>
#include <PvBuffer.h>
#include <PvStream.h>
#include <PvStreamRaw.h>
#include <PvSystem.h>
#include <PvInterface.h>
#include <PvDevice.h>

//#include <opencv.hpp>

// global declarations
// width and height of IMPERX Camera frame
static float width = NUM_XPIXELS;
static float height = NUM_YPIXELS;
static float arcsec_to_pixel = 3.47;   // the plate scale
long int frameCount = 0;
long int saveCount = 0;  // counter for the number of images saved to disk
float camera_temperature = 0.0;

unsigned int calib_center_x = DEFAULT_CALIB_CENTER_X;
unsigned int calib_center_y = DEFAULT_CALIB_CENTER_Y;

bool isSavingImages = SAVE_IMAGES;
unsigned int max_save_threads = MAX_SAVE_THREADS;
unsigned int save_threads_count = 0;
unsigned int mod_save = MOD_SAVE;

FILE* file_ptr = NULL; // Pointer for general files.
static FILE* print_file_ptr = NULL; // Pointer to where print statements should be sent.

char message[100] = "Starting Up";
int cameraID = 0;

// to store the image
unsigned char *data = new unsigned char[NUM_XPIXELS * NUM_YPIXELS];
// to write the image
unsigned char *data_save = new unsigned char[NUM_XPIXELS * NUM_YPIXELS];

GLuint texture[1];      	// Storage for one texture to display the camera image

typedef struct CameraSettings{
    uint16_t exposure;
    uint16_t analogGain;
    int16_t preampGain;
    int blackLevel;
} CameraSettings;

// load default values, should be overwritten by program_settings.txt if exists
CameraSettings settings = { .exposure = DEFAULT_CAMERA_EXPOSURE, .analogGain = DEFAULT_CAMERA_AGAIN,
                            .preampGain = DEFAULT_CAMERA_PGAIN, .blackLevel = DEFAULT_CAMERA_BLEVEL};

bool is_camera_ready = false;


// related to threads
bool stop_message[MAX_THREADS];
pthread_t threads[MAX_THREADS];
bool started[MAX_THREADS];
pthread_mutex_t mutexStartThread; //Keeps new threads from being started simultaneously

sig_atomic_t volatile g_running = 1;

struct Thread_data{
    int thread_id;
    int camera_id;
    uint16_t command_key;
    uint8_t command_num_vars;
    uint16_t command_vars[15];
};
struct Thread_data thread_data[MAX_THREADS];


//Function declarations
void sig_handler(int signum);
void start_thread(void *(*start_routine) (void *), const Thread_data *tdata);
static int current_time(void);
void framerate(void);
static void gl_load_gltextures();
void gl_draw_string( int x, int y, char *str );
void gl_draw_circle(float cx, float cy, float r, int num_segments);
void gl_init(void);
void gl_display (void);
void gl_reshape (int w, int h);
void gl_switchToOrtho (void);

void keyboard (unsigned char key, int x, int y);
void *CameraThread( void * threadargs, int camera_id);
void *ImageSaveThread(void *threadargs);
void read_calibrated_ccd_center(void);
void read_settings(void);
void kill_all_threads();
void writeCurrentUT(char *buffer);

// utilities
timespec TimespecDiff(timespec start, timespec end);


timespec TimespecDiff(timespec start, timespec end){
    timespec diff;
    if ((end.tv_nsec - start.tv_nsec)<0) {
        diff.tv_sec = end.tv_sec - start.tv_sec-1;
        diff.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        diff.tv_sec = end.tv_sec - start.tv_sec;
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return diff;
}

void writeCurrentUT(char *buffer)
{
    struct timeval tv;
    struct tm *now_tm;

    gettimeofday(&tv, NULL);    // Get seconds and microseconds
    now_tm = gmtime(&tv.tv_sec);

    sprintf(buffer, "%04d%02d%02d_%02d%02d%02d_%03d", now_tm->tm_year+1900, now_tm->tm_mon+1,
            now_tm->tm_mday, now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec, (int)(tv.tv_usec/1000));

}

static int current_time(void)
{
    // return current time (in seconds)
    // used to calculate frame rate
    struct timeval tv;
    struct timezone tz;
    (void) gettimeofday(&tv, &tz);
    return (int) tv.tv_sec;
}

void saverate(void){
    // Calculate and display the rate at which images are being saved
}

void framerate(void){
    // calc framerate
    static int t0 = -1;
    static int frames = 0;
    int t = current_time();

    if (t0 < 0)
        t0 = t;

    frames++;

    if (t - t0 >= 5.0) {
        GLfloat seconds = t - t0;
        GLfloat fps = frames / seconds;
        fprintf(print_file_ptr, "%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds, fps);
        t0 = t;
        frames = 0;
    }
}

static void gl_load_gltextures()
{
    // Load bitmaps and convert to textures
    //for( int i = 0; i < height * width; i++){
    //   data[i] = rand();
    //}

    // Typical texture generation using data from the bitmap
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, (GLvoid*)data);
}

void gl_draw_string( int x, int y, char *str ) {
    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f);
    glRasterPos2i( x, y );

    for ( int i=0, len=strlen(str); i<len; i++ ) {
        if ( str[i] == '\n' ) {
            y -= 16;
            glRasterPos2i( x, y );
        } else {
            glutBitmapCharacter( GLUT_BITMAP_TIMES_ROMAN_24, str[i] );
        }
    }
}

void gl_draw_circle(float cx, float cy, float r, int num_segments)
{
    // code courtesy of SiegeLord's Abode
    // http://slabode.exofire.net/circle_draw.shtml
    float theta = 2 * 3.1415926 / float(num_segments);
    float c = cosf(theta);//precalculate the sine and cosine
    float s = sinf(theta);
    float t;

    float x = r;//we start at angle = 0
    float y = 0;

    glBegin(GL_LINE_LOOP);
    for(int ii = 0; ii < num_segments; ii++)
    {
        glVertex2f(x + cx, y + cy);//output vertex

        //apply the rotation matrix
        t = x;
        x = c * x - s * y;
        y = s * t + c * y;
    }
    glEnd();
}

void kill_all_threads()
{
    for(int i = 0; i < MAX_THREADS; i++ ){
        if (started[i]) {
            stop_message[i] = true;
        }
    }
    sleep(SLEEP_KILL);
    for(int i = 0; i < MAX_THREADS; i++ ){
        if (started[i]) {
            fprintf(print_file_ptr, "Quitting thread %i, quitting status is %i\n", i, pthread_cancel(threads[i]));
            started[i] = false;
        }
    }
}

void *CameraThread( void * threadargs)
{
    // camera_id refers to 0 PYAS, 1 is RAS (if valid)
    long tid = (long)((struct Thread_data *)threadargs)->thread_id;
    fprintf(print_file_ptr, "Camera thread #%ld!\n", tid);

    //timespec frameRate = {0,FRAME_CADENCE*1000};
    bool cameraReady = false;

    //cv::Mat localFrame, mockFrame;
    //HeaderData localHeader;
    //timespec localCaptureTime, preExposure, postExposure, timeElapsed, timeToWait;

    char lDoodle[] = "|\\-|-/";
    int lDoodleIndex = 0;
    PvInt64 lImageCountVal = 0;
    double lFrameRateVal = 0.0;
    double lBandwidthVal = 0.0;
    char thread_message[100];

    PvSystem lSystem;
    PvDeviceInfo* lDeviceInfo;
    PvResult lResult;
    PvDevice lDevice;
    PvStream lStream;
    PvPipeline *lPipeline;
    PvGenParameterArray *lStreamParams;
    PvGenParameterArray *lDeviceParams;
    PvUInt32 lDeviceCount;

    while(!stop_message[tid])
    {
        if (!cameraReady){
            strcpy( message, "Searching for Camera.");
            fprintf(print_file_ptr, "%s\n", message);

            // Find all GEV Devices on the network.
            lSystem.SetDetectionTimeout( 2000 );
            lResult = lSystem.Find();
            if( !lResult.IsOK() )
            {
                sprintf(message, "PvSystem::Find Error: %s", lResult.GetCodeString().GetAscii() );
                fprintf(print_file_ptr, "%s\n", message);
                cameraReady = false;
                sleep(SLEEP_CAMERA_CONNECT);
            } else {
                // Get the number of GEV Interfaces that were found using GetInterfaceCount.
                PvUInt32 lInterfaceCount = lSystem.GetInterfaceCount();

                // Display information about all found interface
                // For each interface, display information about all devices.
                for( PvUInt32 x = 0; x < lInterfaceCount; x++ ){
                    // get pointer to each of interface
                    PvInterface * lInterface = lSystem.GetInterface( x );

                    sprintf(message, "Interface %i\nMAC Address: %s\nIP Address: %s\nSubnet Mask: %s\n\n",
                           x,
                           lInterface->GetMACAddress().GetAscii(),
                           lInterface->GetIPAddress().GetAscii(),
                           lInterface->GetSubnetMask().GetAscii() );
                    fprintf(print_file_ptr, "%s\n", message);

                    // Get the number of GEV devices that were found using GetDeviceCount.
                    lDeviceCount = lInterface->GetDeviceCount();

                    for( PvUInt32 y = 0; y < lDeviceCount ; y++ )
                    {
                        lDeviceInfo = lInterface->GetDeviceInfo( y );
                        sprintf(message, "Device %i\nMAC Address: %s\nIP Address: %s\nSerial number: %s\n",
                               y,
                               lDeviceInfo->GetMACAddress().GetAscii(),
                               lDeviceInfo->GetIPAddress().GetAscii(),
                               lDeviceInfo->GetSerialNumber().GetAscii() );
                        fprintf(print_file_ptr, "%s\n", message);
                        cameraID = atoi(lDeviceInfo->GetSerialNumber().GetAscii());
                    }
                }

                // If no device is selected, abort
                if( lDeviceCount == 0 ){
                    sprintf(message, "No Camera Found.\n" );
                    fprintf(print_file_ptr, "%s\n", message);
                    cameraReady = false;
                    sleep(SLEEP_CAMERA_CONNECT);
                } else {
                    // Connect to the GEV Device
                    sprintf(message, "Connecting to %s\n", lDeviceInfo->GetMACAddress().GetAscii() );
                    fprintf(print_file_ptr, "%s\n", message);
                    //sprintf(serial_number, lDeviceInfo->GetSerialNumber().GetAscii());
                    if ( !lDevice.Connect( lDeviceInfo ).IsOK() ){
                        fprintf(print_file_ptr, "Unable to connect to %s\n", lDeviceInfo->GetMACAddress().GetAscii() );
                        cameraReady = false;
                        sleep(SLEEP_CAMERA_CONNECT);
                    } else {
                        sprintf(message, "Successfully connected to %s %s\n", lDeviceInfo->GetMACAddress().GetAscii(), lDeviceInfo->GetSerialNumber().GetAscii() );
                        fprintf(print_file_ptr, "%s\n", message);

                        // Get device parameters need to control streaming
                        lDeviceParams = lDevice.GetGenParameters();

                        // Negotiate streaming packet size
                        lDevice.NegotiatePacketSize();

                        // Open stream - have the PvDevice do it for us
                        sprintf(message, "Opening stream to device\n" );
                        fprintf(print_file_ptr, "%s\n", message);
                        lStream.Open( lDeviceInfo->GetIPAddress() );

                        // Create the PvPipeline object
                        lPipeline = new PvPipeline( &lStream );

                        // Reading payload size from device
                        PvInt64 lSize = 0;
                        lDeviceParams->GetIntegerValue( "PayloadSize", lSize );

                        // Set the Buffer size and the Buffer count
                        lPipeline->SetBufferSize( static_cast<PvUInt32>( lSize ) );
                        lPipeline->SetBufferCount( 16 ); // Increase for high frame rate without missing block IDs

                        // Have to set the Device IP destination to the Stream
                        lDevice.SetStreamDestination( lStream.GetLocalIPAddress(), lStream.GetLocalPort() );

                        // IMPORTANT: the pipeline needs to be "armed", or started before
                        // we instruct the device to send us images
                        sprintf(message, "Starting pipeline\n" );
                        fprintf(print_file_ptr, "%s\n", message);

                        // set camera settings
                        PvResult outcome;
                        lDeviceParams->SetBooleanValue("ProgFrameTimeEnable", false);
                        outcome = lDeviceParams->SetEnumValue("ExposureMode", "Timed");
                        outcome = lDeviceParams->SetIntegerValue("ExposureTimeRaw", settings.exposure);
                        outcome = lDeviceParams->SetIntegerValue("GainRaw", settings.analogGain);

                        PvString value;
                        switch(settings.preampGain)
                        {
                            case -3:
                                value = "minus3dB";
                                break;
                            case 0:
                                value = "zero_dB";
                                break;
                            case 3:
                                value = "plus3dB";
                                break;
                            case 6:
                                value = "plus6dB";
                                break;
                            default:
                                value = "zero_dB";
                        }

                        outcome = lDeviceParams->SetEnumValue("PreAmpRaw", value);
                        if (settings.blackLevel >= 0 && settings.blackLevel <= 1023){
                            outcome = lDeviceParams->SetIntegerValue("BlackLevelRaw", settings.blackLevel);
                        }

                        lPipeline->Start();

                        // Get stream parameters/stats
                        lStreamParams = lStream.GetParameters();

                        // TLParamsLocked is optional but when present, it MUST be set to 1
                        // before sending the AcquisitionStart command
                        lDeviceParams->SetIntegerValue( "TLParamsLocked", 1 );

                        sprintf(message, "Resetting timestamp counter...\n" );
                        fprintf(print_file_ptr, "%s\n", message);
                        lDeviceParams->ExecuteCommand( "GevTimestampControlReset" );

                        // The pipeline is already "armed", we just have to tell the device
                        // to start sending us images
                        sprintf(message, "Sending StartAcquisition command to device\n" );
                        fprintf(print_file_ptr, "%s\n", message);
                        lDeviceParams->ExecuteCommand( "AcquisitionStart" );

                        cameraReady = true;
                        frameCount = 0;
                    }
                }
            }
        }
        else    // camera is ready so start getting images
        {
            // Retrieve next buffer
            PvBuffer *lBuffer = NULL;
            PvResult  lOperationResult;
            PvResult lResult = lPipeline->RetrieveNextBuffer( &lBuffer, 1000, &lOperationResult );

            if ( lResult.IsOK() )
            {
                if ( lOperationResult.IsOK() ){
                    //
                    // We now have a valid buffer. This is where you would typically process the buffer.
                    // -----------------------------------------------------------------------------------------
                    // ...

                    lStreamParams->GetIntegerValue( "ImagesCount", lImageCountVal );
                    lStreamParams->GetFloatValue( "AcquisitionRateAverage", lFrameRateVal );
                    lStreamParams->GetFloatValue( "BandwidthAverage", lBandwidthVal );

                    // If the buffer contains an image, display width and height
                    PvUInt32 lWidth = 0, lHeight = 0;
                    if ( lBuffer->GetPayloadType() == PvPayloadTypeImage )
                    {
                        // Get image specific buffer interface
                        PvImage *lImage = lBuffer->GetImage();

                        // Read width, height
                        lWidth = lBuffer->GetImage()->GetWidth();
                        lHeight = lBuffer->GetImage()->GetHeight();
                        data = lImage->GetDataPointer();

                        // Get the camera temperature - this may slow down image aquisition...
                        long long int lTempValue = -512;
                        char timestamp[TIMESTAMP_LENGTH];
                        writeCurrentUT(timestamp);

                        lDevice.GetGenParameters()->GetIntegerValue( "GetTemperature", lTempValue );
                        if (lTempValue >= 512) lTempValue = lTempValue - 1024;
                        camera_temperature = (float)lTempValue / 4.;
                        sprintf(message, "%s - Acquiring: %5.1f C", timestamp, camera_temperature );

                        if (frameCount % mod_save == 0 && save_threads_count < max_save_threads){
                            // Increment save threads counter.
                            save_threads_count++;
                            sprintf(thread_message, "Incrementing save_threads_count.  Was %u.  Now %u\n",
                                save_threads_count-1, save_threads_count);
                            fprintf(print_file_ptr, "%s", thread_message);

                            // start thread to save the image.
                            Thread_data tdata;
                            start_thread(ImageSaveThread, &tdata);

                            // Decrement save threads counter;
                            save_threads_count--;
                            sprintf(thread_message, "Decrementing save_threads_count. Was %u.  Now %u.\n", save_threads_count+1, save_threads_count);
                            fprintf(print_file_ptr, "%s", thread_message);
                        }
                        frameCount++;
                    }
                    char block_message[255];
                    char timestamp[TIMESTAMP_LENGTH];
                    writeCurrentUT(timestamp);
                    sprintf(block_message, "\n %c BlockID: %016llX W: %i H: %i %.01f FPS %.01f Mb/s at %s\r\n",
                            lDoodle[ lDoodleIndex ],
                            lBuffer->GetBlockID(),
                            lWidth,
                            lHeight,
                            lFrameRateVal,
                            lBandwidthVal / 1000000.0,
                            timestamp);
                    fprintf(print_file_ptr, block_message);
                // We have an image - do some processing (...) and VERY IMPORTANT,
                // release the buffer back to the pipeline
                }
                lPipeline->ReleaseBuffer( lBuffer );
            }
            else
            {
                // Timeout
                fprintf(print_file_ptr, "%c Timeout\r", lDoodle[ lDoodleIndex ] );
            }
        }
    }

    delete lPipeline;
    lPipeline = NULL;

    fprintf(print_file_ptr, "CameraStream thread #%ld exiting\n", tid);
    // clean up the camera
    // Tell the device to stop sending images
    fprintf(print_file_ptr, "Sending AcquisitionStop command to the device\n" );
    lDeviceParams->ExecuteCommand( "AcquisitionStop" );

    // If present reset TLParamsLocked to 0. Must be done AFTER the
    // streaming has been stopped
    lDeviceParams->SetIntegerValue( "TLParamsLocked", 0 );

    // We stop the pipeline - letting the object lapse out of
    // scope would have had the destructor do the same, but we do it anyway
    fprintf(print_file_ptr, "Stop pipeline\n" );
    lPipeline->Stop();

    // Now close the stream. Also optionnal but nice to have
    fprintf(print_file_ptr, "Closing stream\n" );
    lStream.Close();

    // Finally disconnect the device. Optional, still nice to have
    fprintf(print_file_ptr, "Disconnecting device\n" );
    lDevice.Disconnect();

    cameraReady = false;
    started[tid] = false;
    pthread_exit( NULL );
}

void sig_handler(int signum)
{
    if ((signum == SIGINT) || (signum == SIGTERM))
    {
        if (signum == SIGINT) std::cerr << "Keyboard interrupt received\n";
        if (signum == SIGTERM) std::cerr << "Termination signal received\n";
        g_running = 0;
    }
}

void start_thread(void *(*routine) (void *), const Thread_data *tdata)
{
    pthread_mutex_lock(&mutexStartThread);

    int i = 0;
    while (started[i] == true) {
        i++;
        if (i == MAX_THREADS) return; //should probably throw an exception
    }

    //Copy the thread data to a global to prevent deallocation
    if (tdata != NULL) memcpy(&thread_data[i], tdata, sizeof(Thread_data));
    thread_data[i].thread_id = i;

    stop_message[i] = false;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int rc = pthread_create(&threads[i], &attr, routine, &thread_data[i]);
    if (rc != 0) {
        fprintf(print_file_ptr, "ERROR; return code from pthread_create() is %d\n", rc);
    } else started[i] = true;

    pthread_attr_destroy(&attr);
    pthread_mutex_unlock(&mutexStartThread);

    return;
}

void gl_init(void) {
    glGenTextures(1, &texture[0]);		// Create the texture
    glBindTexture(GL_TEXTURE_2D, texture[0]);       // Select our texture

    //glEnable (GL_BLEND);
    //glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glShadeModel(GL_SMOOTH);                    // Enable Smooth Shading
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);       // Black Background
    glClearDepth(1.0f);                         // Depth Buffer Setup
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);     // Set Line Antialiasing
    glLineWidth(2.0);       // change the thickness of the HUD lines
    //gl_load_gltextures();
    glDisable(GL_DEPTH_TEST);
    //glDepthMask(GL_TRUE);
    //glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);				// Black Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	//glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glEnable(GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);
}

void gl_display (void) {
    // the drawing function
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear Screen And Depth Buffer
	glLoadIdentity();									// Reset The Current Modelview Matrix
	//glPushMatrix();

	gl_load_gltextures();

    // draw the camera image as a texture
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(0.0f, 0.0f, 0.0f);	// Bottom left of the texture and quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f(width, 0.0f, 0.0f);	// Bottom right of the texture and quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f(width,  height, 0.0f);	// Top right of the texture and quad
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f,  height, 0.0f);	// Top left of the texture and quad
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // draw the HUD
	glColor4f(1, 1, 1, 1);
    // draw the message string
	gl_draw_string(100, 100, message);

    // X - line
	glBegin(GL_LINES);
    glVertex2f(0.0f, NUM_YPIXELS - calib_center_y);
    glVertex2f(width, NUM_YPIXELS - calib_center_y);
    glEnd();

    // Y - line
    glBegin(GL_LINES);
    glVertex2f(calib_center_x, 0.0f);
    glVertex2f(calib_center_x, height);
    glEnd();

    // Sun is 32 arcminutes across (radius of 16 arcminutes)
    // half a Sun
    gl_draw_circle(calib_center_x, NUM_YPIXELS - calib_center_y, 8*60 / arcsec_to_pixel, NUM_CIRCLE_SEGMENTS);
    // full Sun
    gl_draw_circle(calib_center_x, NUM_YPIXELS - calib_center_y, 16*60 / arcsec_to_pixel, NUM_CIRCLE_SEGMENTS);
    // 1.5 Sun
    gl_draw_circle(calib_center_x, NUM_YPIXELS - calib_center_y, 24*60 / arcsec_to_pixel, NUM_CIRCLE_SEGMENTS);

    for (int i = 0; i < 48; i++) {
        glBegin(GL_LINES);
        if (i < 24) {
            glVertex2f(calib_center_x - 10, NUM_YPIXELS - calib_center_y + i * 60 / arcsec_to_pixel);
            glVertex2f(calib_center_x + 10, NUM_YPIXELS - calib_center_y + i * 60 / arcsec_to_pixel);
        } else {
            glVertex2f(calib_center_x - 10, NUM_YPIXELS - calib_center_y + (24 - i) * 60 / arcsec_to_pixel);
            glVertex2f(calib_center_x + 10, NUM_YPIXELS - calib_center_y + (24 - i) * 60 / arcsec_to_pixel);
        }
        glEnd();
    }


    for (int i = 0; i < 48; i++) {
        glBegin(GL_LINES);
        if (i < 24) {
            glVertex2f(calib_center_x + i * 60 / arcsec_to_pixel, NUM_YPIXELS - calib_center_y - 10);
            glVertex2f(calib_center_x + i * 60 / arcsec_to_pixel, NUM_YPIXELS - calib_center_y + 10);
        } else {
            glVertex2f(calib_center_x + (24 - i) * 60 / arcsec_to_pixel, NUM_YPIXELS - calib_center_y - 10);
            glVertex2f(calib_center_x + (24 - i) * 60 / arcsec_to_pixel, NUM_YPIXELS - calib_center_y + 10);
        }
        glEnd();
    }

    glutSwapBuffers(); //swap the buffers
    framerate();
}

void gl_reshape (int w, int h) {
    glViewport (0, 0, (GLsizei)w, (GLsizei)h); //set the viewport to the current window specifications
    glMatrixMode (GL_PROJECTION); //set the matrix to projection
    glLoadIdentity ();
    gluOrtho2D(0.0f, width, 0, height);
    //gluPerspective(45.0f,(GLfloat)width/(GLfloat)height,0.1f,100.0f);
    glMatrixMode (GL_MODELVIEW); //set the matrix back to model
    glLoadIdentity();
}

void keyboard (unsigned char key, int x, int y) {
    if (key=='q')
    {
        // Quit the program.
        fprintf(print_file_ptr, "\n\nQuitting and cleaning up.\n");
        fflush(print_file_ptr);
        fclose(print_file_ptr);
        glutLeaveGameMode(); //set the resolution how it was
        kill_all_threads();
        pthread_mutex_destroy(&mutexStartThread);
        pthread_exit(NULL);
        sleep(SLEEP_KILL);
        exit(0); //quit the program
    }
    if (key=='s')
    {
        // if images are currently saving automatically disable this functionality
        if (!isSavingImages){
            Thread_data tdata;
            start_thread(ImageSaveThread, &tdata);
        } else {
            sprintf(message, "Manual Saving Disabled.");
        }
    }
}

void read_calibrated_ccd_center(void) {
    file_ptr = fopen("/home/schriste/SAAS/calibrated_ccd_center.txt", "r");
    if (file_ptr == NULL) {
        fprintf(print_file_ptr, "Can't open input file calibrated_ccd_center.txt!\n");
    } else {
        fscanf(file_ptr, "%u %u", &calib_center_x, &calib_center_y);
        fprintf(print_file_ptr, "Found center to be (%u,%u)\n", calib_center_x, calib_center_y);
        fclose(file_ptr);
    }
}

void read_settings(void) {
    int count = 0;
    char varname[128];
    int value;
    file_ptr = fopen("/home/schriste/SAAS/program_settings.txt"  , "r");
    if( file_ptr != NULL){
        fprintf(print_file_ptr, "Reading program settings...\n");
        while (EOF != fscanf(file_ptr, "%s %d", varname, &value)){
            fprintf(print_file_ptr, "%d: %s %d\n", count, varname, value);
            switch (count) {
                case 0:
                    settings.exposure = value;
                    fprintf(print_file_ptr, "Exposure is set to %d\n", settings.exposure);
                    break;
                case 1:
                    settings.analogGain = value;
                    fprintf(print_file_ptr, "analogGain is set to %d\n", settings.analogGain);
                    break;
                case 2:
                    settings.preampGain = value;
                    fprintf(print_file_ptr, "preampGain is set to %d\n", settings.preampGain);
                    break;
                case 3:
                    settings.blackLevel = value;
                    fprintf(print_file_ptr, "blackLevel is set to %d\n", settings.blackLevel);
                    break;
                case 4:
                    isSavingImages = value;
                    fprintf(print_file_ptr, "isSavingImages is set to %d\n", isSavingImages);
                    break;
                case 5:
                    if (value > MAX_SAVE_THREADS) {
                       fprintf(print_file_ptr, "User-defined max save threads (%d) > ", value);
                       fprintf(print_file_ptr, "max allowed number of save threads (%d).  ", MAX_SAVE_THREADS);
                       fprintf(print_file_ptr, "Setting max_save_threads to (%d)\n", MAX_SAVE_THREADS);
                    } else {
                        max_save_threads = value;
                    }
                    break;
                case 6:
                    mod_save = value;
                default:
                    break;
            }
            count++;
        }
    } else {
        fprintf(print_file_ptr, "Can't open input file program_settings.txt!\n");
        fprintf(print_file_ptr, "Default camera settings are (%" SCNu16 " %" SCNu16 " %" SCNd16 " %i)\n", settings.exposure, settings.analogGain, settings.preampGain, settings.blackLevel);
    }
    fclose(file_ptr);
}

void *ImageSaveThread(void *threadargs)
{
    char filename[128];
    char timestamp[TIMESTAMP_LENGTH];
    timespec localCaptureTime;
    timespec preSave, postSave, elapsedSave;
    HeaderData localHeader;

    long tid = (long)((struct Thread_data *)threadargs)->thread_id;
    struct Thread_data *my_data;
    my_data = (struct Thread_data *) threadargs;
    int camera_id = my_data->camera_id;

    clock_gettime(CLOCK_MONOTONIC, &preSave);

    // save an image as a FITS file
    memcpy( &data_save, &data, sizeof(data));

    clock_gettime(CLOCK_REALTIME, &localCaptureTime);

    writeCurrentUT(timestamp);
    sprintf(filename, "FOXSI_SAAS_%s.fits", timestamp);
    filename[128 - 1] = '\0';

    // TODO; add unique ID for camera.
    localHeader.cameraID = cameraID;  // this is the serial number of the camera
    localHeader.frameCount = frameCount;
    localHeader.captureTime = localCaptureTime;
    localHeader.exposure = (int)settings.exposure;
    localHeader.preampGain = (int)settings.preampGain;
    localHeader.analogGain = (float)settings.analogGain;
    localHeader.plateScale = arcsec_to_pixel;
    localHeader.cameraTemperature = camera_temperature;

    writeFITSImage(data_save, localHeader, filename, NUM_XPIXELS, NUM_YPIXELS);
    saveCount++;

    clock_gettime(CLOCK_MONOTONIC, &postSave);
    elapsedSave = TimespecDiff(preSave, postSave);
    fprintf(print_file_ptr, "Saving took: %ld sec %ld nsec \n", elapsedSave.tv_sec, elapsedSave.tv_nsec);

    started[tid] = false;
    pthread_exit(NULL);
}

int main (int argc, char **argv) {
    // Set where print statements should sent: screen or file.
    if (PRINT_TO_FILE == false) {
      print_file_ptr = stdout;
    } else {
      char print_filename[128];
      char timestamp[TIMESTAMP_LENGTH];
      writeCurrentUT(timestamp);
      sprintf(print_filename, "FOXSI_SAAS_print_output_%s.txt", timestamp);
      print_filename[128 - 1] = '\0';
      print_file_ptr = fopen(print_filename, "w");
      fprintf(print_file_ptr, "Created print statement file, %s, at %s", print_filename, timestamp);
    }

    // to catch a Ctrl-C or termination signal and clean up
    signal(SIGINT, &sig_handler);
    signal(SIGTERM, &sig_handler);

    pthread_mutex_init(&mutexStartThread, NULL);
    /* Create worker threads */
    fprintf(print_file_ptr, "In main: creating threads\n");

    for(int i = 0; i < MAX_THREADS; i++ ){
        started[i] = false;
    }

    read_calibrated_ccd_center();
    read_settings();

    // start the camera handling thread
    start_thread(CameraThread, NULL);

    glutInit (&argc, argv);
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_DEPTH); //set the display to Double buffer, with depth
    glutEnterGameMode(); //set glut to fullscreen using the settings in the line above
    gl_init(); // initialize the openGL window
    glutDisplayFunc (gl_display); //use the display function to draw everything
    glutIdleFunc (gl_display); //update any variables in display
    glutReshapeFunc (gl_reshape); //reshape the window accordingly
    glutKeyboardFunc (keyboard); //check the keyboard
    glutMainLoop (); //call the main loop

    /* Last thing that main() should do */
    fprintf(print_file_ptr, "Quitting and cleaning up.\n");
    /* wait for threads to finish */
    kill_all_threads();
    pthread_mutex_destroy(&mutexStartThread);
    pthread_exit(NULL);
    return 0;
}
