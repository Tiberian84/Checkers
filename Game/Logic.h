#pragma once
#include <random>
#include <vector>
#include <cassert> // Для использования assert
#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
public:
    Logic(Board* board, Config* config) : board(board), config(config), Max_depth(5) // Значение по умолчанию
    {
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? static_cast<unsigned>(time(0)) : 0);
        optimization = (*config)("Bot", "Optimization");
        // Можно добавить настройку Max_depth из config, если нужно:
        // if (config->contains("Bot", "MaxDepth")) Max_depth = (*config)("Bot", "MaxDepth");
    }

    std::vector<move_pos> find_best_turns(const bool color) {
        next_move.clear();
        next_best_state.clear();

        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        std::vector<move_pos> res;
        int state = 0;
        do {
            if (static_cast<size_t>(state) < next_move.size() && static_cast<size_t>(state) < next_best_state.size()) {
                res.push_back(next_move[state]);
                state = next_best_state[state];
            }
            else {
                break; // Выход при выходе за пределы
            }
        } while (state != -1 && (static_cast<size_t>(state) < next_move.size() && next_move[state].x != -1));
        return res;
    }

private:
    std::vector<std::vector<POS_T>> make_turn(std::vector<std::vector<POS_T>> mtx, const move_pos& turn) const // Выполняет ход на копии матрицы
    {
        assert(mtx.size() == 8 && !mtx.empty() && mtx[0].size() == 8 && "Invalid matrix size in make_turn");
        if (turn.xb != -1) {
            assert(turn.xb < 8 && turn.yb < 8 && "Invalid beat position");
            mtx[turn.xb][turn.yb] = 0;  // Удаление съеденной шашки
        }
        assert(turn.x < 8 && turn.y < 8 && turn.x2 < 8 && turn.y2 < 8 && "Invalid move position");
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;   // Преобразование шашки в дамку
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];    // Перемещение шашки
        mtx[turn.x][turn.y] = 0;    // Очистка начальной позиции
        return mtx; // Возвращение обновлённой матрицы
    }

    double calc_score(const std::vector<std::vector<POS_T>>& mtx, const bool first_bot_color) const   // Вычисляет оценку позиции
    {
        assert(mtx.size() == 8 && !mtx.empty() && mtx[0].size() == 8 && "Invalid matrix size in calc_score");
        double w = 0, wq = 0, b = 0, bq = 0;    // Счётчики для белых шашек, дамок, чёрных шашек и дамок
        for (POS_T i = 0; i < 8; ++i) {
            for (POS_T j = 0; j < 8; ++j) {
                assert(mtx[i][j] >= 0 && mtx[i][j] <= 4 && "Invalid piece value");
                w += (mtx[i][j] == 1);  // Подсчёт белых шашек
                wq += (mtx[i][j] == 3); // Подсчёт белых дамок
                b += (mtx[i][j] == 2);  // Подсчёт чёрных шашек
                bq += (mtx[i][j] == 4); // Подсчёт чёрных дамок
            }
        }
        if (!first_bot_color) {   // Инверсия значений для игрока другого цвета
            std::swap(b, w);
            std::swap(bq, wq);
        }
        if (w + wq == 0)    // Победа чёрных (все белые съедены)
            return INF;
        if (b + bq == 0)    // Победа белых (все чёрные съедены)
            return 0;
        int q_coef = 4; // Коэффициент для дамок
        return (b + bq * q_coef) / (w + wq * q_coef);   // Нормализованная оценка
    }

    double find_first_best_turn(std::vector<std::vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1) {
        assert(mtx.size() == 8 && !mtx.empty() && mtx[0].size() == 8 && "Invalid matrix size in find_first_best_turn");
        next_move.emplace_back(-1, -1, -1, -1);
        next_best_state.push_back(-1);
        if (state != 0) {
            find_turns(x, y, mtx);
        }
        auto now_turns = turns;
        auto now_have_beats = have_beats;
        if (!now_have_beats && state != 0) {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }
        double best_score = -1;
        for (auto& turn : now_turns) {
            size_t new_state = next_move.size();
            double score;
            if (now_have_beats) {
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, new_state, best_score);
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }
            if (score > best_score) {
                best_score = score;
                if (state < next_move.size()) {
                    next_move[state] = turn;
                    next_best_state[state] = (now_have_beats ? static_cast<int>(new_state) : -1);
                }
            }
        }
        return best_score;
    }

    double find_best_turns_rec(std::vector<std::vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1) {
        assert(mtx.size() == 8 && !mtx.empty() && mtx[0].size() == 8 && "Invalid matrix size in find_best_turns_rec");
        if (depth == Max_depth) {
            return calc_score(mtx, (depth % 2 == color));
        }
        if (x != -1) {
            find_turns(x, y, mtx);
        }
        else {
            find_turns(color, mtx);
        }
        auto now_turns = turns;
        auto now_have_beats = have_beats;
        if (!now_have_beats && x != -1) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        if (turns.empty()) {
            return (depth % 2 ? 0 : INF);
        }

        double min_score = INF + 1;
        double max_score = -1;
        for (auto& turn : now_turns) {
            double score;
            if (now_have_beats) {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            min_score = std::min(min_score, score);
            max_score = std::max(max_score, score);
            // Альфа-бета обрезка
            if (depth % 2) {
                alpha = std::max(alpha, max_score);
            }
            else {
                beta = std::min(beta, min_score);
            }
            if (optimization != "O0" && alpha >= beta) {
                return (depth % 2 ? max_score : min_score);
            }
        }
        return (depth % 2 ? max_score : min_score);
    }

public:
    void find_turns(const bool color)   // Инициализирует поиск всех ходов для указанного цвета
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y)   // Инициализирует поиск ходов для шашки на позиции (x, y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    void find_turns(const bool color, const std::vector<std::vector<POS_T>>& mtx) // Ищет все возможные ходы для указанного цвета
    {
        assert(mtx.size() == 8 && !mtx.empty() && mtx[0].size() == 8 && "Invalid matrix size in find_turns");
        std::vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i) {
            for (POS_T j = 0; j < 8; ++j) {
                if (mtx[i][j] && mtx[i][j] % 2 != color) {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before) {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before) {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        if (!turns.empty()) {
            std::shuffle(turns.begin(), turns.end(), rand_eng);
        }
        have_beats = have_beats_before;
    }

    void find_turns(const POS_T x, const POS_T y, const std::vector<std::vector<POS_T>>& mtx) // Ищет возможные ходы для конкретной шашки
    {
        assert(mtx.size() == 8 && !mtx.empty() && mtx[0].size() == 8 && "Invalid matrix size in find_turns");
        assert(x < 8 && y < 8 && "Invalid position");
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type) {
        case 1:
        case 2:
            for (POS_T i = x - 2; i <= x + 2; i += 4) {
                for (POS_T j = y - 2; j <= y + 2; j += 4) {
                    if (i < 0 || i > 7 || j < 0 || j > 7) continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2) continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default: // queens
            for (POS_T i = -1; i <= 1; i += 2) {
                for (POS_T j = -1; j <= 1; j += 2) {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8; i2 += i, j2 += j) {
                        if (mtx[i2][j2]) {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1)) break;
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2) {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty()) {
            have_beats = true;
            return;
        }
        switch (type) {
        case 1:
        case 2:
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1);
            for (POS_T j = y - 1; j <= y + 1; j += 2) {
                if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j]) continue;
                turns.emplace_back(x, y, i, j);
            }
            break;
        }
        default: // queens
            for (POS_T i = -1; i <= 1; i += 2) {
                for (POS_T j = -1; j <= 1; j += 2) {
                    for (POS_T i2 = x + i, j2 = y + j; i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8; i2 += i, j2 += j) {
                        if (mtx[i2][j2]) break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

public:
    std::vector<move_pos> turns;  // Список всех возможных ходов для текущего состояния
    bool have_beats;  // Флаг, указывающий, есть ли доступные ходы с битьём
    int Max_depth;  // Максимальная глубина поиска для алгоритма минимиакса

private:
    std::default_random_engine rand_eng;  // Генератор случайных чисел для перемешивания ходов
    std::string optimization;  // Уровень оптимизации (например, "O0" для отсутствия альфа-бета обрезки)
    std::vector<move_pos> next_move;  // Список лучших ходов для каждого состояния в дереве поиска
    std::vector<int> next_best_state;  // Список индексов следующего лучшего состояния для каждого состояния
    Board* board;  // Указатель на объект доски для доступа к её состоянию
    Config* config;  // Указатель на объект конфигурации для получения параметров бота
};