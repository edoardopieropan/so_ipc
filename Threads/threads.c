/*!
 * @file esercizio.c
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
#include <pthread.h>

#define SEMAPHORE 0
#define SIGINT 2

//VARIABILI GLOBALI
int fd_a;///<Il file descriptor del file contenente la matrice A
int fd_b;///<Il file descriptor del file contenente la matrice B
int fd_c;///<Il file descriptor del file dove scrivere la matrice C
int shmid_a;///<L'ID della zona di memoria per la matrice A
int shmid_b;///<L'ID della zona di memoria per la matrice B
int shmid_c;///<L'ID della zona di memoria per la matrice C
int shmid_d;///<L'ID della zona di memoria per il risultato della somma


int ordine;
int *pointer_mat_a, *pointer_mat_b, *pointer_mat_c, *pointer_d;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;


//Chiude i file descriptor
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
void closeAll(){
    //Chiudo i tre file
    close(fd_a);
    close(fd_b);
    close(fd_c);

    free(pointer_mat_a);
    free(pointer_mat_b);
    free(pointer_mat_c);
    free(pointer_d);

    pthread_mutex_destroy(&m);
}


void printError(char *stringa){
    write(2, stringa, strlen(stringa));
    closeAll();
    exit(1);
}

void myPrint(char *stringa) {
    if (write(1, stringa, strlen(stringa)) == -1){
        printError("Errore nella scrittura a video\n");
    }
}



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

    lseek(fd, 0, 0); //mi riposiziono all'inizio del file, offset = 0

    //se la matrice è quadrata ritorno il valore N della matrice NxN
    if(righe==colonne)
        return righe;

    //altrimenti ritorno -1
    else
        return -1;
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
void writeInMemory(int fd, int* shm_pointer){
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

    int status=lseek(fd, 0, 0); //mi riposiziono all'inizio del file, offset = 0
    if(status==-1)
    {
        printError("Errore: problema nella lettura del file\n");

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


//Intercetta la signal del CTR+C
/*!
 * Funzione chiamata una volta intercettata la signal del CTR+C
 * @param sig_num la signal intercettata
 */
void catch_int(int sig_num) {
    closeAll();
    exit(0);
}


struct Data{
    char operazione; ///<L'operazione da far compiere al figlio
    int i;///<La riga su cui operare
    int j;///<La colonna su cui operare
    int ID;
};


void *moltiplica(void *arg){
    struct Data *datas;
    datas=(struct Data *)arg;
    int id,i,j;
    id=datas->ID;
    i=datas->i;
    j=datas->j;

    int offset_i = i*ordine, offset_j = j;
    int riga[ordine];
    int somma=0;
    for (int c = 0; c < ordine; c++) {
        riga[c] = pointer_mat_a[c+offset_i];
    }
    int colonna[ordine];
    for (int c = 0; c < ordine; c++){
        colonna[c] = pointer_mat_b[(ordine*c)+offset_j];
    }
    for(int c=0; c<ordine; c++)
        somma+=riga[c]*colonna[c];

    int offset=offset_i+offset_j;
    pointer_mat_c[offset]=somma;

    char *s = malloc(sizeof(char) * 50);
    sprintf(s, "\nHo moltiplicato io, ID: %i\n", id);
    myPrint(s);
    fflush(stdout);
    free(s);
    pthread_exit(NULL);
}

void *functionSomma(void *arg) {
    int somma=0;
    struct Data *datas;
    datas=(struct Data *)arg;
    int id=datas->ID;
    int i=datas->i;
    int offset_i = i*ordine;
    for(int c=0; c<ordine; c++) {
        somma += pointer_mat_c[c + offset_i];
    }
    pthread_mutex_lock(&m);
    pointer_d[0]+=somma;
    pthread_mutex_unlock(&m);
    char *s = malloc(sizeof(char) * 50);
    sprintf(s, "\nHo sommato io, ID: %i\n", id);
    myPrint(s);
    fflush(stdout);
    free(s);
    fflush(stdout);
    pthread_exit(NULL);
}

