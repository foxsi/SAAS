#include <GL/gl.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

//angle of rotation
static char message[1024];

// to compile g++ test2.cpp -lGLU -lm -lGL -lglut

GLuint texture[1];      	// Storage for one texture ( NEW )

// width and height of IMPERX Camera frame
int width = 1296;
int height = 966;
// should only grey scale data here
unsigned char *data = new unsigned char[1296*966];

/* return current time (in seconds) */
static int current_time(void)
{
    struct timeval tv;
    struct timezone tz;
    (void) gettimeofday(&tv, &tz);
    return (int) tv.tv_sec;
}

void framerate(void){
    /* calc framerate */
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


static void LoadGLTextures()    // Load bitmaps and convert to textures
{
    for( int i = 0; i < height * width; i++){
        data[i] = rand();
    }
    
    // Typical texture generation using data from the bitmap
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, height, width, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, (GLvoid*)data);
    
    //glPixelStorei(GL_UNPACK_ALIGNMENT);
    
    // Typical texture generation using data from the bitmap
    
    // free the memory
}

void drawString( int x, int y, char *str ) {
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

void draw_circle(float cx, float cy, float r, int num_segments)
{
    // from http://slabode.exofire.net/circle_draw.shtml
    float theta = 2 * 3.1415926 / float(num_segments);
    float tangetial_factor = tanf(theta);//calculate the tangential factor
    
    float radial_factor = cosf(theta);//calculate the radial factor
    
    float x = r;//we start at angle = 0
    
    float y = 0;
    
    glBegin(GL_LINE_LOOP);
    for(int ii = 0; ii < num_segments; ii++)
    {
        glVertex2f(x + cx, y + cy);//output vertex
        
        //calculate the tangential vector
        //remember, the radial vector is (x, y)
        //to get the tangential vector we flip those coordinates and negate one of them
        
        float tx = -y;
        float ty = x;
        
        //add the tangential vector
        
        x += tx * tangetial_factor;
        y += ty * tangetial_factor;
        
        //correct using the radial factor
        
        x *= radial_factor;
        y *= radial_factor;
    }
    glEnd();
}

void init (void) {
    glDisable(GL_DEPTH_TEST);
    glShadeModel (GL_SMOOTH); //set the shader to smooth shader
    glGenTextures(1, &texture[0]);		// Create the texture
    glEnable(GL_TEXTURE_2D);
    //glOrtho(0,1,0,1,0,-1);
    LoadGLTextures();
}

void display (void) {
    glClearColor (0.0,0.0,0.0,1.0); //clear the screen to black
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear the color buffer and the depth buffer
    glLoadIdentity();
    sprintf( message, "(testing! %lf, %lf)",  -12.0, 3.0);
    drawString(-0.2, -0.9, message);
    glTranslatef(0.0f,0.0f,-8.0f);          // Move into the screen 5 units
    
    LoadGLTextures();
    glBindTexture(GL_TEXTURE_2D, texture[0]);       // Select our texture
    float zoom_factor = 1/130.0;
    float x0 = -width*zoom_factor/2.0;
    float x1 = width*zoom_factor/2.0;
    float y0 = -height*zoom_factor/2.0;
    float y1 = height*zoom_factor/2.0;
    
    glBegin(GL_QUADS);
    // Front face
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x0, y0,  1.0f);	// Bottom left of the texture and quad
    glTexCoord2f(1.0f, 0.0f); glVertex3f( x1, y0,  1.0f);	// Bottom right of the texture and quad
    glTexCoord2f(1.0f, 1.0f); glVertex3f( x1,  y1,  1.0f);	// Top right of the texture and quad
    glTexCoord2f(0.0f, 1.0f); glVertex3f(x0,  y1,  1.0f);	// Top left of the texture and quad
    glEnd();
    
    // draw HUD
    glColor3f(1, 0, 0); //HUD color is red
    
    // cross at center of screen
    // X - line
    glBegin(GL_LINES);
    glVertex2f(-6, 0);
    glVertex2f(6, 0);
    glEnd();
    
    // Y - line
    glBegin(GL_LINES);
    glVertex2f(0, -6);
    glVertex2f(0, 6);
    glEnd();
    
    // box for Sun
    draw_circle(0, 0, 2, 30);

    //
    draw_circle(0, 0, 3, 30);
    draw_circle(0, 0, 4, 30);
    
    
    glutSwapBuffers(); //swap the buffers
    framerate();
}

void reshape (int w, int h) {
    glViewport (0, 0, (GLsizei)w, (GLsizei)h); //set the viewport to the current window specifications
    glMatrixMode (GL_PROJECTION); //set the matrix to projection
    
    glLoadIdentity ();
    gluPerspective (60, (GLfloat)w / (GLfloat)h, 1.0, 100.0); //set the perspective (angle of sight, width, height, , depth)
    glMatrixMode (GL_MODELVIEW); //set the matrix back to model
}

void keyboard (unsigned char key, int x, int y) {
    if (key=='q')
    {
        glutLeaveGameMode(); //set the resolution how it was
        exit(0); //quit the program
    }
}

int main (int argc, char **argv) {
    glutInit (&argc, argv);
    glutInitDisplayMode (GLUT_DOUBLE | GLUT_DEPTH); //set the display to Double buffer, with depth
    //glutGameModeString('1024×768:32@75'); //the settings for fullscreen mode
    glutEnterGameMode(); //set glut to fullscreen using the settings in the line above
    init (); //call the init function
    glutDisplayFunc (display); //use the display function to draw everything
    glutIdleFunc (display); //update any variables in display, display can be changed to anyhing, as long as you move the variables to be updated, in this case, angle++;
    glutReshapeFunc (reshape); //reshape the window accordingly
    
    glutKeyboardFunc (keyboard); //check the keyboard
    glutMainLoop (); //call the main loop
    return 0;
}