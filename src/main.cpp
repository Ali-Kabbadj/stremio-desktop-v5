#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#include <windows.h>
#include <VersionHelpers.h>
#include <gdiplus.h>
#include <shellscalingapi.h>
#include <sstream>
#include <stdio.h>
#include <shellapi.h> 

#include "core/globals.h"
#include "mpv/player.h"
#include "node/server.h"
#include "tray/tray.h"
#include "ui/mainwindow.h"
#include "ui/splash.h"
#include "updater/updater.h"
#include "utils/config.h"
#include "utils/crashlog.h"
#include "utils/discord.h"
#include "utils/helpers.h"
#include "webview/webview.h"
#include "logger/logger.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  SetUnhandledExceptionFilter([](EXCEPTION_POINTERS *info) -> LONG {
    std::wstringstream ws;
    ws << L"Unhandled exception! Code=0x" << std::hex
       << info->ExceptionRecord->ExceptionCode;
    AppendToCrashLog(ws.str());
    Cleanup();
    return EXCEPTION_EXECUTE_HANDLER;
  });
  Logger::Init(GetExeDirectory() + L"\\portable_config");
  InitializeDiscord();
  int argc;
  LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argvW) {
      return -1;
  }
  
  std::vector<char*> argv(argc);
  std::vector<std::string> argv_strings(argc);
  for (int i = 0; i < argc; ++i) {
      argv_strings[i] = WStringToUtf8(argvW[i]);
      argv[i] = &argv_strings[i][0];
  }

  if (IsWindowsVersionOrGreater(10, 0, 14393)) {
    typedef BOOL(WINAPI * SetDpiCtxFn)(DPI_AWARENESS_CONTEXT);
    auto setDpiAwarenessContext = (SetDpiCtxFn)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext");
    if (setDpiAwarenessContext) {
      setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
  } else {
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  }

  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg.rfind("--webui-url=", 0) == 0) {
      g_webuiUrls.insert(g_webuiUrls.begin(), Utf8ToWstring(arg.substr(12)));
    } else if (arg.rfind("--autoupdater-endpoint=", 0) == 0) {
      g_updateUrl = arg.substr(23);
    } else if (arg == "--streaming-server-disabled") {
      g_streamingServer = false;
    } else if (arg == "--autoupdater-force-full") {
      g_autoupdaterForceFull = true;
    }
  }

  std::wstring launchProtocol;
  if (!CheckSingleInstance(argc, argv.data(), launchProtocol)) {
    LocalFree(argvW);
    return 0;
  }
  g_launchProtocol = launchProtocol;
  LocalFree(argvW);

  std::vector<std::wstring> processesToCheck = {L"stremio.exe",
                                                L"stremio-runtime.exe"};
  if (IsDuplicateProcessRunning(processesToCheck)) {
    MessageBoxW(nullptr,
                L"An older version of Stremio or Stremio server may be "
                L"running. There could be issues.",
                L"Stremio Already Running", MB_OK | MB_ICONWARNING);
  }

  Gdiplus::GdiplusStartupInput gpsi;
  if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gpsi, nullptr) != Gdiplus::Ok) {
    AppendToCrashLog(L"[BOOT]: GdiplusStartup failed.");
    return 1;
  }

  LoadSettings();

  // Updater
  // g_updaterThread = std::thread(RunAutoUpdaterOnce);
  // g_updaterThread.detach();

  g_hInst = hInstance;
  g_darkBrush = CreateSolidBrush(RGB(0, 0, 0));

  WNDCLASSEX wcex = {0};
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = g_hInst;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = g_darkBrush;
  wcex.lpszClassName = szWindowClass;
  if (!RegisterClassEx(&wcex)) {
    AppendToCrashLog(L"[BOOT]: RegisterClassEx failed!");
    return 1;
  }

  g_hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 900, nullptr,
                        nullptr, g_hInst, nullptr);
  if (!g_hWnd) {
    AppendToCrashLog(L"[BOOT]: CreateWindow failed!");
    return 1;
  }

  if (!RegisterHotKey(g_hWnd, 1, 0, VK_MEDIA_PLAY_PAUSE)) {
    AppendToCrashLog(L"[BOOT]: Failed to register hotkey!");
  }

  ScaleWithDPI();
  LoadCustomMenuFont();

  WINDOWPLACEMENT wp;
  if (LoadWindowPlacement(wp)) {
    SetWindowPlacement(g_hWnd, &wp);
    ShowWindow(g_hWnd, wp.showCmd);
    UpdateWindow(g_hWnd);
  } else {
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);
  }

  #ifdef NDEBUG 
    PostMessage(g_hWnd, WM_RUN_UPDATER, 0, 0);
  #endif


  CreateSplashScreen(g_hWnd);

  if (!InitMPV(g_hWnd)) {
    DestroyWindow(g_hWnd);
    return 1;
  }

  if (g_streamingServer) {
    StartNodeServer();
  }

  InitWebView2(g_hWnd);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  Cleanup();
  
  return (int)msg.wParam;
}