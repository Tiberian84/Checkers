#pragma once
#include <stdlib.h>

typedef int8_t POS_T;

struct move_pos
{
    POS_T x, y;             // Координаты начальной позиции (откуда начинается ход)
    POS_T x2, y2;           // Координаты конечной позиции (куда перемещается шашка)
    POS_T xb = -1, yb = -1; // Координаты съеденной шашки (по умолчанию -1, если нет битья)

    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2) // Конструктор для создания хода без битья
    {
    }
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)  // Конструктор для создания хода с битьём
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    bool operator==(const move_pos &other) const    // Перегрузка оператора == для сравнения двух ходов по координатам
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }
    bool operator!=(const move_pos &other) const    // Перегрузка оператора != как отрицание оператора ==
    {
        return !(*this == other);
    }
};
