// main.cpp : Defines the entry point for the console application.
// My name: Anton Dementyev
// My student id: 2023638

#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <string>
#include <cstdlib>
#include <condition_variable>

using namespace std;

std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();

void InitClock()
{
	current_time = std::chrono::steady_clock::now();
}

void PrintTime_ms(std::string text)
{
	std::chrono::steady_clock::time_point new_time = std::chrono::steady_clock::now();
	std::cout << text << std::chrono::duration_cast<std::chrono::milliseconds>(new_time - current_time).count() << " (ms)" << std::endl;
}

#define NB_ISLANDS 50 //number of islands
#define NB_BRIDGES 100 //number of bridges
#define NB_TAXIS 20 //number of taxis
#define NB_PEOPLE 40 //number of people per island

class Bridge;
class Person;
class Taxi;
class Island;
class Semaphore;

Bridge *bridges;
Taxi *taxis;
Island *islands;

class Semaphore
{
private:
	int N;
	mutex m;
	condition_variable c;
public:
	Semaphore(int nb) { N = nb; };
	void P(int nb = 1) {
		std::unique_lock<std::mutex> lck(m);
		c.wait(lck, [this, nb](){ return N >= nb; });
		N -= nb;
	}; // Decrement the semaphore count by nb and wait if needed.
	void V(int nb = 1) {
		std::unique_lock<std::mutex> lck(m);
		N += nb;
		c.notify_one();
	}; // Increment the semaphore count by nb.
};


class Island
{
private:
	int nbPeople; //People that will take a taxi to travel somewhere
	int peopleDropped; //People that will take a taxi to travel somewhere
	Semaphore* snp;
	Semaphore* pdr;
public:
	int GetNbPeople() { return nbPeople; }
	int GetNbDroppedPeople() { return peopleDropped; }
	Island() { nbPeople = NB_PEOPLE; peopleDropped = 0; snp = new Semaphore(1); pdr = new Semaphore(1); };
	int GetOnePassenger()
	{
		snp->P();
		if (nbPeople - 1 >= 0) {
			nbPeople--;
			snp->V();
			return 1;
		}
		snp->V();
		return 0;
	}
	void DropOnePassenger()
	{
		pdr->P();
		peopleDropped++;
		pdr->V();
	}
};

class Bridge
{
private:
	int source, dest;
public:
	Bridge() 
	{ 
		source = rand() % NB_ISLANDS; 
		do 
			dest = rand() % NB_ISLANDS; 
		while (dest == source); 
	};
	int GetSource() { return source; };
	int GetDest() { return dest; };
	void SetSource(int v) { source=v; };
	void SetDest(int v) { dest=v; };
};

class Taxi
{
private:
	int location; //island location
	int dest[4] = { -1,-1,-1,-1 }; //Destination of the people taken; -1 for seat is empty
	int GetId() { return this - taxis; }; //a hack to get the taxi thread id; Better would be to pass id throught the constructor
	Semaphore* b;
	Semaphore* ifwd;
	Semaphore* ibwd;
public:
	Taxi() { location = rand() % NB_ISLANDS; b = new Semaphore(2); ifwd = new Semaphore(4); ibwd = new Semaphore(4); };

	void GetNewLocationAndBridge(int &location, int &bridge) 		//find a randomn bridge and returns the island on the other side;
	{
		int shift = rand() % NB_BRIDGES;
		for (int i = 0; i < NB_BRIDGES; i++)
		{
			if (bridges[(i + shift) % NB_BRIDGES].GetSource() == location)
			{
				location = bridges[(i + shift) % NB_BRIDGES].GetDest();
				bridge = (i + shift) % NB_BRIDGES;
				return;
			}
			if (bridges[(i + shift) % NB_BRIDGES].GetDest() == location)
			{
				location = bridges[(i + shift) % NB_BRIDGES].GetSource();
				bridge = (i + shift) % NB_BRIDGES;
				return;
			}
		}
	}

