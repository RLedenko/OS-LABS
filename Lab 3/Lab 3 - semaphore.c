#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>

#define dretva void*
//#define DEBUG

#define X 10
#define Y 5

unsigned nIN, nWORK, nOUT, mem;
char **UMS, **IMS;

sem_t *BSEM, *BSEM_IZLAZNA, *OSEM;

int THREAD_FLAG = 1;

#ifdef DEBUG
void debugf()
{
    printf("DEBUG: \nUMS -> ");
    for(int i = 0; i < nWORK; i++)
    {
        for(int j = 0; j < mem; j++)
            printf("%c", UMS[i][j] <= 32 ? '_' : UMS[i][j]);
        printf(" ");
    }
    printf("\nIMS -> ");
    for(int i = 0; i < nOUT; i++)
    {
        for(int j = 0; j < mem; j++)
            printf("%c", IMS[i][j] <= 32 ? '_' : IMS[i][j]);
        printf(" ");
    }
    printf("\n\n\n");
}
#endif

#pragma region UTIL
char** gen2dArr(size_t x, size_t y)
{
    char **matr = (char**)malloc(x * sizeof(char*));
    for(size_t i = 0; i < x; i++)
        matr[i] = (char*)malloc(y * sizeof(char));
    
    for(size_t i = 0; i < x; i++)
        for(size_t j = 0; j < y; j++)
            matr[i][j] = 0;
    
    return matr;
}

void safe_exit(int sig)
{
    THREAD_FLAG = 0;

    int j = nWORK + 1;
    while(j--)
    {
        for(int i = 0; i < nWORK; i++)
            sem_post(&BSEM[i]);

        for(int i = 0; i < nWORK; i++)
            sem_post(&BSEM_IZLAZNA[i]);

        for(int i = 0; i < nWORK; i++)
            sem_post(&OSEM[i]);
    }
}
#pragma endregion

char dohvati_ulaz(int i)
{
    char r = abs(rand() % 26);
    if(rand() % 2)
        return 'a' + r;
    else
        return 'A' + r;
    return 'a';
}

int obradi_ulaz(int i, char u)
{
    return (i * u + rand()) % nWORK;
}

void obradi(int j, char p, char *res, int *t)
{
    if(p == 'z')
        *res = 'a';
    else if (p == 'Z')
        *res = 'A';
    else
        *res = p + 1;
    
    *t = (j * p + rand()) % nOUT;

    sleep(2 + rand() % 2);
}

int *ULAZ;

dretva ulazna(void *index)
{
    int id = *(int *)index;

    while(THREAD_FLAG)
    {
        char U = dohvati_ulaz(id);
        int T = obradi_ulaz(id, U);

        sem_wait(&BSEM[T]);
        
        UMS[T][ULAZ[T]] = U;
        ULAZ[T] = (ULAZ[T] + 1) % mem;
        
        int vtemp; sem_getvalue(&OSEM[T], &vtemp);
        if(vtemp < mem) sem_post(&OSEM[T]);
        
        /*int vtemp; sem_getvalue(&BSEM[T], &vtemp);
        if(!vtemp) */sem_post(&BSEM[T]);


        sleep(X);
    }


    return NULL;
}

int *OUT;

dretva radna(void *index)
{
    int id = *(int *)index;
    
    int IZLAZ = 0;

    while(THREAD_FLAG)
    {
        sem_wait(&OSEM[id]);
        sem_wait(&BSEM[id]);

        char P = UMS[id][IZLAZ];
        UMS[id][IZLAZ] = 0;
        IZLAZ = (IZLAZ + 1) % mem;

        char res; int t;
        obradi(id, P, &res, &t);

        /*int vtemp; sem_getvalue(&BSEM[id], &vtemp);
        if(!vtemp) */sem_post(&BSEM[id]);

        sem_wait(&BSEM_IZLAZNA[t]);
        IMS[t][OUT[t]] = res;
        OUT[t] = (OUT[t] + 1) % mem;
        sem_post(&BSEM_IZLAZNA[t]);

        sleep(2);   
    }

    return NULL;
}

