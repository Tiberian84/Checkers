#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()  // Основной цикл игры: запускает игру, обрабатывает ходы, проверяет конец игры и возвращает результат (0 - ничья, 1 - победа белых, 2 — победа чёрных)
    {
        auto start = chrono::steady_clock::now();   // Запуск таймера для измерения времени игры
        if (is_replay)  // Если режим повтора: перезагружает логику, конфиг и доску
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else  // Иначе: инициализирует доску для новой игры
        {
            board.start_draw();
        }
        is_replay = false;  // Сброс флага повтора

        int turn_num = -1;  // Инициализация номера хода
        bool is_quit = false;   // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns");    // Получение максимального числа ходов из конфига
        while (++turn_num < Max_turns)  // Цикл по ходам, пока не достигнут лимит
        {
            beat_series = 0;    // Сброс серии битья
            logic.find_turns(turn_num % 2); // Поиск ходов для текущего цвета (0 — белые, 1 — чёрные)
            if (logic.turns.empty())
                break;  // Нет ходов — конец игры
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));   // Установка глубины для бота
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))  // Если ход игрока
            {
                auto resp = player_turn(turn_num % 2);  // Вызов хода игрока
                if (resp == Response::QUIT) // Если выход
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)  // Если повтор
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)    // Если откат
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series) // Откат серии битья
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2); // Ход бота
        }
        auto end = chrono::steady_clock::now(); // Остановка таймера
        ofstream fout(project_path + "log.txt", ios_base::app); // Логирование времени игры
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay)  // Рекурсивный вызов для повтора
            return play();
        if (is_quit)    // Выход
            return 0;
        int res = 2;    // Победа чёрных по умолчанию
        if (turn_num == Max_turns)  // Ничья по лимиту ходов
        {
            res = 0;
        }
        else if (turn_num % 2)  // Победа белых
        {
            res = 1;
        }
        board.show_final(res);  // Показ результата
        auto resp = hand.wait();    // Ожидание ввода после результата
        if (resp == Response::REPLAY)   // Повтор игры
        {
            is_replay = true;
            return play();
        }
        return res; // Возврат результата игры
    }

  private:
    void bot_turn(const bool color)   // Выполняет ход бота: вычисляет оптимальные ходы, применяет их с задержкой, логирует время
    {
        auto start = chrono::steady_clock::now();   // Измерение времени хода бота

        auto delay_ms = config("Bot", "BotDelayMS");   // Получение задержки из конфигурации
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms);  // Поток для начальной задержки
        auto turns = logic.find_best_turns(color);  // Получение оптимальных ходов от алгоритма
        th.join();  // Ожидание завершения задержки
        bool is_first = true;  // Флаг для корректной задержки между ходами
        // making moves
        for (auto turn : turns)  // Применение всех ходов из списка
        {
            if (!is_first)  // Задержка для последующих ходов
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;  // Сброс флага после первого хода
            beat_series += (turn.xb != -1);  // Учёт битья в серии
            board.move_piece(turn, beat_series);  // Выполнение хода
        }

        auto end = chrono::steady_clock::now();  // Окончание измерения времени
        ofstream fout(project_path + "log.txt", ios_base::app);  // Логирование
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)  // Ход игрока: ожидает клика, валидирует ход, обрабатывает серию битья
    {
        // return 1 if quit
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)  // Сбор координат возможных начальных клеток
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells);  // Подсветка возможных ходов
        move_pos pos = {-1, -1, -1, -1};  // Позиция выбранного хода
        POS_T x = -1, y = -1;  // Координаты начальной клетки
        // trying to make first move
        while (true)  // Цикл выбора начальной и конечной клетки
        {
            auto resp = hand.get_cell();  // Ожидание клика по доске
            if (get<0>(resp) != Response::CELL)  // Обработка специальных команд (выход, повтор, откат)
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};  // Полученные координаты

            bool is_correct = false;
            for (auto turn : logic.turns)  // Проверка валидности начальной клетки
            {
                if (turn.x == cell.first && turn.y == cell.second)  // Проверка валидности конечной клетки
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1)  // Найден полный ход
                break;
            if (!is_correct)  // Некорректная начальная клетка
            {
                if (x != -1)  // Если была выбрана предыдущая клетка, очищаем её
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            x = cell.first;  // Выбрана начальная клетка
            y = cell.second;
            board.clear_highlight();  // Очистка предыдущей подсветки
            board.set_active(x, y);  // Установка активной клетки
            vector<pair<POS_T, POS_T>> cells2;  // Сбор координат возможных конечных клеток
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);  // Подсветка возможных ходов из активной клетки
        }
        board.clear_highlight();  // Очистка подсветки
        board.clear_active();  // Очистка подсветки
        board.move_piece(pos, pos.xb != -1);  // Выполнение хода
        if (pos.xb == -1)  // Простой ход без битья
            return Response::OK;
        // continue beating while can
        beat_series = 1;  // Начало серии битья
        while (true)  // Цикл продолжения серии битья
        {
            logic.find_turns(pos.x2, pos.y2);  // Поиск продолжения битья
            if (!logic.have_beats)  // Нет больше битья
                break;

            vector<pair<POS_T, POS_T>> cells;  // Сбор координат для подсветки
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);  // Подсветка возможных продолжений
            board.set_active(pos.x2, pos.y2);  // Активная клетка — текущая после битья
            // trying to make move
            while (true)  // Цикл выбора следующего битья
            {
                auto resp = hand.get_cell();  // Ожидание клика
                if (get<0>(resp) != Response::CELL)  // Специальные команды
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};  // Полученные координаты

                bool is_correct = false;
                for (auto turn : logic.turns)  // Валидация хода
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)   // Некорректный выбор
                    continue;

                board.clear_highlight();  // Очистка подсветки
                board.clear_active();  // Сброс активной клетки
                beat_series += 1;  // Увеличение счётчика серии
                board.move_piece(pos, beat_series);  // Выполнение битья
                break;
            }
        }

        return Response::OK;  // Успешное завершение хода игрока
    }

  private:
    Config config;  // Объект конфигурации игры
    Board board;  // Объект доски
    Hand hand;  // Объект для обработки ввода
    Logic logic;  // Объект логики игры
    int beat_series;  // Счётчик текущей серии битья
    bool is_replay = false;  // Флаг режима повтора игры
};
