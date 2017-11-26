// PixelShaderLight.cpp : Defines the entry point for the application.
#include "stdafx.h"
#include "PixelShaderLight.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "GL/glut.h"		// Dodatkowe bajery OpenGL typu kule, torrusy itp.
#include <Cg/cg.h>			// Biblioteka Cg (obs�uga shader�w pixeli i vertex�w)
#include <Cg/cgGL.h>		// Obs�uga Cg w OpenGL

#include "matrix.h"			// Przekszta�cenia kamery

// Struktura do odczytu textury w formacie BMP z pliku "texture.bmp"
// Jest to po prostu fizyczny nag�owek plik�w BMP systemu windows
typedef struct _BMPTag		
{
 char ident[2];				// BM
 int fsize;
 int dummy;
 int offset;
 int dummy2;
 int bm_x;					// wymiary x i y, textury musz� mie� wymiary b�d�ce pot�gami 2, np.128x128 czy 512x256 itp
 int bm_y;					// inaczej ani opengl ani directx nie wczyta takich textur do pami�ci karty graficznej
 short planes;
 short bpp;					// uproszczona obs�uga, tylko 24bitowe pliki BMP
 int compress;				// skompresowane te� nie s� obs�ugiwane
 int nbytes;
 int no_matter[4];
} BMPTag;


static CGcontext cg_context;					// konteks Cg
static CGprofile v_s_profile, p_s_profile;		// profile shader�w odpowiednio v_s (vertexshader) i p_s (pixelshader)
static CGprogram v_s_program, p_s_program;		// programy w/w profili
// parametry shader�w
// shader wierzcho�k�w (vertex), - macierze przekszta�ce� �wiata w celu obliczenia po�o�enia (przekszta�cenia i rzutowanie)
static CGparameter v_s_view_matrix, v_s_proj_matrix, v_s_timer;
// parametry shadera pixeli (o�wietlenie liczone dla ka�dego pixela), g��wnie parametry o�wietlenia
static CGparameter p_s_global_ambient, p_s_light_color, p_s_light_pos;
static CGparameter p_s_eye_pos, p_s_emission, p_s_ambient, p_s_diffuse, p_s_specular, p_s_shine;

static float light_angle = -0.4;		// k�ty obrot�w �wiat�a, kr��y sobie ono w ob��dny spos�b po sferze o=(0,0,0), r=3
static float light_angle2 = -0.7; 
static float global_ambient[3] = { 0.1, 0.1, 0.1 };  /* Dim */
static float light_color[3] = { 0.95, 0.95, 0.95 };  /* White */

// zmienne globalne programu
float angX, angY, angZ;			// obr�t kamer wok�l osi OX, OY, OZ
float disX, disY, disZ;			// przesuni�cie kamery z punktu (0,0,0) w przestrzeni 3D
float zoom;						// zoom kamery

//Zmienne u�ywane do obliczenia ilo�ci klatek na sekund�
time_t t1,t2;
int fps;

// kolejne bity obrazu tekstury (format RGB), ka�dy kolejny bajt to nasycenie R lub G lub B 0-255
unsigned char* bits;
// ID textury w GPU
GLuint tid;

// Rozmiar textury
long bmx, bmy;
// Czy przypisano textur� (aby zapewni� �e przypisanie odbywa si� tylko raz na pocz�tku)
long tex_bind = 0;

float timer = 0.0;

// Wstawia domy�lne warto�ci do struktury nag��wka BMP
void init_bmp(BMPTag* b)
{
 int i;
 if (!b) return;
 b->ident[0]='B';
 b->ident[1]='M';
 b->fsize=0;
 b->dummy=0;
 b->offset=sizeof(BMPTag);
 b->bm_x=b->bm_y=0x20;
 b->dummy2=40;
 b->planes=0;
 b->bpp=0x18;
 b->planes=1;
 b->compress=0;
 b->nbytes=3*32*32;
 for (i=0;i<4;i++) b->no_matter[i]=0;
}

