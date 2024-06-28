#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#include "param.h"

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

int
thread_create(void (*func)(void*), void* arg)
{
  #if DBGMSG_THREAD_CREATE
  printf(1, "frees before thread stack allocation: %d\n", frees());
  #endif

  uint alloc = (uint)malloc(4096); // stack cant grow larger than 4KB

  #if DBGMSG_THREAD_CREATE
  printf(1, "frees after thread stack allocation:  %d\n", frees());
  #endif

  uint tid = clone((char*)alloc);

  // from here and below two threads are running

  #if DBGMSG_THREAD_CREATE
  printf(1, "alloc val:     %x\n", (uint)alloc);
  printf(1, "tid addr:      %x\n", (uint)&tid);
  #endif

  if (tid == 0){
    func(arg);
    #if DBGMSG_THREAD_CREATE
    printf(1, "frees before thread stack deallocation:  %d\n", frees());
    #endif
    free((void*)alloc);
    #if DBGMSG_THREAD_CREATE
    printf(1, "frees after thread stack deallocation:  %d\n", frees());
    #endif
    exit();
    return -1;
  }
  else if (tid == -1){
    // if clone failed, no child thread even exits.
    // -> just free allocated resources (the rest is freed inside clone)
    free((void*)alloc);
    return -1;
  }
  else{
    return tid;
  }
}

int
thread_join(int tid)
{ 
  int ret;
	while ((ret = join()) != -1){
    // thread joined is the thread that we are waiting for
    if (tid == ret){
      return 0;
    }
  }
  // no threads to join or invalid call
  return -1;
}

