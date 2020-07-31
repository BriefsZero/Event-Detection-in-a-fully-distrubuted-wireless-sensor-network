///////////////////////////////////////////////////////////
// James Schubach, 29743338, jsch0026@student.monash.edu //
///////////////////////////////////////////////////////////

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h> 
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include "encr_tea_omp.c"
#include <inttypes.h>
#include <omp.h> 

struct tuple {
	int node; //Node
	int val; //Value
};

struct event {
	int mainNode;
	int nodes[4];
	int val;
};

char * getIPAddrs() {
	//Function to get Ip address
	char hostbuffer[256]; 
    char *IPbuffer; 
    struct hostent *host_entry; 
    int hostname; 
  
    // To retrieve hostname 
    hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 

    // To retrieve host information 
    host_entry = gethostbyname(hostbuffer); 

    // To convert an Internet network 
    // address into ASCII string 
    IPbuffer = inet_ntoa(*((struct in_addr*) 
                           host_entry->h_addr_list[0])); 
	return IPbuffer;
}

int modulo(int x,int N){
	// Simple modulo function as C's '%' does work exactly like modulo
    return (x % N + N) %N;
}


/* Master */
void master_io() {
	double fullTimeS,fullTimeE;
	fullTimeS = MPI_Wtime();
	MPI_Status stat;

	int rank, size;
	char out[83];
	FILE *fp;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	int total = 0;
	double decryptAvg = 0;
	double timeStart,timeEnd;
	int nonEvent = 0;

	fp = fopen("log.txt", "w+");

	// Loops indefintely until told to exit
	while (1) {
		uint32_t strToRecv[200];
		// Checks for message incoming
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
		//If message has tag 10, then breaks loop
		if (stat.MPI_TAG == 10) {
			nonEvent++;
			break;
		}
		// Otherwise, it's an event
		else {
			total++;
 			MPI_Recv(&strToRecv, 200, MPI_UNSIGNED, stat.MPI_SOURCE, 1, MPI_COMM_WORLD, &stat);
			time_t event_t = time(NULL);
			struct tm *tm = localtime(&event_t);

			
			char out[sizeof(strToRecv)];
			timeStart = MPI_Wtime(); 
			//Decrypts the message sent
			char_decrypt(strToRecv, out, sizeof(strToRecv));
			timeEnd = MPI_Wtime();
			decryptAvg += timeEnd-timeStart;
			// Prints it to the log file
			fprintf(fp, "Time to decrypt: %f\n%s", timeEnd-timeStart, out);
			fprintf(fp, "Time Recieved: %sTotal Events so far: %d\n\n", asctime(tm), total);
		}
	}

	// Prints the summary to the log file then exits
	fullTimeE = MPI_Wtime();
	fprintf(fp, "Summary:\n");
	fprintf(fp, "Simulation completed Sucessfully!\nEvents Detected: %d\nTime Taken in sec: %f\n", total, fullTimeE-fullTimeS);
	fprintf(fp, "Average Time for a Decrypt: %f\nNon-event Messages: %d", (decryptAvg/total), nonEvent);
	fclose(fp);
	return;
}


