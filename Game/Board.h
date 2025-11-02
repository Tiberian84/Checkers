#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    Board() = default;  // Конструктор по умолчанию, не инициализирует ничего
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)  // Конструктор с заданием размеров окна (W — ширина, H — высота)
    {
    }

    // draws start board
    int start_draw()  // Инициализирует SDL, создаёт окно и рендерер, загружает текстуры, устанавливает начальную матрицу доски и отрисовывает её
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)  // Инициализация SDL для видео и изображений
        {
            print_exception("SDL_Init can't init SDL2 lib");  // Логирование ошибки, если инициализация не удалась
            return 1;
        }
        if (W == 0 || H == 0)  // Если размеры не заданы, определяем их по экрану
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))  // Получение режима дисплея
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);  // Установка минимального размера
            W -= W / 15;  // Уменьшение на 1/15 для полей
            H = W;
        }
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);  // Создание окна с названием "Checkers" и флагом изменения размера
        if (win == nullptr)  // Проверка на ошибку создания окна
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);  // Создание рендерера с ускорением и VSync
        if (ren == nullptr)  // Проверка на ошибку создания рендерера
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }
        board = IMG_LoadTexture(ren, board_path.c_str());  // Загрузка текстуры доски
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());  // Загрузка текстуры белой шашки
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());  // Загрузка текстуры чёрной шашки
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());  // Загрузка текстуры белой дамки
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());  // Загрузка текстуры чёрной дамки
        back = IMG_LoadTexture(ren, back_path.c_str());  // Загрузка текстуры кнопки "назад"
        replay = IMG_LoadTexture(ren, replay_path.c_str());  // Загрузка текстуры кнопки "повтор"
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)  // Проверка на ошибку загрузки текстур
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }
        SDL_GetRendererOutputSize(ren, &W, &H);  // Получение текущих размеров рендерера
        make_start_mtx();  // Установка начальной конфигурации доски
        rerender();  // Первая отрисовка доски
        return 0;
    }

    void redraw()  // Сбрасывает состояние доски: очищает историю, устанавливает начальную матрицу, убирает подсветку и активные клетки
    {
        game_results = -1;  // Сброс результата игры
        history_mtx.clear();  // Очистка истории матриц
        history_beat_series.clear();  // Очистка истории серий битья
        make_start_mtx();  // Создание начальной матрицы доски
        clear_active();  // Убрать активную клетку
        clear_highlight();  // Убрать подсветку
    }

    void move_piece(move_pos turn, const int beat_series = 0)  // Перемещение шашки с учётом битья: очищает съеденную шашку, вызывает базовое перемещение
    {
        if (turn.xb != -1)  // Если есть съеденная шашка
        {
            mtx[turn.xb][turn.yb] = 0;  // Очистка съеденной клетки
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);  // Вызов базового перемещения
    }

    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)  // Базовое перемещение: проверяет клетки, обновляет матрицу, добавляет в историю
    {
        if (mtx[i2][j2]) // Проверка, свободна ли конечная клетка
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j]) // Проверка, занята ли начальная клетка
        {
            throw runtime_error("begin position is empty, can't move");
        }
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7)) // Преобразование в дамку при достижении края
            mtx[i][j] += 2;
        mtx[i2][j2] = mtx[i][j];    // Перемещение шашки
        drop_piece(i, j);   // Очистка начальной позиции
        add_history(beat_series);   // Добавление состояния в историю
    }

    void drop_piece(const POS_T i, const POS_T j)   // Удаляет шашку с указанной позиции и перерисовывает доску
    {
        mtx[i][j] = 0;
        rerender();
    }

    void turn_into_queen(const POS_T i, const POS_T j)  // Преобразует шашку в дамку с проверкой позиции
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2) // Проверка возможности преобразования
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2; // Увеличение значения для обозначения дамки
        rerender(); // Перерисовка доски
    }
    vector<vector<POS_T>> get_board() const // Возвращает текущую матрицу доски
    {
        return mtx;
    }

    void highlight_cells(vector<pair<POS_T, POS_T>> cells)  // Подсвечивает указанные клетки
    {
        for (auto pos : cells)  // Перебор всех клеток для подсветки
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender(); // Обновление отображения
    }

    void clear_highlight()  // Убирает всю подсветку с доски
    {
        for (POS_T i = 0; i < 8; ++i)   // Очистка матрицы подсветки
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender(); // Перерисовка
    }

    void set_active(const POS_T x, const POS_T y)  // Устанавливает активную клетку: сохраняет координаты для подсветки
    {
        active_x = x;
        active_y = y;
        rerender(); // Обновление отображения
    }

    void clear_active()  // Очищает активную клетку: сбрасывает координаты
    {
        active_x = -1;
        active_y = -1;
        rerender(); // Перерисовка
    }

    bool is_highlighted(const POS_T x, const POS_T y)   // Проверяет, подсвечена ли указанная клетка
    {
        return is_highlighted_[x][y];
    }

    void rollback() // Выполняет откат последнего хода или серии битья
    {
        auto beat_series = max(1, *(history_beat_series.rbegin())); // Определение длины серии битья
        while (beat_series-- && history_mtx.size() > 1) // Откат всей серии
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());  // Восстановление предыдущего состояния
        clear_highlight();  // Очистка подсветки
        clear_active(); // Сброс активной клетки
    }

    void show_final(const int res)  // Отображает результат игры
    {
        game_results = res; // Установка результата
        rerender(); // Перерисовка с отображением результата
    }

    // use if window size changed
    void reset_window_size()    // Перерисовка с отображением результата
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender(); // Перерисовка с новыми размерами
    }

    void quit() // Освобождает все ресурсы SDL
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    ~Board()    // Деструктор: очищает ресурсы при уничтожении объекта
    {
        if (win)
            quit();
    }

