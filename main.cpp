#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gl/glut.h>       // gltools library

#define GL_CLAMP_TO_EDGE 0x812F
#define GL_PI 3.1415f

typedef float M3DVector3f[3];

// Rotation amounts
static GLfloat xRot = 0.0f;
static GLfloat yRot = 0.0f;

// Moving amounts
static GLfloat xMov = 0.0f, xMov2 = 0.0f, xr = 0.0f;
static GLfloat yMov = 0.0f, yMov2 = 0.0f, yr = -1.0f;
static GLfloat angle = 0.0f, angle2 = 0.0f;

//the layar (nurbs)
GLint nNumPoints = 3;
GLfloat ctrlPoints[3][3][3]= {{{  -4.0f, 0.0f, 4.0f},
            { -2.0f, 4.0f, 4.0f},
         {  4.0f, 0.0f, 4.0f }},

         {{  -4.0f, 0.0f, 0.0f},
            { -2.0f, 4.0f, 0.0f},
         {  4.0f, 0.0f, 0.0f }},

         {{  -4.0f, 0.0f, -4.0f},
            { -2.0f, 4.0f, -4.0f},
         {  4.0f, 0.0f, -4.0f }}};


void m3dCrossProduct(M3DVector3f result, const M3DVector3f u, const M3DVector3f v)
 {
 result[0] = u[1]*v[2] - v[1]*u[2];
 result[1] = -u[0]*v[2] + v[0]*u[2];
 result[2] = u[0]*v[1] - v[0]*u[1];
 }

void m3dFindNormal(M3DVector3f result, const M3DVector3f point1, const M3DVector3f point2,
       const M3DVector3f point3)
 {
 M3DVector3f v1,v2;  // Temporary vectors

 // Calculate two vectors from the three points. Assumes counter clockwise
 // winding!
 v1[0] = point1[0] - point2[0];
 v1[1] = point1[1] - point2[1];
 v1[2] = point1[2] - point2[2];

 v2[0] = point2[0] - point3[0];
 v2[1] = point2[1] - point3[1];
 v2[2] = point2[2] - point3[2];

 // Take the cross product of the two vectors to get
 // the normal vector.
 m3dCrossProduct(result, v1, v2);
 }

#pragma pack(1)
typedef struct
{
    GLbyte identsize;              // Size of ID field that follows header (0)
    GLbyte colorMapType;           // 0 = None, 1 = paletted
    GLbyte imageType;              // 0 = none, 1 = indexed, 2 = rgb, 3 = grey, +8=rle
    unsigned short colorMapStart;          // First colour map entry
    unsigned short colorMapLength;         // Number of colors
    unsigned char  colorMapBits;   // bits per palette entry
    unsigned short xstart;                 // image x origin
    unsigned short ystart;                 // image y origin
    unsigned short width;                  // width in pixels
    unsigned short height;                 // height in pixels
    GLbyte bits;                   // bits per pixel (8 16, 24, 32)
    GLbyte descriptor;             // image descriptor
} TGAHEADER;
#pragma pack(8)