void slave_io(MPI_Comm slaves)
{
	
    //Intialisation of MPI
	MPI_Status stat, stat2;
	MPI_Request request;
	int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	
	
   	char *ip = getIPAddrs();

	char in[83];
	uint32_t crypt[83];
    char out[83];
    

    //Intialisation of Values
    int adjNodes[4];
    int currRank;
    int adjNodeAm = 0;


	currRank = modulo((rank - 4), 20);
	//Finds adjacent nodes
	if (currRank < rank) {
		adjNodes[0] = currRank;
		adjNodeAm++;
	}
	else {
		adjNodes[0] = -1;
	}

	currRank = modulo((rank + 1), 4);
	if (currRank > modulo(rank, 4)) {
		adjNodes[1] = rank + 1;
		adjNodeAm++;
	}
	else {
		adjNodes[1] = -1;
	}

	currRank = modulo((rank + 4), 20);
	if (currRank > rank) {
		adjNodes[2] = currRank;
		adjNodeAm++;
	}
	else {
		adjNodes[2] = -1;
	}

	currRank = modulo((rank - 1), 4);
	if (currRank < modulo(rank, 4)) {
		adjNodes[3] = rank - 1;
		adjNodeAm++;
	}
	else {
		adjNodes[3] = -1;
	}
		
	int r = 15;
	time_t t;
	
	int flag;
	double start, end;
	int ranNum;
	struct tuple resArr[adjNodeAm];
	int i, j;
	struct tuple res;
	int tempBuf;
	int count;
	struct event eve;
	double currTime;

	double timeStart = MPI_Wtime();
	double timeEnd;
	while (r > 0) {
		flag = 1;
		if (rank == 0) {
			start = MPI_Wtime();
		}
		srand((unsigned) time(&t)+rank);
		while (1) {
			
			//Generating random value
			ranNum = modulo(rand(), 50);

			
			//Send val to adj nodes
			i = 0;
			for (; i<4; i++) {
				if (adjNodes[i] != -1) {
					MPI_Isend(&ranNum, 1, MPI_INT, adjNodes[i], 22, slaves, &request);

				}
			}
			//Recv values from adj nodes, stores it in a tuple which then stores in a tuple array
			struct tuple res;
			struct tuple resArr[adjNodeAm];
			int sizee;
			i = 0;
			for (; i<4; i++) {
				if (adjNodes[i] != -1) {
					MPI_Recv(&tempBuf, 1, MPI_INT, adjNodes[i], 22, slaves, &stat);

					res.node = adjNodes[i];
					res.val = tempBuf;
					resArr[i] = res;
				}
				else {
					res.val = -1;
					resArr[i] = res;
				}
			}

			
			MPI_Barrier(slaves);
			//Check for an event
			i = 0;
			count = 1;
			struct event eve;
			eve.mainNode = rank;
			eve.val = ranNum;
			
			char str[200];
			if (adjNodeAm >= 2) {
				j = 0;
				//Loops through possible adjacent nodes, checks to see if values are equal
				for (; j<4; j++) {
					//Increases count if they are
					if (eve.val == resArr[j].val) {
						count++;
						eve.nodes[j] = resArr[j].node;
					}
					//Sets node to minus -1 otherwise
					else {
						eve.nodes[j] = -1;
					}
					//If three nodes have the same value, triggers an event
					if (count > 3) {
						int index = 0;
						for (int x = 0; x<4; x++) {
							if (eve.nodes[x] >= 0) {
								index += snprintf(&str[index], 200-index, "%d, ", eve.nodes[x]);
							}
						}
						time_t event_t = time(NULL);
						struct tm *tm = localtime(&event_t);
						char strToSend[200];
						sprintf(strToSend, "Event triggered at Sensor: %d\nNodes: %s\nValue is: %d\nNetwork Address: %s\nTime Sent: %s", eve.mainNode, str, eve.val, ip, asctime(tm));
						uint32_t crypt[sizeof(strToSend)];
						// Encrypts event then sends it off
						char_encrypt(strToSend, crypt, sizeof(strToSend));
						MPI_Send(crypt, 200, MPI_UNSIGNED, 20, 1, MPI_COMM_WORLD);
						break;
					}
				}
			}

			//Ring communication
			MPI_Barrier(slaves);
			if (rank == 0) {
				end = MPI_Wtime();
				currTime = end-start;
				//Sends message to node next to it letting know the current time
				MPI_Send(&currTime, 1, MPI_DOUBLE, rank+1, 25, MPI_COMM_WORLD);
				if (currTime > 1) {
					//Breaks loop if current time has been a second
					break;
				}
			}
			else {
				//Recieves time and then sends it off to next node
				MPI_Recv(&currTime, 1, MPI_DOUBLE, rank-1, 25, MPI_COMM_WORLD, &stat);
				if (rank+1 != 20) {
					MPI_Send(&currTime, 1, MPI_DOUBLE, rank+1, 25, MPI_COMM_WORLD);
				}
				//Breaks loop if it has been a second
				if (currTime > 1) {
					break;
				}
			}
			MPI_Barrier(slaves);

		}
		r--;
	}
	// Lets base station know that it's time to exit
	MPI_Barrier(slaves);
	if (rank == 0) {
		MPI_Isend(&flag, 1, MPI_INT, 20, 10, MPI_COMM_WORLD, &request);
	}
}

int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm slaves;
	MPI_Comm_split( MPI_COMM_WORLD, ( rank == 20 ), rank, &slaves);

	if (rank == 20) {
		master_io();
	} 
	else {
		slave_io(slaves);
	}

    MPI_Finalize();
	return(0);
}