// Wczytuj� bitmape do zmiennej 'bits' z pliku podanego jako parametr
int load_bmp(char* fn)
{
 FILE* plik;
 int i;
 char b,m;
 BMPTag bm_handle;
 plik = fopen(fn, "rb");		// otwiera plik w trybie binarnym
 if (!plik) return 0;
 init_bmp(&bm_handle);
 i = fscanf(plik,"%c%c",&b,&m);	// sprawdzanie czy to plik BMP
 if (i != 2) return 0;
 if (b != 'B' || m != 'M') return 0;
 fread(&bm_handle.fsize,4,1,plik);	// odczyt nag��wka BMP
 fread(&bm_handle.dummy,4,1,plik);
 fread(&bm_handle.offset,4,1,plik);
 fread(&bm_handle.dummy2,4,1,plik);
 fread(&bm_handle.bm_x,4,1,plik);
 fread(&bm_handle.bm_y,4,1,plik);
 fread(&bm_handle.planes,2,1,plik);
 fread(&bm_handle.bpp,2,1,plik);
 if (bm_handle.bpp != 24) return 0;	// czy 24 bit
 fseek(plik,bm_handle.offset,SEEK_SET);	// przeniesienie si� do pocz�tku pixeli obrazu
 // alokacja pixeli bitmapy
 bits = (unsigned char*)malloc(3*bm_handle.bm_y*bm_handle.bm_x*sizeof(unsigned char));
 bmx = bm_handle.bm_x;	// rozmiar obrazu
 bmy = bm_handle.bm_y;
 fread(bits, 3*bmx*bmy, sizeof(unsigned char), plik);	// odczyt wszystkich pixeli
 printf("Image read (%dx%d).\n", bmx, bmy);
 fclose(plik);	// zamykanie pliku
 return 1;
}

// sprawdza czy nie nast�pi� b��d biblioteki Cg
static void check_error(const char *descr)
{
  CGerror error;
  char errstr[512];
  const char *string = cgGetLastErrorString(&error);

  if (error != CG_NO_ERROR) 
  {
    sprintf(errstr, "ShaderLighting: %s: %s\n", descr, string);
	MessageBox(NULL, errstr, "Cg Error", MB_OK);
    if (error == CG_COMPILER_ERROR) 
	{
      sprintf(errstr, "%s\n", cgGetLastListing(cg_context));
	  MessageBox(NULL, errstr, "Cg Compile Error", MB_OK);
    }
    exit(1);
  }
}

// Zapowiedzi f-cji obs�ugi zdarze� biblioteki GLUT
static void reshape(int width, int height);
static void display(void);
static void keyboard(unsigned char c, int x, int y);
static void idle(void);

// Okienko z klawiszologi�
void Help()
{
	MessageBox(NULL,	"Sterowanie kamer�:\n"
						"Obroty:\n"
						"\ti/k - o� OX\n"
						"\tj/l - o� OY\n"
						"\to/m - o� OZ\n"
						"Przemieszczanie:\n"
						"\tw/s - kierunek prz�d/ty�\n"
						"\ta/d - kierunek lewo/prawo\n"
						"\te/x - kierunek g�ra/d�\n"
						"Zoom: 0.8x-5x:\n"
						"\t1 - przybliz (zwi�ksz zoom)\n"
						"\t2 - oddal (zmniejsz zoom)\n"
						"Inne:\n"
						"\tq/esc - zako�cz\n"
						"\tf - pe�ny ekran\n"
						"\tSPACJA - pocz�tkowe warto�ci"
						,"Informacja", MB_OK);
}

// Wpisanie jakich� domyslnych warto�ci
void Init()
{
	angX = angY = angZ = 0.;
	disX = disY = 0.;
	disZ = 0.;
	zoom = 1.f;
	//Pr�ba odczytu textury
	if (!load_bmp("texture.bmp")) bits = NULL;
}