////////////////////////////////////////////////////////////////////
// Capture the current viewport and save it as a targa file.
// Be sure and call SwapBuffers for double buffered contexts or
// glFinish for single buffered contexts before calling this function.
// Returns 0 if an error occurs, or 1 on success.
GLint gltWriteTGA(const char *szFileName)
 {
    FILE *pFile;                // File pointer
    TGAHEADER tgaHeader;  // TGA file header
    unsigned long lImageSize;   // Size in bytes of image
    GLbyte *pBits = NULL;      // Pointer to bits
    GLint iViewport[4];         // Viewport in pixels
    GLenum lastBuffer;          // Storage for the current read buffer setting

 // Get the viewport dimensions
 glGetIntegerv(GL_VIEWPORT, iViewport);

    // How big is the image going to be (targas are tightly packed)
 lImageSize = iViewport[2] * 3 * iViewport[3];

    // Allocate block. If this doesn't work, go home
    pBits = (GLbyte *)malloc(lImageSize);
    if(pBits == NULL)
        return 0;

    // Read bits from color buffer
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
 glPixelStorei(GL_PACK_ROW_LENGTH, 0);
 glPixelStorei(GL_PACK_SKIP_ROWS, 0);
 glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

    // Get the current read buffer setting and save it. Switch to
    // the front buffer and do the read operation. Finally, restore
    // the read buffer state
    glGetIntegerv(GL_READ_BUFFER, (GLint *)&lastBuffer);
    glReadBuffer(GL_FRONT);
    glReadBuffer(lastBuffer);

    // Initialize the Targa header
    tgaHeader.identsize = 0;
    tgaHeader.colorMapType = 0;
    tgaHeader.imageType = 2;
    tgaHeader.colorMapStart = 0;
    tgaHeader.colorMapLength = 0;
    tgaHeader.colorMapBits = 0;
    tgaHeader.xstart = 0;
    tgaHeader.ystart = 0;
    tgaHeader.width = iViewport[2];
    tgaHeader.height = iViewport[3];
    tgaHeader.bits = 24;
    tgaHeader.descriptor = 0;

    // Attempt to open the file
    pFile = fopen(szFileName, "wb");
    if(pFile == NULL)
  {
        free(pBits);    // Free buffer and return error
        return 0;
  }

    // Write the header
    fwrite(&tgaHeader, sizeof(TGAHEADER), 1, pFile);

    // Write the image data
    fwrite(pBits, lImageSize, 1, pFile);

    // Free temporary buffer and close the file
    free(pBits);
    fclose(pFile);

    // Success!
    return 1;
 }


////////////////////////////////////////////////////////////////////
// Allocate memory and load targa bits. Returns pointer to new buffer,
// height, and width of texture, and the OpenGL format of data.
// Call free() on buffer when finished!
// This only works on pretty vanilla targas... 8, 24, or 32 bit color
// only, no palettes, no RLE encoding.
GLbyte *gltLoadTGA(const char *szFileName, GLint *iWidth, GLint *iHeight, GLint *iComponents, GLenum *eFormat)
 {
    FILE *pFile;   // File pointer
    TGAHEADER tgaHeader;  // TGA file header
    unsigned long lImageSize;  // Size in bytes of image
    short sDepth;   // Pixel depth;
    GLbyte *pBits = NULL;          // Pointer to bits

    // Default/Failed values
    *iWidth = 0;
    *iHeight = 0;

    *iComponents = GL_RGB8;

    // Attempt to open the file
    pFile = fopen(szFileName, "rb");
    if(pFile == NULL)
        return NULL;

    // Read in header (binary)
    fread(&tgaHeader, 18/* sizeof(TGAHEADER)*/, 1, pFile);

    // Get width, height, and depth of texture
    *iWidth = tgaHeader.width;
    *iHeight = tgaHeader.height;
    sDepth = tgaHeader.bits / 8;

    // Put some validity checks here. Very simply, I only understand
    // or care about 8, 24, or 32 bit targa's.
    if(tgaHeader.bits != 8 && tgaHeader.bits != 24 && tgaHeader.bits != 32)
        return NULL;

    // Calculate size of image buffer
    lImageSize = tgaHeader.width * tgaHeader.height * sDepth;

    // Allocate memory and check for success
    pBits = (GLbyte*)malloc(lImageSize * sizeof(GLbyte));
    if(pBits == NULL)
        return NULL;

    // Read in the bits
    // Check for read error. This should catch RLE or other
    // weird formats that I don't want to recognize
    if(fread(pBits, lImageSize, 1, pFile) != 1)
  {
        free(pBits);
        return NULL;
  }

    // Set OpenGL format expected
    switch(sDepth)
  {
        case 3:     // Most likely case

            *iComponents = GL_RGB8;
            break;
        case 4:

            *iComponents = GL_RGBA8;
            break;
        case 1:
            *eFormat = GL_LUMINANCE;
            *iComponents = GL_LUMINANCE8;
            break;
  };


    // Done with File
    fclose(pFile);

    // Return pointer to image data
    return pBits;
 }



