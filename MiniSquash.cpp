#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <iostream>
#include <chrono>
#include <thread>

const wchar_t BLOCK = L'\u2588';
const wchar_t* RED_COLOR = L"\x1b[0;31m";
const wchar_t* BLUE_COLOR = L"\x1b[0;96m";
const wchar_t* RESET_COLOR = L"\x1b[0;0m";

const int BOUND_LENGTH = 40;
const int BOUND_WIDTH = 25;
const auto TIME_INTERVAL = std::chrono::milliseconds(50);

void moveCursorToStart() { wprintf(L"\033[G"); }
void displayBlock() { wprintf(L"%lc%lc", BLOCK, BLOCK); }
void moveCursorToPosition(int row, int col) { wprintf(L"\033[%d;%dH", row, col * 2); }
void clearConsole() { moveCursorToPosition(0, 0); wprintf(L"\033[J"); }

void hBar(int numChars, int rowPosition, int startColumn)
{
    for (size_t i = 0; i < numChars; i++) {
        moveCursorToPosition(rowPosition, startColumn + (int)i);
        displayBlock();
    }
}

void vBar(int numChars, int startRow, int columnPosition)
{
    for (size_t i = 0; i < numChars; i++) {
        moveCursorToPosition(startRow + (int)i, columnPosition);
        displayBlock();
        wprintf(L"\n");
    }
}

void displayBallAtPos(int row, int col)
{
    moveCursorToPosition(row, col);
    wprintf(L"%s%lc%lc%s", RED_COLOR, BLOCK, BLOCK, RESET_COLOR);
}

bool isPositionEmpty(int row, int col, HANDLE hConsole)
{
    // Define the position to check (row and column)
    COORD cursorPos = { row, col * 2 };
    SetConsoleCursorPosition(hConsole, cursorPos);

    // Read the character at the specified position
    char buffer[2]; // Buffer to store the character
    DWORD charsRead;

    // Read a character from the console screen buffer
    ReadConsoleOutputCharacterA(hConsole, buffer, 1, cursorPos, &charsRead);

    if (buffer[0] == ' ') return 1;
    return 0;
}

struct Ball
{
    float Row = BOUND_WIDTH - 3;
    float Col = BOUND_LENGTH / 2.0;
    float PrevRow = -1;
    float PrevCol = -1;
    float RSpeed = 0;
    float CSpeed = 0;
    const float INIT_RSPEED = -1;
    const float INIT_CSPEED = 1;
};

struct Bat
{
    const int Size = 4;
    const int Row = BOUND_WIDTH - 3;
    float PrevLeftEnd = -1;
    float LeftEnd = (BOUND_LENGTH / 2.0) - Size / 2.0;
    float Speed = 0.0;
    const float MaxSpeedMagnitude = 1;
    const float Acceleration = 0.2;
};

void clearBall(int row, int col) { moveCursorToPosition(row, col); wprintf(L"  "); }

void clearBat(Bat* bat)
{
    moveCursorToPosition(bat->Row, bat->PrevLeftEnd);
    for (int i = 0; i < bat->Size; i++) wprintf(L"  ");
}

enum SignAction { NoSignChange, FlipColSpeedSign, FlipRowSpeedSign, BallEscaped };

bool hasHitBat(Ball* ball, Bat* bat) {
    if ((int)ball->Col <= ((int)bat->LeftEnd + bat->Size) && (int)ball->Col >= (int)bat->LeftEnd) return true;
    return false;
}

SignAction getAction(Ball* ball, Bat* bat) {
    if (ball->Row <= 2.0) return FlipRowSpeedSign;
    else if (ball->Row > BOUND_WIDTH - 3) return hasHitBat(ball, bat) ? FlipRowSpeedSign : (ball->Row > BOUND_WIDTH - 1) ? BallEscaped : NoSignChange;
    else if (ball->Col <= 2.0 || ball->Col >= BOUND_LENGTH - 2) return FlipColSpeedSign;
    return NoSignChange;
}

void moveBall(Ball* ball)
{
    displayBallAtPos((int)(ball->Row), (int)(ball->Col));
    ball->PrevRow = ball->Row;
    ball->PrevCol = ball->Col;
    ball->Row += ball->RSpeed;
    ball->Col += ball->CSpeed;
}

void moveBat(Bat* bat)
{
    hBar(bat->Size, bat->Row, bat->LeftEnd);
    bat->PrevLeftEnd = bat->LeftEnd;
    bat->LeftEnd += bat->Speed;
}

void renderConsole(Ball* ball, Bat* bat)
{
    if (ball->PrevRow != -1) clearBall((int)ball->PrevRow, (int)ball->PrevCol);
    if (bat->PrevLeftEnd != -1) clearBat(bat);
    moveBall(ball);
    moveBat(bat);
}

void displayScore(int score)
{
    moveCursorToPosition(BOUND_WIDTH + 4, 5);
    wprintf(L"Score: %s%d%s", BLUE_COLOR, score, RESET_COLOR);
}

void listenForArrowKeys(Bat& bat, Ball& ball) {
    while (true) {
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
            if (bat.Speed > -bat.MaxSpeedMagnitude) {
                bat.Speed -= bat.Acceleration;
            }
            std::this_thread::sleep_for(TIME_INTERVAL);
        }

        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
            if (bat.Speed < bat.MaxSpeedMagnitude) {
                bat.Speed += bat.Acceleration;
            }
            std::this_thread::sleep_for(TIME_INTERVAL);
        }

        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            ball.RSpeed = ball.INIT_RSPEED;
            ball.CSpeed = ball.INIT_CSPEED;
            std::this_thread::sleep_for(TIME_INTERVAL);
        }

        // Sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


int main() {
    _setmode(_fileno(stdout), _O_U8TEXT);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    int count = 0;
    int score = 0;

    Ball ball;
    Bat bat;

    // Creating boundaries
    hBar(BOUND_LENGTH, 0, 0);
    vBar(BOUND_WIDTH, 0, 0);
    vBar(BOUND_WIDTH, 0, BOUND_LENGTH);

    std::thread listener(listenForArrowKeys, std::ref(bat), std::ref(ball));

    while (1)
    {
        count++;
        std::this_thread::sleep_for(TIME_INTERVAL);
        renderConsole(&ball, &bat);
        SignAction actionToExecute = getAction(&ball, &bat);

        if (actionToExecute == BallEscaped || count > 1000) break;
        else if (actionToExecute == FlipRowSpeedSign) {
            ball.RSpeed *= -1;
            score += 20;
        }
        else if (actionToExecute == FlipColSpeedSign) {
            ball.CSpeed *= -1;
            score += 10;
        }
        displayScore(score);
    }

    listener.detach();
    return 0;
}