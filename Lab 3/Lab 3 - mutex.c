#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>

#define dretva void*
int THREAD_FLAG = 1;

#pragma region LIST

typedef struct node
{
    int data;
    struct node *next;
} node;
typedef struct node list;

void pop(int pos, node **head)
{
    node *itter = *head;
    if(pos == 0)
    {
        *head = itter->next;
        return;
    }
    for(int i = 0; i < pos - 1; i++)
        itter = itter->next;
    node *temp = itter->next;
    itter->next = temp->next;
    free(temp);
}

void push_start(double val, node **head)
{
    node *temp = (node *)malloc(sizeof(node));
    temp->data = val;
    temp->next = *head;
    if(*head != NULL) temp->next = *head;
    *head = temp;
}

void push_end(double val, node **head)
{
    if(*head == NULL)
    {
        *head = (node *)malloc(sizeof(node));
        (*head)->data = val;
        (*head)->next = NULL;
        return;
    }
    node *itter = *head;
    while(itter->next != NULL)
        itter = itter->next;
    node *temp = (node *)malloc(sizeof(node));
    itter->next = temp;
    temp->data = val;
    temp->next = NULL;
}

void insert(int val, int pos, node **head)
{
    node *temp = (node *)malloc(sizeof(node));
    temp->data = val;
    temp->next = NULL;
    if(pos == 0)
    {
        temp->next = *head;
        *head = temp;
        return;
    }
    node *itter = *head;
    for(int i = 0; i < pos - 1; i++)
        itter = itter->next;
    temp->next = itter->next;
    itter->next = temp;
}

void reverse(node **head)
{
    node *temp = *head;
    *head = (*head)->next;
    node *hold = *head;
    *head = (*head)->next;
    temp->next = NULL;
    while((*head)->next != NULL)
    {
        hold->next = temp;
        temp = hold;
        hold = *head;
        *head = (*head)->next;
    }
    hold->next = temp;
    (*head)->next = hold;
}

void sort(node **head)
{
    int n = 0;
    node *itter = *head;
    while(itter != NULL)
    {
        n++;
        itter = itter->next;
    }
    int *_vals = (int*)malloc(n * sizeof(int));
    itter = *head;
    for(int i = 0; i < n; i++)
    {
        *(_vals + i) = itter->data;
        itter = itter->next;
    }
    int min_idx, j;
    for (int i = 0; i < n-1; i++)
    {
        min_idx = i;
        for (j = i+1; j < n; j++)
            if (*(_vals + j) < *(_vals + min_idx))
                min_idx = j;
        if(min_idx != i)
        {
            double t = *(_vals + i);
            *(_vals + i) = *(_vals + min_idx);
            *(_vals + min_idx) = t;
        }
    }
    itter = *head;
    for(int i = 0; i < n; i++)
    {
        itter->data = *(_vals + i);
        itter = itter->next;
    }
    free(_vals);
}

int element(int pos, node **head)
{
    node *itter = *head;
    for(int i = 0; i < pos; i++)
        itter = itter->next;
    return itter->data;
}

#pragma endregion LIST

list *head = NULL;
size_t lsize = 0;

pthread_mutex_t mtx;
pthread_cond_t cita, pise, brise;

int read_a = 0, read_w = 0, write_a = 0, write_w = 0, delete_a = 0, delete_w = 0;

void safe_exit(int sig)
{
    THREAD_FLAG = 0;
}

dretva citac(void *index)
{
    int id = *(int*)index;

    while(THREAD_FLAG)
    {
        pthread_mutex_lock(&mtx);
        if(!lsize) {pthread_mutex_unlock(&mtx); continue;} //ako nema što za čitati, izađi iz monitora
        int idx = rand() % lsize;
        printf("Citac (id: %d) pokusava citati vrijdnost na %d-toj poziciji u listi\n", id, idx);

        read_w++;
        while(delete_a + delete_w > 0)
            pthread_cond_wait(&cita, &mtx);
        read_a++;
        read_w--;

        int val = element(idx, &head);

        printf("Citac (id: %d) je procitao sljedece: list[%d] = %d\n", id, idx, val);
        pthread_mutex_unlock(&mtx);

        sleep(5 + rand() % 6);

        pthread_mutex_lock(&mtx);
        read_a--;
        if(read_a == 0 && read_w > 0)
            pthread_cond_broadcast(&cita);
        printf("Citac (id: %d) vise ne koristi listu.\n", id);
        pthread_mutex_unlock(&mtx);
    }

    return NULL;
}

