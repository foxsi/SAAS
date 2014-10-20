#define MAX_THREADS            5
#define SLEEP_CAMERA_CONNECT   1    // waits for errors while connecting to camera
#define SLEEP_KILL             2    // waits when killing all threads
#define DEFAULT_CALIB_CENTER_X       648    // the default calibrated screen center for HUD display
#define DEFAULT_CALIB_CENTER_Y       483    // the default calibrated screen center for HUD display
#define NUM_CIRCLE_SEGMENTS   30    // the number of line segments to use for circles
#define NUM_XPIXELS         1296    // number of X pixels of sensor
#define NUM_YPIXELS         966     // number of Y pixels of sensor

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
static float arcsec_to_pixel = 3.55;   // the plate scale

unsigned int calib_center_x = DEFAULT_CALIB_CENTER_X;
unsigned int calib_center_y = DEFAULT_CALIB_CENTER_Y;

// file pointer
FILE *file_ptr = NULL;

// to store the image
unsigned char *data = new unsigned char[NUM_XPIXELS * NUM_YPIXELS];
GLuint texture[1];      	// Storage for one texture to display the camera image

struct CameraSettings
{
    uint16_t exposure;
    uint16_t analogGain;
    int16_t preampGain;
    int blackLevel;
} settings;

bool is_camera_ready = false;

// related to threads
bool stop_message[MAX_THREADS];
pthread_t threads[MAX_THREADS];
bool started[MAX_THREADS];
pthread_mutex_t mutexFrame;     //Used to protect image data

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
void keyboard (unsigned char key, int x, int y);
void *CameraThread( void * threadargs, int camera_id);
void read_calibrated_ccd_center(void);
void read_camera_settings(void);
void kill_all_threads();

static int current_time(void)
{
    // return current time (in seconds)
    // used to calculate frame rate
    struct timeval tv;
    struct timezone tz;
    (void) gettimeofday(&tv, &tz);
    return (int) tv.tv_sec;
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
        printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
               fps);
        t0 = t;
        frames = 0;
    }
}

static void gl_load_gltextures()
{
    // Load bitmaps and convert to textures
    //for( int i = 0; i < height * width; i++){
    //    data[i] = rand();
    //}
    
    // Typical texture generation using data from the bitmap
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, (GLvoid*)data);
    
    //glPixelStorei(GL_UNPACK_ALIGNMENT);
    
    // Typical texture generation using data from the bitmap
    
    // free the memory
}

void gl_draw_string( int x, int y, char *str ) {
    glColor3f( 1.0f, 1.0f, 1.0f );
    glRasterPos2i( x, y );
    
    for ( int i=0, len=strlen(str); i<len; i++ ) {
        if ( str[i] == '\n' ) {
            y -= 16;
            glRasterPos2i( x, y );
        } else {
            glutBitmapCharacter( GLUT_BITMAP_HELVETICA_12, str[i] );
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

void gl_init(void) {
    glGenTextures(1, &texture[0]);		// Create the texture
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_SMOOTH);                    // Enable Smooth Shading
    glClearColor(0.0f, 0.0f, 0.0f, 0.5f);       // Black Background
    glClearDepth(1.0f);                         // Depth Buffer Setup
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);     // Set Line Antialiasing
    glDisable(GL_BLEND);
    glLineWidth(2.0);       // change the thickness of the HUD lines
    gl_load_gltextures();
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
            printf("Quitting thread %i, quitting status is %i\n", i, pthread_cancel(threads[i]));
            started[i] = false;
        }
    }
}

