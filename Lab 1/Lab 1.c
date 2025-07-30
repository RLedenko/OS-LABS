#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define __USE_XOPEN_EXTENDED 1
#include <signal.h>

const int acc_sig[] = {SIGTERM, SIGINT, SIGALRM, SIGUSR1, SIGUSR2};

int curr = 0;
int statev[6] = {0};
int timev[6] = {0};

void i_enable()
{
    for(int i = 0; i < 5; i++)
        sighold(acc_sig[i]);
}

void i_disable()
{
    for(int i = 0; i < 5; i++)
        sigrelse(acc_sig[i]);
}

void prekid()
{
    if(curr < 1 || curr > 5) return;
	while(timev[curr] > 0)
	{
		printf("SIG_%d", curr);
		for(int i = 0; i <= curr; i++)
			printf("\t");
		printf("deltaT = %ds\n", timev[curr]);
        timev[curr]--;
		sleep(1);
	}
    statev[curr] = 0;
	printf("KUCPOS\tZavrsen prekidni program signala: %d, vracam prethodni kontekst\n", curr);
    return;
}

void kucanski_poslovi(int signum)
{
    int s;
    for(s = 0; acc_sig[s] != signum; s++);
    signum = s + 1;

	printf("KUCPOS\tPrimljen signal prioriteta %d\n", signum);
    statev[signum] = 1;

	if(curr >= signum)
	{
		printf("KUCPOS\tVracam se u prekidni program signala: %d\n", curr);
		return;
	}
	
	while(1)
	{
		int i;
		for(i = 5; i > 0; i--)
            if(statev[i])
                break;
		if(i == 0)
			break;
		
		curr = i;
    	if(!timev[curr]) timev[curr] = 5;
		printf("KUCPOS\tPrelazim u prekidni program signala: %d\n", curr);
        i_disable();
        prekid();
        i_enable();
		curr = 0;
	}
}

int main()
{
	struct sigaction act;

	act.sa_handler = kucanski_poslovi;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	for(int i = 0; i < 5; i++)
		if(sigaction(acc_sig[i], &act, NULL) == -1)
		{
			perror("sigaction err");
			exit(1);
		}

	printf("Program (PID=%ld) zapoceo\n", (long)getpid());

	int i = 1, mainLoop = 1024;
	while(mainLoop--) 
	{
		printf("MAIN\titeracija %d\n", i++);
		sleep(1);
	}

	printf("Program (PID=%ld) zavrsio\n", (long)getpid());

	return 0;
}