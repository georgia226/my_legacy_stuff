#include <X11/Xlib.h>			/*ANSI C 100% compatible source by Morgoth DBMA*/
#include <stdio.h>			/*X11R6 required*/
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#define START_AX -10		/*30*/
#define START_AY -15		/*-25*/
#define START_AZ 0		/*15*/
#define START_DZ 0.0
#define START_DY 0.0
#define START_DX 0.0
#define half_arc 180.F
#define MAXZ 6.0
#define X_PERSP perspective
#define BLACK 0
#define WHITE RGB(0XFF,0XFF,0XFF)
#define RED   RGB(0XFF,0,0)
#define BLUE  RGB(0,0,0XFF)
#define GREEN RGB(0,0XFF,0)
#define NLINES    12			/* count lines in cube */
#define NFLOATS   72			/* number of float data in cube */
#define NPOINTS   24			/* number of points in cube (start and end) */
/* ******* */
typedef unsigned long ulong;
static FILE* _ran_device;		/* where to grt random values */

int debug(char* fmt, ...)  /* print out DEBUG information if DEBUG defined */
{
#ifdef DEBUG
 va_list ap;
 int err;
 va_start(ap,fmt);
 err = vprintf(fmt,ap);
 va_end(ap);
 return err;
#endif
#ifndef DEBUG
 return 0;
#endif
}


int Debug(char* fmt, ...)  /* print out DEBUG information if DEBUG defined */
{
#ifdef DEBUGALG
 va_list ap;
 int err;
 va_start(ap,fmt);
 err = vprintf(fmt,ap);
 va_end(ap);
 return err;
#endif
#ifndef DEBUGALG
 return 0;
#endif
}


int Randomize()   /* init random engine */
{
 debug("Randomize\n");
 if ((_ran_device=fopen("/dev/urandom","r"))==NULL)
   {
    printf("RANLIB Ooops:\nCannot open device: /dev/urandom");
    return 0;
   }
 else return 1;
}


int Random(int lb)   /* randomize number from 0 to lb-1 */
{
 int ins[4],i;
 ulong result;
 debug("Random\n");
 for (i=0;i<4;i++)
     ins[i] = fgetc(_ran_device);
 result = ins[0] + 0x100*ins[1] + 0x10000*ins[2]+0x1000000*ins[3];
 result /= (0xffffffff/lb);
 return (int)result;
}


void Kill_random()  /* close random seed */
{
 debug("Kill_random\n");
  if (_ran_device) fclose(_ran_device);
}


float board_lines[NFLOATS] =
{
 /* Xp,Yp,Zp, Xk, Yk, Zk */   /* size is 6 X 12 */
 -6,6,-6,-6,-6,-6,
 -6,-6,-6,6,-6,-6,
 6,-6,-6,6,6,-6,
 6,6,-6,-6,6,-6, /* FIRST MAIN WALL */
 -6,6,-6,-6,6,6,
 -6,6,6,6,6,6,
 6,6,6,6,6,-6, /* SECOND WALL */
 6,6,6,6,-6,6,
 6,-6,6,6,-6,-6, /* THIRD WALL */
 -6,-6,-6,-6,-6,6,
 -6,-6,6,-6,6,6, /* FORTH WALL */
 -6,-6,6,6,-6,6, /* FIFTH AND SIXTH WALL */
};
float board_lines_buff[NFLOATS]; /* size 6 X 12 */
static int selected = 0;		/* AKTUALNIE ZAZNACZONE POLE */
static int board[27];			/* board 0-empty, 1-human, 2-CPU */
static int end_cond = 0;		/* CPU win or lost currently */
static Display* dsp;			/*Xserver specifics*/
static Window win;
static GC gc;
static int angle_x = START_AX;		/*rotations*/
static int angle_y = START_AY;
static int angle_z = START_AZ;
static float sines[360];		/*tables of function values*/
static float cosines[360];
static int cx = 600;			/* window sizes */
static int cy = 450;
static int hx = 300;
static int hy = 225;
static float perspective = 0.;		/* perspective */
static float sines[360];		/*tables of function values*/
static float cosines[360];
static float delta_z=0.0;
static float delta_y=0.0;
static float delta_x=0.0;
static int cpu1_turn=1;
static int cpu1_wins=0;
static int cpu2_wins=0;

void stats()
{
 printf("\n*************************\n");
 printf("RED  WINS: %d\n",cpu1_wins);
 printf("BLUE WINS: %d\n",cpu2_wins);
 printf("\n*************************\n");
}

#define BITS16
/*#define BITS24*/
#ifdef BITS24

unsigned long RGB(int r, int g, int b)  /*set color from r,g,b*/
{
 return ((r&0xFF)<<0X10)+((g&0XFF)<<0X8)+b;
}


int ReturnRed(ulong col)
{
 return (int)((0xff0000&col)>>0X10);
}


