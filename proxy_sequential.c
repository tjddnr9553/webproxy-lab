#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000 // 최대 캐시 크기
#define MAX_OBJECT_SIZE 102400 // 최대 객체 크기

void doit(int fd);
void parse_uri(char *uri, char *hostname, char *pathname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, clientfd;                // 서버 및 클라이언트 소켓 파일 디스크립터
  char hostname[MAXLINE], port[MAXLINE]; // 클라이언트 호스트 네임(IP) 및 포트번호
  socklen_t clientlen;                   // 클라이언트 주소 구조체 크기
  struct sockaddr_storage clientaddr;    // 클라이언트 주소 구조체

  // 명령행 인수 확인
  if (argc != 2)
  { // 인수 개수가 2가 아니면 오류 메시지 출력
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 클라이언트 연결 수신 소켓 생성
  listenfd = Open_listenfd(argv[1]);
  printf("%s\n", argv[1]);

  // 클라이언트 요청 수락 및 처리
  while (1)
  {
    clientlen = sizeof(clientaddr);                                                 // 클라이언트 주소 구조체 크기 설정
    clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);                     // 연결 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 호스트네임 및 포트번호 추출
    printf("Accepted connection from (%s, %s)\n", hostname, port);                  // 연결 확인 메시지 출력
    doit(clientfd);                                                                 // 클라이언트 요청 처리 함수 호출
    Close(clientfd);                                                                // 연결 종료 및 소켓 닫음
  }
}

/* 프록시 서버의 핵심 동작을 담당하는 함수 */
// 클라이언트로부터 요청을 받아들여 처리하고, 원격 서버에 전달하여 응답을 받아 클라이언트에게 다시 전송하는 함수
void doit(int clientfd)
{
  int serverfd;                                             // 서버 소켓 디스크립터
  char request_buf[MAXLINE], response_buf[MAX_OBJECT_SIZE]; // 요청과 응답을 저장할 버퍼
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE];        // 요청 메소드, URI, 경로
  char hostname[MAXLINE], port[MAXLINE];                    // 호스트명, 포트
  rio_t request_rio, response_rio;                          // 클라이언트로부터의 요청과 서버로부터의 응답을 처리하기 위한 리오 버퍼

  /* 클라이언트의 요청 읽기 */
  Rio_readinitb(&request_rio, clientfd);             // 클라이언트 소켓 디스크립터를 리오 버퍼에 연결
  Rio_readlineb(&request_rio, request_buf, MAXLINE); // 클라이언트로부터 요청 라인을 읽음
  printf("Request header: %s\n", request_buf);       // 디버깅용 요청 헤더 출력

  /* 요청 메소드, URI 읽기 */
  sscanf(request_buf, "%s %s", method, uri); // 요청 라인에서 메소드와 URI를 추출

  /* URI 파싱하여 호스트명, 포트, 경로 추출 */
  parse_uri(uri, hostname, port, path); // URI에서 호스트명, 포트, 경로를 파싱하여 변수에 저장

  printf("uri: %s\n", uri); // 디버깅용 URI 출력

  /* 요청 수정: 새로운 요청 구성 */
  // 프록시에서 엔드 서버로 보낼 헤더를 구성(Host, Connection, Proxy-Connection은 필수, User-Agent는 권장 사항)
  sprintf(request_buf, "%s /%s %s\r\n", method, path, "HTTP/1.1");    // 요청 라인 재구성
  printf("%s\n", request_buf);                                        // 재구성된 요청 헤더 출력
  sprintf(request_buf, "%sConnection: close\r\n", request_buf);       // Connection 헤더 추가
  sprintf(request_buf, "%sProxy-Connection: close\r\n", request_buf); // Proxy-Connection 헤더 추가
  sprintf(request_buf, "%s%s\r\n", request_buf, user_agent_hdr);      // User-Agent 헤더 추가

  /* 요청 메소드가 GET 또는 HEAD가 아닌 경우 오류 응답 전송 */
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not Implemented", "Proxy does not implement this method");
    return;
  }

  /* 원격 서버에 클라이언트의 요청 전송 */
  printf("%s, %s\n", hostname, port);
  serverfd = Open_clientfd(hostname, port); // 서버로의 연결 생성
  if (serverfd < 0)
  {                                                                                              // 연결 실패 시
    clienterror(serverfd, hostname, "404", "Not found", "Proxy couldn't connect to the server"); // 클라이언트에게 오류 응답 전송
    return;
  }
  printf("%s\n", request_buf);                            // 전송할 요청 헤더 출력
  Rio_writen(serverfd, request_buf, strlen(request_buf)); // 서버에 요청 전송
  Rio_readinitb(&response_rio, serverfd);

  /* 서버로부터 응답 받아 클라이언트에 전송 */
  ssize_t n;
  /* 응답 헤더 보내기 */
  while ((n = Rio_readlineb(&response_rio, response_buf, MAX_OBJECT_SIZE)) > 0)
  {
    Rio_writen(clientfd, response_buf, n);
    if (!strcmp(response_buf, "\r\n"))
      break;
  }

  // 헤드와 바디는 \r\n으로 구분되어짐
  /* 응답 본문 보내기 */
  while ((n = Rio_readlineb(&response_rio, response_buf, MAX_OBJECT_SIZE)) > 0)
  {
    Rio_writen(clientfd, response_buf, n);
  }
  Close(serverfd);
}