//Argc: argument counter
//Argv: 0, nome programma; 1-2-..-n, i parametri passati
int main(int argc, char *argv[]) {
    signal(SIGINT, catch_int);

    if(argc != 5)
        printError("Errore: numero di argomenti non sufficiente..\nChiusura...\n");

    //ATTRIBUTI
    int n_mat_a, n_mat_b;
    ordine=atoi(argv[4]);
    size_t size;
    int riga=0, colonna=0;


    //Inizializzo le chiavi
    key_t key_A=ftok("/tmp", 'A'), key_B=ftok("/tmp", 'B'), key_C=ftok("/tmp", 'C'), key_D=ftok("/tmp", 'D');

    //APERTURA FILES
    //Apro il file A
    fd_a = open(argv[1], O_RDONLY);
    if(fd_a==-1) {
        myPrint("Errore: apertura del file 1 fallita\n");
        closeAll();
        exit(1);
    }
    n_mat_a=isSquare(fd_a);

    //Apro il file B
    fd_b = open(argv[2], O_RDONLY);
    if(fd_b==-1) {
        myPrint("Errore: apertura del file 2 fallita\n");
        closeAll();
        exit(1);
    }
    n_mat_b=isSquare(fd_b);

    //Se non hanno lo stesso ordine o non corrisponde con quello inserito dall'utente
    if(!matchingMatrix(n_mat_a, n_mat_b, ordine)) {
        printError("Chiusura...\n");
    }

    size=ordine*ordine*sizeof(int); //imposto la dimensione della matrice


    //Assegno le zone di memoria condivisa e ritorna il puntatore
    pointer_mat_a = malloc(ordine*ordine* sizeof(int));
    pointer_mat_b = malloc(ordine*ordine* sizeof(int));
    pointer_mat_c = malloc(ordine*ordine* sizeof(int));
    pointer_d = malloc(sizeof(int));

    //Scrivo in memoria le matrici
    writeInMemory(fd_a, pointer_mat_a);
    writeInMemory(fd_b, pointer_mat_b);

    struct Data datas[(ordine*ordine)];
    pthread_t thread[(ordine*ordine)];

    for(int i=0; i<ordine*ordine; i++)
    {
        datas[i].i = riga;
        datas[i].j = colonna;
        datas[i].ID = i;

        //Per aumentare righe e colonne
        if (colonna == ordine - 1) {
            riga++;
            colonna = 0;
        } else
            colonna++;
        if(pthread_create(&thread[i], NULL, (void *)moltiplica, &datas[i]))
            myPrint("Errore creazione thread\n");
    }
    for(int i=0; i<(ordine*ordine); i++)
    {
        pthread_join(thread[i], NULL);
    }

    for(int i=0; i<ordine; i++)
    {
        datas[i].i = i;
        datas[i].ID = i;

        if(pthread_create(&thread[i], NULL, (void *)functionSomma, &datas[i]))
            myPrint("Errore creazione thread\n");
    }

    for(int i=0; i<ordine; i++)
    {
        pthread_join(thread[i], NULL);
    }

    fd_c = open(argv[3], O_RDWR);
    if(fd_c==-1) {
        myPrint("Errore apertura del file 3!\n");
        closeAll();
        exit(1);
    }

    char *s = malloc(sizeof(char)*20);
    int counter =0;

    //Per ogni cella scrivo in memoria
    for(int i=0; i<ordine*ordine; i++)
    {
        sprintf(s,"%d", pointer_mat_c[i]);
        if(write(fd_c, s, strlen(s))==-1) {
            myPrint("Errore scrittura\n");
            closeAll();
            exit(1);
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
    *s = malloc(sizeof(char) * 50);
    sprintf(s, "SOMMA TOTALE: %d\n", pointer_d[0]);
    myPrint(s);
    fflush(stdout);
    free(s);
	
    closeAll();

    return 0;
}
