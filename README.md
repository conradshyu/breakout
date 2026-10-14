# Brick Breaker

A small, self-contained brick-breaker game written in C++ with the Win32 API. The game uses GDI for drawing and synthesizes its sound effects in memory through the Windows Multimedia (WinMM) API. It does not require downloaded assets, third-party libraries, or a package manager.

## Requirements

- Windows 10 or Windows 11, 64-bit
- Visual Studio 18 with the **Desktop development with C++** workload
- The MSVC v145 x64 build tools and a Windows 10 SDK (10.0.19041.0 or later)

The C++ workload and Windows SDK are installed through Visual Studio Installer. No additional files or libraries need to be downloaded for this project; `winmm.lib` is part of the Windows SDK.

## Open and build in Visual Studio

1. Extract `brick.zip` to a folder you can write to.
2. Open `BrickBreaker.sln` in Visual Studio.
3. If Visual Studio reports a missing component, open **Tools > Get Tools and Features** and install **Desktop development with C++**, the MSVC v145 x64 build tools, and a Windows 10 SDK.
4. Select **Debug** or **Release** and **x64** in the toolbar.
5. Choose **Build > Build Solution** (or press **Ctrl+Shift+B**).
6. Run with **Debug > Start Without Debugging** (or **Ctrl+F5**).

The executable is written to `bin\x64\Debug\BrickBreaker.exe` or `bin\x64\Release\BrickBreaker.exe`, relative to the solution folder.

## Build from a Developer Command Prompt

Open **Developer Command Prompt for VS 18**, change to the extracted project folder, then run:

```bat
msbuild BrickBreaker.sln /m /p:Configuration=Release /p:Platform=x64
```

To build a debug version, use `/p:Configuration=Debug`. The same x64 platform is used for both configurations.

## Controls and gameplay

- **Left / Right arrows** — move the paddle
- **Space** — launch the ball; after a win or loss, start a new game
- **Esc** — quit

Break all 50 bricks to win. You have three lives. The ball bounces off the walls, bricks, and paddle; where it hits the paddle controls its bounce angle. The score increases by 10 for each brick.

## Project contents

- `BrickBreaker.sln` — Visual Studio solution
- `BrickBreaker.vcxproj` — x64 Debug and Release project configurations
- `BrickBreaker.cpp` — Win32 window, game loop, GDI drawing, collision handling, keyboard input, and generated sound effects

No audio or image files are needed. Sound effect waveforms are generated as PCM samples when the game runs.

## Troubleshooting

- **Missing v145 toolset:** Open Visual Studio Installer, choose **Modify**, and add the current MSVC x64/x86 build tools under **Desktop development with C++**.
- **Missing Windows SDK:** Add a Windows 10 SDK using Visual Studio Installer, then reopen the solution.
- **No sound:** Check that Windows audio output is enabled and that the application is not muted in Volume Mixer. Gameplay remains playable without sound.
- **Visual Studio is unresponsive:** Do not open multiple copies of the solution. Allow the existing IDE process time to recover; building from **Developer Command Prompt for VS 18** with the command above is an alternative that does not require the IDE window.