int ReturnGreen(ulong col)
{
 return (int)((0x00ff00&col)>>0X8);
}


int ReturnBlue(ulong col)
{
 return (int)(0x0000ff&col);
}

#endif
#ifdef BITS16

unsigned long RGB(int r, int g, int b)  /*set color from r,g,b*/
{
 return (((r>>3)&0x1F)<<0X0B)+(((g>>2)&0X3F)<<0X5)+((b>>3)&0x1F);
}


int ReturnRed(ulong col)
{
 return (int)(((0XF800&col)>>0XB)<<0X3);
}


int ReturnGreen(ulong col)
{
 return (int)(((0x7e0&col)>>0X5)<<0X2);
}


int ReturnBlue(ulong col)
{
 return (int)((0X1F&col)<<0X3);
}

#endif

void copy_from_buffers()		 /*set start values before another transformation*/
{
 int i;;
 debug("copy_from_buffers\n");
 for (i=0;i<NFLOATS;i++)  board_lines[i] = board_lines_buff[i];
}


void init() /*create sine/cosine tables and board*/
{
 int i;
 debug("create_f_table\n");
 for (i=0;i<360;i++)
   {
    sines[i] = sin(((float)i*3.1415926F)/half_arc);
    cosines[i] = cos(((float)i*3.1415926F)/half_arc);
   }
 for (i=0;i<27;i++) board[i] = 0;
}


void copy_buffers() /*create dat buffer, once on init*/
{
 int i;
 debug("copy_buffers\n");
 for (i=0;i<NFLOATS;i++)  board_lines_buff[i] = board_lines[i];
}


void check_angles()			/* czy wszystkie dane sa sensowne ? */
{
#ifdef DEBUG
 printf("check_angles\n");
#endif
 if (angle_x<0)     angle_x +=  360;
 if (angle_x>=360)  angle_x -=  360;
 if (angle_y<0)     angle_y +=  360;
 if (angle_y>=360)  angle_y -=  360;
 if (angle_z<0)     angle_z +=  360;
 if (angle_z>=360)  angle_z -=  360;
 if (delta_z<-MAXZ) delta_z  =- MAXZ;
 if (delta_z>MAXZ)  delta_z  =  MAXZ;
 if (delta_x<-MAXZ) delta_x  =- MAXZ;
 if (delta_x>MAXZ)  delta_x  =  MAXZ;
 if (delta_y<-MAXZ) delta_y  =- MAXZ;
 if (delta_y>MAXZ)  delta_y  =  MAXZ;
 if (selected<0)    selected += 27;
 if (selected>=27)  selected -= 27;
}


void world_transforms()		/* przeksztalcenia swiata nie uzywam ZADNYCH macierzy */
{
 int i;
 float y,x;
 debug("world_transforms\n");
 check_angles();
 for (i=0;i<NPOINTS;i++)  board_lines[3*i+2] += delta_z;		/* przesuniecia wzgledem Z */
 for (i=0;i<NPOINTS;i++)  board_lines[3*i  ] += delta_x;		/* przesuniecia wzgledem X */
 for (i=0;i<NPOINTS;i++)  board_lines[3*i+1] += delta_y;		/* przesuniecia wzgledem Y */
 for (i=0;i<NPOINTS;i++)	/* obrot wokow X */
   {
    y             = board_lines[3*i+1]*cosines[angle_x] - board_lines[3*i+2]*sines[angle_x];
    board_lines[3*i+2] = board_lines[3*i+1]*sines[angle_x] + board_lines[3*i+2]*cosines[angle_x];
    board_lines[3*i+1] = y;
   }
 for (i=0;i<NPOINTS;i++)	/* obrot wokol Y */
   {
    x             = board_lines[3*i]*cosines[angle_y] - board_lines[3*i+2]*sines[angle_y];
    board_lines[3*i+2] = board_lines[3*i]*sines[angle_y]   + board_lines[3*i+2]*cosines[angle_y];
    board_lines[3*i]   = x;
   }
 for (i=0;i<NPOINTS;i++)	/* obrot wokol Z */
   {
    x             = board_lines[3*i]*cosines[angle_z] - board_lines[3*i+1]*sines[angle_z];
    board_lines[3*i+1] = board_lines[3*i]*sines[angle_z]   + board_lines[3*i+1]*cosines[angle_z];
    board_lines[3*i] = x;
   }
}


void get_2D_selected_pos(int* dx, int *dy) /* selected to 2D output screen */
{
 int xp,yp,zp;
 *dx = ((cx*59)>>6)-(cx>>3);
 *dy = (cy*5 )>>6;
 xp = (selected%3)-1;
 yp = ((selected/3)%3)-1;
 zp = (selected/9)-1;
 *dx += xp*(cx>>5);
 *dy -= yp*(cy>>5);
 *dx += zp*(cx>>3);
}


