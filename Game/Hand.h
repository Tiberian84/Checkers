#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// methods for hands
class Hand
{
  public:
    Hand(Board *board) : board(board)   // Конструктор: связывает объект Hand с указателем на доску
    {
    }
    tuple<Response, POS_T, POS_T> get_cell() const  // Функция обработки ввода: возвращает ответ и координаты клика
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;   // Инициализация ответа по умолчанию
        int x = -1, y = -1; // Переменные для хранения координат мыши
        int xc = -1, yc = -1;   // Переменные для преобразованных координат клетки
        while (true)    // Бесконечный цикл обработки событий
        {
            if (SDL_PollEvent(&windowEvent))    // Проверка событий SDL
            {
                switch (windowEvent.type)   // Обработка типа события
                {
                case SDL_QUIT:  // Закрытие окна
                    resp = Response::QUIT;
                    break;
                case SDL_MOUSEBUTTONDOWN:   // Нажатие кнопки мыши
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    xc = int(y / (board->H / 10) - 1);  // Преобразование Y-координаты в номер строки
                    yc = int(x / (board->W / 10) - 1);  // Преобразование X-координаты в номер столбца
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)  // Клик на кнопку "назад"
                    {
                        resp = Response::BACK;
                    }
                    else if (xc == -1 && yc == 8)   // Клик на кнопку "повтор"
                    {
                        resp = Response::REPLAY;
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)    // Клик на игровую клетку
                    {
                        resp = Response::CELL;
                    }
                    else  // Некорректный клик
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;
                case SDL_WINDOWEVENT:   // Изменение размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size(); // Обновление размеров окна
                        break;
                    }
                }
                if (resp != Response::OK) // Выход из цикла при специальном ответе
                    break;
            }
        }
        return {resp, xc, yc};  // Возврат кортежа с ответом и координатами
    }

    Response wait() const   // Функция ожидания действия игрока после конца игры
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;   // Инициализация ответа по умолчанию
        while (true)    // Бесконечный цикл обработки событий
        {
            if (SDL_PollEvent(&windowEvent))    // Проверка событий SDL
            {
                switch (windowEvent.type)   // Обработка типа события
                {
                case SDL_QUIT: // Закрытие окна
                    resp = Response::QUIT;
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:  // Изменение размера окна
                    board->reset_window_size(); // Обновление размеров окна
                    break;
                case SDL_MOUSEBUTTONDOWN: { // Нажатие кнопки мыши
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);  // Преобразование Y-координаты
                    int yc = int(x / (board->W / 10) - 1);  // Преобразование X-координаты
                    if (xc == -1 && yc == 8)    // Клик на кнопку "повтор"
                        resp = Response::REPLAY;
                }
                break;
                }
                if (resp != Response::OK)   // Выход из цикла при специальном ответе
                    break;
            }
        }
        return resp;    // Возврат ответа
    }

  private:
    Board *board;   // Указатель на объект доски для взаимодействия
};