void *CameraThread( void * threadargs)
{
    // camera_id refers to 0 PYAS, 1 is RAS (if valid)
    long tid = (long)((struct Thread_data *)threadargs)->thread_id;
    printf("Camera thread #%ld!\n", tid);
    
    //timespec frameRate = {0,FRAME_CADENCE*1000};
    bool cameraReady = false;
    long int frameCount = 0;
    
    //cv::Mat localFrame, mockFrame;
    //HeaderData localHeader;
    //timespec localCaptureTime, preExposure, postExposure, timeElapsed, timeToWait;

    char lDoodle[] = "|\\-|-/";
    int lDoodleIndex = 0;
    PvInt64 lImageCountVal = 0;
    double lFrameRateVal = 0.0;
    double lBandwidthVal = 0.0;
    
    PvSystem lSystem;
    PvDeviceInfo* lDeviceInfo;
    PvResult lResult;
    PvDevice lDevice;
    PvStream lStream;
    PvPipeline *lPipeline;
    PvGenParameterArray *lStreamParams;
    cameraReady = false;
    PvGenParameterArray *lDeviceParams;
    
    while(!stop_message[tid])
    {
        if (!cameraReady)
        {
            std::cout << "ImperxStream::Connect starting" << std::endl;
            
            // Find all GEV Devices on the network.
            lSystem.SetDetectionTimeout( 2000 );
            lResult = lSystem.Find();
            if( !lResult.IsOK() )
            {
                printf( "PvSystem::Find Error: %s", lResult.GetCodeString().GetAscii() );
                cameraReady = false;
                sleep(SLEEP_CAMERA_CONNECT);
            } else {
                
                // Get the number of GEV Interfaces that were found using GetInterfaceCount.
                PvUInt32 lInterfaceCount = lSystem.GetInterfaceCount();
                
                // Display information about all found interface
                // For each interface, display information about all devices.
                for( PvUInt32 x = 0; x < lInterfaceCount; x++ )
                {
                    // get pointer to each of interface
                    PvInterface * lInterface = lSystem.GetInterface( x );
                    
                    printf( "Interface %i\nMAC Address: %s\nIP Address: %s\nSubnet Mask: %s\n\n",
                           x,
                           lInterface->GetMACAddress().GetAscii(),
                           lInterface->GetIPAddress().GetAscii(),
                           lInterface->GetSubnetMask().GetAscii() );
                    
                    // Get the number of GEV devices that were found using GetDeviceCount.
                    PvUInt32 lDeviceCount = lInterface->GetDeviceCount();
                    
                    for( PvUInt32 y = 0; y < lDeviceCount ; y++ )
                    {
                        lDeviceInfo = lInterface->GetDeviceInfo( y );
                        printf( "Device %i\nMAC Address: %s\nIP Address: %s\nSerial number: %s\n\n",
                               y,
                               lDeviceInfo->GetMACAddress().GetAscii(),
                               lDeviceInfo->GetIPAddress().GetAscii(),
                               lDeviceInfo->GetSerialNumber().GetAscii() );
                    }
                }
                
                // If no device is selected, abort
                if( lDeviceInfo == NULL )
                {
                    printf( "No device selected.\n" );
                    cameraReady = false;
                    sleep(SLEEP_CAMERA_CONNECT);
                } else {
                    
                    // Connect to the GEV Device
                    printf( "Connecting to %s\n", lDeviceInfo->GetMACAddress().GetAscii() );
                    if ( !lDevice.Connect( lDeviceInfo ).IsOK() )
                    {
                        printf( "Unable to connect to %s\n", lDeviceInfo->GetMACAddress().GetAscii() );
                        cameraReady = false;
                        sleep(SLEEP_CAMERA_CONNECT);
                    } else {
                        
                        printf( "Successfully connected to %s\n", lDeviceInfo->GetMACAddress().GetAscii() );
                        
                        // Get device parameters need to control streaming
                        lDeviceParams = lDevice.GetGenParameters();
                        
                        // Negotiate streaming packet size
                        lDevice.NegotiatePacketSize();
                        
                        // Open stream - have the PvDevice do it for us
                        printf( "Opening stream to device\n" );
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
                        printf( "Starting pipeline\n" );
                        
                        // set camera settings
                        PvResult outcome;
                        lDeviceParams->SetBooleanValue("ProgFrameTimeEnable", false);
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
                        
                        printf( "Resetting timestamp counter...\n" );
                        lDeviceParams->ExecuteCommand( "GevTimestampControlReset" );
                        
                        // The pipeline is already "armed", we just have to tell the device
                        // to start sending us images
                        printf( "Sending StartAcquisition command to device\n" );
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
                if ( lOperationResult.IsOK() )
                {
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
                    }
                    
                    printf( "%c BlockID: %016llX W: %i H: %i %.01f FPS %.01f Mb/s\r",
                           lDoodle[ lDoodleIndex ],
                           lBuffer->GetBlockID(),
                           lWidth,
                           lHeight,
                           lFrameRateVal,
                           lBandwidthVal / 1000000.0 );
                }
                // We have an image - do some processing (...) and VERY IMPORTANT,
                // release the buffer back to the pipeline
                lPipeline->ReleaseBuffer( lBuffer );
            }
            else
            {
                // Timeout
                printf( "%c Timeout\r", lDoodle[ lDoodleIndex ] );
            }
        }
    }
    
    delete lPipeline;
    lPipeline = NULL;
    
    printf("CameraStream thread #%ld exiting\n", tid);
    // clean up the camera
    // Tell the device to stop sending images
    printf( "Sending AcquisitionStop command to the device\n" );
    lDeviceParams->ExecuteCommand( "AcquisitionStop" );
    
    // If present reset TLParamsLocked to 0. Must be done AFTER the
    // streaming has been stopped
    lDeviceParams->SetIntegerValue( "TLParamsLocked", 0 );
    
    // We stop the pipeline - letting the object lapse out of
    // scope would have had the destructor do the same, but we do it anyway
    printf( "Stop pipeline\n" );
    lPipeline->Stop();
    
    // Now close the stream. Also optionnal but nice to have
    printf( "Closing stream\n" );
    lStream.Close();
    
    // Finally disconnect the device. Optional, still nice to have
    printf( "Disconnecting device\n" );
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
    int i = 0;
    while (started[i] == true) {
        i++;
        if (i == MAX_THREADS) return; //should probably thrown an exception
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
        printf("ERROR; return code from pthread_create() is %d\n", rc);
    } else started[i] = true;
    
    pthread_attr_destroy(&attr);
    return;
}

void gl_display (void) {
    // the drawing function
    glClearColor (0.0, 0.0, 0.0, 1.0); //clear the screen to black
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear the color buffer and the depth buffer
    glLoadIdentity();
    //sprintf( message, "(testing! %lf, %lf)",  -12.0, 3.0);
    //drawString(width/2.0, height/2.0, message);
    //glTranslatef(0.0f,0.0f,-8.0f);          // Move into the screen 5 units
    gl_load_gltextures();
    
    glColor3f(1, 1, 1); //color the texture white
    glBindTexture(GL_TEXTURE_2D, texture[0]);       // Select our texture
    
    // draw the camera image as a texture
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f, 0.0f, 1.0f);	// Bottom left of the texture and quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f(width, 0.0f, 1.0f);	// Bottom right of the texture and quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f(width,  height, 1.0f);	// Top right of the texture and quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f(0.0f,  height, 1.0f);	// Top left of the texture and quad
    glEnd();
    
    glTranslatef(0.0f, 0.0f, -1.0f);
    // draw HUD
    glColor3f(1, 1, 1); //HUD color is white
    
    // cross at center of screen
    // X - line
    glBegin(GL_LINES);
    glVertex2f(0.0f, DEFAULT_CALIB_CENTER_Y);
    glVertex2f(width, DEFAULT_CALIB_CENTER_Y);
    glEnd();
    
    // Y - line
    glBegin(GL_LINES);
    glVertex2f(calib_center_x, 0.0f);
    glVertex2f(calib_center_x, height);
    glEnd();
    
    // circle for Sun
    gl_draw_circle(calib_center_x, calib_center_y, 30 * arcsec_to_pixel, NUM_CIRCLE_SEGMENTS);
    
    // draw larger circles
    gl_draw_circle(calib_center_x, calib_center_y, 60 * arcsec_to_pixel, NUM_CIRCLE_SEGMENTS);
    gl_draw_circle(calib_center_x, calib_center_y, 90 * arcsec_to_pixel, NUM_CIRCLE_SEGMENTS);
    
    glutSwapBuffers(); //swap the buffers
    framerate();
}