	void GetPassengers() //this function is already completed
	{ 
		int cpt = 0;
		for (int i = 0; i < 4; i++)
			if ((dest[i] == -1) && (islands[location].GetOnePassenger()))
			{
				cpt++;
				do
					dest[i] = rand() % NB_ISLANDS;  //generating the destinatio for the individual randomly
				while (dest[i] == location);
			}
		if (cpt > 0)
		{
			printf("Taxi %d has taken %d clients from island %d.\n", GetId(), cpt, location);
		}
	}

	void DropPassengers()
	{
		int cpt = 0;
		for (int i = 0; i < 4; i++)
		{
			if (i == location) {
				islands[location].DropOnePassenger();
				dest[i] = -1;
				cpt++;
			}
		}
		if (cpt > 0)
		{
			printf("Taxi %d has dropped %d clients on island %d.\n", GetId(), cpt, location);
		}
	}

	void CrossBridge() 
	{
		int bridge;
		GetNewLocationAndBridge(location,bridge);
		//Get the right to cross the bridge
		b->P();
		//printf("Taxi %d is crossing bridge %d \n", GetId(), bridge);
		b->V();
	}

	void CrossBridgeIncreasedThroughput() {
		int bridge;
		GetNewLocationAndBridge(location, bridge);
		//Get the right to cross the bridge
		if (bridges[bridge].GetSource() == location) {
			ifwd->P();
			//printf("Taxi %d is crossing bridge %d southbound lane \n", GetId(), bridge);
			ifwd->V();
		} 
		else if (bridges[bridge].GetDest() == location) {
			ibwd->P();
			//printf("Taxi %d is crossing bridge %d northbound lane \n", GetId(), bridge);
			ibwd->V();
		}
	}
};


//code for running the taxis
//Comment here on mutual exclusion and the condition
// *** Termination Criterion ***
// There's no need to put accessing nbDroppedPeople or more code inside a critical section, because:
// a) we aren't modifying the field nbDroppedPeople of an Island, but just  performing a read to get its current value;
// b) we don't care whether it's being accessed modified by other threads / taxis at the moment of calling GetNbDroppedPeople,
// because again, we simply need the value of it at the same exact moment, hence race condition is out of question here.
// *** ********************* ***
bool NotEnd()  //this function is already completed
{															
	int sum = 0;											
	for (int i = 0; i < NB_ISLANDS; i++)					
		sum += islands[i].GetNbDroppedPeople();				
	return sum != NB_PEOPLE * NB_ISLANDS;				
}															
													
void TaxiThread(int id)  //this function is already completed
{
	while (NotEnd())
	{
		taxis[id].GetPassengers();
		taxis[id].CrossBridge();
		taxis[id].DropPassengers();
	}
}

void RunTaxisUntilWorkIsDone()  //this function is already completed
{
	std::thread taxis[NB_TAXIS];
	for (int i = 0; i < NB_TAXIS; i++)
		taxis[i] = std::thread(TaxiThread, i);
	for (int i = 0; i < NB_TAXIS; i++)
		taxis[i].join();
}

//end of code for running taxis


void Init()
{
	bridges = new Bridge[NB_BRIDGES];
	for (int i = 0; i < NB_ISLANDS; i++) //Ensuring at least one path to all islands
	{
		bridges[i].SetSource(i);
		bridges[i].SetDest((i + 1) % NB_ISLANDS);
	}
	islands = new Island[NB_ISLANDS];
	taxis = new Taxi[NB_TAXIS];
}

void DeleteResources()
{
	delete[] bridges;
	delete[] taxis;
	delete[] islands;
}

int main(int argc, char* argv[])
{
	Init();
	InitClock();
	RunTaxisUntilWorkIsDone();
	printf("Taxis have completed!\n ");
	PrintTime_ms("Time multithreaded:");
	DeleteResources();
	return 0;
}

