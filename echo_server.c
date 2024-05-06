// TCP 에코 서버 프로그램

// 라이브러리 헤더 파일을 포함
#include "csapp.h"

// 선언부
void echo(int connfd); // 에코 함수 선언

// 메인 함수
int main(int argc, char **argv)
{

    // 변수 선언
    int listenfd; // 서버 소켓 디스크립터
    int connfd;   // 클라이언트 커넥션 소켓 디스크립터

    // 클라이언트 주소 정보를 담을 구조체와 크기 변수
    struct sockaddr_storage clientaddr;
    socklen_t clientlen;

    // 클라이언트 호스트정보를 저장할 버퍼
    char client_hostname[MAXLINE];
    char client_port[MAXLINE];

    // 명령행 인자 체크
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // 서버 소켓 오픈
    listenfd = Open_listenfd(argv[1]);

    // 무한 루프
    while (1)
    {

        // 클라이언트 접속 수락
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // 클라이언트 호스트정보 얻기
        Getnameinfo((SA *)&clientaddr, clientlen,
                    client_hostname, MAXLINE, client_port, MAXLINE, 0);

        // 연결 정보 출력
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

        // 에코 함수 호출
        echo(connfd);

        // 커넥션 종료
        Close(connfd);
    }

    // 프로그램 종료
    exit(0);
}

// 에코 함수 정의
void echo(int connfd)
{

    // 버퍼와 변수 선언
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    // 버퍼 입출력 초기화
    Rio_readinitb(&rio, connfd);

    // 무한 루프
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("Received %d bytes \n", (int)n);
        printf("Received %s \n", buf);
        Rio_writen(connfd, buf, n);
    }
}