void gl_reshape (int w, int h) {
    glViewport (0, 0, (GLsizei)w, (GLsizei)h); //set the viewport to the current window specifications
    glMatrixMode (GL_PROJECTION); //set the matrix to projection
    
    glLoadIdentity ();

    glOrtho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    glMatrixMode (GL_MODELVIEW); //set the matrix back to model
    glLoadIdentity();
}

void keyboard (unsigned char key, int x, int y) {
    if (key=='q')
    {
        glutLeaveGameMode(); //set the resolution how it was
        kill_all_threads();
        sleep(SLEEP_KILL);
        exit(0); //quit the program
    }
    if (key=='s')
    {
        // save image here
    }
}

void read_calibrated_ccd_center(void) {
    file_ptr = fopen("calibrated_ccd_center.txt", "r");
    if (file_ptr == NULL) {
        printf("Can't open input file calibrated_ccd_center.txt!\n");
    } else {
        fscanf(file_ptr, "%u %u", &calib_center_x, &calib_center_y);
        printf("Found center to be (%u,%u)\n", calib_center_x, calib_center_y);
        fclose(file_ptr);
    }
}

void read_camera_settings(void) {
    file_ptr = fopen("camera_settings.txt"  , "r");
    if (file_ptr == NULL) {
        printf("Can't open input file camera_settings.txt!\n");
    } else {
        fscanf(file_ptr, "%" SCNu16 " %" SCNu16 " %" SCNd16 " %i", &settings.exposure, &settings.analogGain, &settings.preampGain, &settings.blackLevel);
        printf("Found camera settings to be to be (%" SCNu16 " %" SCNu16 " %" SCNd16 " %i)\n", settings.exposure, settings.analogGain, settings.preampGain, settings.blackLevel);
        fclose(file_ptr);
    }
}

int main (int argc, char **argv) {
    // to catch a Ctrl-C or termination signal and clean up
    signal(SIGINT, &sig_handler);
    signal(SIGTERM, &sig_handler);
    
    pthread_mutex_init(&mutexFrame, NULL);
    /* Create worker threads */
    printf("In main: creating threads\n");
    
    for(int i = 0; i < MAX_THREADS; i++ ){
        started[0] = false;
    }
        
    read_calibrated_ccd_center();
    read_camera_settings();

    // start the camera handling thread
    //start_thread(CameraThread, NULL);
    
    glutInit (&argc, argv);
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_DEPTH); //set the display to Double buffer, with depth
    glutEnterGameMode(); //set glut to fullscreen using the settings in the line above
    gl_init(); // initialize the openGL window
    glutDisplayFunc (gl_display); //use the display function to draw everything
    glutIdleFunc (gl_display); //update any variables in display
    glutReshapeFunc (gl_reshape); //reshape the window accordingly
    glutKeyboardFunc (keyboard); //check the keyboard
    glutMainLoop (); //call the main loop
    return 0;
}