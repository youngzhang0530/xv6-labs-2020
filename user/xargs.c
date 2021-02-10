#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  char buffer[512];
  read(0, buffer, sizeof buffer);

  int subargc = 0;
  char *subargv[MAXARG];

  int i = 0;
  for (int j = 0; buffer[j]; j++)
  {
    if (buffer[j] == ' ' || buffer[j] == '\n')
    {
      int length = j - i;
      char *arg = malloc(length + 1);
      memmove(arg, &buffer[i], length);
      *(arg + length) = 0;
      subargv[subargc++] = arg;
      i = j + 1;
    }
  }

  for (int i = 1; i < argc; i++)
    argv[i - 1] = argv[i];

  int status;
  for (int i = 0; i < subargc; i++)
  {
    int cpid = fork();
    if (cpid == 0)
    {
      argv[argc - 1] = subargv[i];
      exec(argv[0], argv);
    }
    else if (cpid > 0)
      wait(&status);
    else
    {
      fprintf(2, "failed to fork\n");
      exit(1);
    }
  }

  for (int i = 0; i < subargc; i++)
    free(subargv[i]);

  exit(status);
}
