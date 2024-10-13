#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <chrono>
#include "c37118.h"
#include "c37118configuration.h"
#include "c37118pmustation.h"
#include "c37118data.h"
#include "c37118header.h"
#include "c37118command.h"

#define SIZE_BUFFER 80000
#define TCP_PORT 4712

bool send_data_flag = false;


struct threadarg {
    int sock;
    DATA_Frame *my_data;
    bool send_data_flag;
};

// Function declarations
void doprocessing(int sock, CONFIG_1_Frame *myconf1, CONFIG_Frame *myconf2, DATA_Frame *my_data, HEADER_Frame *my_header);
void select_cmd_action(int sock, CMD_Frame *cmd, CONFIG_1_Frame *myconf1, CONFIG_Frame *myconf2, DATA_Frame *my_data, HEADER_Frame *my_header);
void *tx_data(void *threadarg);

// Graceful signal handler to clean up connections
void handle_sigint(int sig) {
    printf("Terminating server...\n");
    exit(0);
}

PMU_Station pmu ("PMU LSEE",666,false,true,true,true);

const int TIME_BASE = 1000000000;

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    signal(SIGINT, handle_sigint);  // Handle Ctrl+C for clean exit

    CONFIG_Frame *my_config2 = new CONFIG_Frame();
    CONFIG_1_Frame *my_config1 = new CONFIG_1_Frame();
    DATA_Frame *my_dataframe = new DATA_Frame(my_config2);
    HEADER_Frame *my_header = new HEADER_Frame("PMU VERSAO 1.0 LSEE");

    auto current_time = std::chrono::steady_clock::now().time_since_epoch().count();
    
    // Create config packet
    my_config2->IDCODE_set(7);
    my_config2->SOC_set(current_time/TIME_BASE);
    my_config2->FRACSEC_set(current_time%TIME_BASE);
    my_config2->TIME_BASE_set(TIME_BASE);
    my_config2->DATA_RATE_set(100);
    
    my_config1->IDCODE_set(7);
    my_config1->SOC_set(current_time/TIME_BASE);
    my_config1->FRACSEC_set(current_time%TIME_BASE);
    my_config1->TIME_BASE_set(TIME_BASE);
    my_config1->DATA_RATE_set(100);
    
    my_dataframe->IDCODE_set(7);
    my_dataframe->SOC_set(current_time/TIME_BASE);
    my_dataframe->FRACSEC_set(current_time%TIME_BASE);
 
    
    
    PMU_Station pmu2 ("PMU2 LSEE",667,false,false,false,false);

    
    pmu.PHASOR_add("VA",1,VOLTAGE);
    pmu.PHASOR_add("VB",1,VOLTAGE);
    pmu.PHASOR_add("VC",1,VOLTAGE);
    pmu.PHASOR_add("IA",1,CURRENT);
    pmu.PHASOR_add("IB",1,CURRENT);
    pmu.PHASOR_add("IC",1,CURRENT);

    pmu.FNOM_set(FN_50HZ);        
    pmu.CFGCNT_set(1);
    pmu.STAT_set(2048);


 
    my_config2->PMUSTATION_ADD(&pmu);


    my_config1->PMUSTATION_ADD(&pmu);

	
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    int on = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) {
        perror("ERROR on setsockopt");
        exit(1);
    }

    // Initialize socket structure
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = TCP_PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    listen(sockfd, 100);
    clilen = sizeof(cli_addr);

    // Main server loop
    while (true) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            continue;  // If there is an accept error, continue to accept new connections
        }

        // Handle client connection
        doprocessing(newsockfd, my_config1, my_config2, my_dataframe, my_header);
    }


    close(sockfd);
    return 0;
}

// For simulation purposes

const float PI = 3.14159265358979323846;
auto start = std::chrono::steady_clock::now();

std::complex<float> computePhasor(float magnitude, float frequency, float time) {
    // Calculate the angular frequency (omega)
    float omega = 2 * PI * frequency;
    
    // Calculate the phase angle based on time
    float phaseAngle = omega * time;
    
    // Compute the complex phasor value
    std::complex<float> phasor = magnitude * std::exp(std::complex<float>(0, phaseAngle));
    return phasor;
}

// Update PMU data before sending it to the PDC

