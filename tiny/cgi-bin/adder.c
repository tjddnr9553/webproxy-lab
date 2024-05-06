#include "csapp.h"

int main(void)
{
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  if ((buf = getenv("QUERY_STRING")) != NULL)
  {
    p = strtok(buf, "&");
    // &로 나눈 쿼리문에서 =를 기준으로 나눠서 뒤의 문자를 arg1에 담음
    strcpy(arg1, strchr(p, '=') + 1);

    p = strtok(NULL, "&");
    // 나머지에서 =를 기준으로 나눠서 뒤의 문자를 arg2에 담음
    strcpy(arg2, strchr(p, '=') + 1);

    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  // 여기까지가 헤더

  // 여기까지 바디 출력
  printf("%s", content);
  fflush(stdout);

  exit(0);
}

/* $end adder */