// Called to draw scene
void RenderScene(void)
 {
 static float fElect1 = 0.0f; //change the path(orbit)
 //for texture mapping
 GLbyte *pBytes;
    GLint iWidth, iHeight, iComponents;
    GLenum eFormat;

 GLUquadricObj *pObj; // Quadric object
 glDisable(GL_CULL_FACE); // show back side
 pObj =gluNewQuadric();
 gluQuadricNormals(pObj, GLU_SMOOTH);

 M3DVector3f vNormal; // Storeage for calculated surface normal
 M3DVector3f vCorners[11] = { { 0.0f, 0.0f, 0.0f },// 0 //for the layar
                              { 3.0f, -5.0f, 0.0f },   // 1
                              { -3.0f, -5.0f, 0.0f }, // 2
         {20.0f, 20.0f, 0.0f}, // 3 //for the sea
         {20.0f, -20.0f, 0.0f}, //4
         {-20.0f, 20.0f, 0.0f}, //5
         {-20.0f, -20.0f, 0.0f},//6
         {10.0f, 10.0f, 2.0f},//7 //for the pic
         {10.0f, 10.0f, -2.0f},//8
         {10.0f, 14.0f, -2.0f},//9
         {10.0f, 14.0f, 2.0f}//10
         };

 // Clear the window with current clearing color
 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

 //nite sky
 glPushMatrix();
 glDisable(GL_LIGHTING);
 glDisable(GL_TEXTURE_2D);
 glScalef(10.50f, 5.50f, 0.25f);
 glTranslatef(0.0f, 0.0f, -300.0f);
 glRotatef(yRot, 0.0f, 0.0f, 0.50f); //actually this is zRot ^_^
 glEnable(GL_LIGHTING);
 // Load texture
 glColor3ub(39.0f, 64.0f, 139.0f);
 //glColor3ub(255.0f, 255.0f, 255.0f); //make the water color brighter
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
 pBytes = gltLoadTGA("ayinitesky4.tga", &iWidth, &iHeight, &iComponents, &eFormat);
    glTexImage2D(GL_TEXTURE_2D, 0, iComponents, iWidth, iHeight, 0, eFormat, GL_UNSIGNED_BYTE, pBytes);
 glEnable(GL_TEXTURE_2D);
 glTranslatef(0.0f, 0.0f, 20.10f);
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.30, 0.30);
            glVertex3fv(vCorners[3]);

            glTexCoord2f(0.0, 0.0);
            glVertex3fv(vCorners[6]);

            glTexCoord2f(0.30, 0.0);
            glVertex3fv(vCorners[4]);
 glEnd();
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.30, 0.30);
            glVertex3fv(vCorners[3]);

            glTexCoord2f(0.0, 0.30);
            glVertex3fv(vCorners[5]);

            glTexCoord2f(0.0, 0.0);
            glVertex3fv(vCorners[6]);
 glEnd();
 free(pBytes);
 glDisable(GL_TEXTURE_2D); //disable it to or you just make it worse ;p
 glEnable(GL_LIGHTING);

 glPopMatrix(); //end of nite sky

 /*************************the star***********************************************/
 glPushMatrix();//awal
 glEnable(GL_LIGHT1);
 glColor3ub(255, 255, 0);
 glTranslatef(10.0f, 42.0f,-100.0f);
 glRotatef(yRot, 0.0f, 0.0f, 1.0f); //actually this is zRot ^_^
 glPushAttrib(GL_LIGHTING_BIT);
 //glDisable(GL_LIGHTING);
 gluSphere(pObj, 30.30f, 26, 13);//first star
 glPopAttrib();
 glDisable(GL_LIGHT1);
 glPopMatrix();//awal
 //glEnable(GL_LIGHTING);

 /**************************************end of star*********************************/
 // Save the begin matrix state and do the rotations
 glPushMatrix();
 glTranslatef(0.0f, 0.0f, -30.0f);
 glRotatef(xRot, 1.0f, 0.0f, 0.0f);
 glRotatef(yRot, 0.0f, 0.0f, 1.0f); //actually this is zRot ^_^

 /******************************the ship*******************************/
 // The sea :D
 glPushMatrix();
 glDisable(GL_LIGHTING);
 glDisable(GL_TEXTURE_2D);
 glColor3ub(39.0f, 64.0f, 139.0f);
 glScalef(5.50f, 5.50f, 0.25f);
 //glutSolidCube(40.0f);
 //seasurface
 glEnable(GL_LIGHTING);
 // Load texture
 glColor3ub(39.0f, 64.0f, 139.0f);
 //glColor3ub(255.0f, 255.0f, 255.0f); //make the water color brighter
 glEnable(GL_TEXTURE_2D);
 glTranslatef(0.0f, 0.0f, 20.10f);
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.30, 0.30);
            glVertex3fv(vCorners[3]);

            glTexCoord2f(0.0, 0.0);
            glVertex3fv(vCorners[6]);

            glTexCoord2f(0.30, 0.0);
            glVertex3fv(vCorners[4]);
 glEnd();
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.30, 0.30);
            glVertex3fv(vCorners[3]);

            glTexCoord2f(0.0, 0.30);
            glVertex3fv(vCorners[5]);

            glTexCoord2f(0.0, 0.0);
            glVertex3fv(vCorners[6]);
 glEnd();
 glDisable(GL_TEXTURE_2D); //disable it to or you just make it worse ;p
 glEnable(GL_LIGHTING);
 glPopMatrix();

 /*// The first ship ¢½
 glPushMatrix(); //awal
 glTranslatef(0.0f, 0.0f, 5.30f);
 glTranslatef(xMov, yMov, 0.0f);
 glRotatef(-angle*360.0f/6.35f, 0.0f, 0.0f, 1.0f); //round
 glRotatef(-100, -2.0f, 1.30f, 1.0f); //position
 glScalef(0.5f, 0.5f, 0.5f);
 glPushMatrix(); // first
 glColor3ub(151, 105, 79);
 glTranslatef(0.0f, 0.0f, 5.0f);
 glPushMatrix(); //second
 glRotatef(-180, 0.0f, 1.0f, 1.0f);
 glScalef(0.50f, 2.0f, 1.0f);
 gluCylinder(pObj, 2.0f, 4.0f, 4.0f, 26, 1);
 gluCylinder(pObj, 0.0f, 2.0f, 0.10f, 26, 1);
 glPopMatrix(); //second
 glPushMatrix();//third
 glTranslatef(0.0f, 5.0f, 0.0f);
 glScalef(1.0f, 9.0f, 1.0f);
 glutSolidCube(1.0f);
 glPopMatrix();//third
 glPushMatrix(); //fourth
 glTranslatef(0.0f, 15.0f, 0.0f);
 glScalef(2.0f, 2.0f, 0.0f);
 glEnable(GL_TEXTURE_2D); //we wanna do the mapping
 glEnable(GL_LIGHT1);
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.5, 0.5);
            glVertex3fv(vCorners[0]);

            glTexCoord2f(1.0, 1.0);
            glVertex3fv(vCorners[1]);

            glTexCoord2f(0.0, 0.0);
            glVertex3fv(vCorners[2]);
 glEnd();
 glDisable(GL_LIGHT1);
 glDisable(GL_TEXTURE_2D); //disable it to or you just make it worse ;p
 glPopMatrix(); //fourth
 glPopMatrix(); //first
 glPopMatrix(); //awal*/

 ///The second ship ;p
 glPushMatrix(); //awal
 glTranslatef(24.0f+xMov2, 24.0f+yMov2, 5.30f);
 glRotatef(180, 0.0f, 1.0f, 1.0f); //position
 glRotatef(angle2, 0.0f, 1.0f, 0.0f); //round
 glScalef(0.3f, 0.3f, 0.3f);
 glPushMatrix(); // first
 glColor3ub(151, 105, 79);
 glTranslatef(0.0f, 0.0f, 5.0f);
 glPushMatrix(); //second
 glRotatef(-180, 0.0f, 1.0f, 1.0f);
 glScalef(0.50f, 2.0f, 1.0f);
 gluCylinder(pObj, 2.0f, 4.0f, 4.0f, 26, 1);
 gluCylinder(pObj, 0.0f, 2.0f, 0.10f, 26, 1);
 glPopMatrix(); //second
 glPushMatrix();//third
 glTranslatef(0.0f, 4.0f, 0.0f);
 glScalef(1.0f, 2.0f, 1.0f);
 glutSolidCube(1.0f);
 glPopMatrix();//third
 glPushMatrix(); //fourth
 glTranslatef(0.0f, 15.0f, 0.0f);
 glRotatef(90, 0.0f, 1.0f, 0.0f);
 glScalef(2.0f, 2.0f, 4.0f);
 glEnable(GL_TEXTURE_2D); //we wanna do the mapping
 glEnable(GL_LIGHT1);
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.8, 0.8);
            glVertex3fv(vCorners[0]);

            glTexCoord2f(1.0, 1.0);
            glVertex3fv(vCorners[1]);

            glTexCoord2f(0.0, 0.0);
            glVertex3fv(vCorners[2]);
 glEnd();
 glDisable(GL_LIGHT1);
 glDisable(GL_TEXTURE_2D); //disable it to or you just make it worse ;p
 glPopMatrix(); //fourth
 glPopMatrix(); //first
 glPopMatrix(); //awal

 //the third ship
 glPushMatrix();
 glTranslatef(0.0, 0.0, 5.0f);
 glTranslatef(xMov, yMov, 0.0f);
 glRotatef(-angle*360.0f/6.35f, 0.0f, 0.0f, 1.0f); //round
 glScalef(0.8f, 0.8f, 0.8f);
 glRotatef(180, 0.0f, 1.0f, 1.0f); //position
 glRotatef(180, 0.0f, 1.0f, 0.0f);
 glPushMatrix();//first
 glRotatef(-180, 0.0f, 1.0f, 1.0f);
 glColor3ub( 238,59,59);
 glScalef(3.0, 1.0f, 1.0f );
 gluCylinder(pObj, 2.0f, 4.0f, 3.0f, 26, 1);
 gluCylinder(pObj, 0.0f, 2.0f, 0.10f, 26, 1);
 glColor3ub(47, 47, 79);
 glTranslatef(0.0f, 0.0f, 3.0f);
 gluCylinder(pObj, 4.0f, 5.0f, 5.0f, 26, 1);
 gluDisk(pObj, 0.0f, 4.0f, 26, 13);
 glPopMatrix();//first
 glPushMatrix();//second
 glColor3ub(139, 69, 19);
 glRotatef(-180, 0.0f, 1.0f, 1.0f);
 glTranslatef(10.0f, 0.0f, 3.0f); //first stick
 gluCylinder(pObj, 0.30f, 0.30f, 14.0f, 26, 1);
 glTranslatef(0.0f, 0.0f, 14.0f);
 gluDisk(pObj, 0.0f, 0.30f, 26, 13);
 glTranslatef(-10.0f, 0.0f, -14.0f); //second stick
 gluCylinder(pObj, 0.30f, 0.30f, 18.0f, 26, 1);
 glTranslatef(0.0f, 0.0f, 18.0f);
 gluDisk(pObj, 0.0f, 0.30f, 26, 13);
 glTranslatef(-10.0f, 0.0f, -17.0f); //third stick
 gluCylinder(pObj, 0.30f, 0.30f, 20.0f, 26, 1);
 glTranslatef(0.0f, 0.0f, 20.0f);
 gluDisk(pObj, 0.0f, 0.30f, 26, 13);
 glPopMatrix();//second
 glPushMatrix();//third
 glColor3ub(0, 0, 0);
 glTranslatef(-0.40f, 15.0f, 0.0f);
 glScalef(1.50f, 1.30f, 1.0f);
 glRotatef(-180, 1.0f, 1.0f, 0.0f);
 // Sets up the bezier
 // This actually only needs to be called once and could go in
 // the setup function
 glMap2f(GL_MAP2_VERTEX_3, // Type of data generated
 0.0f,      // Lower u range
 10.0f,      // Upper u range
 3,       // Distance between points in the data
 3,       // Dimension in u direction (order)
 0.0f,      // Lover v range
 10.0f,      // Upper v range
 9,       // Distance between points in the data
 3,       // Dimension in v direction (order)
 &ctrlPoints[0][0][0]);  // array of control points

 // Enable the evaluator
 glEnable(GL_MAP2_VERTEX_3);
 // Map a grid of 10 points from 0 to 10
 glMapGrid2f(10,0.0f,10.0f,10,0.0f,10.0f);
 // Evaluate the grid, using lines
 glEvalMesh2(GL_FILL,0,10,0,10);
 glPopMatrix();//third
 glPushMatrix();//fourth
 glTranslatef(9.70f, 15.0f, 0.0f);
 glScalef(1.50f, 1.5f, 1.0f);
 glRotatef(-180, 1.0f, 1.0f, 0.0f);
 glMap2f(GL_MAP2_VERTEX_3, 0.0f, 10.0f, 3, 3, 0.0f, 10.0f, 9,3, &ctrlPoints[0][0][0]);
 glEnable(GL_MAP2_VERTEX_3);
 glMapGrid2f(10,0.0f,10.0f,10,0.0f,10.0f);
 glEvalMesh2(GL_FILL,0,10,0,10);
 glPopMatrix();//fourth
 glPushMatrix();//fifth
 glTranslatef(-10.50f, 13.0f, 0.0f);
 glScalef(1.30f, 1.0f, 1.0f);
 glRotatef(-180, 1.0f, 1.0f, 0.0f);
 glMap2f(GL_MAP2_VERTEX_3, 0.0f, 10.0f, 3, 3, 0.0f, 10.0f, 9,3, &ctrlPoints[0][0][0]);
 glEnable(GL_MAP2_VERTEX_3);
 glMapGrid2f(10,0.0f,10.0f,10,0.0f,10.0f);
 glEvalMesh2(GL_FILL,0,10,0,10);
 glPopMatrix();//fifth
 glPushMatrix();//sixth
 glTranslatef(0.30f, 10.0f, 0.0f);
 glEnable(GL_LIGHTING);
 glEnable(GL_TEXTURE_2D);
 glColor3ub(255.0f, 255.0f, 255.0f);
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.0, 0.80);
            glVertex3fv(vCorners[7]);

            glTexCoord2f(0.170, 0.80);
            glVertex3fv(vCorners[8]);

            glTexCoord2f(0.170, 1.0);
            glVertex3fv(vCorners[9]);
 glEnd();
 glBegin(GL_TRIANGLES);
            glNormal3f(0.0f, -1.0f, 0.0f);
            glTexCoord2f(0.0, 0.80);
            glVertex3fv(vCorners[7]);

            glTexCoord2f(0.170, 1.0);
            glVertex3fv(vCorners[9]);

            glTexCoord2f(0.0, 1.0);
            glVertex3fv(vCorners[10]);
 glEnd();
 glDisable(GL_TEXTURE_2D);
 glPopMatrix();//sixth
 glPopMatrix();//the end of third ship
 /********************************************************************************/

 glPopMatrix(); // end of begin matrix

 // Display the results
 glutSwapBuffers();

 // Increment the angle of revolution
 fElect1 += 10.0f;
 if(fElect1 > 360.0f)
 fElect1 = 0.0f;

 // Increment the moving of path
 //first ship
 xMov = 12.0f*sin(angle);
 yMov = 12.0f*cos(angle);
 angle += 0.10f;
 if(angle > 6.35) {angle = 0.20f;}

 //second ship
 xMov2 = xMov2 + xr;
 yMov2 = yMov2 + yr;
 if (yMov2 == -50 && xMov2 == 0) { yr = 0; xr = -1; angle2 = 90;}
 if (xMov2 == -50 && yMov2 == -50) { yr = 1; xr = 0; angle2 = 360;}
 if (yMov2 == 0 && xMov2 == -50) { yr = 0; xr = 1; angle2 = 270;}
 if (xMov2 == 0 && yMov2 == 0) { yr = -1; xr = 0; angle2 = 180;}
 }




