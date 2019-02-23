#include "homework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int num_threads;
int resize_factor;
image *threadIN;
image *threadOUT;
int gaussMatrix[3][3] = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
};

pixel **initPixelMatrix(int width, int height) {
    pixel **matr;
    matr = (pixel**) malloc(sizeof(pixel*) * height);
    if(matr == NULL){
        return NULL;
    }

    for(int i = 0; i < height; i++) {
        matr[i] = (pixel*) malloc(sizeof(pixel) * width);
        if(matr[i] == NULL){
            return NULL;
        }
    }
    return matr;
}

char **initGrayscaleMatrix(int width, int height) {
    char **matr;
    matr = (char**) malloc(sizeof(char*) * height);
    if(matr == NULL){
        return NULL;
    }

    for(int i = 0; i < height; i++) {
        matr[i] = (char*) malloc(sizeof(char) * width);
        if(matr[i] == NULL){
            return NULL;
        }
    }
    return matr;
}

void freeMatrix(void **matr, int height) {
    for(int i = 0; i < height; i++){
        free(matr[i]);
    }
    free(matr);
}

void readInput(const char * fileName, image *img) {
    FILE *fp;
    fp = fopen(fileName, "r");
    int i, j;

    if(fp == NULL) {
        return;
    }

    fscanf(fp, "%c%c\n", &img->P[0], &img->P[1]);
    fscanf(fp, "%d %d\n", &img->width, &img->height);
    fscanf(fp, "%d\n", &img->maxVal);

    if(img->P[1] == '6'){
        //imagine color
        img->matrix = (void**)initPixelMatrix(img->width, img->height);
        pixel** help = (pixel**) img->matrix;
        for(i = 0; i < img->height; i++){
            for(j = 0; j < img->width; j++) {
                fread(&help[i][j], sizeof(pixel), 1, fp);
            }
        }

    }
    else if(img->P[1] == '5'){
        //imagine alb-negru
        img->matrix = (void**)initGrayscaleMatrix(img->width, img->height);
        char** help = (char**) img->matrix;
        for(i = 0; i < img->height; i++){
            for(j = 0; j < img->width; j++) {
                fread(&help[i][j], sizeof(char), 1, fp);
            }
        }
    }
    else {
        fclose(fp);
        return;
    }

    fclose(fp);
    return;
}

void writeData(const char * fileName, image *img) {
    FILE *fp;
    fp = fopen(fileName, "w");
    int i, j;
    if(fp == NULL) {
        return;
    }
    fprintf(fp, "%c%c\n", img->P[0], img->P[1]);
    fprintf(fp, "%d %d\n", img->width, img->height);
    fprintf(fp, "%d\n", img->maxVal);
    if(img->P[1] == '6'){
        //imagine color
        pixel** help = (pixel**) img->matrix;
        for(i = 0; i < img->height; i++){
            for(j = 0; j < img->width; j++) {
                fwrite(&help[i][j], sizeof(pixel), 1, fp);
            }
        }
    }
    else if(img->P[1] == '5'){
        //imagine alb-negru
        char** help = (char**) img->matrix;
        for(i = 0; i < img->height; i++){
            for(j = 0; j < img->width; j++) {
                fwrite(&help[i][j], sizeof(char), 1, fp);
            }
        }
    }
    else {
        fclose(fp);
        return;
    }
    fclose(fp);
    freeMatrix(img->matrix, img->height);

}

