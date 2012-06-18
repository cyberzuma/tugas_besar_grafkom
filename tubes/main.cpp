#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif

static GLfloat spin, spin2 = 0.0;
float angle = 0;
float angles = 0.0f;
float deltaAngle = 0.0f;
float deltaMove = 0;
int xOrigin = -1;
//float x=0.0f, z=15.0f;
float x=0.0f, z=15.0f;
float lx=0.0f,lz=-1.0f;
float home=15.0f,end=15.0f;
//float home=85.0f,end=15.0f;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 50;
static int viewy = 24;
static int viewz = 80;

static float mv_c=1, mv_c1=1, mv=1, mv1=1, mv2=1, mv_r=1, mv_r1=1;
static float mva_c=1, mva_c1=1, mva=1, mva1=1, mva2=1, mva3=1, mva4=1, mva5=1;

float rot = 0;

// variables to compute frames per second
int frame;
long time, timebase;
char s[60];
char currentMode[80];

void renderBitmapString(
		float x,
		float y,
		float z,
		void *font,
		char *string) {

	char *c;
	glRasterPos3f(x, y,z);
	for (c=string; *c != '\0'; c++) {
		glutBitmapCharacter(font, *c);
	}
}

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Width
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.4f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;

const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
	delete _terrainAir;
}

//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	float scale = 1000.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

//=================================================================
// House
//=================================================================
void jendela()
{
    //window
	glPushMatrix();
	glTranslatef(-18.0f, 5.0f, 15.0f);
	glColor4f(0.35, 0.15, 0.0, 0.0);
	glScalef(1.5,1,0.1);
	glutSolidCube(7.0f);
	glPopMatrix();

	//kaca
	glPushMatrix();
	glTranslatef(-18.0f, 5.0f, 15.2f);
	glColor3f(0.5, 1, 1);
	glScalef(2.4,1.5,0.1);
	glutSolidCube(4.0f);
	glPopMatrix();
}


void drawRumah_hijau()
{
    //Wall
    glPushMatrix();
	glTranslatef(0.0f, 0.0f, -5.0f);
	glColor3f(0.5, 1, 0.5);
	glScalef(2.5,1,1);
	glutSolidCube(20.0f);
	glPopMatrix();

	glPushMatrix();
	//glRotatef(90, 1, 0, 0);
	glTranslatef(-13.0f, 0.0f, 10.0f);
	glColor3f(0.5, 1, 0.5);
	glScalef(1.2,1,0.5);
	glutSolidCube(20.0f);
	glPopMatrix();

    //tutup loteng
	glPushMatrix();
	glTranslatef(-15.0f, 10.1f, -5.0f);
	glColor3f(0, 0, 0);
	glScalef(0.5,0.02,0.5);
	glutSolidCube(20.0f);
	glPopMatrix();

	//Pintu
	glPushMatrix();
	glTranslatef(-5.0f, 4.0f, 15.0f);
	glColor4f(0.35, 0.15, 0.2, 1.0);
	glScalef(0.6,1,0.1);
	glutSolidCube(10.0f);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(-7.0f, 4.0f, 15.8f);
	glColor4f(0.35, 0.15, 0.2, 1.0);
	glutSolidSphere(0.5f,50.0f,25.0f);
	glPopMatrix();

	//window
	jendela();

	glPushMatrix();
	glTranslatef(28.0f, 1.0f, 3.55f);
	glScalef(0.4,0.85,0.1);
	jendela();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(22.0f, 1.0f, 3.55f);
	glScalef(0.4,0.85,0.1);
	jendela();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(16.0f, 1.0f, 3.55f);
	glScalef(0.4,0.85,0.1);
	jendela();
	glPopMatrix();

	//atap
	glPushMatrix();
	glTranslatef(16.0f, 1.0f, 3.55f);
	//glScalef(0.4,0.85,0.1);

	glPopMatrix();

   return;
}

