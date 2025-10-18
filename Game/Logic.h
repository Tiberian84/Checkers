 #pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color);

private:
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const // Выполняет ход на копии матрицы: удаляет съеденную шашку, проверяет превращение в дамку, перемещает шашку
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;  // Удаление съеденной шашки, если ход включает битьё
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;   // Преобразование шашки в дамку при достижении края доски
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];    // Перемещение шашки на новую позицию
        mtx[turn.x][turn.y] = 0;    // Очистка начальной позиции
        return mtx; // Возвращение обновлённой матрицы
    }

    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const   // Вычисляет оценку позиции: считает количество шашек и дамок с учётом потенциала
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;    // Счётчики для белых шашек (w), белых дамок (wq), чёрных шашек (b), чёрных дамок (bq)
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);  // Подсчёт белых шашек
                wq += (mtx[i][j] == 3); // Подсчёт белых дамок
                b += (mtx[i][j] == 2);  // Подсчёт чёрных шашек
                bq += (mtx[i][j] == 4); // Подсчёт чёрных шашек
                if (scoring_mode == "NumberAndPotential")   // Если используется режим с потенциалом
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i); // Добавление бонуса за продвижение белых шашек
                    b += 0.05 * (mtx[i][j] == 2) * (i); // Добавление бонуса за продвижение чёрных шашек
                }
            }
        }
        if (!first_bot_color)   // Инверсия значений для игрока другого цвета
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)    // Победа чёрных (все белые съедены)
            return INF;
        if (b + bq == 0)    // Победа белых (все чёрные съедены)
            return 0;
        int q_coef = 4; // Коэффициент для дамок
        if (scoring_mode == "NumberAndPotential")   // Увеличение веса дамок в режиме с потенциалом
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef);   // Возвращение нормализованной оценки (отношение сил)
    }

    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1);

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1);

public:
    void find_turns(const bool color)   // Инициализирует поиск всех ходов для указанного цвета (true — чёрные, false — белые), вызывает перегруженную версию с матрицей доски
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y)   // Инициализирует поиск ходов для шашки на позиции (x, y), вызывает перегруженную версию с матрицей доски
    {
        find_turns(x, y, board->get_board());
    }

private:
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx) // Ищет все возможные ходы для указанного цвета на заданной матрице, проверяя каждую шашку и приоритизируя битья
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx) // Ищет возможные ходы для конкретной шашки на (x, y), проверяя сначала битья, затем обычные перемещения
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    vector<move_pos> turns;
    bool have_beats;
    int Max_depth;

  private:
    default_random_engine rand_eng;
    string optimization;
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board;
    Config *config;
};