void draw_2D_objects()			/* draw 2D objects */
{
 int xpoints[4];
 int ypoints[4];
 int i,j,tmps,x;
 debug("draw_2D_objects\n");
 XSetForeground(dsp,gc,WHITE);
 /* grid */
 xpoints[3] = (cx*31)>>5;
 xpoints[2] = (cx*30)>>5;
 xpoints[1] = (cx*29)>>5;
 xpoints[0] = (cx*28)>>5;
 ypoints[0] = (cy*1 )>>5;
 ypoints[1] = (cy*2 )>>5;
 ypoints[2] = (cy*3 )>>5;
 ypoints[3] = (cy*4 )>>5;
 for (i=0;i<3;i++)
        {
 	 XDrawLine(dsp,win,gc, xpoints[3], ypoints[0], xpoints[3], ypoints[3]);
	 XDrawLine(dsp,win,gc, xpoints[2], ypoints[0], xpoints[2], ypoints[3]);
	 XDrawLine(dsp,win,gc, xpoints[1], ypoints[0], xpoints[1], ypoints[3]);
	 XDrawLine(dsp,win,gc, xpoints[0], ypoints[0], xpoints[0], ypoints[3]);
	 XDrawLine(dsp,win,gc, xpoints[3], ypoints[0], xpoints[0], ypoints[0]);
	 XDrawLine(dsp,win,gc, xpoints[3], ypoints[1], xpoints[0], ypoints[1]);
	 XDrawLine(dsp,win,gc, xpoints[3], ypoints[2], xpoints[0], ypoints[2]);
	 XDrawLine(dsp,win,gc, xpoints[3], ypoints[3], xpoints[0], ypoints[3]);
	 for (j=0;j<4;j++) xpoints[j] -= cx>>3;
	}
 /* selected */
 get_2D_selected_pos(&i, &j);
 XSetForeground(dsp,gc,GREEN);
 XDrawLine(dsp,win,gc, i-(cx>>6), j-(cx>>6),i+(cx>>6),j-(cx>>6));
 XDrawLine(dsp,win,gc, i-(cx>>6), j+(cx>>6),i+(cx>>6),j+(cx>>6));
 XDrawLine(dsp,win,gc, i-(cx>>6), j-(cx>>6),i-(cx>>6),j+(cx>>6));
 XDrawLine(dsp,win,gc, i+(cx>>6), j-(cx>>6),i+(cx>>6),j+(cx>>6));
 /* game state */
 for (x=0;x<27;x++) if (board[x])
   {
    tmps = selected;
    selected = x;
    get_2D_selected_pos(&i, &j);
    XSetForeground(dsp,gc,(board[selected]==1)?RED:BLUE);
    XFillRectangle(dsp,win,gc, i-(cx>>7), j-(cx>>7),cx>>6,cy>>6);
    selected = tmps;
   }
}


void move_x(int arg) /* move selected +/- X */
{
 int tst;
 int tst2;
 debug("move_x:%d\n", arg);
 tst = (selected%3);
 if (arg>0)
   {
    selected++;
    tst2 = (selected%3);
    if (tst2<tst) selected -=3;
   }
 else
   {
    selected--;
    if (selected<0) { selected += 3; return ; }
    tst2 = (selected%3);
    if (tst2>tst) selected +=3;
   }
}


void move_y(int arg) /* move selected +/- Y */
{
 int tst;
 int tst2;
 debug("move_y:%d\n", arg);
 tst = ((selected/3)%3);
 if (arg>0)
   {
    selected+=3;
    tst2 = ((selected/3)%3);
    if (tst2<tst) selected -=9;
   }
 else
   {
    selected-=3;
    if (selected<0) { selected +=9; return; }
    tst2 = ((selected/3)%3);
    if (tst2>tst) selected +=9;
   }
}


void move_z(int arg) /* move selected +/- Z */
{
 int tst;
 debug("move_z:%d\n", arg);
 tst = (selected/9);
 if (arg>0)
   {
    selected+=9;
    if (selected>=27) selected-=27;
   }
 else
   {
    selected-=9;
    if (selected<0) selected+=27;
   }
}


void get_3D_selected_pos(float* dx, float* dy, float* dz)
{
 *dx = (float)((selected%3)-1.)*4.0;
 *dy = (float)(((selected/3)%3)-1.)*4.0;
 *dz = (float)((selected/9)-1.)*4.0;
 debug("get_3D_selected_point(%f,%f,%f)\n",*dx,*dy,*dz);
}


void set_at(int x, int y, int z, int value)
{
 board[x+3*y+9*z] = value;
 debug("set_at(%d,%d,%d=>%d,%d)\n",x,y,z,x+3*y+9*z,value);
}


int get_at(int x, int y, int z)
{
 debug("get_at(%d,%d,%d=>%d):%d\n",x,y,z,x+3*y+9*z,board[x+3*y+9*z]);
 return board[x+3*y+9*z];
}


