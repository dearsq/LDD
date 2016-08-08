/*************************************************************************
    > File Name: strcut_foo.c
    > Author: Younix
    > Mail: foreveryounix@gmail.com 
    > Created Time: 2016年08月04日 星期四 11时49分46秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>

struct foo
{
  int a;
  int b;
  int c;
};


int
main ()
{
  struct foo *gp = NULL;
  struct foo *p;

  /* . . . */
  p = malloc (sizeof (*p));
  p->a = 1;
  p->b = 2;
  p->c = 3;
  gp = p;

  return 0;
}
