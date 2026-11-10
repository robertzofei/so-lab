#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>

const char* SHM_NAME = "/shm_counter_lab";
const char* SEM_NAME = "/sem_counter_lab";

void run_worker(int id, int* mem, sem_t* sem) {
    // Initializam generatorul de numere aleatoare cu un seed diferit per proces
    std::srand((unsigned int)std::time(nullptr) ^ (getpid() << 16));

    while (true) {
        // Rezerva semaforul (Wait / P)
        sem_wait(sem);

        // 1. Citeste memoria
        if (*mem >= 1000) {
            sem_post(sem); // Elibereaza semaforul inainte de iesire
            break;
        }

        // 2. Da cu banul: random(2) -> valori {1, 2}
        // Cat timp cade 2, scrie in memorie urmatorul numar
        int ban = (std::rand() % 2) + 1;
        while (ban == 2 && *mem < 1000) {
            (*mem)++;
            std::cout << "[Proces " << id << " | PID " << getpid()
                      << "] Banul a picat 2 -> a scris valoarea: " << *mem << "\n";
            ban = (std::rand() % 2) + 1;
        }

        // 3. Elibereaza semaforul (Post / V)
        sem_post(sem);

        // Scurta pauza pentru a ceda executia celuilalt proces
        usleep(100);
    }
}

int main() {
    // Curatare prealabila a eventualelor descriptoare ramase deschise
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);

    // 1. Creare obiect de memorie partajata (POSIX Shared Memory)
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Eroare shm_open");
        return 1;
    }

    // Setam dimensiunea memoriei partajate pentru un numar intreg
    if (ftruncate(shm_fd, sizeof(int)) == -1) {
        perror("Eroare ftruncate");
        return 1;
    }

    // Mapare in spatiul de adrese al procesului
    int* mem = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        perror("Eroare mmap");
        return 1;
    }

    *mem = 0; // Contorul porneste de la 0

    // 2. Creare semafor numit (valoare initiala 1)
    sem_t* sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("Eroare sem_open");
        return 1;
    }

    std::cout << "[Parinte PID " << getpid() << "] Am creat memoria partajata si semaforul.\n";
    std::cout << "[Parinte PID " << getpid() << "] Lansez procesul copil prin fork()...\n\n";

    pid_t pid = fork();

    if (pid < 0) {
        perror("Eroare fork");
        return 1;
    }

    if (pid == 0) {
        // Proces Copil (Procesul 2)
        run_worker(2, mem, sem);

        sem_close(sem);
        munmap(mem, sizeof(int));
        close(shm_fd);
        _exit(0);
    } else {
        // Proces Parinte (Procesul 1)
        run_worker(1, mem, sem);

        // Asteapta finalizarea copilului
        int status;
        waitpid(pid, &status, 0);

        std::cout << "\n[Parinte] Numarare finalizata cu succes!\n";
        std::cout << "[Parinte] Valoare finala citita din memorie: " << *mem << "\n";

        // Curatare resurse din sistem
        sem_close(sem);
        sem_unlink(SEM_NAME);
        munmap(mem, sizeof(int));
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }

    return 0;
}