int heuristic_count_moves_to_win(int player)
{
 int xc,yc,zc,i;
 int vec[3];
 int cmin;
 int min;
 debug("MOVES_TO_WIN FOR PLAYER: %d\n", player);
 min = 4;
 /* GO BY Z PARALLEL LINES */
 /* BY Z */
 for (xc=0;xc<3;xc++)
 for (yc=0;yc<3;yc++)
   {
    for (zc=0;zc<3;zc++) vec[zc] = get_at(xc,yc,zc);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
   }
 /* BY Y */
 for (xc=0;xc<3;xc++)
 for (zc=0;zc<3;zc++)
   {
    for (yc=0;yc<3;yc++) vec[yc] = get_at(xc,yc,zc);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
   }
 /* BY X */
 for (zc=0;zc<3;zc++)
 for (yc=0;yc<3;yc++)
   {
    for (xc=0;xc<3;xc++) vec[xc] = get_at(xc,yc,zc);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
   }
 /* PRZEKATNE 2D */
 /* WGLAB Z */
 for (zc=0;zc<3;zc++)
   {
    vec[0] = get_at(0,2, zc);
    vec[1] = get_at(1,1, zc);
    vec[2] = get_at(2,0, zc);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
    vec[0] = get_at(0,0, zc);
    vec[1] = get_at(1,1, zc);
    vec[2] = get_at(2,2, zc);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
   }
 /* WGLAB Y */
 for (yc=0;yc<3;yc++)
   {
    vec[0] = get_at(0,yc,2);
    vec[1] = get_at(1,yc,1);
    vec[2] = get_at(2,yc,0);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
    vec[0] = get_at(0,yc, 0);
    vec[1] = get_at(1,yc, 1);
    vec[2] = get_at(2,yc, 2);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
   }
 /* WGLAB X */
 for (xc=0;xc<3;xc++)
   {
    vec[0] = get_at(xc,2,0);
    vec[1] = get_at(xc,1,1);
    vec[2] = get_at(xc,0,2);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
    vec[0] = get_at(xc,0,0);
    vec[1] = get_at(xc,1,1);
    vec[2] = get_at(xc,2,2);
    cmin=3;
    for (i=0;i<3;i++)
       {
        if (vec[i] && vec[i]!=player)
	   {
	    cmin=6;
	    break;
	   }
	if (vec[i]==player) cmin--;
       }
    if (cmin<min) min=cmin;
   }
 /* PRZEKATNE 3D */
 vec[0] = get_at(0,0,0);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(2,2,2);
 cmin=3;
 for (i=0;i<3;i++)
    {
     if (vec[i] && vec[i]!=player)
	{
	 cmin=6;
	 break;
	}
     if (vec[i]==player) cmin--;
    }
 if (cmin<min) min=cmin;
 vec[0] = get_at(2,0,0);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(0,2,2);
 cmin=3;
 for (i=0;i<3;i++)
    {
     if (vec[i] && vec[i]!=player)
	{
	 cmin=6;
	 break;
	}
     if (vec[i]==player) cmin--;
    }
 if (cmin<min) min=cmin;
 vec[0] = get_at(0,2,0);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(2,0,2);
 cmin=3;
 for (i=0;i<3;i++)
    {
     if (vec[i] && vec[i]!=player)
	{
	 cmin=6;
	 break;
	}
     if (vec[i]==player) cmin--;
    }
 if (cmin<min) min=cmin;
 vec[0] = get_at(0,0,2);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(2,2,0);
 cmin=3;
 for (i=0;i<3;i++)
    {
     if (vec[i] && vec[i]!=player)
	{
	 cmin=6;
	 break;
	}
     if (vec[i]==player) cmin--;
    }
 if (cmin<min) min=cmin;
 /* KONIEC PRZEKATNYCH 3D */
 /* printf("MIN = %d\n", min);*/
 return min;
}