// tutaj aplikacja zaczyna dzia�anie
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
  char* name;
  int lb;
  lb = 1;							// ilo�� argument�w z linii polece� (w aplikacji wndows nie u�ywane, sztucznie ustawione na 1)
  name = new char[20];				// jedyny parametr nazwa programu: OpenGL camera
  strcpy(name, "PixelShared Lighting");
  glutInitWindowSize(600, 600);
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
  // MULTI_SAMLE musi by� u�yte bo r�zne dane s� trzymane w jednostkach texturowania
  // jednostka pierwsza: tex0 (textura)
  // jednostka druga przechowuje vektor pozycji wierzcho�ka (bo POSITION niedost�pne w pixelshaderach)
  // jednostka trzecia przechowuje normalne
  // Wi�c do PS trafia na wej�cie interpolowane: tex0(textura), tex1(po�o�enie akt. pixela w przestrzeni)
  // oraz tex2( �rednia normalna do tego pixela w przestrzeni) - wszystko co potrzebne do obliczenia �wiat�a
  glutInit(&lb, &name);

  Init();
  glutCreateWindow("ShaderLighting");
  glutDisplayFunc(display);
  glutKeyboardFunc(keyboard);
  glutReshapeFunc(reshape);
  glutIdleFunc(idle);

  glClearColor(0.1, 0.1, 0.1, 0);  /* Kolor t�a: szary */
  glEnable(GL_DEPTH_TEST);         /* Test g��bi, wykluczanie zas�oni�tych pixeli z przetwarzania */

  cg_context = cgCreateContext();	// stworz kontekst Cg
  check_error("creating context");
  cgGLSetDebugMode(CG_FALSE);		// wy��cz debugowanie
  cgSetParameterSettingMode(cg_context, CG_DEFERRED_PARAMETER_SETTING);	// zezwol na zmiany parametr�w w trakcie renderingu

  v_s_profile = cgGLGetLatestProfile(CG_GL_VERTEX);	//znajd� najnowszy profil wierzcho�k�w obs�ugiwany przez aktualn� kart� graficzn�
  cgGLSetOptimalOptions(v_s_profile);				// i ustaw lub sprawd� b��dy
  check_error("selecting vertex profile");

  // zadaj program karcie, program VertexShader.cg(plik), f-cja main
  // program ten b�dzie wykonany zawsze gdy GPU przetwarza wierzcho�ek (glVertex*)
  v_s_program = cgCreateProgramFromFile(cg_context, CG_SOURCE, "VertexShader.cg", v_s_profile, "main", NULL);
  check_error("creating vertex program from file");
  cgGLLoadProgram(v_s_program);
  check_error("loading vertex program");

  // Pobierz uchwyty do parametr�w wewn�trznych programu Cg
  v_s_view_matrix = cgGetNamedParameter(v_s_program, "matrixView");
  check_error("could not get modelView parameter");

  v_s_proj_matrix = cgGetNamedParameter(v_s_program, "matrixProj");
  check_error("could not get modelProj parameter");

  v_s_timer = cgGetNamedParameter(v_s_program, "timer");
  check_error("could not get timer parameter");

  // j.w. - znajd� najnowszy profil pixel-shadera i aktywuj go
  p_s_profile = cgGLGetLatestProfile(CG_GL_FRAGMENT);
  cgGLSetOptimalOptions(p_s_profile);
  check_error("selecting fragment profile");

  // zadaj program karcie, program PixelShader.cg(plik), f-cja main
  // program ten b�dzie wykonany zawsze gdy GPU przetwarza pixel (czyli przy ostatecznym rysowaniu bufora na ekranie)
  // programy z PS wykonuj� si� cz�ciej: bo np. 1024x768 = oko�o 770,000 na klatk�, czyli przy 100FPS mamy 77 milion�w
  // razy na sekund�!
  // program VS wykonuje si� tyle razy ile jest wierzcho�k�w na scenie, czyli na og� duuuu�o mniej, w tym przyk�adzie 
  // jest ich maksymalnie kilka tysi�cy
  // jednak obliczanie o�wiuetlenia dla ka�dego pixela ma sens (bo ka�dy ma dok�adnie takie o�wietlenie jakie jest obliczone
  // je�li by�my to zrobilki na poziomie wierzcho�k�w to tylko wierzcho�ki miua�yby obliczone o�wietlenie a wsz�dzie 
  // indziej by�aby �rednia z wierzcho�k�w danego tr�jk�ta, czyli np. moznaby pomin�� rozb�yski itp.
  p_s_program = cgCreateProgramFromFile(cg_context, CG_SOURCE, "PixelShader.cg", p_s_profile, "main", NULL);

  check_error("creating fragment program from string");
  cgGLLoadProgram(p_s_program);
  check_error("loading fragment program");

  // Zadaj parametry programu fragment�w (pixeli)
  // globalne rozproszone �wiat�o
  p_s_global_ambient = cgGetNamedParameter(p_s_program, "globalAmbient");
  check_error("could not get globalAmbient parameter");

  p_s_light_color = cgGetNamedParameter(p_s_program, "lightColor");
  check_error("could not get lightColor parameter");

  p_s_light_pos = cgGetNamedParameter(p_s_program, "lightPosition");
  check_error("could not get lightPosition parameter");

  p_s_eye_pos = cgGetNamedParameter(p_s_program, "eyePosition");
  check_error("could not get eyePosition parameter");

  // parametry �wiat�a kolejno: emisja, otoczenia, rozproszone, odbicia, wsp�czynnik rozbicia
  p_s_emission = cgGetNamedParameter(p_s_program, "emission");
  check_error("could not get emission parameter");

  p_s_ambient = cgGetNamedParameter(p_s_program, "ambient");
  check_error("could not get ambient parameter");

  p_s_diffuse = cgGetNamedParameter(p_s_program, "diffuse");
  check_error("could not get diffuse parameter");

  p_s_specular = cgGetNamedParameter(p_s_program, "specular");
  check_error("could not get apecular parameter");

  p_s_shine = cgGetNamedParameter(p_s_program, "shininess");
  check_error("could not get shininess parameter");


  // Ustaw sta�e parametry �wiat�a - tylko raz
  cgSetParameter3fv(p_s_global_ambient, global_ambient);
  cgSetParameter3fv(p_s_light_color, light_color);

  glutMainLoop();
  return 0;
}