/* 주어진 URI를 호스트명, 포트, 경로로 파싱하는 함수 */
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  printf("---parse_uri: %s\n", uri); // 디버깅용 메시지 출력
  // 호스트명(도메인)을 가리키는 포인터 설정
  char *hostname_ptr = strstr(uri, "//") != NULL ? strstr(uri, "//") + 2 : uri + 1;
  // 포트를 가리키는 포인터 설정
  char *port_ptr = strstr(hostname_ptr, ":");
  // 경로를 가리키는 포인터 설정
  char *path_ptr = strstr(hostname_ptr, "/");

  // 경로가 존재한다면
  if (path_ptr > 0)
  {
    *path_ptr = '\0';           // 경로 부분을 끝낼 문자('\0')로 대체 / path_ptr을 기준으로 문자열을 나눔
    strcpy(path, path_ptr + 1); // 경로를 path 버퍼에 복사 / path를 경로에 저장
  }
  // 포트가 존재한다면
  if (port_ptr > 0)
  {
    *port_ptr = '\0';           // 포트 부분을 끝낼 문자('\0')로 대체 /port_ptr 기준으로 문자열을 나눔
    strcpy(port, port_ptr + 1); // 포트를 port 버퍼에 복사 / 포트 번호를 포트에 저장
  }

  strcpy(hostname, hostname_ptr); // 호스트명을 hostname 버퍼에 복사
  printf("---parse_uri host: %s, port: %s, path: %s\n", hostname, port, path); // 호스트명, 포트, 경로를 출력
}

/* 클라이언트에게 에러 메시지를 전송하는 함수 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // 에러 메시지 생성
  sprintf(body, "<html><title>Proxy Error</title>"); // HTML 페이지의 시작 부분 작성
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);                                              // 페이지 바탕색 설정
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);      // 에러 번호와 짧은 메시지 추가
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);     // 에러 원인 추가
  sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body); // 서버 정보 추가

  // HTTP 응답 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);      // HTTP 응답 라인 작성 (상태 코드와 메시지)
  Rio_writen(fd, buf, strlen(buf));                          // 클라이언트에게 전송
  sprintf(buf, "Content-type: text/html\r\n");               // HTML 컨텐츠 타입 설정
  Rio_writen(fd, buf, strlen(buf));                          // 클라이언트에게 전송
  sprintf(buf, "Content-length: %lu\r\n\r\n", strlen(body)); // HTML 본문의 길이 설정
  Rio_writen(fd, buf, strlen(buf));                          // 클라이언트에게 전송
  Rio_writen(fd, body, strlen(body));                        // HTML 본문을 클라이언트에게 전송
}

/* 클라이언트로부터의 HTTP 요청 헤더를 읽어들이는 함수 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  // HTTP 헤더를 읽어들임
  Rio_readlineb(rp, buf, MAXLINE);

  while (strcmp(buf, "\r\n"))
  { // 빈 줄이 나올 때까지 반복
    // 바디를 읽어들임
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf); // 헤더를 화면에 출력하거나 다른 작업을 수행할 수 있음
  }
  return;
}