int heuristic_count_ways_to_win(int player)
{
 int ways;
 int vec[3];
 int xc,yc,zc,i;
 debug("WAYS_TO_WIN FOR PLAYER: %d\n", player);
 ways = 0;
 /* GO BY Z PARALLEL LINES */
 /* BY Z */
 for (xc=0;xc<3;xc++)
 for (yc=0;yc<3;yc++)
   {
    for (zc=0;zc<3;zc++) vec[zc] = get_at(xc,yc,zc);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
   }
 /* BY Y */
 for (xc=0;xc<3;xc++)
 for (zc=0;zc<3;zc++)
   {
    for (yc=0;yc<3;yc++) vec[yc] = get_at(xc,yc,zc);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
   }
 /* BY X */
 for (zc=0;zc<3;zc++)
 for (yc=0;yc<3;yc++)
   {
    for (xc=0;xc<3;xc++) vec[xc] = get_at(xc,yc,zc);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
   }
 /* BY Y */
 /* PRZEKATNE 2D */
 /* WGLAB Z */
 for (zc=0;zc<3;zc++)
   {
    vec[0] = get_at(0,2, zc);
    vec[1] = get_at(1,1, zc);
    vec[2] = get_at(2,0, zc);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
    vec[0] = get_at(0,0, zc);
    vec[1] = get_at(1,1, zc);
    vec[2] = get_at(2,2, zc);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
   }
 /* WGLAB Y */
 for (yc=0;yc<3;yc++)
   {
    vec[0] = get_at(0,yc,2);
    vec[1] = get_at(1,yc,1);
    vec[2] = get_at(2,yc,0);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
    vec[0] = get_at(0,yc, 0);
    vec[1] = get_at(1,yc, 1);
    vec[2] = get_at(2,yc, 2);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
   }
 /* WGLAB X */
 for (xc=0;xc<3;xc++)
   {
    vec[0] = get_at(xc,2,0);
    vec[1] = get_at(xc,1,1);
    vec[2] = get_at(xc,0,2);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
    vec[0] = get_at(xc,0,0);
    vec[1] = get_at(xc,1,1);
    vec[2] = get_at(xc,2,2);
    for (i=0;i<3;i++)
        if (vec[i] && vec[i]!=player)
	   {
	    ways--;
	    break;
	   }
    ways++;
   }
 /* PRZEKATNE 3D */
 vec[0] = get_at(0,0,0);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(2,2,2);
 for (i=0;i<3;i++)
     if (vec[i] && vec[i]!=player)
	{
	 ways--;
	 break;
	}
 ways++;
 vec[0] = get_at(2,0,0);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(0,2,2);
 for (i=0;i<3;i++)
     if (vec[i] && vec[i]!=player)
	{
	 ways--;
	 break;
	}
 ways++;
 vec[0] = get_at(0,2,0);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(2,0,2);
 for (i=0;i<3;i++)
    {
     if (vec[i] && vec[i]!=player)
	{
	 ways--;
	 break;
	}
    }
 ways++;
 vec[0] = get_at(0,0,2);
 vec[1] = get_at(1,1,1);
 vec[2] = get_at(2,2,0);
 for (i=0;i<3;i++)
     if (vec[i] && vec[i]!=player)
	{
	 ways--;
	 break;
	}
 ways++;
 /* KONIEC PRZEKATNYCH 3D */
 /* printf("WAYS = %d\n", ways); */
 return ways;
}


void clear_board()
{
 int i;
 debug("clear_board()\n");
 for (i=0;i<27;i++) board[i] = 0;
}

int get_current_heuristics(int*,int*,int*,int*,int,int);

int try_random_move(int player)
{
 int i,j;
 int* at;
 if (Random(54)) return 0;
 /*printf("Doing random move!\n");*/
 j=0;
 for (i=0;i<27;i++) if (!board[i]) j++;
 at = (int*)malloc(j<<2);
 j=0;
 for (i=0;i<27;i++) if (!board[i])
   {
    at[j] = i;
    j++;
   }
 board[at[Random(j)]] = player;
 free(at);
 return 1;
}