// Zmiana rozmiaru okna
static void reshape(int w, int h)
{
  glViewport(0, 0, (GLsizei)w, (GLsizei)h);	/* widok od (0,0) do (w,h) */
  glMatrixMode(GL_PROJECTION);				// tryb macierzy rzutowania
  glLoadIdentity();
  glFrustum(-1., 1., -1., 1., zoom, 2000.);	// prespektywa (�ciety graniastos�up) x: -1->1, y: -1->1, z: 1->2000
  glMatrixMode(GL_MODELVIEW);				// tryb widoku sceny
}

// Ustaw "chropowaty" materia�
static void set_material_sharp(void)
{
  const float sharpEmissive[3] = {0.0,  0.0,  0.0},
              sharpAmbient[3]  = {0.33, 0.22, 0.03},
              sharpDiffuse[3]  = {0.78, 0.57, 0.11},
              sharpSpecular[3] = {0.99, 0.91, 0.81},
              sharpShininess = 27.8;

  // Ustawia parametry materia�u w pixel-shaderze
  cgSetParameter3fv(p_s_emission, sharpEmissive);
  check_error("setting emission parameter");
  cgSetParameter3fv(p_s_ambient, sharpAmbient);
  check_error("setting ambient parameter");
  cgSetParameter3fv(p_s_diffuse, sharpDiffuse);
  check_error("setting diffuse parameter");
  cgSetParameter3fv(p_s_specular, sharpSpecular);
  check_error("setting specular parameter");
  cgSetParameter1f(p_s_shine, sharpShininess);
  check_error("setting shininess parameter");
}


