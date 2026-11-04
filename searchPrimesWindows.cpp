#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

// Functie pentru verificarea numerelor prime
bool is_prime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; ++i) {
        if (n % i == 0) return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    // ---------------------------------------------------------
    // COD EXECUTAT DE PROCESUL COPIL
    // ---------------------------------------------------------
    if (argc == 3) {
        // Preluam handle-urile din argumentele liniei de comanda convertite din uintptr_t
        HANDLE hReadFromParent = (HANDLE)(uintptr_t)std::stoull(argv[1]);
        HANDLE hWriteToParent = (HANDLE)(uintptr_t)std::stoull(argv[2]);

        int interval[2];
        DWORD bytesRead;
        // Citim capetele intervalului primite de la parinte
        if (ReadFile(hReadFromParent, interval, sizeof(interval), &bytesRead, NULL) && bytesRead > 0) {
            for (int i = interval[0]; i <= interval[1]; ++i) {
                if (is_prime(i)) {
                    DWORD bytesWritten;
                    // Trimitem numarul prim gasit catre parinte
                    WriteFile(hWriteToParent, &i, sizeof(i), &bytesWritten, NULL);
                }
            }
        }
        
        // Inchidem capetele pipe-urilor din copil pentru a evita scurgerile de resurse
        CloseHandle(hReadFromParent);
        CloseHandle(hWriteToParent);
        return 0;
    }

    // ---------------------------------------------------------
    // COD EXECUTAT DE PROCESUL PARINTE
    // ---------------------------------------------------------
    const int NUM_PROCESSES = 10;
    HANDLE process_handles[NUM_PROCESSES];
    HANDLE p2c_write[NUM_PROCESSES]; // Pipe-uri prin care parintele scrie (Parent to Child)
    HANDLE c2p_read[NUM_PROCESSES];  // Pipe-uri din care parintele citeste (Child to Parent)

    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE; // Permite mostenirea handle-urilor de catre procesul copil
    saAttr.lpSecurityDescriptor = NULL;

    std::cout << "[Parinte PID " << GetCurrentProcessId() << "] Creez cele " << NUM_PROCESSES << " procese pe Windows...\n\n";

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        HANDLE p2c_read_child, p2c_write_parent;
        HANDLE c2p_read_parent, c2p_write_child;

        // 1. Pipe pentru trimiterea intervalului (Parinte -> Copil)
        CreatePipe(&p2c_read_child, &p2c_write_parent, &saAttr, 0);
        SetHandleInformation(p2c_write_parent, HANDLE_FLAG_INHERIT, 0); // Oprim mostenirea capatului parintelui

        // 2. Pipe pentru primirea rezultatelor (Copil -> Parinte)
        CreatePipe(&c2p_read_parent, &c2p_write_child, &saAttr, 0);
        SetHandleInformation(c2p_read_parent, HANDLE_FLAG_INHERIT, 0); // Oprim mostenirea capatului parintelui

        p2c_write[i] = p2c_write_parent;
        c2p_read[i] = c2p_read_parent;

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        // Transmitem capetele care trebuie folosite de copil ca argumente in linia de comanda
        std::string cmd = "\"" + std::string(exe_path) + "\" " + 
                          std::to_string((unsigned long long)(uintptr_t)p2c_read_child) + " " + 
                          std::to_string((unsigned long long)(uintptr_t)c2p_write_child);

        if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            std::cerr << "Eroare la crearea procesului " << (i + 1) << "!\n";
            return 1;
        }

        process_handles[i] = pi.hProcess;
        CloseHandle(pi.hThread); // Inchidem handle-ul de thread (nu ne este util pentru sincronizare)

        // Inchidem capetele care apartin exclusiv copilului din spatiul parintelui
        CloseHandle(p2c_read_child);
        CloseHandle(c2p_write_child);

        // Trimitem intervalul catre copil (ex: 1-1000, 1001-2000)
        int interval[2] = {i * 1000 + 1, (i + 1) * 1000};
        DWORD bytesWritten;
        WriteFile(p2c_write_parent, interval, sizeof(interval), &bytesWritten, NULL);
        
        // Inchidem capatul de scriere pentru ca procesul copil sa primeasca semnalul de sfarsit de fisier (EOF)
        CloseHandle(p2c_write_parent);
    }

    // Parintele citeste si afiseaza numerele prime de la fiecare copil
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        std::cout << "--- Primele gasite de procesul " << (i + 1) << " (Interval " << (i * 1000 + 1) << " - " << ((i + 1) * 1000) << ") ---\n";
        int prime;
        DWORD bytesRead;
        // Citeste pana cand copilul isi inchide pipe-ul de scriere
        while (ReadFile(c2p_read[i], &prime, sizeof(prime), &bytesRead, NULL) && bytesRead > 0) {
            std::cout << prime << " ";
        }
        std::cout << "\n\n";
        CloseHandle(c2p_read[i]);
    }

    // Asteptam terminarea tuturor proceselor copil pentru a nu avea procese reziduale
    WaitForMultipleObjects(NUM_PROCESSES, process_handles, TRUE, INFINITE);
    for (int i = 0; i < NUM_PROCESSES; ++i) CloseHandle(process_handles[i]);

    return 0;
}