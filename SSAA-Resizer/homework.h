#ifndef HOMEWORK_H
#define HOMEWORK_H

typedef struct {
    char P[2];
    int width, height;
    int maxVal;
    void **matrix;

}image;

typedef struct {
    char R, G, B;
}pixel;

void readInput(const char * fileName, image *img);

void writeData(const char * fileName, image *img);

void resize(image *in, image * out);

#endif /* HOMEWORK_H */