// Ustaw materia� pod�o�a
static void set_material_ground(void)
{
  const float groundEmissive[3] = {0.05,  0.05,  0.05},
              groundAmbient[3]  = {0.22, 0.11, 0.05},
              groundDiffuse[3]  = {0.36, 0.89, 0.32},
              groundSpecular[3] = {0.12, 0.27, 0.99},
              groundShininess = 13.1;

  cgSetParameter3fv(p_s_emission, groundEmissive);
  check_error("setting emission parameter");
  cgSetParameter3fv(p_s_ambient, groundAmbient);
  check_error("setting ambient parameter");
  cgSetParameter3fv(p_s_diffuse, groundDiffuse);
  check_error("setting diffuse parameter");
  cgSetParameter3fv(p_s_specular, groundSpecular);
  check_error("setting specular parameter");
  cgSetParameter1f(p_s_shine, groundShininess);
  check_error("setting shininess parameter");
}

// Ustaw materia�: "plastik"
static void set_material_plastic(void)
{
  const float redPlasticEmissive[3] = {0.0,  0.0,  0.0},
              redPlasticAmbient[3]  = {0.0, 0.0, 0.0},
              redPlasticDiffuse[3]  = {0.5, 0.0, 0.0},
              redPlasticSpecular[3] = {0.7, 0.6, 0.6},
              redPlasticShininess = 32.0;

  cgSetParameter3fv(p_s_emission, redPlasticEmissive);
  check_error("setting emission parameter");
  cgSetParameter3fv(p_s_ambient, redPlasticAmbient);
  check_error("setting ambient parameter");
  cgSetParameter3fv(p_s_diffuse, redPlasticDiffuse);
  check_error("setting diffuse parameter");
  cgSetParameter3fv(p_s_specular, redPlasticSpecular);
  check_error("setting specular parameter");
  cgSetParameter1f(p_s_shine, redPlasticShininess);
  check_error("setting shininess parameter");
}

// Ustaw materia� �wiec�cego �wiat�a (widoczne jako ma�a kulka)
static void set_emmission_light(void)
{
  const float zero[3] = {0.0,  0.0,  0.0};

  cgSetParameter3fv(p_s_emission, light_color);
  check_error("setting Ke parameter");
  cgSetParameter3fv(p_s_ambient, zero);
  check_error("setting Ka parameter");
  cgSetParameter3fv(p_s_diffuse, zero);
  check_error("setting Kd parameter");
  cgSetParameter3fv(p_s_specular, zero);
  check_error("setting Ks parameter");
  cgSetParameter1f(p_s_shine, 0);
  check_error("setting shininess parameter");
}

// Oblicza FPS: zlicza wykonane klatki do czasu a� aktualna sekunda si� zmieni
void time_counter()
{
 char tstr[0x100];
 if (t1 == (time_t)0)	// pocz�tkjowa sytuacja
   {
    time(&t1);
    time(&t2);
    return;
   }
 fps++;
 time(&t2);
 if (t2 > t1)			// czy od ostatniej klatki zmieni�a si� sekunda?
   {
	   sprintf(tstr, "ShaderLighting, FPS: %d", 
		   fps/(int)(t2-t1));
    t1 = t2;
    glutSetWindowTitle(tstr);
    fps = 0;
   }

   timer += 0.01;
}

// Je�li odczytano textur� z pliku to ustaw j� w GPU
void BindTex()
{
 if (tex_bind) return;
 if (bits)
	{
		glEnable(GL_TEXTURE_2D);			// w��cz texturowanie 2D
		glGenTextures(1, &tid);				// wygeneruj ID textury
		glBindTexture(GL_TEXTURE_2D, tid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	// spos�b interpolacji texeli
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	// interpolacja liniowa - troch� wolniejsze od GL_NEAREST
		// Wczytaj textur� do pami�ci GPU
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmx, bmy, 0, GL_RGB, GL_UNSIGNED_BYTE, bits);
		glBindTexture(GL_TEXTURE_2D, tid);	// w��cz texturowanie (ustaw aktualn� textur�)
		tex_bind = 1;						// zapisz flag� �e textur� ustawiono
	}
}


