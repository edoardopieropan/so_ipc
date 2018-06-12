/*!
 * @file processes.c
 * Il padre, fa controlli sulle matrici e manda in esecuzione i figli. Passa loro le operazioni da svolgere finchè non sono terminate. \n
 * Termina tutte le IPCs prima di terminare il processo fornendo anche un event handler per il CTRL+C
 * @author Pieropan Edoardo
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <memory.h>
#include <sys/msg.h>
#include <sys/sem.h>

#define SEMAPHORE 0
#define SIGINT 2
#define SIGKILL 9

//VARIABILI GLOBALI
int fd_a;///<Il file descriptor del file contenente la matrice A
int fd_b;///<Il file descriptor del file contenente la matrice B
int fd_c;///<Il file descriptor del file dove scrivere la matrice C
int shmid_a;///<L'ID della zona di memoria per la matrice A
int shmid_b;///<L'ID della zona di memoria per la matrice B
int shmid_c;///<L'ID della zona di memoria per la matrice C
int shmid_d;///<L'ID della zona di memoria per il risultato della somma
int semid;///<L'ID della zona di memoria per il semaforo
int idMessage;///<L'ID della zona di memoria per la coda di messaggi


/*union semun {
    int                 val;
    struct semid_ds *   buf;
    unsigned short *    array;
    struct seminfo *    __buf;

};*/


//Chiude le IPCs e i file descriptor
/*!
 * Chiude tutte le IPCs e i file descriptor
 * @param idMessage l'ID della coda di messaggi
 * @param shmid_a l'ID della zona di memoria matrice A
 * @param shmid_b l'ID della zona di memoria matrice B
 * @param shmid_c l'ID della zona di memoria matrice C
 * @param shmid_d l'ID della zona di memoria della somma
 * @param semid l'ID della zona di memoria del semaforo
 * @param fd_a file descriptor del file A
 * @param fd_b file descriptor del file B
 * @param fd_c file descriptor del file C
 */
void closeIpcsFile(){
    //Chiusura IPC
    msgctl(idMessage, IPC_RMID, 0);
    shmctl(shmid_a, IPC_RMID, 0);
    shmctl(shmid_b, IPC_RMID, 0);
    shmctl(shmid_c, IPC_RMID, 0);
    shmctl(shmid_d, IPC_RMID, 0);
    semctl(semid, IPC_RMID, 0);

    //Chiudo i tre file
    close(fd_a);
    close(fd_b);
    close(fd_c);
}


void printError(char *stringa){
    write(2, stringa, strlen(stringa));
    closeIpcsFile();
    exit(1);
}

//Ritorna -1 se la matrice non è quadrata, altrimenti il numero N
/*!Controlla se la matrice inserita tramite file descriptor è quadrata
 * @param fd - Il file descriptor del file da controllare
 * @return -1 se la matrice non è quadrata
 *@return se sono quadrate restituisce l'ordine delle matrici
 */
int isSquare(int fd) {
    char character;
    int status; //per controllare l'esito della lettura
    //status=read(fd,&character,1);
    int colonne=1, righe=1;

    //leggo finchè ci sono caratteri validi
    while((status=read(fd, &character, 1))>0) {
        if(status==-1)
        {
            printError("Errore: problema nella lettura del file!\n");
        }
        //aumento il contatore delle colonne solamente alla prima riga e contanto i ;
        if (character == ';' && righe==1)
            colonne++;
        //aumento il contatore delle righe ogni \n
        if (character == '\n')
            righe++;
    }

    status=lseek(fd, 0, 0); //mi riposiziono all'inizio del file, offset = 0
    if(status==-1)
    {
        printError("Errore: problema nella lettura del file!\n");

    }

    //se la matrice è quadrata ritorno il valore N della matrice NxN
    if(righe==colonne)
        return righe;

    //altrimenti ritorno -1
    else
        return -1;
}

void myPrint(char *stringa) {
    if (write(1, stringa, strlen(stringa)) == -1){
        printError("Errore nella scrittura a videon\n");
    }
}