dretva pisac(void *index)
{
    int id = *(int*)index;

    while(THREAD_FLAG)
    {
        int val = 1 + rand() % 99;

        pthread_mutex_lock(&mtx);
        printf("Pisac (id: %d) generira vrijednost %d i pokusava ju upisat u listu.\n", id, val);
        write_w++;
        while(write_a + delete_a + delete_w > 0)
            pthread_cond_wait(&pise, &mtx);
        write_a++;
        write_w--;

        push_end(val, &head);
        lsize++;
        printf("Pisac (id: %d) upisuje vrijednost %d u listu.\n", id, val);

        write_a--;
        if(write_a == 0 && write_w > 0)
            pthread_cond_broadcast(&pise);
        printf("Pisac (id: %d) vise ne koristi listu.\n", id);
        pthread_mutex_unlock(&mtx);

        sleep(2 + rand() % 6);
    }

    return NULL;
}

dretva brisac(void *index)
{
    int id = *(int*)index;

    while(THREAD_FLAG)
    {

        pthread_mutex_lock(&mtx);
        if(!lsize) {pthread_mutex_unlock(&mtx); continue;} //ako nema što za brisati, izađi iz monitora
        int idx = rand() % lsize;
        printf("Brisac (id: %d) pokusava brisati vrijdnost na %d-toj poziciji u listi\n", id, idx);

        delete_w++;
        while(delete_a > 0)
            pthread_cond_wait(&brise, &mtx);
        delete_a++;
        delete_w--;

        pop(idx, &head);
        lsize--;
        sleep(1);

        printf("Brisac (id: %d) je izbrisao sljedece: list[%d]\n", id, idx);
        
        delete_a--;
        if(delete_a == 0 && delete_w > 0)
            pthread_cond_broadcast(&brise);
        printf("Brisac (id: %d) vise ne koristi listu.\n", id);
        pthread_mutex_unlock(&mtx);
        
        sleep(5 + rand() % 6);
    }

    return NULL;
}

int main()
{
    srand(time(NULL));
    signal(SIGINT, safe_exit);

    pthread_mutex_init(&mtx, NULL);

    pthread_cond_init(&pise, NULL);
    pthread_cond_init(&cita, NULL);
    pthread_cond_init(&brise, NULL);

    int nREAD = 8 + rand() % 9,
        nWRITE = 3 + rand() % 4,
        nDELETE = 1 + rand() % 3;

    printf("Generirani brojevi pojedinih dretvi:\n\tcitaci - %d\n\tpisaci - %d\n\tbrisaci - %d\n", nREAD, nWRITE, nDELETE);
    sleep(2); 

    pthread_t citaci[nREAD], pisaci[nWRITE], brisaci[nDELETE];
    
    for(size_t i = 0; i < nWRITE; i++)
    {
        int *var = (int*)malloc(sizeof(int));
        *var = i;
        pthread_create(&pisaci[i], NULL, pisac, (void*)var);
    }

    sleep(5); //Vrijeme da pisaci napune listu
    
    for(size_t i = 0; i < nREAD; i++)
    {
        int *var = (int*)malloc(sizeof(int));
        *var = i;
        pthread_create(&citaci[i], NULL, citac, (void*)var);
    }

    for(size_t i = 0; i < nDELETE; i++)
    {
        int *var = (int*)malloc(sizeof(int));
        *var = i;
        pthread_create(&brisaci[i], NULL, brisac, (void*)var);
    }

    while(THREAD_FLAG)
    {
#ifdef DEBUG
        node *itter = head;
        while(itter != NULL)
        {
            printf("%d ", itter->data);
            itter = itter->next;
        }
        printf("\n");
        sleep(5);
#endif
    }

    printf("Zapocinje ciscenje memorije, moguci sljedeci ispisi su garbage data\n");

    for(size_t i = 0; i < nREAD; i++)
        pthread_join(citaci[i], NULL);
    
    for(size_t i = 0; i < nWRITE; i++)
        pthread_join(pisaci[i], NULL);
    
    for(size_t i = 0; i < nDELETE; i++)
        pthread_join(brisaci[i], NULL);

    pthread_mutex_destroy(&mtx);

    pthread_cond_destroy(&pise);
    pthread_cond_destroy(&cita);
    pthread_cond_destroy(&brise);

    printf("Memorija ociscena\n");
    return 0;
}