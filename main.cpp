#include <windows.h>
#include <cstdio>
#include <string>
#include <shlobj.h>

// IDs dos controles
#define ID_EDIT_URL     1
#define ID_BUTTON_MP4   2
#define ID_BUTTON_MP3   3
#define ID_EDIT_OUTPUT  4
#define ID_BUTTON_CLEAR 5
#define ID_BUTTON_OPEN  6

// Append text no campo multiline
void AppendText(HWND hEdit, const char* newText) {
    int length = GetWindowTextLength(hEdit);
    SendMessage(hEdit, EM_SETSEL, length, length);
    SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)newText);
}

// Cria diretório se não existir
void CreateDirectoryIfNotExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(path.c_str(), NULL);
    }
}

// Função principal de download
void RunYtDlp(HWND hwnd, const char* url, const char* format) {
    // Caminho da área de trabalho
    char desktopPath[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath);

    std::string musicDir = std::string(desktopPath) + "\\MUSICAS";
    CreateDirectoryIfNotExists(musicDir);

    char commandLine[4096];

    if (strcmp(format, "mp3") == 0) {
        snprintf(commandLine, sizeof(commandLine),
            "yt-dlp.exe -o \"%s\\%%(title)s.%%(ext)s\" -x --audio-format mp3 %s",
            musicDir.c_str(), url);
    } else {
        snprintf(commandLine, sizeof(commandLine),
            "yt-dlp.exe -o \"%s\\%%(title)s.%%(ext)s\" %s",
            musicDir.c_str(), url);
    }

    // Diretório do exe
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    for (int i = strlen(exePath) - 1; i >= 0; --i) {
        if (exePath[i] == '\\') {
            exePath[i] = '\0';
            break;
        }
    }
    const char* workingDir = exePath;

    // Desabilita botões
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP4), FALSE);
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP3), FALSE);

    HWND outputCtrl = GetDlgItem(hwnd, ID_EDIT_OUTPUT);
    SetWindowText(outputCtrl, "Iniciando download...\r\n");

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
        EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP4), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP3), TRUE);
        return;
    }

    CloseHandle(hChildStd_OUT_Wr);

    char buffer[512];
    DWORD dwRead;

    // Leitura em tempo real
    for (;;) {
        BOOL success = ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer) - 1, &dwRead, NULL);
        if (!success || dwRead == 0)
            break;
        buffer[dwRead] = '\0';
        AppendText(outputCtrl, buffer);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);

    AppendText(outputCtrl, "\r\nDownload concluído.\r\n");

    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP4), TRUE);
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_MP3), TRUE);
}

// Callback da janela
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
        {
            HWND hEditUrl = CreateWindowEx(
                0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                20, 20, 550, 25,
                hwnd, (HMENU)ID_EDIT_URL, NULL, NULL
            );

            CreateWindow("BUTTON", "MP4", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                580, 20, 80, 25, hwnd, (HMENU)ID_BUTTON_MP4, NULL, NULL);

            CreateWindow("BUTTON", "MP3", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                670, 20, 80, 25, hwnd, (HMENU)ID_BUTTON_MP3, NULL, NULL);

            CreateWindow("BUTTON", "ABRIR PASTA", WS_TABSTOP | WS_VISIBLE | WS_CHILD,
                420, 55, 150, 25, hwnd, (HMENU)ID_BUTTON_OPEN, NULL, NULL);

            CreateWindow("BUTTON", "LIMPAR", WS_TABSTOP | WS_VISIBLE | WS_CHILD,
                580, 55, 170, 25, hwnd, (HMENU)ID_BUTTON_CLEAR, NULL, NULL);

            HWND hEditOutput = CreateWindowEx(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                20, 90, 730, 200,
                hwnd, (HMENU)ID_EDIT_OUTPUT, NULL, NULL
            );

            HFONT hFont = CreateFont(
                -16, 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, TEXT("Consolas")
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
                if (LOWORD(wParam) == ID_BUTTON_MP4) RunYtDlp(hwnd, buffer, "mp4");
                if (LOWORD(wParam) == ID_BUTTON_MP3) RunYtDlp(hwnd, buffer, "mp3");
            }
            else if (LOWORD(wParam) == ID_BUTTON_CLEAR) {
                SetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), "");
                SetWindowText(GetDlgItem(hwnd, ID_EDIT_OUTPUT), "");
            }
            else if (LOWORD(wParam) == ID_BUTTON_OPEN) {
                // Abre pasta MUSICAS
                char desktopPath[MAX_PATH];
                SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktopPath);
                std::string musicDir = std::string(desktopPath) + "\\MUSICAS";
                ShellExecuteA(NULL, "open", musicDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
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

    // Centraliza na tela
    int width = 780;
    int height = 350;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "YT Baixar",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd); // Para vir ao topo

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