//Ritorna true se le matrici hanno lo stesso N e il valore inserito corrisponde
/*!
 * Controlla se le due matrici inserite tramite i rispettivi file descriptor hanno lo stesso ordine e se
 * corrispondono al valore inserito dall'utente
 * @param n_mat_a l'ordine della matrice A
 * @param n_mat_b l'ordine della matrice B
 * @param valore_inserito  il valore inserito dall'utente
 * @return 1 se le matrici sono corrispondenti tra loro e al valore inserito dall'utente
 * @return 0 se le matrici non sono corrispondenti tra loro e/o al valore inserito dall'utente
 */
int matchingMatrix(int n_mat_a, int n_mat_b, int valore_inserito) {
    //se una delle due matrici non è quadrata
    if(n_mat_a==-1 || n_mat_b==-1){
        myPrint("Errore: ordine delle matrici non valido\n");
        return 0;
    }
    else
        //se le matrici non corrispondono con il valore inserito
        if((n_mat_a != valore_inserito) || (n_mat_b != valore_inserito)){
            myPrint("Errore: ordine delle matrici non corrispondente al valore inserito\n");
            return 0;
        }

    return 1;
}

//Alloca una nuova zona di memoria condivisa
/*!
 * Crea una zona di memoria condivisa tramite shmget
 * @param size la grandezza della zona di memoria che si intende creare
 * @param key la chiave
 * @return l'ID della zona di memoria appena creata
 */
int allocateMemory(size_t size, key_t key){
    int status;
    status = shmget(key, size, IPC_CREAT | 0666);
    //se è fallita
    if (status == -1)
    {
        myPrint("Errore: shmget fallita\n");
        closeIpcsFile();
        exit(1);
    }
    return status;
}

//Attach della zona di memoria, ritorna l'indirizzo della zona di memoria condivisa
/*!
 * "Collego" la zona di memoria condivisa al processo corrente tramite shmat
 * @oaram shmid l'ID della zona di memoria condivisa da "collegare"
 */
int *attachMemory(int shmid){
    int *status;
    status = shmat(shmid, 0, 0);
    if(*status==-1)
    {
        myPrint("Errore: shmat fallita\n");
        closeIpcsFile();
        exit(1);
    }
    else
        return status;
}

//Scrive la matrice in memoria condivisa
/*!
 * @brief Legge la matrice inserita e la scrive nella zona di memoria condivisa
 * - Legge tutto il file inserito salvandosi i valori su un buffer
 * - Split delle righe salvandole su un array di char
 * - Split dei ';' salvandosi i numeri in un unico array
 * - Scrivo l'array di numeri in memoria condivisa
 * @param fd file descriptor della matrice da scrivere in memoria
 * @param ordine l'ordine della matrice inserita
 * @param shm_pointer puntatore alla zona di memoria condivisa
 */
void writeInMemory(int fd, int ordine, int* shm_pointer){
    //Creo l'array di interi da scrivere in memoria
    struct stat infoFile; //struttura che mi permette di sapere con il campo st_size la dimensione del file
    fstat(fd, &infoFile);
    char buffer[infoFile.st_size];
    char *token;
    char *riga[ordine];
    int matrice[ordine*ordine];

    //Leggo tutto il file
    if(read(fd, &buffer, infoFile.st_size)<=0){
        printError("Errore: lettura file fallita\n");
    }

    int status=lseek(fd, 0, 0);

    if(status==-1)
    {
        printError("Errore: problema nella lettura del file!\n");
    }

    int c=0;

    //Separo ogni riga
    token=strtok(buffer,"\n");

    //finchè ci sono sottostringhe
    while(token!=NULL)
    {
        //salvo la riga
        riga[c++]=token;
        token=strtok(NULL, "\n");
    }

    c=0;
    //Separo ogni numero di ogni riga
    for(int i=0; i<ordine; i++) {
        token = strtok(riga[i], ";");
        while (token != NULL) {
            int tmp = atoi(token); //converto in int i caratteri letti
            matrice[c++]=tmp;
            token = strtok(NULL, ";");
        }
    }

    for(int i=0; i<ordine*ordine;i++)
        shm_pointer[i] = matrice[i];

    memset(buffer, 0, infoFile.st_size); //per pulire il buffer, altrimenti l'ultimo valore della matrice A andava sulla B
}

