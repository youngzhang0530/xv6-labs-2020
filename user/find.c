#include "find.h"

struct stat status;
struct dirent entry;
char buffer[512];

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(2, "usage: find [path] [file_name]\n");
    exit(1);
  }
  strcpy(buffer, argv[1]);
  find(argv[1], argv[2]);

  exit(0);
}

char *extract_filename(char *path)
{
  char *p;
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  return p;
}

void find(char *path, char *name)
{
  int fd;

  if ((fd = open(path, O_RDONLY)) == -1)
  {
    fprintf(2, "failed to open %s\n", path);
    exit(1);
  }

  if (fstat(fd, &status) != 0)
  {
    fprintf(2, "failed to stat %s\n", path);
    close(fd);
    exit(1);
  }
  if (status.type == T_DIR)
  {
    char *p = buffer + strlen(path);
    *p++ = '/';

    while (read(fd, &entry, sizeof entry) == sizeof entry)
    {
      if (entry.inum == 0)
        break;
      if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0)
        continue;

      memmove(p, entry.name, DIRSIZ);
      find(buffer, name);
    }
  }
  else if (strcmp(extract_filename(path), name) == 0)
    printf("%s\n", path);

  close(fd);
}
