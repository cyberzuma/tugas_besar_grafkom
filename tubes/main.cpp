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

/*char gameModeString[40] = "800x600";
void init();*/

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
	glTranslatef(-13.0f, 0.0f, 10.0f);
	glColor3f(0.5, 1, 0.5);
	glScalef(1.2,1,0.5);
	glutSolidCube(20.0f);
	glPopMatrix();

	//Pintu
	glPushMatrix();
	glTranslatef(-5.0f, 2.0f, 15.0f);
	glColor4f(0.35, 0.15, 0.2, 1.0);
	glScalef(0.6,1.3,0.1);
	glutSolidCube(10.0f);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(-7.0f, 1.0f, 15.8f);
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

//Meja
void Meja()
{
	// Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f);     // kaki kanan depan
    glTranslatef(1.0f, -2.0f, 3.0f);
    glScaled(1.0, 3.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kaki kanan belakang
    glTranslatef(1.0f, -2.0f, 1.0f);
    glScaled(1.0, 3.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kaki kiri belakang
    glTranslatef(-3.0f, -2.0f, 1.0f);
    glScaled(1.0, 3.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kaki kiri depan
    glTranslatef(-3.0f, -2.0f, 3.0f);
    glScaled(1.0, 3.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // alas meja
    glTranslatef(-1.0f, -1.25f, 2.0f);
    glScaled(9.0, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();
}

//Kayu Bakar
void KayuBakar()
{
    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kanan bawah
    glTranslatef(1.1f, -3.0f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kiri bawah
    glTranslatef(-1.1f, -3.0f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu depan bawah
    glTranslatef(0.0f, -2.75f, -1.85f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu belakang bawah
    glTranslatef(0.0f, -2.75f, -4.1f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kanan bawah 1
    glTranslatef(1.1f, -2.5f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kiri bawah 1
    glTranslatef(-1.1f, -2.5f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu depan bawah 1
    glTranslatef(0.0f, -2.25f, -1.85f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu belakang bawah 1
    glTranslatef(0.0f, -2.25f, -4.1f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kanan bawah 2
    glTranslatef(1.1f, -2.0f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kiri bawah 2
    glTranslatef(-1.1f, -2.0f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu depan bawah 2
    glTranslatef(0.0f, -1.75f, -1.85f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu belakang bawah 2
    glTranslatef(0.0f, -1.75f, -4.1f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kanan bawah 3
    glTranslatef(1.1f, -1.5f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu kiri bawah 3
    glTranslatef(-1.1f, -1.5f, -3.0f);
    glScaled(0.5, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu depan bawah 3
    glTranslatef(0.0f, -1.25f, -1.85f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();

    //kayu
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kayu belakang bawah 3
    glTranslatef(0.0f, -1.25f, -4.1f);
    glScaled(5.0, 0.5 , 0.5);
    glutSolidCube(0.5f);
    glPopMatrix();
}

void Bintang()
{
    glPushMatrix();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTranslatef(0.0f, -1.25f, -4.1f);
    glScaled(0.5, 0.5, 0.5);
    glutSolidSphere(1.0f, 100, 10);
    glPopMatrix();
}

void Kursi()
{
    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f);     // kanan depan
    glTranslatef(-1.0f, -2.0f, 3.0f);
    glScaled(1.0, 3.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kanan belakang
    glTranslatef(-1.0f, -1.0f, 1.0f);
    glScaled(1.0, 7.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f, 0.15f, 0.0f, 1.0f); // kiri belakang
    glTranslatef(-3.0f, -1.0f, 1.0f);
    glScaled(1.0, 7.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f,0.15f,0.0f,1.0f); // kiri depan
    glTranslatef(-3.0f, -2.0f, 3.0f);
    glScaled(1.0, 3.0, 1.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f,0.15f,0.0f,1.0f); // alas
    glTranslatef(-2.0f, -1.25f, 2.0f);
    glScaled(5.0, 0.5 , 5.0);
    glutSolidCube(0.5f);
    glPopMatrix();

    // Draw kubus
    glPushMatrix();
	glColor4f(0.35f,0.15f,0.0f,1.0f); // alas atas
    glTranslatef(-2.0f, 0.24f, 1.25f);
    glScaled(5.0, 2.0 , 0.25);
    glutSolidCube(0.5f);
    glPopMatrix();
}

void Tenda()
{
    // Draw Pyramid
    glPushMatrix();
    glColor4f(0.0f, 0.1f, 0.0f, 0.0f);

	glBegin(GL_TRIANGLES);
		glNormal3f(1,0,1);
        // first triangle
		glVertex3f(1,0,1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(-1,0,1); //
		glVertex3f(-1,0,1); //
		glNormal3f(-1,0,1);
        // second triangle
		glVertex3f(-1,0,1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(-1,0,-1); //
		glVertex3f(-1,0,-1); //
		glNormal3f(-1,0,-1);
        // third triangle
		glVertex3f(-1,0,-1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(1,0,-1); //
		glVertex3f(1,0,-1); //
		glNormal3f(1,0,-1);
        // last triangle
		glVertex3f(1,0,-1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(1,0,1); //
		glVertex3f(1,0,1); //
	glEnd();

    // alas piramid
    glColor4f(0.0f, 0.1f, 0.0f, 0.0f);
	glBegin(GL_TRIANGLES);
		glNormal3f(-1,0,1);
		glVertex3f(-1,0,1);
		glNormal3f(-1,0,-1);
		glVertex3f(-1,0,-1);
		glNormal3f(1,0,1);
		glVertex3f(1,0,1);
		glNormal3f(1,0,1);
		glVertex3f(1,0,1);
		glNormal3f(-1,0,-1);
		glVertex3f(-1,0,-1);
		glNormal3f(1,0,-1);
		glVertex3f(1,0,-1);
	glEnd();
	glPopMatrix();
}

void PintuTenda()
{
    // Draw PintuTenda
    glPushMatrix();
    glColor4f(0.3f, 0.0f, 0.0f, 0.0f);

	glBegin(GL_TRIANGLES);
		glNormal3f(1,0,1);
        // first triangle
		glVertex3f(1,0,1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(-1,0,1); //
		glVertex3f(-1,0,1); //
		glNormal3f(-1,0,1);
        // second triangle
		glVertex3f(-1,0,1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(-1,0,-1); //
		glVertex3f(-1,0,-1); //
		glNormal3f(-1,0,-1);
        // third triangle
		glVertex3f(-1,0,-1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(1,0,-1); //
		glVertex3f(1,0,-1); //
		glNormal3f(1,0,-1);
        // last triangle
		glVertex3f(1,0,-1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(1,0,1); //
		glVertex3f(1,0,1); //
	glEnd();

    // alas piramid
    glColor4f(0.0f, 0.1f, 0.0f, 0.0f);
	glBegin(GL_TRIANGLES);
		glNormal3f(-1,0,1);
		glVertex3f(-1,0,1);
		glNormal3f(-1,0,-1);
		glVertex3f(-1,0,-1);
		glNormal3f(1,0,1);
		glVertex3f(1,0,1);
		glNormal3f(1,0,1);
		glVertex3f(1,0,1);
		glNormal3f(-1,0,-1);
		glVertex3f(-1,0,-1);
		glNormal3f(1,0,-1);
		glVertex3f(1,0,-1);
	glEnd();
	glPopMatrix();
}

void AtapRumah()
{
    // Draw atap
    glPushMatrix();
    glColor4f(0.0f, 0.1f, 0.5f, 0.0f);

	glBegin(GL_TRIANGLES);
		glNormal3f(1,0,1);
        // first triangle
		glVertex3f(1,0,1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(-1,0,1); //
		glVertex3f(-1,0,1); //
		glNormal3f(-1,0,1);
        // second triangle
		glVertex3f(-1,0,1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(-1,0,-1); //
		glVertex3f(-1,0,-1); //
		glNormal3f(-1,0,-1);
        // third triangle
		glVertex3f(-1,0,-1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(1,0,-1); //
		glVertex3f(1,0,-1); //
		glNormal3f(1,0,-1);
        // last triangle
		glVertex3f(1,0,-1); //
		glNormal3f(0,2,0); //
		glVertex3f(0,2,0); //
		glNormal3f(1,0,1); //
		glVertex3f(1,0,1); //
	glEnd();

    // alas piramid
    glColor4f(0.0f, 0.1f, 0.5f, 0.0f);
	glBegin(GL_TRIANGLES);
		glNormal3f(-1,0,1);
		glVertex3f(-1,0,1);
		glNormal3f(-1,0,-1);
		glVertex3f(-1,0,-1);
		glNormal3f(1,0,1);
		glVertex3f(1,0,1);
		glNormal3f(1,0,1);
		glVertex3f(1,0,1);
		glNormal3f(-1,0,-1);
		glVertex3f(-1,0,-1);
		glNormal3f(1,0,-1);
		glVertex3f(1,0,-1);
	glEnd();
	glPopMatrix();
}

void pohon1()
{
    // Draw daun bulet"
    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 5.24f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

   // tengah
 	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 4.14f, 1.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 4.8f, 0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 3.4f, 1.1f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 2.7f, 0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

   // daun kanan
 	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(1.0f, 4.8f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(1.1f, 4.15f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(1.0f, 3.4f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.7f, 2.7f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	// daun kiri
 	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-1.0f, 4.8f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-1.1f, 4.15f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-1.0f, 3.4f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.7f, 2.7f, 0.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	// daun belakang
	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 4.14f, -1.0f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 4.8f, -0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 3.4f, -1.1f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 2.7f, -0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	// serong kanan
	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.7f, 4.8f, 0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(1.0f, 4.15f, 0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.8f, 3.4f, 0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.7f, 2.7f, 0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	// serong kiri
	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.7f, 4.8f, 0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-1.0f, 4.15f, 0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.8f, 3.4f, 0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.7f, 2.7f, 0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	// serong kanan belakang
	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.7f, 4.8f, -0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(1.0f, 4.15f, -0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.8f, 3.4f, -0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.7f, 2.7f, -0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	// serong kiri belakang
		glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.7f, 4.8f, -0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-1.0f, 4.15f, -0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

	glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.8f, 3.4f, -0.8f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();

    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(-0.7f, 2.7f, -0.7f);
	glScaled(3,3,3);
	glutSolidSphere(0.25f,50,50);
	glPopMatrix();


	// Draw batang
	glPushMatrix();
	glColor3f(0.8f, 0.4f, 0.1f);     // coklat
    glTranslatef(0.0f, 1.5f, 0.0f);
    glScaled(1.1,6,1);
    glutSolidCube(0.5f);
    glPopMatrix();

}

void pohon2()
{
    // Draw daun cone
    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 2, 0.0f);
	glScaled(2.5,2.5,2.5);
	glRotated(-90,1,0,0);
	glutSolidCone(1,2,60,1);
	glPopMatrix();



	// Draw batang
	glPushMatrix();
	glColor3f(0.8f, 0.4f, 0.1f);     // coklat
    glTranslatef(0.0f, 1.5f, 0.0f);
    glScaled(1.1,6,1);
    glutSolidCube(0.5f);
    glPopMatrix();

}

void pohon3()
{
    // Draw daun octahedron
    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 4.6f, 0.0f);
	glScaled(2.5,2.5,2.5);
	glutSolidOctahedron();
	glPopMatrix();

	// Draw batang
	glPushMatrix();
	glColor3f(0.8f, 0.4f, 0.1f);     // coklat
    glTranslatef(0.0f, 1.5f, 0.0f);
    glScaled(1.1,6,1);
    glutSolidCube(0.5f);
    glPopMatrix();

}

void pohon4()
{
    // Draw daun bulet
    glPushMatrix();
    glColor3f(0.0f, 0.3f, 0.0f);
    glTranslatef(0.0f, 3.24f, 0.0f);
	glScaled(7,7,7);
	glutSolidSphere(0.25f,5,50);
	glPopMatrix();

	// Draw batang
	glPushMatrix();
	glColor3f(0.8f, 0.4f, 0.1f);     // coklat
    glTranslatef(0.0f, 1.5f, 0.0f);
    glScaled(1.1,6,1);
    glutSolidCube(0.5f);
    glPopMatrix();
}

void computePos(float deltaMove) {

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

    gluLookAt(	x, home, z,
			x+lx, 15.0f,  z+lz,
			0.0f, 1.0f,  0.0f);

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

	//meja 1
    glPushMatrix();
    glTranslatef(65, 3, -230);
    glScalef(2.5f, 2.5f, 2.5f);
    Meja();
    glPopMatrix();

    //meja 2
    glPushMatrix();
    glTranslatef(125, 3, -230);
    glScalef(2.5f, 2.5f, 2.5f);
    Meja();
    glPopMatrix();

    //meja 3
    glPushMatrix();
    glTranslatef(185, 3, -230);
    glScalef(2.5f, 2.5f, 2.5f);
    Meja();
    glPopMatrix();

    //meja 4
    glPushMatrix();
    glTranslatef(245, 3, -230);
    glScalef(2.5f, 2.5f, 2.5f);
    Meja();
    glPopMatrix();

    //kayubakar 1
    glPushMatrix();
    glTranslatef(63, 0, -240);
    glScalef(1.0f, 1.0f, 1.0f);
    KayuBakar();
    glPopMatrix();

    //kayubakar 2
    glPushMatrix();
    glTranslatef(123, 0, -240);
    glScalef(1.0f, 1.0f, 1.0f);
    KayuBakar();
    glPopMatrix();

    //kayubakar 3
    glPushMatrix();
    glTranslatef(183, 0, -240);
    glScalef(1.0f, 1.0f, 1.0f);
    KayuBakar();
    glPopMatrix();

    //kayubakar 4
    glPushMatrix();
    glTranslatef(243, 0, -240);
    glScalef(1.0f, 1.0f, 1.0f);
    KayuBakar();
    glPopMatrix();

    for (int i = -10; i < 10; i++)
        for (int j = -10; j < 10; j++)
        {
            glPushMatrix();
            glTranslatef(i*280, 200, j*200);
            glScalef(3.5f, 3.5f, 3.5f);
            Bintang();
            glPopMatrix();
        }

    //kursi1
    glPushMatrix();
    glTranslatef(65, 0, -235);
    glScalef(1.5f, 1.5f, 1.5f);
    Kursi();
    glPopMatrix();

    //kursi2
    glPushMatrix();
    glTranslatef(125, 0, -235);
    glScalef(1.5f, 1.5f, 1.5f);
    Kursi();
    glPopMatrix();

    //kursi3
    glPushMatrix();
    glTranslatef(185, 0, -235);
    glScalef(1.5f, 1.5f, 1.5f);
    Kursi();
    glPopMatrix();

    //kursi4
    glPushMatrix();
    glTranslatef(245, 0, -235);
    glScalef(1.5f, 1.5f, 1.5f);
    Kursi();
    glPopMatrix();

    //kursi5
    glPushMatrix();
    glTranslatef(50, 0, -228);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi6
    glPushMatrix();
    glTranslatef(110, 0, -228);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi7
    glPushMatrix();
    glTranslatef(170, 0, -228);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi8
    glPushMatrix();
    glTranslatef(230, 0, -228);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi9
    glPushMatrix();
    glTranslatef(75, 0, -222);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(-90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi10
    glPushMatrix();
    glTranslatef(135, 0, -222);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(-90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi11
    glPushMatrix();
    glTranslatef(195, 0, -222);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(-90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi12
    glPushMatrix();
    glTranslatef(255, 0, -222);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(-90.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi13
    glPushMatrix();
    glTranslatef(59, 0, -215);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(180.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi14
    glPushMatrix();
    glTranslatef(119, 0, -215);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(180.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi15
    glPushMatrix();
    glTranslatef(179, 0, -215);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(180.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

    //kursi16
    glPushMatrix();
    glTranslatef(239, 0, -215);
    glScalef(1.5f, 1.5f, 1.5f);
    glRotatef(180.0f, 0.0f , 1.0f ,0.0f);
    Kursi();
    glPopMatrix();

//======Tenda======

    //tenda
    glPushMatrix();
    glTranslatef(50, -3, -250);
    glScalef(7.0f, 6.0f, 7.0f);
    Tenda();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(50, -3, -249.5);
    glScalef(1.0f, 5.5f, 6.7f);
    PintuTenda();
    glPopMatrix();

    //tenda2
    glPushMatrix();
    glTranslatef(110, -3, -250);
    glScalef(7.0f, 6.0f, 7.0f);
    Tenda();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(110, -3, -249.5);
    glScalef(1.0f, 5.5f, 6.7f);
    PintuTenda();
    glPopMatrix();

    //tenda3
    glPushMatrix();
    glTranslatef(170, -3, -250);
    glScalef(7.0f, 6.0f, 7.0f);
    Tenda();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(170, -3, -249.5);
    glScalef(1.0f, 5.5f, 6.7f);
    PintuTenda();
    glPopMatrix();

    //tenda2
    glPushMatrix();
    glTranslatef(230, -3, -250);
    glScalef(7.0f, 6.0f, 7.0f);
    Tenda();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(230, -3, -249.5);
    glScalef(1.0f, 5.5f, 6.7f);
    PintuTenda();
    glPopMatrix();


//======Atap Rumah======

    //atap1
    glPushMatrix();
    glTranslatef(73, 10, -185);
    glScalef(14.0f, 5.0f, 11.0f);
    AtapRumah();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(47, 10, -170);
    glScalef(13.0f, 4.0f, 5.0f);
    AtapRumah();
    glPopMatrix();

    //atap2
    glPushMatrix();
    glTranslatef(133, 10, -185);
    glScalef(14.0f, 5.0f, 11.0f);
    AtapRumah();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(107, 10, -170);
    glScalef(13.0f, 4.0f, 5.0f);
    AtapRumah();
    glPopMatrix();

    //atap3
    glPushMatrix();
    glTranslatef(193, 10, -185);
    glScalef(14.0f, 5.0f, 11.0f);
    AtapRumah();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(167, 10, -170);
    glScalef(13.0f, 4.0f, 5.0f);
    AtapRumah();
    glPopMatrix();

    //atap4
    glPushMatrix();
    glTranslatef(253, 10, -185);
    glScalef(14.0f, 5.0f, 11.0f);
    AtapRumah();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(227, 10, -170);
    glScalef(13.0f, 4.0f, 5.0f);
    AtapRumah();
    glPopMatrix();


//======pohon======

    //pohon1
    glPushMatrix();
    glTranslatef(70,-2.5,-270);
    glScalef(7,7,7);
    pohon1();
    glPopMatrix();

    //pohon1.1
    glPushMatrix();
    glTranslatef(130,-2.5,-270);
    glScalef(7,7,7);
    pohon1();
    glPopMatrix();

    //pohon1.2
    glPushMatrix();
    glTranslatef(190,-2.5,-270);
    glScalef(7,7,7);
    pohon1();
    glPopMatrix();


    //pohon1.3
    glPushMatrix();
    glTranslatef(250,-2.5,-270);
    glScalef(7,7,7);
    pohon1();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(73,-2.5,-150);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3.1
    glPushMatrix();
    glTranslatef(133,-2.5,-150);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3.2
    glPushMatrix();
    glTranslatef(193,-2.5,-150);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3.3
    glPushMatrix();
    glTranslatef(250,-2.5,-150);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon2.21
    glPushMatrix();
    glTranslatef(450,5,200);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.22
    glPushMatrix();
    glTranslatef(400,5,200);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.24
    glPushMatrix();
    glTranslatef(300,5,200);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.23
    glPushMatrix();
    glTranslatef(350,5,200);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.25
    glPushMatrix();
    glTranslatef(250,5,200);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.26
    glPushMatrix();
    glTranslatef(200,5,200);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.11
    glPushMatrix();
    glTranslatef(400,5,150);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.11
    glPushMatrix();
    glTranslatef(350,5,150);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.11
    glPushMatrix();
    glTranslatef(300,5,150);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.31
    glPushMatrix();
    glTranslatef(450,3,250);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.32
    glPushMatrix();
    glTranslatef(400,5,250);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.33
    glPushMatrix();
    glTranslatef(350,3,250);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.34
    glPushMatrix();
    glTranslatef(300,5,250);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.35
    glPushMatrix();
    glTranslatef(250,5,250);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.36
    glPushMatrix();
    glTranslatef(200,5,250);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.41
    glPushMatrix();
    glTranslatef(450,10,300);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.42
    glPushMatrix();
    glTranslatef(400,10,300);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.43
    glPushMatrix();
    glTranslatef(350,6,300);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.44
    glPushMatrix();
    glTranslatef(300,5,300);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.45
    glPushMatrix();
    glTranslatef(250,5,300);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();


    //pohon2.51
    glPushMatrix();
    glTranslatef(450,4,350);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.52
    glPushMatrix();
    glTranslatef(400,3,350);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon2.53
    glPushMatrix();
    glTranslatef(350,9,350);
    glScalef(7,7,7);
    pohon2();
    glPopMatrix();

    //pohon
    for (int i = -9; i < -4; i++)
        for (int j = -7; j < -2; j++)
        {
            glPushMatrix();
            glTranslatef(i*50,14,j*50);
            glScalef(7,7,7);
            pohon2();
            glPopMatrix();
        }

    for (int i = -6; i < -2; i++)
        for (int j = 3; j < 8; j++)
        {
            glPushMatrix();
            glTranslatef(i*50,5,j*50);
            glScalef(7,7,7);
            pohon2();
            glPopMatrix();
        }

    for (int i = 7; i < 10; i++)
        for (int j = -7; j < -2; j++)
        {
            glPushMatrix();
            glTranslatef(i*50,2.5,j*50);
            glScalef(7,7,7);
            pohon2();
            glPopMatrix();
        }

    for (int i = 1; i < 10; i++)
        for (int j = -2; j < 0; j++)
        {
            glPushMatrix();
            glTranslatef(i*50,-2.5,j*20);
            glScalef(2,2,2);
            pohon3();
            glPopMatrix();
        }

    //pohon4
    glPushMatrix();
    glTranslatef(120,-2.5,0);
    glScalef(5,5,5);
    pohon4();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(50,-2.5,0);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();


    //pohon3
    glPushMatrix();
    glTranslatef(100,-2.5,0);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    for (int i = 1; i < 3; i++)
        for (int j = -2; j < 6; j++)
        {
            glPushMatrix();
            glTranslatef(i*50,-2.5,j*20);
            glScalef(2,2,2);
            pohon3();
            glPopMatrix();
        }

    //pohon3
    glPushMatrix();
    glTranslatef(90,-2.5,120);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(40,-2.5,120);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(80,-2.5,140);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(30,-2.5,140);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(70,-2.5,160);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(20,-2.5,160);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(60,-2.5,180);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon31
    glPushMatrix();
    glTranslatef(10,-2.5,180);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(50,-2.5,200);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(0,-2.5,200);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(40,-2.5,220);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-10,-2.5,220);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(30,-2.5,240);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-20,-2.5,240);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(20,-2.5,260);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-30,-2.5,260);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(10,-2.5,280);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-40,-2.5,280);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(0,-2.5,300);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-50,-2.5,300);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-10,-2.5,320);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon31
    glPushMatrix();
    glTranslatef(-60,-2.5,320);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-20,-2.5,340);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-70,-2.5,340);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-30,-2.5,360);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

    //pohon3
    glPushMatrix();
    glTranslatef(-80,-2.5,360);
    glScalef(2,2,2);
    pohon3();
    glPopMatrix();

	//Sleep(5);
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

	// OpenGL init
	/*glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);*/
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

	//glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	//glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 1;
}