//Alloca lo spazio in memoria per la coda di messaggi
/*!
 * Alloca una zona di memoria per la coda di messaggi
 * @param key la chiave
 * @return l'ID della zona di memoria
 */
int allocateMessage(key_t key){
    int status;
    status = msgget(key, IPC_CREAT | 0666);
    if(status==-1)
    {
        printError("Errore: msgget fallita\n");

    }
    return status;
}

//Alloca lo spazio in memoria per il semaforo
/*!
 * Alloca una zona di memoria per il semaforo
 * @param key la chiave del semaforo
 * @return l'ID della zona di memoria
 */
int allocateSemaphore(key_t key){
    int status;
    status=semget(key, 1, 0666 | IPC_CREAT | IPC_EXCL);
    if(status==-1) {
        printError("Errore: semget fallita\n");
    }
    return status;
}


//Intercetta la signal del CTR+C
/*!
 * Funzione chiamata una volta intercettata la signal del CTR+C
 * @param sig_num la signal intercettata
 */
void catch_int(int sig_num) {
    printf("Ucciso processo %d\n",getpid());
    closeIpcsFile();
    exit(0);
}


///Struttura che contiene i dati dei messaggi ricevuti dal figlio
typedef struct{
    long mtype;///<Il tipo di messaggio
    int numeroProcesso;///<Il numero (ID) del processo che invia il messaggio
}Message;

///Struttura che contiene le operazioni, la riga e la colonna da inviare al figlio
typedef struct{
    char operazione; ///<L'operazione da far compiere al figlio
    int i;///<La riga su cui operare
    int j;///<La colonna su cui operare
}Data;