int forseen_best_move(int h1, int h2, int h3, int h4, int cpu, int opp)
{
 int i,j,c;
 int *hx1,*hx2,*hx3,*hx4, *at,*val;
 int *gmove;
 int nmoves,min,hmov,cmov;
 nmoves=0;
 Debug("foreseen_best_move()\n");
 for (i=0;i<27;i++) if (!board[i]) nmoves++;
 hx1 = (int*)malloc(nmoves<<2);
 hx2 = (int*)malloc(nmoves<<2);
 hx3 = (int*)malloc(nmoves<<2);
 hx4 = (int*)malloc(nmoves<<2);
 at  = (int*)malloc(nmoves<<2);
 val = (int*)malloc(nmoves<<2);
 j=0;
 for (i=0;i<27;i++)
   if (!board[i]) 		/* ten ruch mozliwy */
     {
      board[i] = cpu;		/* zrob go */
      get_current_heuristics(&hx1[j], &hx2[j], &hx3[j], &hx4[j], cpu, opp);
      				/* wez ststystyki po ruchu */
      at[j] = i;		/* gdzie */
      val[j] = 1;
      board[i] = 0;
      j++;			/* ilosc dozwolonych ruchow */
     }
 c=try_random_move(cpu);
 if (c)
   {
    get_current_heuristics(&hx1[0], &hx2[0], &hx3[0], &hx4[0], cpu, opp);
    if (hx1[0]<=0)
      {
       /*printf("HEURISTICS BEFORE: h1=%d,h2=%d,h3=%d,h4=%d\n",h1,h2,h3,h4);
       printf("WIN (RANDOM) HEURISTICS: hx1=%d,hx2=%d,hx3=%d,hx4=%d,at=%d,val=%d,i=%d\n",hx1[i],hx2[i],hx3[i],hx4[i],at[i],val[i],i);
       printf("ONBOARD:%d>> (%d,%d,%d)\n",i,i%3,(i/3)%3,i/9);
       printf("MSI COMPUTED RESULT\n");*/
       free(at);
       free(val);
       free(hx1);
       free(hx2);
       free(hx3);
       free(hx4);
       /* VICTORY HERE! */
       return 1;
      }
    return 0;
   }
 c=0;
 for (i=0;i<27;i++) if (board[i]==1) c++;
 if (c==1 && board[13]==1);  /* printf("CPU: Uzytkownik zaczal od srodkowego pola, bedzie trudno wygrac!\n");*/
 Debug("h1=%d,h2=%d,h3=%d,h4=%d\n",h1,h2,h3,h4);
 for (i=0;i<nmoves;i++)
   Debug("hx1=%d,hx2=%d,hx3=%d,hx4=%d,at=%d,val=%d,i=%d\n",hx1[i],hx2[i],hx3[i],hx4[i],at[i],val[i],i);
 Debug("matrix:\n");
 for (i=0;i<nmoves;i++) if (hx1[i]<=0) /* gdy komputer moze wygrac */
   {
    board[at[i]] = cpu;
    /* printf("HEURISTICS BEFORE: h1=%d,h2=%d,h3=%d,h4=%d\n",h1,h2,h3,h4);
    printf("WIN HEURISTICS: hx1=%d,hx2=%d,hx3=%d,hx4=%d,at=%d,val=%d,i=%d\n",hx1[i],hx2[i],hx3[i],hx4[i],at[i],val[i],i);
    printf("ONBOARD:%d>> (%d,%d,%d)\n",i,i%3,(i/3)%3,i/9);
    printf("MSI COMPUTED RESULT\n");*/
    free(at);
    free(val);
    free(hx1);
    free(hx2);
    free(hx3);
    free(hx4);
    /* VICTORY HERE! */
    return 1;
   }
 for (i=0;i<nmoves;i++) Debug("%d", val[i]); Debug("\n");
 /* CO Z PRZECIWNIKIEM PO RUCHU */
 for (i=0;i<nmoves;i++) if (hx2[i]<=1) val[i] = 0;	/* GDY PO TYM RUCH PRZECIWNIK MOZE WYGRAC */
 c=0;
 for (i=0;i<nmoves;i++) if (val[i]) c++;
 if (c==1) for (i=0;i<nmoves;i++) if (val[i])   /* RATUJ SIE PRZED PRZEGRANA */
   {
    board[at[i]] = cpu;
    free(at);
    free(val);
    free(hx1);
    free(hx2);
    free(hx3);
    free(hx4);
    /* UARATOWANY? */
    return 0;
   }
 for (i=0;i<nmoves;i++) Debug("%d", val[i]); Debug("\n");
 /* ZEBY PO RUCHU (JAK NIE DA SIE WYGRAC I NIE MA BEZPOSREDNIEGO ZAGROZENIA PRZEGRANA)
  * BYC JAK NAJBLIZEJ ZWYCIESTWA */
 min = 6;
 for (i=0;i<nmoves;i++) if (hx1[i]<min) min = hx1[i];
 for (i=0;i<nmoves;i++) if (hx1[i]>min) val[i] = 0;
 c=0;
 for (i=0;i<nmoves;i++) if (val[i]) c++;
 if (c==1) for (i=0;i<nmoves;i++) if (val[i])   /* JAK TYLKO JEDEN MOZLIWY RUCH */
   {
    board[at[i]] = cpu;
    free(at);
    free(val);
    free(hx1);
    free(hx2);
    free(hx3);
    free(hx4);
    /* POWINNA BYC DOBRA SYTUACJA */
    return 0;
   }
 for (i=0;i<nmoves;i++) Debug("%d", val[i]); Debug("\n");
 /* ABY JAK NAJWIECEJ MOZLIWOSCI RUCHU PO AKTUALNYM*/
 cmov=0;
 for (i=0;i<nmoves;i++) if (hx3[i]>cmov) cmov = hx3[i];
 for (i=0;i<nmoves;i++) if (hx3[i]<cmov) val[i] = 0;
 c=0;
 for (i=0;i<nmoves;i++) if (val[i]) c++;
 if (c==1) for (i=0;i<nmoves;i++) if (val[i])   /* JAK TYLKO JEDEN MOZLIWY RUCH */
   {
    board[at[i]] = cpu;
    free(at);
    free(val);
    free(hx1);
    free(hx2);
    free(hx3);
    free(hx4);
    return 0;
   }
 for (i=0;i<nmoves;i++) Debug("%d", val[i]); Debug("\n");
 /* ABY CZLOWIEK MAIL JAK NAJMNIEJ MOZLIWOSCI RUCH PO AKTUALNYM */
 hmov=50;
 for (i=0;i<nmoves;i++) if (hx4[i]<hmov) hmov = hx4[i];
 for (i=0;i<nmoves;i++) if (hx4[i]>hmov) val[i] = 0;
 for (i=0;i<nmoves;i++) Debug("%d", val[i]); Debug("\n");
 c=0;
 for (i=0;i<nmoves;i++) if (val[i]) c++;
 if (c==1) for (i=0;i<nmoves;i++) if (val[i])   /* JAK TYLKO JEDEN MOZLIWY RUCH */
   {
    board[at[i]] = cpu;
    free(at);
    free(val);
    free(hx1);
    free(hx2);
    free(hx3);
    free(hx4);
    return 0;
   }
 Debug("\nThere are %d possible good moves\n", c);
 if (c==0) /* NIE MA DOBREGO RUCHU, PRAKTYCZNIE PRZEGRANA, ZROB COKOLWIEK */
 {
  Debug("SYSTUACJA JEST BARDZO ZLA!!!\n");
  hmov=4;
  for (i=0;i<nmoves;i++) if (hx1[i]<hmov)  hmov = hx1[i];
  for (i=0;i<nmoves;i++) if (hx1[i]==hmov) { board[at[i]] = cpu; break; }
  /* board[at[Random(nmoves)]] = cpu; */
 }
 else
   {
    gmove = (int*)malloc(c<<2);
    j=0;
    for (i=0;i<nmoves;i++)
      {
       if (val[i])
         {
          gmove[j] = at[i];
	  j++;
         }
      }
    board[gmove[Random(c)]] = cpu;
    free(gmove);
   }
 free(at);
 free(val);
 free(hx1);
 free(hx2);
 free(hx3);
 free(hx4);
 return 0;
}