void drawRumah_merah()
{
    //Wall
    glPushMatrix();
	glTranslatef(0.0f, 0.0f, -5.0f);
	glColor3f(0.2, 1, 0.2);
	glScalef(2.5,1,1);
	glutSolidCube(20.0f);
	glPopMatrix();

	glPushMatrix();
	//glRotatef(90, 1, 0, 0);
	glTranslatef(-13.0f, 0.0f, 10.0f);
	glScalef(1.2,1,0.5);
	glutSolidCube(20.0f);
	glPopMatrix();

    //tutup loteng
	glPushMatrix();
	glTranslatef(-15.0f, 10.1f, -5.0f);
	glColor3f(0, 0, 0);
	glScalef(0.5,0.02,0.5);
	glutSolidCube(20.0f);
	glPopMatrix();

	//Pintu
	glPushMatrix();
	glTranslatef(-5.0f, 4.0f, 15.0f);
	glColor4f(0.35, 0.15, 0.2, 1.0);
	glScalef(0.6,1,0.1);
	glutSolidCube(10.0f);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(-7.0f, 4.0f, 15.8f);
	glColor4f(0.35, 0.15, 0.2, 1.0);
	glutSolidSphere(0.5f,50.0f,25.0f);
	glPopMatrix();

	//window
	jendela();

	glPushMatrix();
	glTranslatef(28.0f, 1.0f, 3.55f);
	glScalef(0.4,0.85,0.1);
	jendela();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(22.0f, 1.0f, 3.55f);
	glScalef(0.4,0.85,0.1);
	jendela();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(16.0f, 1.0f, 3.55f);
	glScalef(0.4,0.85,0.1);
	jendela();
	glPopMatrix();

	//atap
	glPushMatrix();
	glTranslatef(16.0f, 1.0f, 3.55f);

	glPopMatrix();
   return;
}

//=================================================================
// Matras
//=================================================================
void drawMatras()
{

//----------------------------------------------

	glColor3f(1, 1, 1);
	glPushMatrix();
	//glBindTexture(GL_TEXTURE_2D, texture[5]);
	glScalef(30, 1, 30);
	//glRotatef(45, 0, 1, 0);
	glRotatef(90, 1, 0, 0);
	glTranslatef(-1.2, 1.7, -0.5);

	glBegin(GL_QUADS);
	glTexCoord2f(1, 1);
	glVertex3f(-1, -1, 0);
	glTexCoord2f(1, 0);
	glVertex3f(-1, 1, 0);
	glTexCoord2f(0, 0);
	glVertex3f(1, 1, 0);
	glTexCoord2f(0, 1);
	glVertex3f(1, -1, 0);
	glEnd();
	glPopMatrix();

   return;
}


//=================================================================
// Drone Toy
//=================================================================
void drawFan()
{
   glPushMatrix();
   glTranslatef(0.0f, 4.0f, -8.0f);
   glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
   glRotatef(0.0f, 1.0f, 0.0f, 0.0f);
   glColor3f(1.0f, 0.0f, 0.0f);
   glutSolidSphere(0.5, 20, 30);
   glPopMatrix();

    //kiri
    int i = 1;
    GLfloat xS = 20;
	GLfloat yS = 12;
	GLfloat zS = 50;

    //for (int i = 1; i <= 3; i++) {
    glPushMatrix();
    glTranslatef(0.0f, 4.0f, -8.0f);
	//glTranslatef(xS - 6, yS - 5 - 1, zS - 8.2);
	glRotatef(90*i, 1*i, 0, 0);
	//glRotatef(-15 + (i*8), 0, 1, 0);
	//glRotatef(10.5, 0, 0, 1);
	glScalef(4*4.5-i, 0.6 * 3, 2.5 * 3 - 1);

	glNormal3f(-0.97014254f, 0.0f, 0.24253564f);
	glutSolidSphere(0.5, 20, 30);

	glPopMatrix();
    //}

    //========================
    glColor3f(1.0f, 1.0f, 0.0f);
    glPushMatrix();
    glTranslatef(0.0f, 4.0f, -8.0f);
	//glTranslatef(xS - 6, yS - 5 - 1, zS - 8.2);
	glRotatef(100*i, 1, 90, 0);
	glScalef(4*4.5-i, 0.6 * 3, 2.5 * 3 - 1);

	glNormal3f(-0.97014254f, 0.0f, 0.24253564f);
	glutSolidSphere(0.5, 20, 30);

	glPopMatrix();

	for (int i = 0; i < 3; i++) {
		//kiri
		glPushMatrix();
		glTranslatef(xS - 8, yS - 7 - i, zS - 9);
		glRotatef(80 + 15, 1, 0, 0);
		glRotatef(-15 + (i * 38), 0, 1, 0);
		//glRotatef(10.5, 0, 0, 1);
		glScalef(4 * 4.5 - i, 0.6 * 3, 2.5 * 3 - 1);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();
	}
   return;
}