// G�owna f-cja wy�wietlaj�ca - wywo�ywana w ka�dej klatce
static void display(void)
{
  /* World-space positions for light and eye. */
  const float eyePosition[4] = { disX, disY, disZ, 1. };		// pozycja oka
  const float lightPosition[4] = 
  { 3.*sin(light_angle), 3.*sin(light_angle2), 3.*cos(light_angle)*cos(light_angle2), 1 };
  //pozycja �wiat�a ( kr�czy po sferze o=(0,0,0) i r=3
  float viewMatrix[16], projMatrix[16];	// macierze odpowiednio widoku i rzutowania pobierane ze stanu maszyny openGL

  glMatrixMode(GL_PROJECTION);				// tryb macierzy rzutowania
  glLoadIdentity();
  glFrustum(-1., 1., -1., 1., zoom, 2000.);	// u�ywaj zoom-u
  glMatrixMode(GL_MODELVIEW);				// tryb widoku sceny
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();											// wyczy�� macierz przekszta�ce�
  gluLookAt(0., 0., 5., 0., 0., 0., 0., 1., 0.);			// rzut kamery:
  glRotatef(angX, 1., 0. , 0.);							// obr�� swiat o aktualny obr�t
  glRotatef(angY, 0., 1. , 0.);
  glRotatef(angZ, 0., 0. , 1.);
  glTranslatef(disX, disY, disZ);						// przesu� na miejsce gdzie jest obserwator
  BindTex();

 
  // jak to si� ustawi na 0 to mo�na zobaczy� ile shader robi w programie ;-)
  static int use_shader = 1;

  if (use_shader)
  {
	cgGLBindProgram(v_s_program);				// przypisz program VS
	check_error("binding vertex program");

	cgGLEnableProfile(v_s_profile);				// uruchom program	VS
	check_error("enabling vertex profile");

	cgGLBindProgram(p_s_program);
	check_error("binding fragment program");	// przypisz program PS

	cgGLEnableProfile(p_s_profile);
	check_error("enabling fragment profile");	// uruchom program PS

	// pozycje oka i �wiat�a
	cgSetParameter3f(p_s_eye_pos, 0,0,10);
	cgSetParameter3f(p_s_light_pos, lightPosition[0],lightPosition[1], lightPosition[2]);
	cgSetParameter1f(v_s_timer, timer);
  }
 
  glPushMatrix();
	// pozycja i orientacja obiektu
    glTranslatef(-2., 0., 0.);
	glScalef(0.7, 0.7, 0.7);
	glRotatef(-50., 1., 0., 0.);
	glRotatef(20., 0., 1., 0.);

	if (use_shader)
	{
		// materia� "chropowaty" (dla PS)
		set_material_sharp();
		// ustawienie macierzy widoku i rzutu dla VS
		glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
		glGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
		cgSetMatrixParameterfr(v_s_view_matrix, viewMatrix);
		cgSetMatrixParameterfr(v_s_proj_matrix, projMatrix);
		// Uaktualniej zmienne programu
		cgUpdateProgramParameters(v_s_program);
		cgUpdateProgramParameters(p_s_program);
	}

	// Fili�anka
	glutSolidTeapot(1.0);
  glPopMatrix();

  
  glPushMatrix();
	glTranslatef(2., 0., 0.);
	glScalef(0.2, 0.2, 0.2);
	glRotatef(-30, .5, .6, .7);
	if (use_shader)
	{
		// materia� "plastik" dla PS
		set_material_plastic();
		// ustawienie macierzy widoku i rzutu dla VS
		glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
		glGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
		cgSetMatrixParameterfr(v_s_view_matrix, viewMatrix);
		cgSetMatrixParameterfr(v_s_proj_matrix, projMatrix);
		cgUpdateProgramParameters(v_s_program);
		cgUpdateProgramParameters(p_s_program);
	}
	// Do�� dok�adny torrus
	glutSolidTorus(3., 6., 32, 32);
  glPopMatrix();
  


  glPushMatrix();
	glTranslatef(lightPosition[0], lightPosition[1], lightPosition[2]);
	if (use_shader)
	{
		// materia� "�wiat�o" dla PS
		set_emmission_light();
		// ustawienie macierzy widoku i rzutu dla VS
		glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
		glGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
		cgSetMatrixParameterfr(v_s_view_matrix, viewMatrix);
		cgSetMatrixParameterfr(v_s_proj_matrix, projMatrix);
		cgUpdateProgramParameters(v_s_program);
		cgUpdateProgramParameters(p_s_program);  
	}
	//Sfera ma�o dok�adna (bo to i tak �wiat�o bia�ego koloru)
	glutSolidSphere(.1, 8, 8);
  glPopMatrix(); 


  if (use_shader)
  {
	// materia� "pod�o�e" dla PS
	set_material_ground();
	// ustawienie macierzy widoku i rzutu dla VS
	glGetFloatv(GL_MODELVIEW_MATRIX, viewMatrix);
	glGetFloatv(GL_PROJECTION_MATRIX, projMatrix);
	cgSetMatrixParameterfr(v_s_view_matrix, viewMatrix);
	cgSetMatrixParameterfr(v_s_proj_matrix, projMatrix);
	cgUpdateProgramParameters(v_s_program);
	cgUpdateProgramParameters(p_s_program);  
  }

  // Pod�o�e -> czworobok texturowany w PS
  glBegin(GL_QUADS);
	glNormal3f(0., 1., 0.);
	glTexCoord2f(0., 0.);
	glVertex3f(-8., -3.5, -8.);
	glTexCoord2f(1., 0.);
	glVertex3f(8., -3.5, -8.);
	glTexCoord2f(1., 1.);
	glVertex3f(8., -3.5, 8.);
	glTexCoord2f(0., 1.);
	glVertex3f(-8., -3.5, 8.);
  glEnd();

  
  if (use_shader)
  {
    // Wy��czanie shader�w
	cgGLDisableProfile(v_s_profile);
	check_error("disabling vertex profile");

	cgGLDisableProfile(p_s_profile);
	check_error("disabling fragment profile");
  }
  

  // Oblicz FPS
  time_counter();
  // Zamie� bufory (podw�jne buforowanie aby zapobiega� miganiu obrazu)
  glutSwapBuffers();
}

