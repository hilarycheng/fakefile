#define FUNC_CHOWD   0
#define FUNC_CHOWN   1
#define FUNC_MKNOD   2
#define FUNC_UNLINK  3
#define FUNC_SYMLINK 4

typedef struct {
  int type;
  uint32_t uid;
  uint32_t gid;
  uint64_t ino;
  uint64_t dev;
  uint64_t rdev;
  uint32_t mode;
  uint32_t nlink;
  char file[PATH_MAX];
} FILE_MSG;

