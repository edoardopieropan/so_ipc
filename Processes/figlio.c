/*!
 * @file figlio.c
 * Legge dal padre l'operazione da eseguire tramite pipe e una volta terminata scrive nella coda di messaggi comunicando il suo numero di processo (ID) \n
 * Sincronizza la scrittura della zona di memoria della somma con i semafori
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <string.h>

#define SEMAPHORE 0


void printError(char *stringa){
    if(write(2, stringa, strlen(stringa))==-1) {
        exit(1);
    }
}

void myPrint(char *stringa) {
    if (write(1, stringa, strlen(stringa)) == -1){
        printError("Errore nella scrittura a videon\n");
    }
}


//Alloca lo spazio in memoria per il semaforo
/*!
 * Alloca una zona di memoria per il semaforo
 * @param key la chiave del semaforo
 * @return l'ID della zona di memoria
 */
int allocateSemaphore(key_t key){
    int status;
    status=semget(key, 1, 0666);
    if(status==-1)
          printError("Errore: semget fallita\n");
  
    return status;
}

/*!
 * Wait del semaforo, decrementa di 1 il valore del semaforo
 * @param semid l'ID del semaforo
 * @param sem_number
 */
void waitSem(int semid, int sem_number){
    struct sembuf sembuf1;
    sembuf1.sem_op=-1;
    sembuf1.sem_flg=0;
    sembuf1.sem_num=sem_number;

    if((semop(semid, &sembuf1, 1))==-1)
    {
        printError("Errore semop\n");
    }
    return;
}

/*!
 * Signal del semaforo, aumenta di 1 il valore del semaforo
 * @param semid l'ID del semaforo
 * @param sem_number
 */
void signalSem(int semid, int sem_number){
    struct sembuf sembuf1;
    sembuf1.sem_op=1;
    sembuf1.sem_flg=0;
    sembuf1.sem_num=sem_number;

    if((semop(semid, &sembuf1, 1))==-1)
    {
        printError("Errore semop\n");
    }

    return;
}

/*!
 * Somma i valori di una riga della matrice C in un'unica zona di memoria, gestendo i problemi di sincronizzazione tramite semaforo
 * @param i la riga da sommare
 * @param ordine l'ordine della matrice
 * @param pointer_c il puntatore alla matrice C
 * @param pointer_d il puntatore alla zona di memoria contenente la somma
 * @param semid l'ID del semaforo
 */
void somma(int i, int ordine, int *pointer_c, int *pointer_d, int semid) {
    int somma=0;
    int offset_i = i*ordine;
    for(int c=0; c<ordine; c++)
        somma+=pointer_c[c+offset_i];
    waitSem(semid, SEMAPHORE);
    pointer_d[0]+=somma;
    signalSem(semid, SEMAPHORE);
}

/*!
 * Moltiplicazione di una riga i e colonna j di due matrici
 * @param i la riga della matrice A
 * @param j la colonna della matrice B
 * @param ordine l'ordine delle matrici
 * @param pointer_a il puntatore alla matrice A
 * @param pointer_b il puntatore alla matrice B
 * @param pointer_c il puntatore alla matrice C
 */
void moltiplica(int i, int j, int ordine, int *pointer_a,int *pointer_b, int *pointer_c) {
    int offset_i = i*ordine, offset_j = j;
    int riga[ordine];
    int somma=0;
    for (int c = 0; c < ordine; c++) {
        riga[c] = pointer_a[c+offset_i];
    }
    int colonna[ordine];
    for (int c = 0; c < ordine; c++){
        colonna[c] = pointer_b[(ordine*c)+offset_j];
    }
    for(int c=0; c<ordine; c++)
        somma+=riga[c]*colonna[c];

    int offset=offset_i+offset_j;
    pointer_c[offset]=somma;
}

/*!
 * Alloca una nuova zona di memoria condivisa
 * @param size quanta memoria riservare
 * @param key la chiave
 * @return -1 se fallisce
 * @return id della zona di memoria
 */
