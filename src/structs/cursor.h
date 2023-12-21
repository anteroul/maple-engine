#ifndef MAPLEENGINE_CURSOR_H
#define MAPLEENGINE_CURSOR_H

struct Cursor {
    int x, y;

    void setCoords(double _x, double _y)
    {
        x = (int)_x;
        y = (int)_y;
    }
};

#endif //MAPLEENGINE_CURSOR_H
