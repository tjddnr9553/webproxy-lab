/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd; // 서버 및 클라이언트 소켓 파일 디스크립터
  char hostname[MAXLINE], port[MAXLINE]; // 클라이언트 호스트네임 및 포트번호
  socklen_t clientlen; // 클라이언트 주소 구조체 크기
  struct sockaddr_storage clientaddr; // 클라이언트 주소 구조체

  // 명령행 인수 확인
  if (argc != 2) { // 인수 개수가 2가 아니면 오류 메시지 출력
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 클라이언트 연결 수신 소켓 생성
  listenfd = Open_listenfd(argv[1]); 

  // 클라이언트 요청 수락 및 처리
  while (1) {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 구조체 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 연결 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 호스트네임 및 포트번호 추출
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 연결 확인 메시지 출력
    doit(connfd); // 클라이언트 요청 처리 함수 호출
    Close(connfd); // 연결 종료 및 소켓 닫음
  }
}

void doit(int fd)
{
  int is_static; // 정적 파일 여부를 나타내는 플래그
  struct stat sbuf; // 파일의 상태 정보를 저장하는 구조체
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 버퍼 및 요청 라인 구성 요소
  char filename[MAXLINE], cgiargs[MAXLINE]; // 요청된 파일의 경로 및 CGI 인자
  rio_t rio; // 리오 버퍼

  /* 클라이언트가 rio로 보낸 요청 라인 분석 */
  Rio_readinitb(&rio, fd);                          //  리오 버퍼를 특정 파일 디스크립터(fd=>connfd)와 연결
  Rio_readlineb(&rio, buf, MAXLINE);                // rio(connfd)에 있는 string 한 줄(Request line)을 모두 buf로 옮긴다.
  printf("Request headers:\n");
  printf("%s", buf);                                // GET /index.html HTTP/1.1
  sscanf(buf, "%s %s %s", method, uri, version); 
  // method: "GET"
	// uri: "/index.html"
	// version: "HTTP/1.1"

  /* HTTP 요청의 메서드가 "GET"이 아닌 경우에 501 오류를 클라이언트에게 반환 */
  /* Homework 11.11 "HEAD"가 아닌 경우 추가 */
  if (strcasecmp(method, "GET") * strcasecmp(method, "HEAD"))
  { // 조건문에서 하나라도 0이면 0
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  
  /* read_requesthdrs: 요청 헤더를 읽어들여서 출력 */
  /* 헤더를 읽는 것은 클라이언트의 요청을 처리하고 응답하기 위해 필요한 전처리 작업의 일부 */
  read_requesthdrs(&rio);

  /* parse_uri: 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분한다.*/
  /* is_static이 1이면 정적 컨텐츠, 0이면 동적 컨텐츠 */
  is_static = parse_uri(uri, filename, cgiargs);

  /* filename: 클라이언트가 요청한 파일의 경로 */
  if (stat(filename, &sbuf) < 0) 
  { // 파일이 없다면 -1, 404에러
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* 정적 컨텐츠 */
  if (is_static)
  {
    // !(일반 파일이다) or !(읽기 권한이 있다)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 정적 서버에 파일의 사이즈를 같이 보낸다. => Response header에 Content-length 위해
    serve_static(fd, filename, sbuf.st_size, method);
  }

  /* 동적 컨텐츠 */
  else
  {
    // !(일반 파일이다) or !(실행 권한이 있다)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 동적 서버에 인자를 같이 보낸다.
    // filename은 CGI 프로그램의 경로이고, cgiargs는 CGI 프로그램을 실행할 때 필요한 인자들을 포함
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void read_requesthdrs(rio_t *rp) {

  char buf[MAXLINE]; // 버퍼
  Rio_readlineb(rp, buf, MAXLINE); // 요청 라인을 읽어들여 무시한다. (요청 헤더부터 읽기 시작)

  // "\r\n"이 나올 때까지 요청 헤더를 읽어들여 화면에 출력
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE); // 한 줄을 읽는다.
    printf("%s", buf); // 읽어들인 헤더를 화면에 출력
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) 
{
  char *ptr;

  /* 정적 콘텐츠 확인 */
  if (!strstr(uri, "cgi-bin")) { // URI에 "cgi-bin" 문자열이 포함되어 있지 않은 경우
    strcpy(cgiargs, ""); // CGI 인자를 빈 문자열로 설정
    strcpy(filename, "."); // 파일 경로를 현재 디렉토리로 설정
    strcat(filename, uri); // URI를 파일 경로에 추가
    if (uri[strlen(uri)-1] == '/') { // URI의 마지막 문자가 '/'인 경우
      strcat(filename, "home.html"); // 파일 경로에 "home.html"을 추가하여 기본 파일로 설정
    }
    return 1; // 정적 콘텐츠임을 나타내는 플래그 반환
  }

  /* 동적 콘텐츠 확인 */
  else { // URI에 "cgi-bin" 문자열이 포함되어 있는 경우
    ptr = index(uri, '?'); // URI에서 '?' 문자의 위치를 찾음

    if (ptr){ // URI에 '?' 문자가 존재하는 경우
      strcpy(cgiargs, ptr+1); // '?' 이후의 문자열을 CGI 인자로 복사
      *ptr = '\0'; // URI 문자열에서 '?' 문자 이후의 부분을 종료하는 널 문자로 대체
    }
    else { // URI에 '?' 문자가 존재하지 않는 경우
      strcpy(cgiargs, ""); // CGI 인자를 빈 문자열로 설정
    }

    strcpy(filename, "."); // 파일 경로를 현재 디렉토리로 설정
    strcat(filename, uri); // URI를 파일 경로에 추가
    return 0; // 동적 콘텐츠임을 나타내는 플래그 반환
  }
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");

  // Homework 11.7: MP4 비디오 타입 추가
  else if (strstr(filename, ".mpg"))
    strcpy(filetype, "video/mpg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;                // 파일 디스크립터
  char *srcp,               // 파일 내용을 메모리에 매핑한 포인터
       filetype[MAXLINE],   // 파일의 MIME 타입
       buf[MAXBUF];         // 응답 헤더를 저장할 버퍼

  /* 응답 헤더 생성 및 전송 */
  get_filetype(filename, filetype);                         // 파일 타입 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                      // 응답 라인 작성
  // 응답 헤더
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);       // 서버 정보 추가
  sprintf(buf, "%sConnections: close\r\n", buf);            // 연결 종료 정보 추가
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  // 컨텐츠 길이 추가
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); // 컨텐츠 타입 추가

  /* 응답 라인과 헤더를 클라이언트에게 보냄 */
  Rio_writen(fd, buf, strlen(buf)); 
  printf("Response headers: \n");
  printf("%s", buf);

  if (strcasecmp(method, "HEAD") == 0)
      return;

  /* 응답 바디 전송 */
  srcfd = Open(filename, O_RDONLY, 0);                       // 파일 열기
  //srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 메모리에 동적할당

  /* Homework 11.9 */
  srcp = (char *) malloc(filesize); // mmap 대신 malloc 사용
  rio_readn(srcfd, srcp, filesize); // rio_readn 사용하기

  Close(srcfd);                                             // 파일 닫기
  Rio_writen(fd, srcp, filesize);                           // 클라이언트에게 파일 내용 전송
  // Munmap(srcp, filesize);                                // 메모리 할당 해제
  free(srcp);                                               // malloc 사용 => munmap에서 free로 변경  
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{ 
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* 클라이언트에 HTTP 응답 라인과 헤더를 전송 */
  sprintf(buf, "HTTP/1.1 200 OK\r\n"); // HTTP 응답 라인 생성
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트에 응답 라인 전송
  sprintf(buf, "Server: Tiny Web Server\r\n"); // 서버 정보를 응답 헤더에 추가
  Rio_writen(fd, buf, strlen(buf)); // 클라이언트에 응답 헤더 전송

	/* CGI 실행을 위해 자식 프로세스를 생성 */
  if (Fork() == 0) // fork() 자식 프로세스 생성됐으면 0을 반환 (성공)
  { 
    setenv("QUERY_STRING", cgiargs, 1); // CGI 프로그램에 필요한 환경 변수 설정
    Dup2(fd, STDOUT_FILENO); // 자식 프로세스의 표준 출력을 클라이언트로 리다이렉션
    Execve(filename, emptylist, environ); // CGI 프로그램 실행
  }

  Wait(NULL); // 부모 프로세스가 자식 프로세스의 종료를 대기
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* HTTP 응답 본문 생성 */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

  /* HTTP 응답 헤더 출력 */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  /* 에러 메시지와 응답 본체를 클라이언트에게 전송 */
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}