int allocateMemory(size_t size, key_t key){
    int status;
    status = shmget(key, size, 0666);
    //se è fallita
    if (status == -1)
    {
        printError("Errore: shmget fallita\n");
    }
    return status;
}

//Attach della zona di memoria, ritorna l'indirizzo della zona di memoria condivisa
int *attachMemory(int shmid){
    int *status;
    status = shmat(shmid, 0, 0);
    if(*status==-1)
    {
        printError("Errore: shmat fallita\n");
    }
    return status;
}

//Struttura che contiene i dati dei messaggi da inviare al padre
typedef struct{
    long mtype;
    int numeroProcesso;
}Message;

//Struttura che contiene le operazioni, la riga e la colonna
typedef struct{
    char operazione;
    int i;
    int j;
}Data;

//Alloca lo spazio in memoria per la coda di messaggu
int allocateMessage(key_t key){
    int status;
    status = msgget(key, 0666);
    if(status==-1)
    {
        printError("Errore: msgget fallita\n");
    }
    return status;
}

int main(int argc, char *argv[]) {

    //ATTRIBUTI
    int fd_pipe_read = atoi(argv[0]);
    int ordine = atoi(argv[1]);
    int numeroProcesso = atoi(argv[2]);
    int *pointer_mat_a, *pointer_mat_b, *pointer_mat_c, *pointer_d;
    int shmid_a, shmid_b, shmid_c, shmid_d, semid;
    size_t size=ordine*ordine*sizeof(int);
    int i,j;
    Data datas;
    char operazione;
    int idMessage;


    /*union semun {
        int                 val;
        struct semid_ds *   buf;
        unsigned short *    array;
        struct seminfo *    __buf;

    };*/


    //Inizializzo le chiavi
    key_t key_A=ftok("/tmp", 'A'), key_B=ftok("/tmp", 'B'), key_C=ftok("/tmp", 'C'), key_D=ftok("/tmp", 'D');
    key_t keyMessage=ftok("/tmp", 'M');

    key_t keySemaphore=ftok("/tmp", 'K');

    //Alloco due zone di memoria condivisa
    shmid_a = allocateMemory(size, key_A);
    shmid_b = allocateMemory(size, key_B);
    shmid_c = allocateMemory(size, key_C);
    shmid_d= allocateMemory(size, key_D);


    //Assegno le zone di memoria condivisa e ritorna il puntatore
    pointer_mat_a=attachMemory(shmid_a);
    pointer_mat_b=attachMemory(shmid_b);
    pointer_mat_c=attachMemory(shmid_c);
    pointer_d=attachMemory(shmid_d);


    //Allocazione coda di messaggi
    idMessage=allocateMessage(keyMessage);
    Message msg;
    msg.mtype=2;
    fflush(stdout);
    semid = allocateSemaphore(keySemaphore);
    fflush(stdout);
    union semun st_sem;

    //invio messaggio al padre tipo 2 così sono sicuro che tutti avevano creato le ipc
    msgsnd(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1);

    do {
        fflush(stdout);
        read(fd_pipe_read, &datas, sizeof(Data));
        operazione = datas.operazione;
        i = datas.i;
        j = datas.j;
        msg.mtype=1;
        fflush(stdout);
        switch (operazione) {
           case 'm': //se moltiplicazione
               moltiplica(i, j, ordine, pointer_mat_a, pointer_mat_b, pointer_mat_c);
               msg.numeroProcesso = numeroProcesso;
               msgsnd(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1);

               break;
           case 's': //se somma
                somma(i, ordine, pointer_mat_c, pointer_d, semid);
                msg.numeroProcesso = numeroProcesso;
                msgsnd(idMessage, (void *) &msg, sizeof(msg) - sizeof(msg.mtype), 1);
               break;
           default:
               break;
        }
    }while(operazione!='e'); //Finchè non ho un messaggio di chiusura dal padre

    shmdt(pointer_mat_a);
    shmdt(pointer_mat_b);
    shmdt(pointer_mat_c);
    shmdt(pointer_d);

    return 0;

}
