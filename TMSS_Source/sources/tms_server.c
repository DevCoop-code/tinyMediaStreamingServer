#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define TRUE 1
#define FALSE 0

#define BUF_SIZE 1024
#define SMALL_BUF 100

void request_handler(int clnt_sockfd, char* req_line);
void send_data(FILE* fp, char* ct, char* file_name);
char* content_type(char* file);
void send_error(FILE* fp);
void error_handling(char* message);

int main(int argc, char* argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    struct timeval timeout;
    fd_set reads, cpy_reads;
    char buf[BUF_SIZE];

    if (argc != 2) {
        printf("Usage : %s <port> \n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        error_handling("socket() error");
    }

    // Prevent the Binding Error
    int option;
    socklen_t optlen = sizeof(option);
    option = TRUE;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        error_handling("bind() error");
    }

    if (listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }

    FD_ZERO(&reads);
    FD_SET(serv_sock, &reads);
    int fd_max = serv_sock;
    printf("File Descriptor MAX: %d \n", fd_max);

    while (1) {
        cpy_reads = reads;
        
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;

        int fd_num = select(fd_max + 1, &cpy_reads, 0, 0, &timeout);
        if (fd_num == -1) {
            perror("select");
            error_handling("select error");
            break;
        }
        if (fd_num == 0) {
            // timeout
            continue;
        }

        for (int i = 0; i < fd_max + 1; i++) {
            if (FD_ISSET(i, &cpy_reads)) {      // 상태 변화가 있었던(수신된 데이터가 있는 소켓의) 파일 디스크립터를 찾고 있음
                if (i == serv_sock) {
                    socklen_t adr_sz = sizeof(clnt_adr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
                    FD_SET(clnt_sock, &reads);
                    if (fd_max < clnt_sock) {
                        fd_max = clnt_sock;
                    }
                    printf("Connected client: %d \n", clnt_sock);
                }
                else {      // Read Message
                    // 'i' is the clnt_sock file descriptor
                    char req_line[SMALL_BUF];
                    // int str_len = read(i, req_line, SMALL_BUF);
                    FILE* clnt_read;
                    clnt_read = fdopen(i, "r");
                    char* read = fgets(req_line, SMALL_BUF, clnt_read);
                    if (read == NULL || req_line == NULL) {
                        FD_CLR(i, &reads);
                        close(i);
                        printf("Disconnected client: %d \n \n", i);
                    } else {
                        request_handler(i, req_line);
                    }
                }
            }
        }
    }

    close(serv_sock);
    close(clnt_sock);
    return 0;
}

void request_handler(int clnt_sockfd, char* req_line) {
    FILE* clnt_write;

    char method[10];
    char ct[15];
    char file_name[30];

    clnt_write = fdopen(dup(clnt_sockfd), "w");
    
    printf("Request DATA: %s \n", req_line);

    // Check It is 'HTTP' protocol
    if (strstr(req_line, "HTTP/") == NULL) {
        printf("Request is not using HTTP Protocol \n");
        send_error(clnt_write);
        // fclose(clnt_read);
        // fclose(clnt_write);
        return;
    }

    strcpy(method, strtok(req_line, " "));
    strcpy(file_name, strtok(NULL, " "));
    strcpy(ct, content_type(file_name));

    char file[250];
    char* file_token = strtok(file_name, "/");
    strcat(file, file_token);
    
    while (file_token != NULL) {
        file_token = strtok(NULL, "/");
        if (file_token != NULL) {
            strcat(file, "/");
            strcat(file, file_token);
        }
    }
    printf("Request Message Line Information: method[%s], filename[%s], contenttype[%s] \n", method, file, ct);

    if (strcmp(method, "GET") != 0) {
        send_error(clnt_write);
        close(clnt_write);
        return;
    }

    send_data(clnt_write, ct, file);
}

void send_data(FILE* fp, char* ct, char* file_name) {
    char protocol[] = "HTTP/1.0 200 0K \r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[SMALL_BUF];
    char buf[BUF_SIZE];
    FILE* send_file;

    sprintf(cnt_type, "Content-type: %s \r\n\r\n", ct); //sprintf: 서식을 지정하여 문자열을 만들 수 있음
    send_file = fopen(file_name, "r");
    if (send_file == NULL) {
        send_error(fp);
        return;
    }

    /* 헤더 정보 전송 */
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);

    /* 요청 데이터 전송 */
    while(fgets(buf, BUF_SIZE, send_file) != NULL) {
        fputs(buf, fp);
        fflush(fp);
    }

    fflush(fp);

    // Setting the timeout
    sleep(2);

    shutdown(fileno(fp), SHUT_WR);
    fclose(fp);
}

char* content_type(char* file) {
    char extension[SMALL_BUF];
    char file_name[SMALL_BUF];
    strcpy(file_name, file);
    strtok(file_name, ".");
    strcpy(extension, strtok(NULL, "."));

    if (!strcmp(extension, "html") || !strcmp(extension, "htm")) {
        return "text/html";
    } else {
        return "text/plain";
    }
}

void send_error(FILE* fp) {
    char protocol[] = "HTTP/1.0 400 Bad Request\r\n";
    char server[] = "Server:Linux Web Server \r\n";
    char cnt_len[] = "Content-length:2048\r\n";
    char cnt_type[] = "Content-type:text/html\r\n\r\n";
    char content[] = "<html><head><title>NETWORK</title></head><body><font size=+5><br>오류 발생! 요청 파일명 및 요청 방식 확인!</font></body></html>";
    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_type, fp);
    fflush(fp);
}

void error_handling(char* buf) {
    fputs(buf, stderr);
    fputc('\n', stderr);
    exit(1);
}