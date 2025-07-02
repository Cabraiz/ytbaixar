#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

// IDs dos controles
#define ID_EDIT_URL   1
#define ID_BUTTON_MP4 2
#define ID_BUTTON_MP3 3
#define ID_EDIT_LOG   4
#define ID_BUTTON_CLEAR 5

// Armazena as últimas 5 saídas
std::vector<std::string> lastLogs;

// Atualiza o campo de log
void UpdateLogWindow(HWND hwnd) {
    HWND hLog = GetDlgItem(hwnd, ID_EDIT_LOG);
    std::string combined;

    for (auto it = lastLogs.rbegin(); it != lastLogs.rend(); ++it) {
        combined += *it;
        combined += "\r\n---\r\n";
    }

    SetWindowText(hLog, combined.c_str());
}

// Função auxiliar para rodar yt-dlp
void RunYtDlp(HWND hwnd, const char* url) {
    char commandLine[2048];
    snprintf(commandLine, sizeof(commandLine),
        "yt-dlp.exe %s", url);

    // Caminho onde está o exe
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    for (int i = strlen(exePath) - 1; i >= 0; --i) {
        if (exePath[i] == '\\') {
            exePath[i] = '\0';
            break;
        }
    }
    const char* workingDir = exePath;

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
        MessageBox(hwnd, "Erro ao iniciar yt-dlp.\nExecute como administrador e verifique o caminho.", "Erro", MB_OK | MB_ICONERROR);
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

    // Adiciona ao histórico
    lastLogs.push_back(output);
    if (lastLogs.size() > 5)
        lastLogs.erase(lastLogs.begin());

    UpdateLogWindow(hwnd);
}

// Callback da janela
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
        {
            // Botão LIMPAR
            CreateWindow(
                "BUTTON", "LIMPAR",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                20, 10, 80, 25,
                hwnd, (HMENU)ID_BUTTON_CLEAR, NULL, NULL
            );

            // Campo URL
            HWND hEditUrl = CreateWindowEx(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                110, 10, 400, 25,
                hwnd, (HMENU)ID_EDIT_URL, NULL, NULL
            );
            SendMessage(hEditUrl, EM_LIMITTEXT, (WPARAM)2048, 0);

            // Botões MP4 e MP3
            CreateWindow("BUTTON", "MP4",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                520, 10, 80, 25,
                hwnd, (HMENU)ID_BUTTON_MP4, NULL, NULL);

            CreateWindow("BUTTON", "MP3",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                610, 10, 80, 25,
                hwnd, (HMENU)ID_BUTTON_MP3, NULL, NULL);

            // Campo de LOG multilinha
            HWND hEditLog = CreateWindowEx(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                20, 50, 670, 320,
                hwnd, (HMENU)ID_EDIT_LOG, NULL, NULL
            );
        }
        break;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BUTTON_CLEAR) {
                // Limpa o campo de URL
                SetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), "");
            }
            else if (LOWORD(wParam) == ID_BUTTON_MP4 || LOWORD(wParam) == ID_BUTTON_MP3) {
                char buffer[2048];
                GetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), buffer, sizeof(buffer));

                if (strlen(buffer) == 0) {
                    MessageBox(hwnd, "Digite a URL primeiro.", "Aviso", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                RunYtDlp(hwnd, buffer);
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
    const char CLASS_NAME[] = "MinhaJanela";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "YT Baixar",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 440,
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
