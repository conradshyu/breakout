// Brick.cpp : Defines the entry point for the application.
//

#include "framework.h"

#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr int kClientWidth = 800;
    constexpr int kClientHeight = 600;
    constexpr int kBrickColumns = 10;
    constexpr int kBrickRows = 5;
    constexpr float kBallRadius = 8.0f;
    constexpr float kPaddleWidth = 120.0f;
    constexpr float kPaddleHeight = 14.0f;
    constexpr float kPaddleY = 548.0f;

    enum class GameState
    {
        Ready,
        Playing,
        Won,
        Lost
    };

    struct Brick
    {
        RECT bounds{};
        COLORREF color{};
        bool alive = true;
    };

    struct Game
    {
        std::vector<Brick> bricks;
        float paddleX = (kClientWidth - kPaddleWidth) / 2.0f;
        float ballX = kClientWidth / 2.0f;
        float ballY = kPaddleY - kBallRadius - 1.0f;
        float ballVelocityX = 0.0f;
        float ballVelocityY = 0.0f;
        int score = 0;
        int lives = 3;
        int remainingBricks = 0;
        bool leftPressed = false;
        bool rightPressed = false;
        GameState state = GameState::Ready;
    };

    Game g_game;
    HWND g_window = nullptr;

    void AppendU16(std::vector<unsigned char>& data, std::uint16_t value)
    {
        data.push_back(static_cast<unsigned char>(value & 0xff));
        data.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    }

    void AppendU32(std::vector<unsigned char>& data, std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            data.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
        }
    }

    void AppendTag(std::vector<unsigned char>& data, const char* tag)
    {
        data.insert(data.end(), tag, tag + 4);
    }

    std::vector<unsigned char> MakeSound(float startFrequency, float endFrequency, int durationMs)
    {
        constexpr std::uint32_t sampleRate = 22050;
        const std::uint32_t sampleCount = sampleRate * static_cast<std::uint32_t>(durationMs) / 1000;
        std::vector<unsigned char> wav;
        wav.reserve(44 + sampleCount * 2);
        AppendTag(wav, "RIFF");
        AppendU32(wav, 36 + sampleCount * 2);
        AppendTag(wav, "WAVE");
        AppendTag(wav, "fmt ");
        AppendU32(wav, 16);
        AppendU16(wav, 1);
        AppendU16(wav, 1);
        AppendU32(wav, sampleRate);
        AppendU32(wav, sampleRate * 2);
        AppendU16(wav, 2);
        AppendU16(wav, 16);
        AppendTag(wav, "data");
        AppendU32(wav, sampleCount * 2);

        for (std::uint32_t i = 0; i < sampleCount; ++i)
        {
            const float progress = static_cast<float>(i) / static_cast<float>(sampleCount);
            const float frequency = startFrequency + (endFrequency - startFrequency) * progress;
            const float envelope = (std::min)(1.0f, (std::min)(progress * 16.0f, (1.0f - progress) * 12.0f));
            const float phase = 6.28318530718f * frequency * static_cast<float>(i) / sampleRate;
            const auto sample = static_cast<std::int16_t>(std::sin(phase) * envelope * 10000.0f);
            AppendU16(wav, static_cast<std::uint16_t>(sample));
        }
        return wav;
    }

    void PlayHitSound()
    {
        static const auto sound = MakeSound(720.0f, 430.0f, 65);
        PlaySoundW(reinterpret_cast<LPCWSTR>(sound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }

    void PlayBrickSound()
    {
        static const auto sound = MakeSound(980.0f, 520.0f, 95);
        PlaySoundW(reinterpret_cast<LPCWSTR>(sound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }

    void PlayLifeSound()
    {
        static const auto sound = MakeSound(390.0f, 130.0f, 240);
        PlaySoundW(reinterpret_cast<LPCWSTR>(sound.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }

    void ResetBall()
    {
        g_game.ballX = g_game.paddleX + kPaddleWidth / 2.0f;
        g_game.ballY = kPaddleY - kBallRadius - 1.0f;
        g_game.ballVelocityX = 0.0f;
        g_game.ballVelocityY = 0.0f;
        if (g_game.state == GameState::Playing)
        {
            g_game.state = GameState::Ready;
        }
    }

    void ResetGame()
    {
        g_game = Game{};
        g_game.bricks.reserve(kBrickColumns * kBrickRows);
        constexpr int left = 32;
        constexpr int top = 82;
        constexpr int brickWidth = 68;
        constexpr int brickHeight = 22;
        constexpr int horizontalGap = 8;
        constexpr int verticalGap = 8;
        constexpr COLORREF rowColors[kBrickRows] = {
            RGB(255, 92, 92),
            RGB(255, 160, 72),
            RGB(255, 214, 74),
            RGB(80, 205, 140),
            RGB(82, 160, 255)
        };

        for (int row = 0; row < kBrickRows; ++row)
        {
            for (int column = 0; column < kBrickColumns; ++column)
            {
                const int x = left + column * (brickWidth + horizontalGap);
                const int y = top + row * (brickHeight + verticalGap);
                g_game.bricks.push_back({
                    {x, y, x + brickWidth, y + brickHeight},
                    rowColors[row],
                    true
                    });
            }
        }
        g_game.remainingBricks = static_cast<int>(g_game.bricks.size());
    }

    void LaunchBall()
    {
        if (g_game.state == GameState::Won || g_game.state == GameState::Lost)
        {
            ResetGame();
        }
        if (g_game.state == GameState::Ready)
        {
            g_game.ballVelocityX = 220.0f;
            g_game.ballVelocityY = -340.0f;
            g_game.state = GameState::Playing;
        }
    }

    bool CircleIntersectsRect(float x, float y, float radius, const RECT& rect)
    {
        const float closestX = std::clamp(x, static_cast<float>(rect.left), static_cast<float>(rect.right));
        const float closestY = std::clamp(y, static_cast<float>(rect.top), static_cast<float>(rect.bottom));
        const float dx = x - closestX;
        const float dy = y - closestY;
        return dx * dx + dy * dy <= radius * radius;
    }

    void UpdateGame(float deltaSeconds)
    {
        constexpr float paddleSpeed = 520.0f;
        if (g_game.leftPressed)
        {
            g_game.paddleX -= paddleSpeed * deltaSeconds;
        }
        if (g_game.rightPressed)
        {
            g_game.paddleX += paddleSpeed * deltaSeconds;
        }
        g_game.paddleX = std::clamp(g_game.paddleX, 0.0f, kClientWidth - kPaddleWidth);

        if (g_game.state != GameState::Playing)
        {
            if (g_game.state == GameState::Ready)
            {
                g_game.ballX = g_game.paddleX + kPaddleWidth / 2.0f;
            }
            return;
        }

        const float previousX = g_game.ballX;
        const float previousY = g_game.ballY;
        g_game.ballX += g_game.ballVelocityX * deltaSeconds;
        g_game.ballY += g_game.ballVelocityY * deltaSeconds;

        if (g_game.ballX - kBallRadius < 0.0f)
        {
            g_game.ballX = kBallRadius;
            g_game.ballVelocityX = std::abs(g_game.ballVelocityX);
            PlayHitSound();
        }
        else if (g_game.ballX + kBallRadius > kClientWidth)
        {
            g_game.ballX = kClientWidth - kBallRadius;
            g_game.ballVelocityX = -std::abs(g_game.ballVelocityX);
            PlayHitSound();
        }
        if (g_game.ballY - kBallRadius < 0.0f)
        {
            g_game.ballY = kBallRadius;
            g_game.ballVelocityY = std::abs(g_game.ballVelocityY);
            PlayHitSound();
        }

        const RECT paddle = {
            static_cast<LONG>(g_game.paddleX),
            static_cast<LONG>(kPaddleY),
            static_cast<LONG>(g_game.paddleX + kPaddleWidth),
            static_cast<LONG>(kPaddleY + kPaddleHeight)
        };
        if (g_game.ballVelocityY > 0.0f &&
            CircleIntersectsRect(g_game.ballX, g_game.ballY, kBallRadius, paddle) &&
            previousY <= kPaddleY + kPaddleHeight)
        {
            const float offset = std::clamp(
                (g_game.ballX - (g_game.paddleX + kPaddleWidth / 2.0f)) / (kPaddleWidth / 2.0f),
                -1.0f,
                1.0f);
            const float speed = 410.0f;
            g_game.ballVelocityX = offset * speed * 0.88f;
            g_game.ballVelocityY = -std::sqrt(speed * speed - g_game.ballVelocityX * g_game.ballVelocityX);
            g_game.ballY = kPaddleY - kBallRadius;
            PlayHitSound();
        }

        for (auto& brick : g_game.bricks)
        {
            if (!brick.alive || !CircleIntersectsRect(g_game.ballX, g_game.ballY, kBallRadius, brick.bounds))
            {
                continue;
            }

            brick.alive = false;
            --g_game.remainingBricks;
            g_game.score += 10;
            const bool crossedVerticalFace =
                previousY + kBallRadius <= brick.bounds.top ||
                previousY - kBallRadius >= brick.bounds.bottom;
            const bool crossedHorizontalFace =
                previousX + kBallRadius <= brick.bounds.left ||
                previousX - kBallRadius >= brick.bounds.right;
            if (crossedVerticalFace &&
                (!crossedHorizontalFace || std::abs(g_game.ballVelocityY) >= std::abs(g_game.ballVelocityX)))
            {
                g_game.ballVelocityY = -g_game.ballVelocityY;
            }
            else
            {
                g_game.ballVelocityX = -g_game.ballVelocityX;
            }
            PlayBrickSound();
            break;
        }

        if (g_game.remainingBricks == 0)
        {
            g_game.state = GameState::Won;
            return;
        }
        if (g_game.ballY - kBallRadius > kClientHeight)
        {
            --g_game.lives;
            PlayLifeSound();
            if (g_game.lives <= 0)
            {
                g_game.state = GameState::Lost;
            }
            else
            {
                ResetBall();
            }
        }
    }

    void DrawUiText(HDC dc, const wchar_t* text, RECT bounds, int size, COLORREF color, UINT format)
    {
        HFONT font = CreateFontW(
            -size, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        SetTextColor(dc, color);
        SetBkMode(dc, TRANSPARENT);
        ::DrawTextW(dc, text, -1, &bounds, format);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }

    void DrawGame(HDC dc)
    {
        RECT background{ 0, 0, kClientWidth, kClientHeight };
        HBRUSH backgroundBrush = CreateSolidBrush(RGB(14, 20, 38));
        FillRect(dc, &background, backgroundBrush);
        DeleteObject(backgroundBrush);

        RECT header{ 22, 16, kClientWidth - 22, 58 };
        wchar_t hud[128]{};
        swprintf_s(hud, L"SCORE  %04d                                      LIVES  %d", g_game.score, g_game.lives);
        DrawUiText(dc, hud, header, 22, RGB(229, 236, 255), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        for (const auto& brick : g_game.bricks)
        {
            if (!brick.alive)
            {
                continue;
            }
            HBRUSH brush = CreateSolidBrush(brick.color);
            FillRect(dc, &brick.bounds, brush);
            DeleteObject(brush);
            RECT highlight = brick.bounds;
            highlight.bottom = highlight.top + 3;
            HBRUSH shine = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(dc, &highlight, shine);
            DeleteObject(shine);
        }

        RECT paddle{
            static_cast<LONG>(g_game.paddleX),
            static_cast<LONG>(kPaddleY),
            static_cast<LONG>(g_game.paddleX + kPaddleWidth),
            static_cast<LONG>(kPaddleY + kPaddleHeight)
        };
        HBRUSH paddleBrush = CreateSolidBrush(RGB(105, 186, 255));
        FillRect(dc, &paddle, paddleBrush);
        DeleteObject(paddleBrush);

        HBRUSH ballBrush = CreateSolidBrush(RGB(255, 255, 255));
        HGDIOBJ oldBrush = SelectObject(dc, ballBrush);
        HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(
            dc,
            static_cast<int>(g_game.ballX - kBallRadius),
            static_cast<int>(g_game.ballY - kBallRadius),
            static_cast<int>(g_game.ballX + kBallRadius + 1),
            static_cast<int>(g_game.ballY + kBallRadius + 1));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(ballBrush);

        if (g_game.state != GameState::Playing)
        {
            RECT overlay{ 80, 265, kClientWidth - 80, 360 };
            const wchar_t* title = L"READY";
            const wchar_t* subtitle = L"Move with  \x2190  /  \x2192     |     Press SPACE to launch";
            if (g_game.state == GameState::Won)
            {
                title = L"YOU WIN!";
                subtitle = L"Press SPACE to play again";
            }
            else if (g_game.state == GameState::Lost)
            {
                title = L"GAME OVER";
                subtitle = L"Press SPACE to try again";
            }
            DrawUiText(dc, title, overlay, 44, RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            overlay.top += 58;
            DrawUiText(dc, subtitle, overlay, 18, RGB(188, 203, 231), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_KEYDOWN:
            if (wParam == VK_LEFT)
            {
                g_game.leftPressed = true;
            }
            else if (wParam == VK_RIGHT)
            {
                g_game.rightPressed = true;
            }
            else if (wParam == VK_SPACE && (lParam & (1LL << 30)) == 0)
            {
                LaunchBall();
            }
            else if (wParam == VK_ESCAPE)
            {
                DestroyWindow(window);
            }
            return 0;
        case WM_KEYUP:
            if (wParam == VK_LEFT)
            {
                g_game.leftPressed = false;
            }
            else if (wParam == VK_RIGHT)
            {
                g_game.rightPressed = false;
            }
            return 0;
        case WM_KILLFOCUS:
            g_game.leftPressed = false;
            g_game.rightPressed = false;
            return 0;
        case WM_TIMER:
            UpdateGame(0.016f);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            HDC memoryDc = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, kClientWidth, kClientHeight);
            HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
            DrawGame(memoryDc);
            BitBlt(dc, 0, 0, kClientWidth, kClientHeight, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memoryDc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(window, 1);
            PlaySoundW(nullptr, nullptr, 0);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    ResetGame();

    const wchar_t className[] = L"BrickBreakerWindow";
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;

    if (!RegisterClassW(&windowClass))
    {
        MessageBoxW(nullptr, L"Could not register the game window.", L"Brick Breaker", MB_ICONERROR);
        return 1;
    }

    RECT windowBounds{ 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&windowBounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_window = CreateWindowExW(
        0,
        className,
        L"Brick Breaker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowBounds.right - windowBounds.left,
        windowBounds.bottom - windowBounds.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_window)
    {
        MessageBoxW(nullptr, L"Could not create the game window.", L"Brick Breaker", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_window, showCommand);
    UpdateWindow(g_window);

    if (SetTimer(g_window, 1, 16, nullptr) == 0)
    {
        MessageBoxW(g_window, L"Could not start the game timer.", L"Brick Breaker", MB_ICONERROR);
        DestroyWindow(g_window);
        return 1;
    }

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
