tut1.c - Obliczanie liczby PI metodą Monte Carlo (wieloprocesowo). Używa fork(), anonimowego mmap do dzielenia tablicy obliczeń oraz mapowania pliku (O_CREAT) do logowania wyników.

tut2_server.c - Serwer "gry" oparty na pamięci dzielonej w RAM. Inicjalizuje pamięć przez shm_open + ftruncate, tworzy odporny mutex procesowy (PTHREAD_MUTEX_ROBUST) i elegancko kończy pracę przy sygnale SIGINT.

tut2_client.c - Klient do powyższego serwera. Podłącza się pod istniejące shm_open, synchronizuje odczyty/zapisy z planszy współdzielonym mutexem i posiada mechanizm nagłej śmierci, testujący odporność serwera.

zad1.c - Równoległy histogram zliczający znaki w pliku. Procesy potomne mapują plik do odczytu (MAP_PRIVATE), zliczają znaki lokalnie, a wyniki agregują w anonimowej pamięci dzielonej chronionej przez odporny mutex (z obsługą EOWNERDEAD).

zad2.c - Bezpieczna inicjalizacja i zliczanie współpracujących procesów. Używa semafora nazwanego (sem_open), aby wyeliminować zjawisko wyścigu przy tworzeniu obiektu shm_open. Wykorzystuje odporny mutex i automatycznie usuwa obiekty IPC (unlink), gdy odłączy się ostatni proces.
