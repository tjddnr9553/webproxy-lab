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

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  // rio_readlineb를 위해 rio_t 타입(구조체)의 읽기 버퍼를 선언
  rio_t rio;

  /* Read request line and headers */
  /* Rio = Robust I/O */
  // rio_t 구조체를 초기화 해준다.
  Rio_readinitb(&rio, fd);           // &rio 주소를 가지는 읽기 버퍼와 식별자 connfd를 연결한다.
  Rio_readlineb(&rio, buf, MAXLINE); // 버퍼에서 읽은 것이 담겨있다.
  printf("Request headers:\n");
  printf("%s", buf);                             // "GET / HTTP/1.1"
  sscanf(buf, "%s %s %s", method, uri, version); // 버퍼에서 자료형을 읽는다, 분석한다.

  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  /* GET method라면 읽어들이고, 다른 요청 헤더들을 무시한다. */
  read_requesthdrs(&rio);

  /* Parse URI form GET request */
  /* URI 를 파일 이름과 비어 있을 수도 있는 CGI 인자 스트링으로 분석하고, 요청이 정적 또는 동적 컨텐츠를 위한 것인지 나타내는 플래그를 설정한다.  */
  is_static = parse_uri(uri, filename, cgiargs);
  printf("uri : %s, filename : %s, cgiargs : %s \n", uri, filename, cgiargs);

  /* 만일 파일이 디스크상에 있지 않으면, 에러메세지를 즉시 클라아언트에게 보내고 메인 루틴으로 리턴 */
  if (stat(filename, &sbuf) < 0)
  { // stat는 파일 정보를 불러오고 sbuf에 내용을 적어준다. ok 0, errer -1
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* Serve static content */
  if (is_static)
  {
    // 파일 읽기 권한이 있는지 확인하기
    // S_ISREG : 일반 파일인가? , S_IRUSR: 읽기 권한이 있는지? S_IXUSR 실행권한이 있는가?

    // 일반파일이 아니거나, 사용자의 읽기 권한이 없다면 수행
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      // 권한이 없다면 클라이언트에게 에러를 전달
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 그렇다면 클라이언트에게 파일 제공
    serve_static(fd, filename, sbuf.st_size, method);
  }
  else
  { /* Serve dynamic content */
    /* 실행 가능한 파일인지 검증 */

    // 일반파일이 아니거나, 사용자의 읽기 권한이 없다면 수행
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      // 실행이 불가능하다면 에러를 전달
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 그렇다면 클라이언트에게 파일 제공.
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];
  /* Build the HTTP response body */
  // HTTP 응답 전송을 하기 위해서는 헤더와 본문이 함께 쓰여야 하기 때문에, body로 HTML 본문을 구성하고, buf로 HTTP 헤더를 구성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* Tiny는 요청 헤더 내의 어떤 정보도 사용하지 않는다
 * 단순히 이들을 읽고 무시한다.
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  /* strcmp 두 문자열을 비교하는 함수 */
  /* 헤더의 마지막 줄은 비어있기에 \r\n 만 buf에 담겨있다면 while문을 탈출한다.  */
  while (strcmp(buf, "\r\n"))
  {
    // rio 설명에 나와있다 싶이 rio_readlineb는 \n를 만날때 멈춘다.
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    // 멈춘 지점 까지 출력하고 다시 while
  }
  return;
}

