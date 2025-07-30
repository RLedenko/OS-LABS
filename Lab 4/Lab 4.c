#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>

#define dretva void*
//#define DEBUG
#define PROCESS_MAX_MEMORY 8192

typedef struct thread_info
{
    uint64_t id;
    uint64_t duration;
    uint64_t memstart;
    uint64_t pages;
    uint64_t relative_memstart;
    uint64_t disk_region;
} thread_info;

#pragma region LIST

typedef struct node
{
    thread_info data;
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

void push_end(thread_info val, node **head)
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

#pragma endregion LIST

unsigned char pages[1024], memory[65336], disk[1048576], RUN = 1;
uint64_t id = 0;
list *processes = NULL;
node *itter;
unsigned optimal, sim_delay;

uint64_t getid(uint64_t idx)
{
    return ((uint64_t)disk[idx]) << 56 + ((uint64_t)disk[idx + 1]) << 48 + ((uint64_t)disk[idx + 2]) << 40 + ((uint64_t)disk[idx + 3]) << 32 + 
           ((uint64_t)disk[idx + 4]) << 24 + ((uint64_t)disk[idx + 5]) << 16 + ((uint64_t)disk[idx + 6]) <<  8 + ((uint64_t)disk[idx + 7]);
}

void clear_process_disk_data(uint64_t id)
{
    for(uint64_t i = 0; i < 1048576; i++)
    {
        if(getid(i) == id)
        {
            for(uint64_t j = 0; j < 8; j++)
                disk[i + j] = 0;
            i += 8;
            while(disk[i])
            {
                disk[i] = 0;
            }
        }
    }
}

void remove_ended()
{
    node *itter = processes;
    uint64_t i = 0;
    while(itter)
    {
        if(itter->data.duration) {itter=itter->next; i++; continue;}

        for(int j = itter->data.memstart; j < itter->data.memstart + itter->data.pages; j++)
            pages[j] = 0;
        printf("Proces (id: %lu) je zavrsio sa izvodenjem, oslobadam %lu stranica\n", itter->data.id, itter->data.pages);
        
        clear_process_disk_data(itter->data.id);
        itter = itter->next;
        pop(i, &processes);
    }
}

void randomize_memory(uint64_t begin, uint64_t end)
{
    for(uint64_t i = begin; i < end; i++)
        memory[i] = 1 + rand() % 254;
}

int store_to_disk(thread_info *data, uint64_t p)
{
    uint64_t size = data->pages * 64ULL + 10;

    for(uint64_t i = 0; i < 1048576; i++)
    {
        if(getid(i) == data->id)
        {
            data->disk_region = disk[i + 8];
            data->relative_memstart = disk[i + 8] * data->pages;
            i+=9;
            for(uint64_t j = data->memstart; j < data->memstart + data->pages * 64ULL; j++, i++)
                memory[j] = disk[i];
            return 0;
        }
        else
            while(disk[++i]);
    }

    for(uint64_t i = 0; i < 1048576 - size; i++)
    {
        char found = 1;
        for(uint64_t j = i; j < i + size; j++)
            if(disk[j])
            {
                found = 0;
                break;
            }
        if(found)
        {
            disk[i+1] = data->id >> 56;
            disk[i+2] = data->id >> 48;
            disk[i+3] = data->id >> 40;
            disk[i+4] = data->id >> 32;
            disk[i+5] = data->id >> 24;
            disk[i+6] = data->id >> 16;
            disk[i+7] = data->id >>  8;
            disk[i+8] = data->id;
            disk[i+9] = p;
            i+=10;
            for(uint64_t j = data->memstart; j < data->memstart + data->pages * 64ULL; j++, i++)
                disk[i+1] = memory[j] ? memory[j] : 0xFF;
            return 0;
        }
    }
    return 1;
}

void load_from_disk(thread_info *data, uint64_t p)
{
    for(uint64_t i = 0; i < 1048576; i++)
    {
        if(getid(i) == data->id && disk[i + 8] == p)
        {
            data->disk_region = disk[i + 8];
            data->relative_memstart = disk[i + 8] * data->pages * 64ULL;
            i+=9;
            for(uint64_t j = data->memstart; disk[i]; j++, i++)
                memory[j] = disk[i];
            return;
        }
        else
            while(disk[i++]);
    }

    for(uint64_t j = data->memstart; j < data->memstart + data->pages * 64ULL; j++)
        memory[j] = 0;
    data->disk_region++;
}

void safe_exit(int sig)
{
    printf("\nPrimljen sigint, simulacija zavrsava\n.");
    RUN = 0;
}

int main()
{
    srand(time(NULL));
    signal(SIGINT, safe_exit);

    printf("Unesi vremenski razmak izmedu dva koraka simulacije (u sekundama): ");
    scanf("%u", &sim_delay);

    while(RUN)
    {
        sleep(sim_delay);
#ifdef DEBUG
        system("clear");
        printf("\n\nMemorija:\n");
        for(int i = 0; i < 16; i++)
        {
            for(int j = 0; j < 64; j++)
                printf("%c", pages[(i << 6) + j] ? 'X' : '_');
            printf("\n");
        }

        printf("\n\nDisk:\n");
        for(int i = 0; i < 64; i++)
        {
            for(int j = 0; j < 64; j++)
                printf("%c", disk[(i << 6) + j] ? 'X' : '_');
            printf("\n");
        }
#endif
        if(processes)
        {
            node *itter = processes;
            while(itter)
            {
                if(itter->data.duration) itter->data.duration--;
                itter = itter->next;
            }
            remove_ended();
        }

        itter = processes;
        while(itter)
        {
            uint64_t adr = rand() % 65536;
            if(!(rand() % 5))
            {
                if(rand() % 2)
                {
                    printf("Proces (id: %lu) cita sa relativne memorijske adrese 0x%lx (realna adresa: 0x%lx)\n", itter->data.id, adr, itter->data.memstart + adr);
                    if(adr <= itter->data.pages * 64ULL)
                        printf("Procitana vrijednost iznosi %u\n\n", memory[itter->data.memstart + adr]);
                    else
                    {
                        if(store_to_disk(&(itter->data), itter->data.disk_region))
                        {
                            printf("Segmantacijska greska, terminiram proces\n\n");
                            itter->data.duration = 0;
                        }
                        printf("Ucitavanje stranica sa diska\n");
                        load_from_disk(&(itter->data), adr / (itter->data.pages * 64ULL));
                        printf("Ucitano sa diska, procitana vrijednost iznosi %u\n\n", memory[itter->data.memstart + adr]);
                    }
                }
                else
                {
                    unsigned char value = 1 + rand() % 254;
                    printf("Proces (id: %lu) upisuje vrijednost %u na relativnu memorijsku adresu 0x%lx (realna adresa: 0x%lx) \n", itter->data.id, value, adr, itter->data.memstart + adr);
                    if(adr >= itter->data.relative_memstart && adr <= itter->data.relative_memstart + itter->data.pages * 64ULL)
                    {
                        memory[itter->data.memstart + adr % (itter->data.pages * 64ULL)] = value;
                        printf("Vrijednost upisana\n\n");
                    }
                    else
                    {
                        if(store_to_disk(&(itter->data), itter->data.disk_region))
                        {
                            printf("Segmantacijska greska, terminiram proces\n\n");
                            itter->data.duration = 0;
                        }
                        printf("Ucitavanje stranica sa diska\n");
                        load_from_disk(&(itter->data), adr / (itter->data.pages * 64ULL));
                        memory[itter->data.memstart + adr % (itter->data.pages * 64ULL)] = value;
                        printf("Ucitano sa diska, procitana vrijednost upisana\n\n");
                    }
                }
            }
            itter = itter->next;
        }

        if(id && ((1 + rand() % 20) < 5)) 
            continue;

        uint64_t p_mem = 16 + rand() % (PROCESS_MAX_MEMORY - 15);
        printf("Proces (id: %lu) se pokrece i zahtjeva %lu okteta memorije.\n", ++id, p_mem);
        uint64_t p_pages = p_mem / 64 + ((p_mem % 64) ? 1 : 0);
        uint64_t i, unable_to_find = 0;
        for(i = 0; i <= 1024 - p_pages; i++)
        {
            if(i == 1024 - p_pages)
            {
                unable_to_find = 1;
                break;
            }

            char found_flag = 1;
            for(uint64_t j = i; j < p_pages + i; j++)
                if(pages[j])
                {
                    found_flag = 0;
                    break;
                }
            if(found_flag)
            {
                for(uint64_t j = i; j < p_pages + i; j++)
                    pages[j] = 1;
                break;
            }   
        }

        if(!unable_to_find)
            printf("Pronasao sam prostor u memoriji za proces (id: %lu), pocetak memorije procesa je na %lu-toj stanici i zauzima %lu stranica\n\n", id, i, p_pages);
        else
        {
            printf("Memorija je ili prepuna ili fragmentirana do te mjere da proces ne može stati u nju. Proces se odbacuje.\n");
            continue;
        }

        randomize_memory(i, p_pages * 64);
        thread_info _process = {id, 1 + rand() % 19, i, p_pages, 0, 0};
        push_end(_process, &processes);
        printf("Proces (id: %lu) pocinje izvodenje\n\n", id);
    }

    itter = processes;
    node *prev;
    while(itter)
    {
        prev = itter;
        itter = itter->next;
        free(prev);
    }

#ifdef DEBUG
    for(int i = 0; i < 1048576; i++)
        printf("%d ", disk[i]);
#endif

    return 0;
}