int get_current_heuristics(int* h1, int* h2, int* h3, int* h4, int cpu, int opp)
{
 *h1 = heuristic_count_moves_to_win(cpu);				/*  CPU  */
 *h2 = heuristic_count_moves_to_win(opp);				/* HUMAN */
 *h3 = heuristic_count_ways_to_win(cpu);				/*  CPU  */
 *h4 = heuristic_count_ways_to_win(opp);	        		/* HUMAN */
 debug("H1=%d, H2=%d, H3=%d, H4=%d\n", *h1, *h2, *h3, *h4);
 if (*h2<=0) return -1;
 else return 0;
}


void cpu_msi_move(int player)
{
 int h1,h2,h3,h4,ret;
 debug("CPU_MSI_MOVE()\n");
 end_cond=0;
 if (player) ret = get_current_heuristics(&h1, &h2, &h3, &h4, 2, 1);
 else ret = get_current_heuristics(&h1, &h2, &h3, &h4, 1, 2);
 if (ret==-1)
   {
     printf("HEURISTICS WHEN DEFETED: h1=%d,h2=%d,h3=%d,h4=%d\n",h1,h2,h3,h4);
    printf("PANIC: MSI COMPUTED RESULT, CPU LOST\n");
    if (player) printf("\nCPU1: I HAVE LOST!\n");
    else        printf("\nCPU2: I HAVE LOST!\n");
    end_cond=1;
    return ;
   }
 if (player) ret = forseen_best_move(h1,h2,h3,h4, 2, 1);
 else ret = forseen_best_move(h1,h2,h3,h4, 1, 2);
 if (ret)
   {
    if (player)  { printf("\nCPU1: I HAVE WIN!\n"); cpu1_wins++; }
    else         { printf("\nCPU2: I HAVE WIN!\n"); cpu2_wins++; }
    stats();
    end_cond=1;
   }
}