/*HTTP URI를 분석한다. */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) // static content
  {
    strcpy(cgiargs, "");             // cgi인자 스트링을 지우고
    strcpy(filename, ".");           // URIfmf ./index.html 같은 상대 리눅스 경로이름으로 변환
    strcat(filename, uri);           // 까지
    if (uri[strlen(uri) - 1] == '/') // 만약에 uri가 /로 끝난다면
      strcat(filename, "home.html"); // 기본 파일 이름을 추가
    return 1;
  }
  else // Dynamic content
  {
    ptr = index(uri, '?'); // 동적이라면
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // CGI 모든 인자를 추출
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, ""); // 까지

    strcpy(filename, "."); // 나머지 URI 부분을 상대 리눅스 파일 이름으로 변환
    strcat(filename, uri); // 까지
    return 0;
  }
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
  {
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif"))
  {
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png"))
  {
    strcpy(filename, "image/.png");
  }
  else if (strstr(filename, ".jpg"))
  {
    strcpy(filetype, "image/jpeg");
    /* 11.7 숙제 문제 - Tiny 가 MPG  비디오 파일을 처리하도록 하기.  */
  }
  else if (strstr(filename, ".mpg"))
  {
    strcpy(filetype, "video/mpg");
  }
  else if (strstr(filename, ".mp4"))
  {
    strcpy(filetype, "video/mp4");
  }
  else
  {
    strcpy(filetype, "text/plain");
  }
}

void serve_static(int fd, char *filename, int filesize, char *method)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF], *fbuf;
  /* send response headers to client*/
  // 파일 접미어 검사해서 파일이름에서 타입 가지고
  get_filetype(filename, filetype); // 접미어를 통해 파일 타입 결정한다.

  // 클라이언트에게 응답 줄과 응답 헤더 보낸다
  // 클라이언트에게 응답 보내기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer : Tiny Web Server \r\n", buf);
  sprintf(buf, "%sConnection : close \r\n", buf);
  sprintf(buf, "%sConnect-length : %d \r\n", buf, filesize);
  sprintf(buf, "%sContent-type : %s \r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  // 서버에 출력
  printf("Response headers : \n");
  printf("%s", buf);

  if (!strcasecmp(method, "HEAD"))
    return;

  // 읽을 수 있는 파일로 열기
  srcfd = Open(filename, O_RDONLY, 0); // open read only

  // 파일을 어떤 메모리 공간에 대응시키고 첫주소를 리턴
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // mmap : 요청한 파일을 가상메모리 영역으로 매핑한다
  // // mmap 호출시 위에서 받아온 모든 요청 정보들(srcfd)을 전부 매핑해서 srcp로 받는다(포인터)
  // Close(srcfd); // srcfd 내용을 메모리로 매핑한 후에 더 이상 이 식별자 필요X, 파일을 닫는다. 안 닫으면 메모리 누수 치명적
  // Rio_writen(fd, srcp, filesize); // 실제로 파일을 클라이언트에게 전송. // srcp내용을 fd에 filesize만큼 복사해서 넣는다
  // Munmap(srcp, filesize); // 매핑된 srcp 주소를 반환한다. 치명적인 메모리 누수를 피한다
  // // mmap-munmap은 malloc-free처럼 세트

  fbuf = malloc(filesize);          // filesize 만큼의 가상 메모리(힙)를 할당한 후(malloc은 아무것도 없는 빈 상태에서 시작) , Rio_readn 으로 할당된 가상 메모리 공간의 시작점인 fbuf를 기준으로 srcfd 파일을 읽어 복사해넣는다.
  Rio_readn(srcfd, fbuf, filesize); // srcfd 내용을 fbuf에 넣는다(버퍼에 채워줌)
  Close(srcfd);                     // 윗줄 실행 후 필요 없어져서 닫아준다 // 양 쪽 모두 생성한 파일 식별자 번호인 srcfd 를 Close() 해주고
  Rio_writen(fd, fbuf, filesize);   // Rio_writen 함수 (시스템 콜) 을 통해 클라이언트에게 전송한다
  // fbuf를 fd에다가 넣는다(fbuf는 사실 포인터. 걔를 밀면서 writen, 미는 애는 새로 선언한 usrbuf)
  free(fbuf); // Mmap은 Munmap, malloc은 free로 할당된 가상 메모리를 해제해준다.
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = {NULL};
  /* Return first part of HTTP response*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf)); // 왜 두번 쓰나요?
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (!strcasecmp(method, "HEAD"))
    return;

  // 클라이언트는 성공을 알려주는 응답라인을 보내는 것으로 시작한다.
  if (Fork() == 0)
  { // 타이니는 자식프로세스를 포크하고 동적 컨텐츠를 제공한다.
    setenv("QUERY_STRING", cgiargs, 1);

    // 자식은 QUERY_STRING 환경변수를 요청 uri의 cgi인자로 초기화 한다.  (15000 & 213)
    Dup2(fd, STDOUT_FILENO); // 자식은 자식의 표준 출력을 연결 파일 식별자로 재지정하고,

    Execve(filename, emptylist, environ);
    // 그 후에 cgi프로그램을 로드하고 실행한다.
    // 자식은 본인 실행 파일을 호출하기 전 존재하던 파일과, 환경변수들에도 접근할 수 있다.
  }
  Wait(NULL); // 부모는 자식이 종료되어 정리되는 것을 기다린다.
}
