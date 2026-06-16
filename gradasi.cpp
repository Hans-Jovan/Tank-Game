#include <graphics.h>
#include <conio.h>
#include <dos.h>
#include <stdlib.h>

int main() {
    int gd = DETECT, gm;
    initgraph(&gd, &gm, "C:\\TURBOC3\\BGI");  // Path BGI di Turbo C

    int x, y;
    int left = 100, top = 100, right = 200, bottom = 200;
    int size;
    void *buffer;

    // Membuat gradasi manual (vertikal)
    for (y = top; y <= bottom; y++) {
        int color = (y - top) * 15 / (bottom - top);  // 0 to 15
        setcolor(color);
        line(left, y, right, y);
    }

    // Mengambil gambar ke buffer
    size = imagesize(left, top, right, bottom);
    buffer = malloc(size);
    getimage(left, top, right, bottom, buffer);

    // Menampilkan hasil gradasi di tempat lain
    putimage(250, 100, buffer, COPY_PUT);

    getch();
    free(buffer);
    closegraph();
    return 0;
}
