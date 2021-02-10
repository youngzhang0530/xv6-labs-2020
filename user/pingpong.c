#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  char buffer;
  int parent[2];
  int child[2];
  if (pipe(parent) != 0)
  {
    fprintf(2, "failed to open a pipe\n");
    exit(1);
  }
  if (pipe(child) != 0)
  {
    fprintf(2, "failed to open a pipe\n");
    exit(1);
  }

  int cpid = fork();
  if (cpid == 0)
  {
    close(child[0]);
    close(parent[1]);
    read(parent[0], &buffer, sizeof(char));
    printf("%d: received ping\n", getpid());
    write(child[1], &buffer, sizeof(char));
    close(child[1]);
    close(parent[0]);
  }
  else if (cpid > 0)
  {
    close(child[1]);
    close(parent[0]);
    write(parent[1], &buffer, sizeof(char));
    read(child[0], &buffer, sizeof(char));
    close(child[0]);
    close(parent[1]);
    printf("%d: received pong\n", getpid());
    wait(0);
  }
  else
  {
    fprintf(2, "failed to fork\n");
    exit(1);
  }

  exit(0);
}
