#include "primes.h"

int main(int argc, char *argv[])
{
  int fds[2];
  if (pipe(fds) == -1)
  {
    fprintf(2, "failed to open a pipe\n");
    exit(1);
  }

  int status;
  int cpid = fork();
  if (cpid == 0)
  {
    close(fds[1]);
    prime(fds[0]);
  }
  else if (cpid > 0)
  {
    close(fds[0]);
    for (int i = 2; i <= 35; i++)
    {
      write(fds[1], &i, sizeof(int));
    }
    close(fds[1]);
    wait(&status);
  }
  else
  {
    fprintf(2, "failed to fork\n");
    exit(1);
  }
  exit(status);
}

void prime(int fd)
{
  int buffer;
  if (read(fd, &buffer, sizeof(int)) <= 0)
  {
    close(fd);
    exit(0);
  }

  int fds[2];
  if (pipe(fds) != 0)
  {
    fprintf(2, "failed to open a pipe\n");
    exit(1);
  }

  int cpid = fork();
  if (cpid == 0)
  {
    close(fd);
    close(fds[1]);
    prime(fds[0]);
  }
  else if (cpid > 0)
  {
    close(fds[0]);
    printf("prime %d\n", buffer);
    int n = buffer;
    while (read(fd, &buffer, sizeof(int)) > 0)
    {
      if (buffer % n != 0)
      {
        write(fds[1], &buffer, sizeof(int));
      }
    }
    close(fds[1]);
    close(fd);
    wait(0);
  }
  else
  {
    fprintf(2, "failed to fork\n");
    close(fd);
    close(fds[0]);
    close(fds[1]);
    exit(1);
  }
}