// Przesuwaj �wiat�o i odnawiaj klatk� animacji
static void idle(void)
{
  light_angle += 0.0081851; 
  light_angle2 -= 0.0063543; 
  if (light_angle > 2*PI) 
  {
    light_angle -= 2*PI;
  }
  if (light_angle2 < 0.) 
  {
    light_angle2 += 2*PI;
  }
  glutPostRedisplay();
}


// Reakcja na klawiatur�, opis w f-cji Help(0 (Klawiszologia)
void keyboard(unsigned char key, int x, int y)
{
 switch (key)
   {
		case 27:  /* Esc key */
		case 'q':
			cgDestroyProgram(v_s_program);
			cgDestroyContext(cg_context);
			exit(0);
			break;
        case 'i': matrix_rotate(-2., 0., 0.); break;	// Rotacje aktualnej sceny
        case 'k': matrix_rotate(2., 0., 0.);  break;
        case 'j': matrix_rotate(0., -2., 0.); break;
        case 'l': matrix_rotate(0., 2., 0.);  break;
        case 'o': matrix_rotate(0., 0., 2.);  break;
        case 'm': matrix_rotate(0., 0., -2.); break;
        case 'w': matrix_translate(0., 0., .1); break;	// translacje kamery (trzeba obliczy� jaki kierunek to
        case 's': matrix_translate(0., 0., -.1);  break;	// w prz�d, w ty�, w bok, do g�ry itp.
        case 'a': matrix_translate(-.1, 0., 0.); break;	// patrz operacje przekszta�ce� w matrix.cpp i matrix.h
        case 'd': matrix_translate(.1, 0., 0.);  break;
        case 'e': matrix_translate(0., -.1, 0.); break;
        case 'x': matrix_translate(0., .1, 0.);  break;
		case '1': zoom *= 1.03; break;
		case '2': zoom /= 1.03; break;
		case 'h': Help(); break;
		case 'f': glutFullScreen(); break;
		case ' ': angY = angZ = angX = 0.; disX = disY = 0.; disZ = 0.; zoom = 1.; break;	// warto�ci startowe
   }
   if (zoom < 0.8f) zoom = 0.8f;
   if (zoom > 5.0f) zoom = 5.0f;
}