//Argc: argument counter
//Argv: 0, nome programma; 1-2-..-n, i parametri passati
int main(int argc, char *argv[]) {
    signal(SIGINT, catch_int);
    //Controllo il numero di parametri passati
    if(argc != 6)
	printError("Errore: numero di argomenti non sufficiente..\nChiusura...\n");


    //ATTRIBUTI
    int n_mat_a, n_mat_b;
    int *pointer_mat_a, *pointer_mat_b, *pointer_mat_c, *pointer_d;
    int ordine=atoi(argv[4]);
    size_t size;
    int numeroProcessi=atoi(argv[5]);
    int fd_pipe[2];
    char fd_pipe_read[10];
    int arrayPipe[numeroProcessi];
    int riga=0, colonna=0;

    

    //Controllo il numero di processi inserito
    if(numeroProcessi<1)
        printError("Errore: numero processi deve essere >= 1\nChiusura...\n");
    

    //Inizializzo le chiavi
    key_t key_A=ftok("/tmp", 'A'), key_B=ftok("/tmp", 'B'), key_C=ftok("/tmp", 'C'), key_D=ftok("/tmp", 'D');
    key_t keyMessage=ftok("/tmp", 'M');
    key_t  semkey = ftok("/tmp", 'K');

    //APERTURA FILES
    //Apro il file A
    fd_a = open(argv[1], O_RDONLY);
    if(fd_a==-1) {
        printError("Errore apertura del file 1!\n");
    }
    n_mat_a=isSquare(fd_a);

    //Apro il file B
    fd_b = open(argv[2], O_RDONLY);
    if(fd_b==-1) {
        printError("Errore apertura del file 2!\n");
    }
    n_mat_b=isSquare(fd_b);

    //Se non hanno lo stesso ordine o non corrisponde con quello inserito dall'utente
    if(!matchingMatrix(n_mat_a, n_mat_b, ordine)) {
        printError("Chiusura...\n");
    }

    size=ordine*ordine*sizeof(int); //imposto la dimensione della matrice

    //GESTIONE MEMORIA CONDIVISA

    //Alloco due zone di memoria condivisa
    shmid_a = allocateMemory(size, key_A);
    shmid_b = allocateMemory(size, key_B);
    shmid_c = allocateMemory(size, key_C);
    shmid_d = allocateMemory(size, key_D);


    //Assegno le zone di memoria condivisa e ritorna il puntatore
    pointer_mat_a=attachMemory(shmid_a);
    pointer_mat_b=attachMemory(shmid_b);
    pointer_mat_c=attachMemory(shmid_c);
    pointer_d=attachMemory(shmid_d);

    //Scrivo in memoria le matrici
    writeInMemory(fd_a, ordine, pointer_mat_a);
    writeInMemory(fd_b, ordine, pointer_mat_b);

    //ALLOCAZIONE CODA DI MESSAGGI
    idMessage=allocateMessage(keyMessage);
    Message msg;
    Data datas;

    /*union semun st_sem;
    semid=allocateSemaphore(semkey);
    st_sem.val=1;
    if((semctl(semid, SEMAPHORE, SETVAL, st_sem))==-1) {
        printError("Errore semctl\n");
    }*/


    int operazioniRimanenti=ordine*ordine;

    //GESTIONE PROCESSI
    //Caso processi maggiori o uguali al numero di operazione da fare
    if(numeroProcessi>=ordine*ordine) {
        //Creo tanti processi quanti richiesto
        for (int i = 0; i < numeroProcessi; i++){
            pipe(fd_pipe);
            arrayPipe[i] = fd_pipe[1]; //salvo la pipe in scrittura su un array
            char *str_contatore = malloc(sizeof(char)*20);
            sprintf(fd_pipe_read, "%i", fd_pipe[0]); //converto la pipe in lettura per poterla passare come argomento
            sprintf(str_contatore, "%i", i);
            char *arguments[] = {fd_pipe_read, argv[4], str_contatore, NULL}; //creo la coda argomenti
            free(str_contatore);
            int pid;
            pid = fork();
            //Se sono il figlio
            if (pid == 0) {
                printf("\n-Figlio %i partito-\n", i + 1);
                if (execv("figlio.x", arguments) < 0) {
                    printError("Errore: EXEC fallita\n");
                }
            }
            //Se sono il padre
            else {
                //Mando i dati da eseguire solo per un numero di proc. pari alle operazioni da eseguire
                if (i < ordine * ordine) {

                    //passo i, j e l'operazione da eseguire
                    datas.i = riga;
                    datas.j = colonna;

                    //Per aumentare righe e colonne
                    if (colonna == ordine - 1) {
                        riga++;
                        colonna = 0;
                    } else
                        colonna++;

                    datas.operazione = 'm'; //azione moltiplicazione
                    write(arrayPipe[i], &datas, sizeof(Data)); //scrivo nella pipe la riga, colonna e operazione da eseguire
                }
            }
        }
    }
    //Se il numero di operazioni è maggiore del numero di processi
    else{
        //Creo tanti processi quanti richiesto
        for (int i = 0; i < numeroProcessi; i++) {
            pipe(fd_pipe);
            arrayPipe[i] = fd_pipe[1]; //salvo la pipe in scrittura su un array
            char *str_contatore = malloc(sizeof(char)*20);
            sprintf(fd_pipe_read, "%i", fd_pipe[0]); //converto la pipe in lettura per poterla passare come argomento
            sprintf(str_contatore, "%i", i);
            char *arguments[] = {fd_pipe_read, argv[4], str_contatore, NULL}; //creo la coda argomenti
            free(str_contatore);
            int pid;
            pid = fork();
            //Se sono il figlio
            if (pid == 0) {
                printf("\n-Figlio %i partito-\n", i + 1);

                if (execv("figlio.x", arguments) < 0) {
                    printError("Errore EXEC!\n");
                }
            }
            //Se sono il padre
            else {
                //passo i, j e l'operazione da eseguire
                datas.i = riga;
                datas.j = colonna;

                //Per aumentare righe e colonne
                if (colonna == ordine - 1) {
                        riga++;
                        colonna = 0;
                    } else
                        colonna++;

                datas.operazione = 'm'; //azione moltiplicazione
                write(arrayPipe[i], &datas, sizeof(Data)); //scrivo nella pipe la riga, colonna e operazione da eseguire

                operazioniRimanenti--;
            }
        }
        //Finchè non ho finito le operazioni rimanenti
        while(operazioniRimanenti!=0)
        {
            //Aspetto la risposta di un figlio
            msgrcv(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1, 0);
            printf("numero processo riutilizzato per la moltiplicazione: %i\n", msg.numeroProcesso);

            //preparo i dati da inviare
            datas.i = riga;
            datas.j = colonna;

            //Per aumentare righe e colonne
            if (colonna == ordine - 1) {
                riga++;
                colonna = 0;
            } else
                colonna++;

            datas.operazione = 'm'; //azione moltiplicazione

            //scrivo nella pipe del proc. che ha risposto la riga, colonna e operazione da eseguire
            write(arrayPipe[msg.numeroProcesso], &datas, sizeof(Data));
            operazioniRimanenti--;
            fflush(stdout);
        }
    }

    //aspetto che i processi che ho lanciato abbiano termianto
    int terminati=0;
    //aspetto n processi che ho chiamato
    if(numeroProcessi<ordine*ordine)
        while(terminati!=numeroProcessi){
            msgrcv(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1, 0);
            terminati++;
        }
    //ordine*ordine processi, gli altri non sono attivi quindi non danno risposte
    else
        while(terminati!=ordine*ordine){
            msgrcv(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1, 0);
            terminati++;
        }


    operazioniRimanenti=ordine;
    if(numeroProcessi>=ordine) {
        for (int i = 0; i < ordine; i++) {
            //passo i
            datas.i = i;

            datas.operazione = 's'; //azione somma
            write(arrayPipe[i], &datas, sizeof(Data)); //scrivo nella pipe la riga, colonna e operazione da eseguire
            operazioniRimanenti--;
        }
    }
    else {
        for (int i = 0; i < numeroProcessi; i++) {
            //passo i
            datas.i = i;

            datas.operazione = 's'; //azione somma
            write(arrayPipe[i], &datas, sizeof(Data)); //scrivo nella pipe la riga, colonna e operazione da eseguire
            operazioniRimanenti--;
        }

        while (operazioniRimanenti != 0) {
            //Aspetto la risposta di un figlio
            msgrcv(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1, 0);
            printf("numero processo riutilizzato per la somma: %i\n", msg.numeroProcesso);
            fflush(stdout);
            //preparo i dati da inviare
            datas.i++;

            datas.operazione = 's'; //azione moltiplicazione

            //scrivo nella pipe del proc. che ha risposto la riga, colonna e operazione da eseguire
            write(arrayPipe[msg.numeroProcesso], &datas, sizeof(Data));
            operazioniRimanenti--;
            fflush(stdout);
        }
    }

    //aspetto che i processi che ho lanciato abbiano termianto
    terminati=0;
    //aspetto n processi che ho chiamato
    if(numeroProcessi<ordine)
        while(terminati!=numeroProcessi){
            msgrcv(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1, 0);
            terminati++;
        }
        //ordine*ordine processi, gli altri non sono attivi quindi non danno risposte
    else
        while(terminati!=ordine){
            msgrcv(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1, 0);
            terminati++;
        }

    char *s = malloc(sizeof(char) * 50);
    sprintf(s, "SOMMA TOTALE: %d\n", pointer_d[0]);
    myPrint(s);
    fflush(stdout);

    //Chiusura dei processi tramite messaggio "e" in pipe
    for(int i=0; i<numeroProcessi; i++)
    {
        Data datas;
        datas.operazione='e'; //Azione chiusura
        write(arrayPipe[i], &datas, sizeof(Data)); //Scrivo nella pipe
        fflush(stdout);
    }

    //Apro il file C
    fd_c = open(argv[3], O_RDWR);
    if(fd_c==-1) {
        printError("Errore: apertura del file 3 fallita\n");
    }

    s = malloc(sizeof(char)*20);
    int counter =0;
    //Per ogni cella scrivo in memoria
    for(int i=0; i<ordine*ordine; i++)
    {
        sprintf(s,"%d", pointer_mat_c[i]);
        if(write(fd_c, s, strlen(s))==-1) {
            free(s);
            printError("Errore scrittura\n");
        }

        if(counter==(ordine-1))    {
            write(fd_c, "\n", sizeof(char));
            counter =0;
        }
        else{
            write(fd_c, ";", sizeof(char));

            counter++;
        }
    }
    free(s);

    for(int i=0; i<numeroProcessi; i++)
    {
        wait(0);
    }

    closeIpcsFile();
    return 0;
}