void update_pmu(void *args) {
    struct threadarg *my_args = (struct threadarg *)(args);
    pmu.FREQ_set(50.5);
    pmu.DFREQ_set(1.2);
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed_time = now - start;

    printf("Elapsed Time: %f\n", elapsed_time.count());
    pmu.PHASOR_VALUE_set(computePhasor(220, 50.0, elapsed_time.count()),0);
    pmu.PHASOR_VALUE_set(computePhasor(220, 50.0, elapsed_time.count() + 0.005),1);
    pmu.PHASOR_VALUE_set(computePhasor(220, 50.0, elapsed_time.count() + 0.01),2);
    pmu.PHASOR_VALUE_set(computePhasor(1.6, 50.0, elapsed_time.count() + 0.003),3);
    pmu.PHASOR_VALUE_set(computePhasor(1.6, 50.0, elapsed_time.count() + 0.008),4);
    pmu.PHASOR_VALUE_set(computePhasor(1.6, 50.0, elapsed_time.count() + 0.013),5);
    
    auto current_time = now.time_since_epoch().count();
    my_args->my_data->SOC_set(current_time/TIME_BASE);
    my_args->my_data->FRACSEC_set(current_time%TIME_BASE);

    printf("SOC: %ld\n", my_args->my_data->SOC_get());
    printf("FRACSEC: %ld\n", my_args->my_data->FRACSEC_get());
}

void doprocessing(int sock, CONFIG_1_Frame *myconf1, CONFIG_Frame *myconf2, DATA_Frame *my_data, HEADER_Frame *my_header) {
    unsigned char buffer[SIZE_BUFFER];
    CMD_Frame *cmd = new CMD_Frame();
    pthread_t threads;
    struct threadarg t_arg;
    t_arg.sock = sock;
    t_arg.my_data = my_data;
    t_arg.send_data_flag = false;

    if (pthread_create(&threads, NULL, tx_data, (void *)&t_arg)) {
        perror("ERROR creating data transmission thread");
        close(sock);
        return;
    }

    while (true) {
        bzero(buffer, SIZE_BUFFER);
        int n = read(sock, buffer, SIZE_BUFFER);
        if (n <= 0) {
            // If n is 0, it means the client has closed the connection.
            if (n < 0) {
                perror("ERROR reading from socket");
            } else {
                printf("Client disconnected.\n");
            }
            break;  // Exit on error or disconnection
        }

        if (buffer[0] == A_SYNC_AA) {
            switch (buffer[1]) {
                case A_SYNC_CMD:
                    cmd->unpack(buffer);
                    select_cmd_action(sock, cmd, myconf1, myconf2, my_data, my_header);
                    break;
                default:
                    printf("Unknown Packet\n");
                    break;
            }
        }
    }

    // Clean up the thread
    pthread_cancel(threads);
    pthread_join(threads, NULL);
    close(sock);  // Close the socket after exiting the loop
}


void select_cmd_action(int sock, CMD_Frame *cmd, CONFIG_1_Frame *myconf1, CONFIG_Frame *myconf2, DATA_Frame *my_data, HEADER_Frame *my_header) {
    int n;
    unsigned short size;
    unsigned char *buffer;

    printf("Processing Socket Packet\n");
    switch (cmd->CMD_get()) {
        case 0x01:  // Disable Data Output
            send_data_flag = false;
            break;

        case 0x02:  // Enable Data Output
            printf("Enable Data Output\n");
            send_data_flag = true;
            break;

        case 0x03:  // Transmit Header Record Frame
            size = my_header->pack(&buffer);
            n = write(sock, buffer, size);
            free(buffer);
            buffer = NULL;
            if (n < 0) {
                printf("ERROR writing to socket, but continuing to run.\n");
            }
            break;

        case 0x04:  // Transmit Configuration #1 Record Frame
            size = myconf1->pack(&buffer);
            n = write(sock, buffer, size);
            free(buffer);
            buffer = NULL;
            if (n < 0) {
                printf("ERROR writing to socket, but continuing to run.\n");
            }
            break;

        case 0x05:  // Transmit Configuration #2 Record Frame
            size = myconf2->pack(&buffer);
            n = write(sock, buffer, size);
            free(buffer);
            buffer = NULL;
            if (n < 0) {
                printf("ERROR writing to socket, but continuing to run.\n");
            }
            break;

        default:
            break;
    }
}

void *tx_data(void *args) {
    struct threadarg *my_args = (struct threadarg *)(args);
    unsigned short size, n;
    unsigned char *buffer;

    while (1) {
        my_args->send_data_flag = send_data_flag;
        printf("my_args->send_data_flag: %d\n", my_args->send_data_flag);

        update_pmu(my_args);

        if (!my_args->send_data_flag) {
            continue;
        }

        buffer = NULL;
        size = my_args->my_data->pack(&buffer);
        n = write(my_args->sock, buffer, size);
        free(buffer);
        buffer = NULL;
        if (n < 0) {
            perror("ERROR writing to socket");
            pthread_exit(NULL);
        }

        usleep(1E6 / my_args->my_data->associate_current_config->DATA_RATE_get());
    }

    pthread_exit(NULL);
}