dretva izlazna(void *index)
{
    int id = *(int *)index;
    
    int i = 0, j = 0;

    while (THREAD_FLAG)
    {
        sleep(Y);
        
        sem_wait(&BSEM_IZLAZNA[id]);

        if(IMS[id][i])
        {
            printf("IZLAZNA DRETVA (id = %d) - IMS[%d][%d] = %c(%d)\n", id, id, i, IMS[id][i], IMS[id][i]);

            IMS[id][!i ? mem - 1 : i - 1] = 0;

            j = i;
            i = (i + 1) % mem;
        }
        else
            printf("IZLAZNA DRETVA (id = %d) - IMS[%d][%d] = %c(%d)\n", id, id, j, IMS[id][j], IMS[id][j]);

        sem_post(&BSEM_IZLAZNA[id]);
    }

    return NULL;
}

int main()
{
    srand(time(NULL));
    signal(SIGINT, safe_exit);

    printf("Unesi redom broj ulaznih radnih i izlaznih dretvi (tim redoslijedom): ");
    scanf("%u %u %u", &nIN, &nWORK, &nOUT);
    printf("Unesi velicinu meduspremnika: ");
    scanf("%u", &mem);

    UMS = gen2dArr(nWORK, mem), IMS = gen2dArr(nOUT, mem);

    OUT = (int*)malloc(nOUT * sizeof(int));
    for(int i = 0; i < nOUT; i++) OUT[i] = 0;

    ULAZ = (int*)malloc(nWORK * sizeof(int));
    for(int i = 0; i < nWORK; i++) ULAZ[i] = 0;

    BSEM = (sem_t *)malloc((nWORK) * sizeof(sem_t));
    for(int i = 0; i < nWORK; i++)
        if(sem_init(&BSEM[i], 0, 1)) {printf("BSEM error\n"); return -1;}

    BSEM_IZLAZNA = (sem_t *)malloc((nOUT) * sizeof(sem_t));
    for(int i = 0; i < nOUT; i++)
        if(sem_init(&BSEM_IZLAZNA[i], 0, 1)) {printf("IZLAZNA error\n"); return -1;}

    OSEM = (sem_t *)malloc((nWORK) * sizeof(sem_t));
    for(int i = 0; i < nWORK; i++)
        if(sem_init(&OSEM[i], 0, 0)) {printf("OSEM error\n"); return -1;};

    pthread_t ulazne[nIN], radne[nWORK], izlazne[nOUT];
    for(size_t i = 0; i < nIN; i++)
    {
        int *var = malloc(sizeof(int));
        *var = i;
        if(pthread_create(&ulazne[i], NULL, ulazna, (void*)var)) {printf("Thread (ulazna) creation error\n"); return -1;};
    }
    
    for(size_t i = 0; i < nWORK; i++)
    {
        int *var = malloc(sizeof(int));
        *var = i;
        if(pthread_create(&radne[i], NULL, radna, (void*)var)) {printf("Thread (radna) creation error\n"); return -1;};
    }
    
    for(size_t i = 0; i < nOUT; i++)
    {
        int *var = malloc(sizeof(int));
        *var = i;
        if(pthread_create(&izlazne[i], NULL, izlazna, (void*)var)) {printf("Thread (izlazna) creation error\n"); return -1;};
    }
    
    while(THREAD_FLAG)
    #ifdef DEBUG
    {
        debugf();
        sleep(1);
    }
    #else
    ;
    #endif
    

    printf("\nIzvodenje prekinuto signalom, svi (moguci) sljedeći ispisi ispisnih dretvi su garbage data\nPocinje ciscenje memorije\n");

    for(size_t i = 0; i < nIN; i++)
        pthread_join(ulazne[i], NULL);
    
    for(size_t i = 0; i < nWORK; i++)
        pthread_join(radne[i], NULL);
    
    for(size_t i = 0; i < nOUT; i++)
        pthread_join(izlazne[i], NULL);

    free(OUT);
    free(ULAZ);
    for(int i = 0; i < nWORK; i++)
        free(UMS[i]);
    free(UMS);
    for(int i = 0; i < nOUT; i++)
        free(IMS[i]);
    free(IMS);
    for(int i = 0; i < nWORK; i++)
    {
        sem_destroy(BSEM + i);
        sem_destroy(OSEM + i);
    }
    for(int i = 0; i < nOUT; i++)
        sem_destroy(BSEM_IZLAZNA + i);
    free(BSEM);
    free(OSEM);
    free(BSEM_IZLAZNA);

    printf("\nMemorija ociscena, main zavrsava\n");
    
    return 0;
}