void* threadColorEven(void *var){
    int thread_id = *(int*) var;

    int i, j, m, n;
    int outHeight = threadOUT->height;
    int outWidth = threadOUT->width;
    int sumR, sumG, sumB;
    pixel **helpIN = (pixel**) threadIN->matrix;
    pixel **helpOUT = (pixel**) threadOUT->matrix;

    int start = thread_id * outHeight / num_threads;
    int end = (thread_id + 1) * outHeight / num_threads;

    for (i = start; i < end; i++) {
        for (j = 0; j < outWidth; j++) {
            sumR = 0;
            sumG = 0;
            sumB = 0;
            for (m = i * resize_factor; (m < ((i + 1) * resize_factor)) && (m < threadIN->height); m++) {
                for (n = j * resize_factor; (n < ((j + 1) * resize_factor)) && (n < threadIN->width); n++) {
                    sumR += (unsigned char) helpIN[m][n].R;
                    sumG += (unsigned char) helpIN[m][n].G;
                    sumB += (unsigned char) helpIN[m][n].B;

                }
            }
            helpOUT[i][j].R = (unsigned char) (sumR / (resize_factor * resize_factor));
            helpOUT[i][j].G = (unsigned char) (sumG / (resize_factor * resize_factor));
            helpOUT[i][j].B = (unsigned char) (sumB / (resize_factor * resize_factor));
        }
    }
    return NULL;
}
void* threadColorOdd(void *var){
    int thread_id = *(int*) var;

    int i, j, m, n, lstI, lstJ;
    int outHeight = threadOUT->height;
    int outWidth = threadOUT->width;
    int sumR, sumG, sumB;
    pixel **helpIN = (pixel**) threadIN->matrix;
    pixel **helpOUT = (pixel**) threadOUT->matrix;

    int start = thread_id * outHeight / num_threads;
    int end = (thread_id + 1) * outHeight / num_threads;

    for (i = start; i < end; i++) {
        for (j = 0; j < outWidth; j++) {
            sumR = 0;
            sumG = 0;
            sumB = 0;
            lstI = i * resize_factor; //linii
            lstJ = j * resize_factor; //coloane

            for(m = lstI; (m < 3 + lstI) && (m < threadIN->height); m++){
                for(n = lstJ; (n < 3 + lstJ) && (n < threadIN->width); n++){
                    sumR+= gaussMatrix[m - lstI][n - lstJ] * (unsigned char) helpIN[m][n].R;
                    sumG+= gaussMatrix[m - lstI][n - lstJ] * (unsigned char) helpIN[m][n].G;
                    sumB+= gaussMatrix[m - lstI][n - lstJ] * (unsigned char) helpIN[m][n].B;
                }
            }
            helpOUT[i][j].R = (unsigned char) (sumR / 16);
            helpOUT[i][j].G = (unsigned char) (sumG / 16);
            helpOUT[i][j].B = (unsigned char) (sumB / 16);
        }
    }
    return NULL;
}
void* threadBWEven(void *var) {
    int thread_id = *(int*) var;

    int i, j, m, n, sum;
    int outHeight = threadOUT->height;
    int outWidth = threadOUT->width;
    char** helpOUT = (char**) threadOUT->matrix;
    char** helpIN = (char**) threadIN->matrix;

    int start = thread_id * outHeight / num_threads;
    int end = (thread_id + 1) * outHeight / num_threads;

    for (i = start; i < end; i++) {
        for (j = 0; j < outWidth; j++) {
            sum = 0;
            for (m = i * resize_factor; (m < ((i + 1) * resize_factor)) && (m < threadIN->height); m++) {
                for (n = j * resize_factor; (n < ((j + 1) * resize_factor)) && (n < threadIN->width); n++) {
                    sum += (unsigned char) helpIN[m][n];
                }
            }
            helpOUT[i][j] = (unsigned char) (sum / (resize_factor * resize_factor));
        }
    }
    return NULL;
}
void* threadBWOdd(void *var) {
    int thread_id = *(int *) var;

    int i, j, m, n, lstI, lstJ, sum;
    int outHeight = threadOUT->height;
    int outWidth = threadOUT->width;
    char** helpOUT = (char**) threadOUT->matrix;
    char** helpIN = (char**) threadIN->matrix;

    int start = thread_id * outHeight / num_threads;
    int end = (thread_id + 1) * outHeight / num_threads;
    for (i = start; i < end; i++) {
        for (j = 0; j < outWidth; j++) {
            sum = 0;
            lstI = i * resize_factor; //linii
            lstJ = j * resize_factor; //coloane

            for(m = lstI; (m < 3 + lstI) && (m < threadIN->height); m++){
                for(n = lstJ; (n < 3 + lstJ) && (n < threadIN->width); n++){
                    sum+= gaussMatrix[m - lstI][n - lstJ] * (unsigned char) helpIN[m][n];
                }
            }
            helpOUT[i][j] = (unsigned char) (sum / 16);
        }
    }
    return NULL;
}


void resize(image *in, image * out) {

    int i;
    int outHeight = in->height / resize_factor;
    int outWidth = in->width / resize_factor;
    out->P[0] = in->P[0];
    out->P[1] = in->P[1];
    out->width = outWidth;
    out->height = outHeight;
    out->maxVal = in->maxVal;

    pthread_t tid[num_threads];
    int thread_id[num_threads];
    for(i = 0;i < num_threads; i++) {
        thread_id[i] = i;
    }

    if(in->P[1] == '6') {
        //imagine color
        out->matrix = (void**) initPixelMatrix(outWidth, outHeight);
        threadIN = in;
        threadOUT = out;
        if(resize_factor % 2 == 0) {
            for(i = 0; i < num_threads; i++) {
                pthread_create(&(tid[i]), NULL, threadColorEven, &(thread_id[i]));
            }
            for(i = 0; i < num_threads; i++) {
                pthread_join(tid[i], NULL);
            }
        }
        else if(resize_factor == 3){
            for(i = 0; i < num_threads; i++) {
                pthread_create(&(tid[i]), NULL, threadColorOdd, &(thread_id[i]));
            }
            for(i = 0; i < num_threads; i++) {
                pthread_join(tid[i], NULL);
            }
        }
        else {
            return;
        }
    }

    else if(in->P[1] == '5'){
        //imagine alb-negru
        out->matrix = (void**) initGrayscaleMatrix(outWidth, outHeight);
        threadIN = in;
        threadOUT = out;

        if(resize_factor % 2 == 0) {
            for(i = 0; i < num_threads; i++) {
                pthread_create(&(tid[i]), NULL, threadBWEven, &(thread_id[i]));
            }
            for(i = 0; i < num_threads; i++) {
                pthread_join(tid[i], NULL);
            }
        }
        else if(resize_factor == 3){
            for(i = 0; i < num_threads; i++) {
                pthread_create(&(tid[i]), NULL, threadBWOdd, &(thread_id[i]));
            }
            for(i = 0; i < num_threads; i++) {
                pthread_join(tid[i], NULL);
            }
        }
        else {
            return;
        }
    }
    else {
        return;
    }
    freeMatrix(in->matrix, in->height);

}