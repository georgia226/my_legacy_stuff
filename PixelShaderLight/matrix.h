//Biblioteka operacji na macierzach i wektorach
#ifndef __MATRIX_H__		// czy ju� do��czano plik (do��cza tylko jak tego nie robiono)
#define __MATRIX_H__		// definicja zmiennej kt�ra jak jest zdefiniowana to zapobiega ponownemu dodawaniu tego pliku

#include <stdlib.h>			// u�ywa standardowej biblioteki
#include <math.h>			// u�ywa funkcji matematycznych (sin, cos)
#define PI 3.14159265358979323846

void matrix_translate(float tx, float ty, float tz);	// przesu� si� o wybrany wektor (oblicza jki to kierunek przy aktualnym obrocie)
void matrix_rotate(float rx, float ry, float rz);		// obr�� si� o wybrane k�ty
float* vector(int siz);									// tworzy siz-elementowy wektor
float** matrix(int siz);								// tworzy macierz siz x siz
float** matrix2(int siz1, int siz2);					// tworzy macierz siz1 x siz2
float*** matrix3(int siz1, int siz2, int siz3);			// tworzy 3-wymiarow� macierz (tensor) siz1 x siz2 x siz3
void I_matrix(float** dst, int siz);					// wpisuje macierz jednostkow� na wybranej macierzy
float** matrix_mul(float** m, float** n, int ma, int mb, int na, int nb); // zwraca wynik mnozenia macierzy m x n (sprawdza rozmiary itd.)
void matrix_mul_vector(float* dst, float** m, float* v, int len);	// mno�y wektor przez macierz (przekszta�ca wektor macierz�)
void free_matrix(float** m, int siz);					// zwalnia macierz
void free_matrix3(float*** m, int siz1, int siz2);		// zwalnia macierz 3D (tensor)
void copy_matrix(float** dst, float** src, int siz);	// kopiuje macierz
void copy_matrix3(float*** dst, float*** src, int siz1, int siz2, int siz3);	// kopiuje tensor
int try_swap(float** m, int idx, int dim);				// u�ywane w trakcie odwracania macierzy gdy dany wiersz/kolumna jest nieodpowiedni do przekszta�cenia
float** invert_matrix(float** srcC, int dim);			// odwraca macierz (nie ka�d� si� da - wtedy zwraca null)
void rotatex(float** m, float ang);						// tworzy macierz obrotu o k�t ang wzgl�dem osi OX
void rotatey(float** m, float ang);						// tworzy macierz obrotu o k�t ang wzgl�dem osi OY
void rotatez(float** m, float ang);						// tworzy macierz obrotu o k�t ang wzgl�dem osi OZ
void translatex(float** m, float arg);					// tworzy macierz przesuni�cia o arg wzd�u� osi OX
void translatey(float** m, float arg);					// tworzy macierz przesuni�cia o arg wzd�u� osi OY
void translatez(float** m, float arg);					// tworzy macierz przesuni�cia o arg wzd�u� osi OZ
void translate(float** m, float x, float y, float z);	// tworzy macierz przesuni�cia o (x,y,z)
void scalex(float** m, float arg);						// tworzy macierz skalowania o rag wzgl�dem osi OX
void scaley(float** m, float arg);						// tworzy macierz skalowania o rag wzgl�dem osi OY
void scalez(float** m, float arg);						// tworzy macierz skalowania o rag wzgl�dem osi OZ
void scale(float** m, float x, float y, float z);		// tworzy macierz skalowania o (x,y,z)
void m_ownmatrix(float*** m, float** mat);				// przekszta�ca zadan� macierz m o macierz mat
void m_translate(float*** m, float x, float y, float z);// przekszta�ca zadan� macierz m o translacj� (x,y,z)
void m_scale(float*** m, float x, float y, float z);	// przekszta�ca zadan� macierz m o skal� (x,y,z)
void m_rotatex(float*** m, float ang);					// przekszta�ca zadan� macierz m o obr�t wzgl�dem osi OX o k�t ang
void m_rotatey(float*** m, float ang);					// przekszta�ca zadan� macierz m o obr�t wzgl�dem osi OY o k�t ang
void m_rotatez(float***m, float ang);					// przekszta�ca zadan� macierz m o obr�t wzgl�dem osi OZ o k�t ang

// Koniec nag��wk�w funkcji bibliotecznych
#endif