// This function does any needed initialization on the rendering
// context.
void SetupRC()
    {
    // Light values and coordinates
    GLfloat  ambientLight[] = { 0.5f, 0.5f, 0.5f, 0.0f };
    GLfloat  diffuseLight[] = { 0.7f, 0.7f, 0.7f, 1.0f };
    GLfloat  specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat  specref[] = { 1.0f, 1.0f, 1.0f, 1.0f };

 // Hidden surface removal
 glEnable(GL_DEPTH_TEST);

    // Enable lighting
    glEnable(GL_LIGHTING);

    // Setup and enable light 0
    glLightfv(GL_LIGHT0,GL_AMBIENT,ambientLight);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuseLight);
    glLightfv(GL_LIGHT0,GL_SPECULAR, specular);
    glEnable(GL_LIGHT0);


 // Light values and coordinates
    GLfloat  ambientLight1[] = { 1.30f, 1.30f, 1.30f, 1.0f };
    GLfloat  diffuseLight1[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat  specular1[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat  specref1[] = { 1.0f, 1.0f, 1.0f, 1.0f };
 // Setup and enable light 1
    glLightfv(GL_LIGHT1,GL_AMBIENT,ambientLight1);
    glLightfv(GL_LIGHT1,GL_DIFFUSE,diffuseLight1);
    glLightfv(GL_LIGHT1,GL_SPECULAR, specular1);

    // Enable color tracking
    glEnable(GL_COLOR_MATERIAL);

    // Set Material properties to follow glColor values
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    // All materials hereafter have full specular reflectivity
    // with a high shine
    glMaterialfv(GL_FRONT, GL_SPECULAR, specref);
    glMateriali(GL_FRONT, GL_SHININESS, 128);

    // choose background
    //glClearColor(1.0f, 1.0f, 1.0f, 1.0f ); //white
 glClearColor(0.621f, 0.71f, 0.80f, 1.0f); //soft light blue
 //glClearColor(0.999f, 0.644f, 0.0f, 1.0f); //sunset

    glEnable(GL_NORMALIZE);


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glEnable(GL_TEXTURE_2D);

 // Refresh the Window
 glutPostRedisplay();
    }

void TimerFunc(int value)
    {
    glutPostRedisplay();
    glutTimerFunc(100, TimerFunc, 1);
    }

// Handle arrow keys
void SpecialKeys(int key, int x, int y)
    {
    if(key == GLUT_KEY_DOWN)
  if(xRot>-85)
        xRot-= 5.0f;

    if(key == GLUT_KEY_UP)
  if(xRot<85)
        xRot += 5.0f;

    if(key == GLUT_KEY_LEFT)
        yRot -= 1.0f;

    if(key == GLUT_KEY_RIGHT)
        yRot += 1.0f;

    if(key > 360.0f)
        xRot = 0.0f;

    if(key < -1.0f)
        xRot = 355.0f;

    if(key > 356.0f)
        yRot = 0.0f;

    if(key < -1.0f)
        yRot = 355.0f;

    // Refresh the Window
    glutPostRedisplay();
    }


//////////////////////////////////////////////////////////
// Reset projection and light position
void ChangeSize(int w, int h)
    {
    GLfloat fAspect;
    GLfloat lightPos[] = { -50.f, 50.0f, 100.0f, 1.0f };
 GLfloat lightPos1[] = { 0.f, 42.0f, -100.0f, 1.0f };

    // Prevent a divide by zero
    if(h == 0)
        h = 1;

    // Set Viewport to window dimensions
    glViewport(0, 0, w, h);

    // Reset coordinate system
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    fAspect = (GLfloat) w / (GLfloat) h;
    gluPerspective(45.0f, fAspect, 1.0f, 225.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glLightfv(GL_LIGHT0,GL_POSITION,lightPos);
 glGetLightfv(GL_LIGHT1,GL_POSITION,lightPos1);

    glTranslatef(0.0f, 0.0f, -50.0f);
    }

int main(int argc, char* argv[])
    {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(500,500);
 glutInitWindowPosition(170,100);
 glutCreateWindow(".perahu layar.");
    glutReshapeFunc(ChangeSize);
    glutSpecialFunc(SpecialKeys);
    glutDisplayFunc(RenderScene);
 glutTimerFunc(500, TimerFunc, 1);
    SetupRC();
    glutMainLoop();

    return 0;
    }