private:
    void add_history(const int beat_series = 0) // Добавляет текущее состояние доски в историю
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }
    // function to make start matrix
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)   // Заполнение матрицы начальными позициями
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)  // Установка чёрных шашек
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1)  // Установка белых шашек
                    mtx[i][j] = 1;
            }
        }
        add_history();  // Сохранение начального состояния
    }

    // function that re-draw all the textures
    void rerender()
    {
        // draw board
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // draw pieces
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j]) // Пропуск пустых клеток
                    continue;
                int wpos = W * (j + 1) / 10 + W / 120;  // Расчёт позиции X
                int hpos = H * (i + 1) / 10 + H / 120;  // Расчёт позиции Y
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };    // Определение области для шашки

                SDL_Texture* piece_texture; // Выбор текстуры шашки
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);    // Отрисовка шашки
            }
        }

        // draw hilight
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);  // Установка цвета подсветки (зелёный)
        const double scale = 2.5;   // Масштаб для подсветки
        SDL_RenderSetScale(ren, scale, scale);  // Применение масштаба
        for (POS_T i = 0; i < 8; ++i)   // Отрисовка подсветки
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j]) // Пропуск неподсвеченных клеток
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };    // Определение области клетки
                SDL_RenderDrawRect(ren, &cell); // Отрисовка прямоугольника подсветки
            }
        }

        // draw active
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);  // Установка цвета активной клетки (красный)
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),   // Определение области
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);  // Отрисовка активной клетки
        }
        SDL_RenderSetScale(ren, 1, 1);  // Сброс масштаба

        // draw arrows
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };   // Позиция и размер кнопки "назад"
        SDL_RenderCopy(ren, back, NULL, &rect_left);    // Отрисовка кнопки "назад"
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };  // Позиция и размер кнопки "повтор"
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);    // Отрисовка кнопки "повтор"

        // draw result
        if (game_results != -1) // Если есть результат игры
        {
            string result_path = draw_path; // Путь к изображению по умолчанию (ничья)
            if (game_results == 1)  // Победа белых
                result_path = white_path;
            else if (game_results == 2) // Победа чёрных
                result_path = black_path;
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());    // Загрузка текстуры результата
            if (result_texture == nullptr)  // Проверка успешности загрузки
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };   // Позиция и размер результата
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);  // Отрисовка результата
            SDL_DestroyTexture(result_texture);  // Освобождение памяти
        }

        SDL_RenderPresent(ren);  // Показ отрисовки
        // next rows for mac os
        SDL_Delay(10);  // Задержка для совместимости с Mac
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);  // Обработка событий
    }

    void print_exception(const string& text) {  // Логирует ошибку в файл log.txt с описанием и SDL_GetError()
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;
        fout.close();
    }

  public:
    int W = 0;  // Ширина окна
    int H = 0;  // Высота окна
    // history of boards
    vector<vector<vector<POS_T>>> history_mtx;  // История состояний доски

  private:
    SDL_Window *win = nullptr;  // Указатель на окно SDL
    SDL_Renderer *ren = nullptr;  // Указатель на рендерер SDL
    // textures
    SDL_Texture *board = nullptr;  // Текстура доски
    SDL_Texture *w_piece = nullptr;  // Текстура белой шашки
    SDL_Texture *b_piece = nullptr;  // Текстура чёрной шашки
    SDL_Texture *w_queen = nullptr;  // Текстура белой дамки
    SDL_Texture *b_queen = nullptr;  // Текстура чёрной дамки
    SDL_Texture *back = nullptr;  // Текстура кнопки "назад"
    SDL_Texture *replay = nullptr;  // Текстура кнопки "повтор"
    // texture files names
    const string textures_path = project_path + "Textures/";  // Путь к текстурам
    const string board_path = textures_path + "board.png";  // Путь к текстуре доски
    const string piece_white_path = textures_path + "piece_white.png";  // Путь к белой шашке
    const string piece_black_path = textures_path + "piece_black.png";  // Путь к чёрной шашке
    const string queen_white_path = textures_path + "queen_white.png";  // Путь к белой дамке
    const string queen_black_path = textures_path + "queen_black.png";  // Путь к чёрной дамке
    const string white_path = textures_path + "white_wins.png";  // Путь к изображению победы белых
    const string black_path = textures_path + "black_wins.png";  // Путь к изображению победы чёрных
    const string draw_path = textures_path + "draw.png";  // Путь к изображению ничьей
    const string back_path = textures_path + "back.png";  // Путь к кнопке назад
    const string replay_path = textures_path + "replay.png";  // Путь к кнопке повтор
    // coordinates of chosen cell
    int active_x = -1, active_y = -1;  // Координаты выбранной клетки
    // game result if exist
    int game_results = -1;  // Результат игры (1 — белые, 2 — чёрные, 0 — ничья, -1 — не завершена)
    // matrix of possible moves
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));  // Матрица подсветки клеток
    // matrix of possible moves
    // 1 - white, 2 - black, 3 - white queen, 4 - black queen
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));  // Матрица доски (0 — пусто, 1 — белая шашка, 2 — чёрная шашка, 3 — белая дамка, 4 — чёрная дамка)
    // series of beats for each move
    vector<int> history_beat_series;  // История серий битья
};