//=================================================================
// Duck Toy
//=================================================================
void drawBebek() {

	GLfloat xS = 20;
	GLfloat yS = 12;
	GLfloat zS = 50;
	//kepala
	glColor3f(0.88, 0.88, 0.1);
	//glPushMatrix();
	glPushMatrix();

	glTranslatef(xS + 1, yS + 3, zS);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	glColor3f(0.8, 0.3, 0.4);
	//glPushMatrix();

	glPushMatrix();
	glTranslatef(xS + 1, yS + 6, zS);
	glScalef(3 * 4, 3 * 4, 0.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	GLfloat x = 0.6 - 50;
	GLfloat y = 2 + 12;
	GLfloat z = 1.4 + 24;

	glColor3f(0.7, 0.6, 0.4);
	glPushMatrix();
	glRotatef(95, 0, 1, 0);
	glTranslatef(x - 4.4, y + 2, z - 4.6);
	glScalef(1.5 * 4, 1.5 * 4, 1.5 * 4);

	glutSolidSphere(0.3, 20, 30);

	glPopMatrix();

	glColor3f(0.0, 0.0, 0.0);
	glPushMatrix();
	glRotatef(95, 0, 1, 0);
	glTranslatef(x - 4.7, y + 2.1, z - 3.4);
	glScalef(1.5 * 3, 1.5 * 3, 1.5 * 3);
	glutSolidSphere(0.18, 20, 30);

	glPopMatrix();

	glPushMatrix();
	glColor3f(0.7, 0.6, 0.4);
	glRotatef(83, 0, 1, 0);
	glTranslatef(x + 4.9, y + 2.1, z + 5.8);
	glScalef(1.5 * 4, 1.5 * 4, 1.5 * 4);
	glutSolidSphere(0.3, 20, 30);

	glPopMatrix();

	glPushMatrix();
	glColor3f(0.0, 0.0, 0.0);
	glRotatef(83, 0, 1, 0);
	glTranslatef(x + 4.9, y + 2.1, z + 6.65);
	glScalef(1.5 * 4, 1.5 * 4, 1.5 * 4);
	glutSolidSphere(0.18, 20, 30);

	glPopMatrix();

	glColor3f(0.88, 0.88, 0.3);

	//mulut


	glPushMatrix();

	glTranslatef(xS + 8, yS + 2, zS);
	glRotatef(180, 0, 0, 1);
	glScalef(4 * 3, 0.6 * 3, 2.5 * 3);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();
	//

	glColor3f(0.88, 0.2, 0.5);

	glPushMatrix();

	glTranslatef(xS + 1, yS - 3, zS);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);
	glRotatef(90, 1, 0, 0);

	glutSolidTorus(0.2, 0.3, 20, 20);

	//badan agak atas
	glPopMatrix();

	glColor3f(0.88, 0.88, 0.1);

	glPushMatrix();
	glTranslatef(xS + 1, yS - 6, zS - 0.4);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	//badan utama
	glPushMatrix();
	glColor3f(0.88, 0.88, 0.1);
	glTranslatef(xS - 7, yS - 10, zS - 0.3);
	glScalef(5 * 4, 4 * 4, 4 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();

	//badan depan bawah
	glPushMatrix();
	glColor3f(0.88, 0.88, 0.1);
	glTranslatef(xS, yS - 8, zS - 0.2);
	glScalef(3 * 4, 3 * 4, 3.5 * 4);

	glutSolidSphere(0.5, 20, 30);
	glPopMatrix();
	//
	int sudutSayap = 80;
	//sayap
	for (int i = 0; i < 3; i++) {
		//kanan
		glPushMatrix();
		glTranslatef(xS - 6, yS - 5 - i, zS + 7);
		glRotatef(sudutSayap, 1, 0, 0);
		glRotatef(-15 + (i * 8), 0, 1, 0);
		glRotatef(-8.5, 0, 0, 1);
		glScalef(4 * 4.5 - i, 0.6 * 3, 2.5 * 3 - 1);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();
		//kiri
		glPushMatrix();
		glTranslatef(xS - 6, yS - 5 - i, zS - 8.2);
		glRotatef(sudutSayap + 15, 1, 0, 0);
		glRotatef(-15 + (i * 8), 0, 1, 0);
		glRotatef(10.5, 0, 0, 1);
		glScalef(4 * 4.5 - i, 0.6 * 3, 2.5 * 3 - 1);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();

	}

	//buntut
	int sudut = 145;
	for (int i = 0; i < 4; i++) {
		glPushMatrix();

		glTranslatef(xS - 12.5 + i, yS - 5 - i, zS);
		glRotatef(sudut, 0, 0, 1);
		glScalef(4 * 6.5, 0.6 * 6, (2.5 * 4) - i);

		glutSolidSphere(0.5, 20, 30);
		glPopMatrix();
		sudut += 10;
	}
	glPopMatrix();
}

unsigned int LoadTextureFromBmpFile(char *filename);

void computePos(float deltaMove) {

	/*x += deltaMove * lx * 0.01f;
	z += deltaMove * lz * 0.01f;*/

	x += deltaMove * lx * 0.25f;
	z += deltaMove * lz * 0.25f;
}

void display(void) {

    if (deltaMove)
		computePos(deltaMove);

	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	//glClearColor(0.0, 0.6, 0.8, 1); //warna langit
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	//gluLookAt(viewx-80, viewy, viewz-80, 0.0, 0.0, 5.0, 0.0, 1.0, 0.0);

    gluLookAt(	x, home, z,
			x+lx, 15.0f,  z+lz,
			0.0f, 1.0f,  0.0f);

    //gluLookAt(view,camera,);

	glPushMatrix();
	drawSceneTanah(_terrain, 0.4f, 1.0f, 0.1f);
	glPopMatrix();

	glPushMatrix();
	drawSceneTanah(_terrainTanah, 0.6f, 0.2f, 0.1f);
	glPopMatrix();

	glPushMatrix();
	drawSceneTanah(_terrainAir, 0.1f, 0.2f, 0.9f);
	glPopMatrix();

    //Rumah
    glPushMatrix();
    glTranslatef(240.0,0.0,-180.0);
    drawRumah_hijau();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(180.0,0.0,-180.0);
    drawRumah_hijau();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(120.0,0.0,-180.0);
    drawRumah_hijau();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(60.0,0.0,-180.0);
    drawRumah_hijau();
    glPopMatrix();

    //matras
    //drawMatras();

    //fan
    //drawFan();

    //sample bebek1
    glPushMatrix();
    glTranslatef(400-(1.5*mv)+(1.5*mv1), -11, 80+(mv*0.35)-(mv1*0.35));
	glScalef(0.2, 0.2, 0.2);
	glRotatef(195, 0, 2, 0);
	drawBebek();
	glPopMatrix();

	//sample bebek2
    glPushMatrix();
    glTranslatef(140+(1.5*mv)-(1.5*mv1), -11, 95-(mv*0.35)+(mv1*0.35));
    //glTranslatef(400-(1.5*mv2), -10, 80+(mv2*0.35));
	glScalef(0.3, 0.3, 0.3);
	glRotatef(195, 0, 2, 0);
	glRotated(175, 0, 1, 0);

	if(mv_c==0)
	{glRotated((mv_r-mv_r1), 0, 1, 0);}

	drawBebek();
	glPopMatrix();

	//sample bebek2_a
    glPushMatrix();
    glTranslatef(135+(1.5*mv)-(1.5*mv1), -11, 95-(mv*0.35)+(mv1*0.35));
    //glTranslatef(400-(1.5*mv2), -10, 80+(mv2*0.35));
	glScalef(0.1, 0.1, 0.1);
	glRotatef(195, 0, 2, 0);
	glRotated(175, 0, 1, 0);
	if(mv_c==0)
	{glRotated((mv_r-mv_r1), 0, 1, 0);}
	drawBebek();
	glPopMatrix();

	//sample bebek2_b
    glPushMatrix();
    glTranslatef(128+(1.5*mv)-(1.5*mv1), -11, 90-(mv*0.35)+(mv1*0.35));
    //glTranslatef(400-(1.5*mv2), -10, 80+(mv2*0.35));
	glScalef(0.1, 0.1, 0.1);
	glRotatef(195, 0, 2, 0);
	glRotated(175, 0, 1, 0);
	if(mv_c==0)
	{glRotated((mv_r-mv_r1), 0, 1, 0);}
	drawBebek();
	glPopMatrix();

	//sample bebek2_c
    glPushMatrix();
    glTranslatef(160+(1.5*mv)-(1.5*mv1), -11, 95-(mv*0.35)+(mv1*0.35));
    //glTranslatef(400-(1.5*mv2), -10, 80+(mv2*0.35));
	glScalef(0.1, 0.1, 0.1);
	glRotatef(195, 0, 2, 0);
	glRotated(175, 0, 1, 0);
	if(mv_c==0)
	{glRotated((mv_r-mv_r1), 0, 1, 0);}
	drawBebek();
	glPopMatrix();

	//sample bebek2_d
    glPushMatrix();
    glTranslatef(160+(1.5*mv)-(1.5*mv1), -11, 90-(mv*0.35)+(mv1*0.35));
    //glTranslatef(400-(1.5*mv2), -10, 80+(mv2*0.35));
	glScalef(0.1, 0.1, 0.1);
	glRotatef(195, 0, 2, 0);
	glRotated(175, 0, 1, 0);
	if(mv_c==0)
	{glRotated((mv_r-mv_r1), 0, 1, 0);}
	drawBebek();
	glPopMatrix();

	//sample bebek3
    glPushMatrix();
    glTranslatef(130+(0.05*mva)-(0.5*mva1)+mva2, -11, 120+(mva*1.35)-(mva1*1.35)-(mva2*1.90));
    //glTranslatef(400-(1.5*mv2), -10, 80+(mv2*0.35));
	glScalef(0.2, 0.2, 0.2);
	glRotatef(195, 0, 2, 0);
	glRotated(75, 0, 1, 0);
	drawBebek();
	glPopMatrix();


	//sample bebek4
    glPushMatrix();
    glTranslatef(350-(1.5*mv)+(1.5*mv1), -11, 80+(mv*0.35)-(mv1*0.35));
	glScalef(0.2, 0.2, 0.2);
	glRotatef(195, 0, 2, 0);
	drawBebek();
	glPopMatrix();

	glutSwapBuffers();
}

void reshape(int w, int h) {

	float ratio;
    ratio = 1.0f * w / h;
	// Reset the coordinate system before modifying
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Set the viewport to be the entire window
    glViewport(0, 0, w, h);

	// Set the clipping volume
	gluPerspective(45,ratio,0.1,1000);
	glMatrixMode(GL_MODELVIEW);
}

// -----------------------------------
//             KEYBOARD
// -----------------------------------

void processNormalKeys(unsigned char key, int xx, int yy) {

	switch (key) {
		case 27:
			if (glutGameModeGet(GLUT_GAME_MODE_ACTIVE) != 0)
				glutLeaveGameMode();
			exit(0);
			break;
	}

	if (key == 'w')
	{deltaMove = 10.0f;}
	else
	if (key == 's')
	{deltaMove = -10.0f;}
	else
	{deltaMove = 0.0f;}
}


void pressKey(int key, int xx, int yy) {
	switch (key) {
		case GLUT_KEY_UP : deltaMove = 10.0f; break;
		case GLUT_KEY_DOWN : deltaMove = -10.0f; break;
		case GLUT_KEY_HOME : home += 10.0f; break;
		case GLUT_KEY_END : home -= 10.0f; break;
		case GLUT_KEY_PAGE_UP : home += 0.5f; break;
		case GLUT_KEY_PAGE_DOWN : home -= 0.5f; break;

	}
	if (glutGameModeGet(GLUT_GAME_MODE_ACTIVE) == 0)
		sprintf(currentMode,"Current Mode: Window");
	else
		sprintf(currentMode,
			"Current Mode: Game Mode %dx%d at %d hertz, %d bpp",
			glutGameModeGet(GLUT_GAME_MODE_WIDTH),
			glutGameModeGet(GLUT_GAME_MODE_HEIGHT),
			glutGameModeGet(GLUT_GAME_MODE_REFRESH_RATE),
			glutGameModeGet(GLUT_GAME_MODE_PIXEL_DEPTH));
}

void releaseKey(int key, int x, int y) {

	switch (key) {
		case GLUT_KEY_UP :
		case GLUT_KEY_DOWN : deltaMove = 0;break;
	}
}

// -----------------------------------
//             MOUSE
// -----------------------------------

void mouseMove(int x, int y) {

	// this will only be true when the left button is down
	if (xOrigin >= 0) {

		// update deltaAngle
		deltaAngle = (x - xOrigin) * 0.01f;

		// update camera's direction
		lx = sin(angle + deltaAngle);
		lz = -cos(angle + deltaAngle);
	}
}

void mouseButton(int button, int state, int x, int y) {

	// only start motion if the left button is pressed
	if (button == GLUT_LEFT_BUTTON) {

		// when the button is released
		if (state == GLUT_UP) {
			angle += deltaAngle;
			xOrigin = -1;
		}
		else  {// state = GLUT_DOWN
			xOrigin = x;
		}
	}
}

void animate()
{
     //==============================
    if(mv_c==1)
    {
        mv+=1;
        if(mv==160)
        {
            mv_c=0;
        }
    }

    //berotasi_1 180 derajat
    if(mv_c==0 and mv_r<=180)
    mv_r+=1;

    if(mv_c==0 and mv_r==181 and mv_c1==1)
    {
        mv1+=1;
        if(mv1==160)
        {
            mv_c1=0;
        }
    }

    //berotasi_2 180 derajat
    if(mv_c1==0 and mv_r1<=180)
    mv_r1+=1;

    if(mv_c1==0 and mv_r1==181)
    {
        mv=1;
        mv1=1;
        mv_c=1;
        mv_c1=1;
        mv_r=0;
        mv_r1=0;
    }

    //moving bebek 3
    //==============================
    if(mva_c==1 and mva_c1==1)
    {
        mva+=1;
        if(mva==160)
        mva_c=0;
    }

    if(mva_c==0 and mva_c1==1)
    {
        mva1+=1;
        if(mva1==100)
        mva_c1=0;
    }

    if(mva_c==0 and mva_c1==0)
    {
        mva2+=1;
        if(mva2==45)
        {
            mva_c=1;
            mva_c1=1;

            mva=1;
            mva1=1;
            mva2=1;
        }
    }

	// Normaly openGL doesn't continuosly draw frames. It puts one in place and waits for you to tell him what to do next.
	// Calling glutPostRedisplay() forces a redraw with the new angle
	glutPostRedisplay();
}

void init() {

	// register callbacks
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);

    glutIgnoreKeyRepeat(1);
	glutKeyboardFunc(processNormalKeys);
	glutSpecialFunc(pressKey);
	glutSpecialUpFunc(releaseKey);
	glutMouseFunc(mouseButton);
	glutMotionFunc(mouseMove);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	_terrain = loadTerrain("heightmap.bmp", 20);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", 20);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("TuBes - Grafkomp");
	init();
	initRendering();

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 1;
}
