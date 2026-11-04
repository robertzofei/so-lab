#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Functie pentru verificarea numerelor prime
bool is_prime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; ++i) {
        if (n % i == 0) return false;
    }
    return true;
}

int main() {
    const int NUM_PROCESSES = 10;
    // p2c := parent to child;
    int p2c[NUM_PROCESSES][2]; // Pipe-uri prin care scrie Parintele, citeste Copilul.
    // c2p := child to parent;
    int c2p[NUM_PROCESSES][2]; // Pipe-uri prin care scrie Copilul, citeste Parintele.
    pid_t pids[NUM_PROCESSES];

    std::cout << "[Parinte PID " << getpid() << "] Creez cele " << NUM_PROCESSES << " procese pe Linux...\n\n";

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        // Initializam ambele canale de comunicare.
        // Verificam daca a esuat crearea oricaruia dintre cele doua canale de 
        // comunicare (pipe-uri).
        if (pipe(p2c[i]) < 0 || pipe(c2p[i]) < 0) {
            std::cerr << "Eroare la crearea pipe-urilor!\n";
            return 1;
        }

        pids[i] = fork();

        if (pids[i] < 0) {
            std::cerr << "Eroare la apelul fork() pentru procesul " << (i + 1) << "!\n";
            return 1;
        }

        // ---------------------------------------------------------
        // COD EXECUTAT DE PROCESUL COPIL
        // ---------------------------------------------------------
        // Verifica daca e copil.
        if (pids[i] == 0) {
            close(p2c[i][1]); // Inchidem capatul de scriere inutil (Parinte -> Copil)
            close(c2p[i][0]); // Inchidem capatul de citire inutil (Copil -> Parinte)

            int interval[2];
            // Copilul citeste intervalul furnizat de parinte
            read(p2c[i][0], interval, sizeof(interval));

            // Calculeaza si trimite inapoi prin pipe numerele prime gasite
            for (int n = interval[0]; n <= interval[1]; ++n) {
                if (is_prime(n)) {
                    write(c2p[i][1], &n, sizeof(n));
                }
            }

            close(p2c[i][0]); // Elibereaza descriptorul folosit
            close(c2p[i][1]); // Inchiderea acestui capat semnaleaza EOF parintelui
            _exit(0); // Iesire imediata, fara executarea functiilor destructoare in plus
        } 
        
        // ---------------------------------------------------------
        // COD EXECUTAT DE PROCESUL PARINTE
        // ---------------------------------------------------------
        else {
            close(p2c[i][0]); // Inchidem capatul de citire inutil (Parinte -> Copil)
            close(c2p[i][1]); // Inchidem capatul de scriere inutil (Copil -> Parinte)

            // Trimitem intervalul asignat (ex: 1-1000, 1001-2000)
            int interval[2] = {i * 1000 + 1, (i + 1) * 1000};
            write(p2c[i][1], interval, sizeof(interval));
            
            close(p2c[i][1]); // Inchidem imediat dupa scriere pentru a elibera resursele si a preveni blocajele
        }
    }

    // Parintele citeste numerele prime furnizate de fiecare copil si le afiseaza
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        std::cout << "--- Primele gasite de copilul " << (i + 1) << " (Interval " << (i * 1000 + 1) << " - " << ((i + 1) * 1000) << ") ---\n";
        
        int prime;
        // Citeste in bucla atata timp cat copilul pompeaza date si se opreste la EOF (0 returnat)
        while (read(c2p[i][0], &prime, sizeof(prime)) > 0) {
            std::cout << prime << " ";
        }
        std::cout << "\n\n";
        close(c2p[i][0]); // Inchidem canalul dupa citirea finala

        // Asteptam iesirea copilului pentru a elimina intrarea lui (starea <defunct>) din tabelul kernelului
        int status;
        waitpid(pids[i], &status, 0);
    }

    return 0;
}
