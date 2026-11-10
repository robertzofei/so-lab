#include <iostream>
#include <cstdlib>
#include <ctime>
#include <windows.h>
#include <string>

const char* SHM_NAME = "Local\\SO_Shm_Counter";
const char* SEM_NAME = "Local\\SO_Sem_Counter";

void run_worker(int id, int* mem, HANDLE hSem) {
    std::srand((unsigned int)std::time(nullptr) ^ (GetCurrentProcessId() << 16));

    while (true) {
        // Rezerva semaforul
        WaitForSingleObject(hSem, INFINITE);

        // 1. Citeste memoria
        if (*mem >= 1000) {
            ReleaseSemaphore(hSem, 1, NULL); // Elibereaza semaforul inainte de oprire
            break;
        }

        // 2. Da cu banul: random(2) genereaza 1 sau 2
        int ban = (std::rand() % 2) + 1;
        while (ban == 2 && *mem < 1000) {
            (*mem)++;
            std::cout << "[Proces " << id << " | PID " << GetCurrentProcessId()
                      << "] Banul a picat 2 -> a scris valoarea: " << *mem << "\n";
            ban = (std::rand() % 2) + 1;
        }

        // 3. Elibereaza semaforul
        ReleaseSemaphore(hSem, 1, NULL);

        Sleep(1); // Cedeaza executia pentru a permite alternanta proceselor
    }
}

int main(int argc, char* argv[]) {
    // -------------------------------------------------------------
    // RAMURA PROCES COPIL (Procesul 2)
    // -------------------------------------------------------------
    if (argc == 2 && std::string(argv[1]) == "child") {
        HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
        if (!hMapFile) return 1;

        int* mem = (int*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
        if (!mem) {
            CloseHandle(hMapFile);
            return 1;
        }

        HANDLE hSem = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, FALSE, SEM_NAME);
        if (!hSem) {
            UnmapViewOfFile(mem);
            CloseHandle(hMapFile);
            return 1;
        }

        run_worker(2, mem, hSem);

        UnmapViewOfFile(mem);
        CloseHandle(hMapFile);
        CloseHandle(hSem);
        return 0;
    }

    // -------------------------------------------------------------
    // RAMURA PROCES PARINTE (Procesul 1)
    // -------------------------------------------------------------
    // 1. Creare obiect de memorie mapata (sustinut de pagefile)
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(int),
        SHM_NAME
    );
    if (!hMapFile) return 1;

    int* mem = (int*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    if (!mem) {
        CloseHandle(hMapFile);
        return 1;
    }

    *mem = 0; // Initializare contor

    // 2. Creare semafor numit (valoare initiala 1, valoare maxima 1)
    HANDLE hSem = CreateSemaphoreA(NULL, 1, 1, SEM_NAME);
    if (!hSem) {
        UnmapViewOfFile(mem);
        CloseHandle(hMapFile);
        return 1;
    }

    std::cout << "[Parinte PID " << GetCurrentProcessId() << "] Initializat memorie partajata si semafor pe Windows.\n";

    // 3. Lansare proces copil prin CreateProcessA
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string cmd = "\"" + std::string(exe_path) + "\" child";

    if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "Eroare la crearea procesului copil!\n";
        UnmapViewOfFile(mem);
        CloseHandle(hMapFile);
        CloseHandle(hSem);
        return 1;
    }

    CloseHandle(pi.hThread);

    // Parintele ruleaza ca Procesul 1
    run_worker(1, mem, hSem);

    // Asteapta terminarea copilului
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    std::cout << "\n[Parinte] Numarare finalizata cu succes!\n";
    std::cout << "[Parinte] Valoare finala citita din memorie: " << *mem << "\n";D

    // Curatare resurse
    UnmapViewOfFile(mem);
    CloseHandle(hMapFile);
    CloseHandle(hSem);

    return 0;
}