void DrawScene(int isBlack)	/* Rysuje cala scene */
{
 int i,l1x,l2x,l1y,l2y;
 float dx,dy,dz;
 int nelem,j,tmps;
 int* eltable;
 debug("DrawScene(%d)\n", isBlack);
 if (isBlack) { XClearWindow(dsp,win); return; }
 perspective = (float)cx/4.0;
 draw_2D_objects();
 copy_from_buffers();
 world_transforms();
 XSetForeground(dsp,gc,WHITE);
 for (i=0;i<NLINES;i++)
   {
    l1x = hx + (int)(X_PERSP*board_lines[6*i  ]/(20.0+board_lines[6*i+2]));
    l2x = hx + (int)(X_PERSP*board_lines[6*i+3]/(20.0+board_lines[6*i+5]));
    l1y = hy - (int)(X_PERSP*board_lines[6*i+1]/(20.0+board_lines[6*i+2]));
    l2y = hy - (int)(X_PERSP*board_lines[6*i+4]/(20.0+board_lines[6*i+5]));
    XDrawLine(dsp,win,gc,l1x,l1y,l2x,l2y);
   }
 /* now draw elements */
 nelem=0;
 for (i=0;i<27;i++) if (board[i]) nelem++;
 if (nelem)
  {
   eltable = (int*)malloc(nelem<<2);
   j=0;
   for (i=0;i<27;i++) if (board[i])
     {
      eltable[j] = i;
      j++ ;
     }
   for (j=0;j<nelem;j++)
     {
      tmps = selected;
      selected = eltable[j];
      copy_from_buffers();
      for (i=0;i<NFLOATS;i++) board_lines[i] /= 10;
      get_3D_selected_pos(&dx,&dy,&dz);
      XSetForeground(dsp,gc, (board[selected]==1)?RED:BLUE);
      for (i=0;i<NPOINTS;i++)
         {
          board_lines[3*i  ] += dx;
          board_lines[3*i+1] += dy;
          board_lines[3*i+2] += dz;
         }
     world_transforms();
     for (i=0;i<NLINES;i++)
        {
         l1x = hx + (int)(X_PERSP*board_lines[6*i  ]/(20.0+board_lines[6*i+2]));
         l2x = hx + (int)(X_PERSP*board_lines[6*i+3]/(20.0+board_lines[6*i+5]));
         l1y = hy - (int)(X_PERSP*board_lines[6*i+1]/(20.0+board_lines[6*i+2]));
         l2y = hy - (int)(X_PERSP*board_lines[6*i+4]/(20.0+board_lines[6*i+5]));
         XDrawLine(dsp,win,gc,l1x,l1y,l2x,l2y);
        }
      selected = tmps;
     }
   free(eltable);
  }
 /* now draw selected */
 copy_from_buffers();
 for (i=0;i<NFLOATS;i++) board_lines[i] /= 3;
 get_3D_selected_pos(&dx, &dy, &dz);
 for (i=0;i<NPOINTS;i++)
   {
    board_lines[3*i  ] += dx;
    board_lines[3*i+1] += dy;
    board_lines[3*i+2] += dz;
   }
 world_transforms();
 XSetForeground(dsp,gc,GREEN);
 for (i=0;i<NLINES;i++)
   {
    l1x = hx + (int)(X_PERSP*board_lines[6*i  ]/(20.0+board_lines[6*i+2]));
    l2x = hx + (int)(X_PERSP*board_lines[6*i+3]/(20.0+board_lines[6*i+5]));
    l1y = hy - (int)(X_PERSP*board_lines[6*i+1]/(20.0+board_lines[6*i+2]));
    l2y = hy - (int)(X_PERSP*board_lines[6*i+4]/(20.0+board_lines[6*i+5]));
    XDrawLine(dsp,win,gc,l1x,l1y,l2x,l2y);
   }
}


int main(int lb, char** par)		/* tutaj UNIX przekazuje sterowanie do Xengine */
{
  int s_num,msec;
  int dx,dy;
  int font_h;
  XFontStruct* font_info;
  char* font_name = "*-helvetica-*-12-*";
  if (lb<2) { printf("usage: %s [number_of_msec_between_moves, 0 to max_speed]\n", par[0]); return 0; }
  msec = atoi(par[1]);
  if (msec<0) return 1;
  dsp = XOpenDisplay(NULL);
  if (!dsp) {printf("X-server error %s\n", par[0]); return 1;}
  s_num = DefaultScreen(dsp);
  dx = DisplayWidth(dsp, s_num);
  dy = DisplayHeight(dsp, s_num);
  cx = 800;
  cy = 600;
  hx = 400;
  hy = 300;
  win = XCreateSimpleWindow
  (dsp, RootWindow(dsp, s_num),0, 0, cx, cy, 1,WhitePixel(dsp, s_num),BlackPixel(dsp, s_num));
  XMapWindow(dsp, win);
  XFlush(dsp);
  gc = XCreateGC(dsp, win, 0, NULL);
  if (gc<(GC)0) {printf("GC failed to create!\n"); return 2;}
  XSetForeground(dsp, gc, WhitePixel(dsp, s_num));
  XSetBackground(dsp, gc, BlackPixel(dsp, s_num));
  XSelectInput(dsp, win, ExposureMask | KeyPressMask | StructureNotifyMask);
  font_info = XLoadQueryFont(dsp, font_name);
  if (!font_info){ printf("XLoadQueryFont: failed loading font '%s'\n", font_name);return 4;}
  XSetFont(dsp, gc, font_info->fid);
  font_h = font_info->ascent + font_info->descent;
  init();
  copy_buffers();
  Randomize();
  if (Random(2)) cpu1_turn = 0;
  DrawScene(0);
  while (1)
    {
     DrawScene(1);
     cpu_msi_move(cpu1_turn);
     cpu1_turn = ! cpu1_turn;
     DrawScene(0);
     if (end_cond)
        {
	 clear_board();
	 end_cond=0;
	}
     if (!Random(50)) move_x(1);
     if (!Random(50)) move_y(1);
     if (!Random(50)) move_z(1);
     if (!Random(50)) angle_x+=Random(6);
     if (!Random(50)) angle_y+=Random(6);
     if (!Random(50)) angle_z+=Random(6);
     if (msec) usleep(msec);
   }
 return 0;
}

/* CopyLeft Morgoth DBMA, +48693582014, morgothdbma@o2.pl, heroine@o2.pl */