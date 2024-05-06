// TCP 에코 클라이언트 프로그램

// 헤더파일 포함
#include "csapp.h"

// main 함수
int main(int argc, char **argv)
{

    // 파일디스크립터, 호스트정보, 포트번호, 버퍼 정의
    int clientfd;
    char *host, *port, buf[MAXLINE];

    // 버퍼 입출력을 위한 rio 변수
    rio_t rio;

    // 명령행 인자 확인
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    // 인자에서 호스트정보와 포트번호 저장
    host = argv[1];
    port = argv[2];

    // 서버에 연결
    clientfd = Open_clientfd(host, port);

    // 입출력 버퍼 초기화
    Rio_readinitb(&rio, clientfd);

    // 사용자 입력을 서버로 보내고 에코된 값을 받는 루프
    while (Fgets(buf, MAXLINE, stdin) != NULL)
    {

        Rio_writen(clientfd, buf, strlen(buf));
        rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
    }

    // 연결 종료
    Close(clientfd);

    // 프로그램 종료
    exit(0);
}
