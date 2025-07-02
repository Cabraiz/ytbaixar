#include <windows.h>
#include <cstdio>
#include <string>

// IDs dos controles
#define ID_EDIT_URL     1
#define ID_BUTTON_MP4   2
#define ID_BUTTON_MP3   3
#define ID_EDIT_OUTPUT  4
#define ID_BUTTON_CLEAR 5

// Função auxiliar para rodar yt-dlp
void RunYtDlp(HWND hwnd, const char* url, const char* format) {
    char commandLine[2048];

    if (strcmp(format, "mp3") == 0) {
        snprintf(commandLine, sizeof(commandLine),
            "yt-dlp.exe -x --audio-format mp3 %s", url);
    } else {
        snprintf(commandLine, sizeof(commandLine),
            "yt-dlp.exe %s", url);
    }

    // Obtém diretório do EXE
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    for (int i = strlen(exePath) - 1; i >= 0; --i) {
        if (exePath[i] == '\\') {
            exePath[i] = '\0';
            break;
        }
    }
    const char* workingDir = exePath;

    // DESABILITA botões MP4 e MP3
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP4), FALSE);
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP3), FALSE);

    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0);
    SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = { sizeof(si) };
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.hStdError = hChildStd_OUT_Wr;

    PROCESS_INFORMATION pi;

    if (!CreateProcess(
            NULL,
            commandLine,
            NULL, NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,
            workingDir,
            &si,
            &pi))
    {
        MessageBox(hwnd, "Erro ao iniciar yt-dlp. Execute como administrador e verifique o caminho.", "Erro", MB_OK | MB_ICONERROR);
        // REABILITA botões caso falhe
        EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP4), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP3), TRUE);
        return;
    }

    CloseHandle(hChildStd_OUT_Wr);

    char buffer[4096];
    DWORD dwRead;
    std::string output;

    for (;;) {
        BOOL success = ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer) - 1, &dwRead, NULL);
        if (!success || dwRead == 0)
            break;
        buffer[dwRead] = '\0';
        output += buffer;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);

    // Atualiza campo de log
    SetWindowText(GetDlgItem(hwnd, ID_EDIT_OUTPUT), output.c_str());

    // REABILITA botões depois de terminar
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP4), TRUE);
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP3), TRUE);
}


// Callback da janela
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
        {
            // Campo de URL
            HWND hEditUrl = CreateWindowEx(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                20, 20, 550, 25,
                hwnd, (HMENU)ID_EDIT_URL, NULL, NULL
            );

            // Botão MP4
            CreateWindow(
                "BUTTON", "MP4",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                580, 20, 80, 25,
                hwnd, (HMENU)ID_BUTTON_MP4, NULL, NULL
            );

            // Botão MP3
            CreateWindow(
                "BUTTON", "MP3",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                670, 20, 80, 25,
                hwnd, (HMENU)ID_BUTTON_MP3, NULL, NULL
            );

            // Botão LIMPAR
            CreateWindow(
                "BUTTON", "LIMPAR",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD,
                580, 55, 170, 25,
                hwnd, (HMENU)ID_BUTTON_CLEAR, NULL, NULL
            );

            // Campo de log de saída
            HWND hEditOutput = CreateWindowEx(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                20, 90, 730, 200,
                hwnd, (HMENU)ID_EDIT_OUTPUT, NULL, NULL
            );

            // Fonte monoespaçada para melhor leitura
            HFONT hFont = CreateFont(
                -16, 0, 0, 0,
                FW_NORMAL,
                FALSE, FALSE, FALSE,
                DEFAULT_CHARSET,
                OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                VARIABLE_PITCH,
                TEXT("Consolas")
            );
            SendMessage(hEditUrl, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        break;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BUTTON_MP4 || LOWORD(wParam) == ID_BUTTON_MP3) {
                char buffer[2048];
                GetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), buffer, sizeof(buffer));

                if (strlen(buffer) == 0) {
                    MessageBox(hwnd, "Digite a URL primeiro.", "Aviso", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                if (LOWORD(wParam) == ID_BUTTON_MP4) {
                    RunYtDlp(hwnd, buffer, "mp4");
                }
                else if (LOWORD(wParam) == ID_BUTTON_MP3) {
                    RunYtDlp(hwnd, buffer, "mp3");
                }
            }
            else if (LOWORD(wParam) == ID_BUTTON_CLEAR) {
                SetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), "");
                SetWindowText(GetDlgItem(hwnd, ID_EDIT_OUTPUT), "");
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const char CLASS_NAME[] = "YTBaixarJanela";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "YT Baixar",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 780, 350,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
