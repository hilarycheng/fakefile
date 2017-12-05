#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <linux/limits.h>
#include <time.h>
#include "message.h"

volatile int fakesd = -1;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((__constructor__,__used__)) static void lib_init();
static void lib_init()
{
}

static struct sockaddr *addr(void) {
  static struct sockaddr_in addr = { 0, 0, { 0 } };
  if (!addr.sin_port) {
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(24120);
  }
  return (struct sockaddr *) &addr;
}

void connectFake() {
  if (fakesd > 0)
    return;

  fprintf(stderr, "Connect Fake SD : %d\n", fakesd);
  fakesd = socket(PF_INET, SOCK_STREAM, 0);
  if (fakesd < 0)
    exit(0);

  if (fcntl(fakesd, F_SETFD, FD_CLOEXEC) < 0)
    exit(0);

  while (1) {
    if (connect(fakesd, addr(), sizeof(struct sockaddr_in)) < 0) {
      if (errno != EINTR)
        exit(0);
    } else
      break;
  }
  fprintf(stderr, "Connect Fake Finish\n");
}

void sendMsg(FILE_MSG *msg) {
  unsigned char *ptr = (unsigned char *) msg;
  ssize_t rc = 0, remaining = 0, total = sizeof(FILE_MSG);

  connectFake();
 
  remaining = sizeof(FILE_MSG);
  while (remaining > 0) {
    rc = write(fakesd, ptr + (total - remaining), remaining);
    // fprintf(stderr, "Write Fake : %d %d\n", (int) rc, (int) total);
    if (rc <= 0) {
      if (remaining == total) break;
       else exit(0);
    } else {
      remaining -= rc;
    }
  }
}

int sendStat(struct stat *st, int func, const char *path, const char *link) {
  FILE_MSG msg;
  int rc = 0;

  pthread_mutex_lock(&mutex);
  bzero(&msg, sizeof(FILE_MSG));

  msg.type  = func;
  msg.mode  = st->st_mode;
  msg.ino   = st->st_ino;
  msg.uid   = st->st_uid;
  msg.gid   = st->st_gid;
  msg.dev   = st->st_dev;
  msg.rdev  = st->st_rdev;
  msg.nlink = st->st_nlink;
  strncpy(msg.file, path, PATH_MAX);
  if (link != NULL) strncpy(msg.link, link, PATH_MAX);

  sendMsg(&msg);

  if (func == FUNC_STAT) {
    rc = read(fakesd, &msg, sizeof(FILE_MSG));
    // fprintf(stderr, "READ : %d %d\n", (int) rc, (int) sizeof(FILE_MSG));
    if (rc == sizeof(FILE_MSG)) {
      // fprintf(stderr, "READ type : %04X\n", (unsigned int) msg.type);
      if ((msg.type & 0x8000) != 0) {
        pthread_mutex_unlock(&mutex);
        return 1;
      }
      // fprintf(stderr, "READ type success : %04X %d\n", (unsigned int) msg.type, (int) time(0));
      st->st_mode  = msg.mode;
      st->st_ino   = msg.ino;
      st->st_uid   = msg.uid;
      st->st_gid   = msg.gid;
      st->st_dev   = msg.dev;
      st->st_rdev  = msg.rdev;
      st->st_nlink = msg.nlink;
      if (link != NULL) memcpy(link, msg.link, PATH_MAX);
      pthread_mutex_unlock(&mutex);
      return 0;
    }
  }

  pthread_mutex_unlock(&mutex);
  return 0;
}

typedef int (*orig_open)(const char *pathname, int flags);

int open(const char *pathname, int flags, ...)
{
  orig_open open_func;

  open_func = (orig_open) dlsym(RTLD_NEXT,"open");
  return open_func(pathname,flags);
}

typedef char *(*orig_get_current_dir_name)(void);
char *get_current_dir_name(void)
{
  orig_get_current_dir_name dir_func;

  dir_func = (orig_get_current_dir_name) dlsym(RTLD_NEXT,"get_current_dir_name");
  return dir_func();
}

typedef int (*__XSTAT)(int ver, const char *path, struct stat *buf);
int __xstat(int ver, const char *path, struct stat *buf)
{
  int r = 0;
  __XSTAT xstat_func;
  struct stat st;

  fprintf(stderr, "__xstat\n");
  xstat_func = (__XSTAT) dlsym(RTLD_NEXT,"__xstat");
  r = xstat_func(ver, path, buf);
  if (r)
    return -1;

  memcpy(&st, buf, sizeof(struct stat));
  if (sendStat(&st, FUNC_STAT, path, NULL) != 0) {
    return 0;
  }
  memcpy(buf, &st, sizeof(struct stat));
  fprintf(stderr, "__xstat sucecss : %s\n", path);

  return 0;
}

typedef int (*orig__lxstat)(int ver, const char *path, struct stat *buf);
int __lxstat(int ver, const char *path, struct stat *buf)
{
  int r = 0;
  struct stat st;
  orig__lxstat xstat_func;

  fprintf(stderr, "__lxstat %s\n", path);
  xstat_func = (orig__lxstat) dlsym(RTLD_NEXT,"__lxstat");
  r = xstat_func(ver, path, buf);
  if (r)
    return -1;

  memcpy(&st, buf, sizeof(struct stat));
  if (sendStat(&st, FUNC_STAT, path, NULL) != 0) {
    fprintf(stderr, "__lxstat not in db : %s\n", path);
    return 0;
  }
  memcpy(buf, &st, sizeof(struct stat));
  fprintf(stderr, "__lxstat sucecss : %s\n", path);

  return 0;
}

typedef int (*orig__fxstat)(int ver, int fd, struct stat *buf);
int __fxstat(int ver, int fd, struct stat *buf)
{
  orig__fxstat fxstat_func;

  fprintf(stderr, "__fxstat\n");
  fxstat_func = (orig__fxstat) dlsym(RTLD_NEXT,"__fxstat");
  return fxstat_func(ver, fd, buf);
}

typedef int (*FCHOWN)(int fd, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group) {
  fprintf(stderr, "fchown start\n");
  return 0;
}

typedef int (*LCHOWN)(const char *path, uid_t owner, gid_t group);
int lchown(const char *path, uid_t owner, gid_t group)
{
  int r = 0;
  struct stat st;
  // LCHOWN lchown_func;

  // fprintf(stderr, "lchown start %s\n", path);
  // lchown_func = (LCHOWN) dlsym(RTLD_NEXT, "lchown");

  __XSTAT stat_func = (__XSTAT) dlsym(RTLD_NEXT, "__xstat");
  r  = stat_func(0, path, &st);
  fprintf(stderr, "lchown start %s %d\n", path, r);
  if (r)
    return -1;

  if (sendStat(&st, FUNC_STAT, path, NULL) != 0) {
    return 0;
  }

  st.st_uid = owner;
  st.st_gid = group;
  sendStat(&st, FUNC_CHOWN, path, NULL);

  return 0;
}

typedef int (*orig__fxstatat)(int ver, int dirfd, const char *path, struct stat *buf, int flags);
int __fxstatat(int ver, int dirfd, const char *path, struct stat *buf, int flags)
{
  orig__fxstatat fxstat_func;

  fxstat_func = (orig__fxstatat) dlsym(RTLD_NEXT, "__fxstatat");
  return fxstat_func(ver, dirfd, path, buf, flags);
}

#if 0
typedef int (*orig_openat_2)(int dirfd, const char *path, int flags);
int openat_2(int dirfd, const char *path, int flags)
{
  orig_openat_2 openat_func;

  fprintf(stderr, "openat_2 start\n"); 
  openat_func = (orig_openat_2) dlsym(RTLD_NEXT, "openat_2");
  return openat_func(dirfd, path, flags);
}

typedef int (*orig_openat)(int dirfd, const char *path, int flags, mode_t mode);
int openat(int dirfd, const char *path, int flags, mode_t mode)
{
  orig_openat openat_func;

  fprintf(stderr, "openat start : %d\n", mode); 
  openat_func = (orig_openat) dlsym(RTLD_NEXT, "openat");
  return openat_func(dirfd, path, flags, mode);
}
#endif

typedef int (*orig_mknod)(const char *path, mode_t mode, dev_t dev);
int mknod(const char *path, mode_t mode, dev_t dev)
{
  orig_mknod mknod_func;

  fprintf(stderr, "mknod start\n"); 
  mknod_func = (orig_mknod) dlsym(RTLD_NEXT, "mknod");
  return mknod_func(path, mode, dev);
}

typedef int (*orig_mknodat)(int dirfd, const char *path, mode_t mode, dev_t dev);
int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
{
  orig_mknodat mknod_func;

  fprintf(stderr, "mknodat start\n"); 
  mknod_func = (orig_mknodat) dlsym(RTLD_NEXT, "mknodat");
  return mknod_func(dirfd, path, mode, dev);
}

typedef int (*STAT)(const char *path, struct stat *buf);

void *get_libc() {
  void *lib = 0;
  if (!lib) {
    lib = dlopen("/lib/x86_64-linux-gnu/libc.so.6", RTLD_LAZY);
    fprintf(stderr, "dlopen : %p\n", lib);
  }
  return lib;
}

// typedef int (*XMKNOD)(int ver, const char *path, mode_t mode, dev_t *dev);
int __xmknod(int ver, const char *path, mode_t mode, dev_t *dev)
{
  struct stat st;
  int fd = -1, r;
  mode_t old_mask = umask(022);
  fprintf(stderr, "__xmknod start\n");
  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 00644);
  if (fd == -1)
    return -1;

  close(fd);

  __XSTAT stat_func = (__XSTAT) dlsym(RTLD_NEXT, "__xstat");
  r = stat_func(ver, path, &st);
  if (r)
    return -1;

  st.st_mode = mode & ~old_mask;
  st.st_rdev = *dev;

  sendStat(&st, FUNC_MKNOD, path, NULL);

  return 0;
}

typedef int (*orig__xmknodat)(int ver, int dirfd, const char *path, mode_t mode, dev_t *dev);
int __xmknodat(int ver, int dirfd, const char *path, mode_t mode, dev_t *dev)
{
  orig__xmknodat mknod_func;

  fprintf(stderr, "__xmknod start\n"); 
  mknod_func = (orig__xmknodat) dlsym(RTLD_NEXT, "__xmknodat");
  return mknod_func(ver, dirfd, path, mode, dev);
}

#if 0
typedef int (*orig_dup2)(int oldfd, int newfd);
int dup2(int oldfd, int newfd)
{
  orig_dup2 dup2_func;

  dup2_func = (orig_dup2) dlsym(RTLD_NEXT, "dup2");
  return dup2_func(oldfd, newfd);
}
#endif

typedef int (*CHOWN)(const char *path, uid_t owner, gid_t group);
int chown(const char *path, uid_t owner, gid_t group) {
  fprintf(stderr, "chown start\n");
  return 0;
}

typedef int (*LINK)(const char *path1, const char *path2);
int link(const char *path1, const char *path2) {
#if 0
  SYMLINK symlinkFunc;

  fprintf(stderr, "symlink start\n"); 
  symlinkFunc = (SYMLINK) dlsym(RTLD_NEXT, "symlink");
  return symlinkFunc(path1, path2);
#endif
  struct stat st;
  int fd = -1, r = 0;
  mode_t old_mask = umask(022);

  fprintf(stderr, "link start : %s\n", path2); 

  fd = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 00644);
  if (fd == -1)
    return -1;
  close(fd);

  __XSTAT stat_func = (__XSTAT) dlsym(RTLD_NEXT, "__xstat");
  r = stat_func(0, path2, &st);
  if (r)
    return -1;

  st.st_mode = ~old_mask;
  st.st_mode = (st.st_mode & ~S_IFMT) | S_IFLNK;
  sendStat(&st, FUNC_SYMLINK, path2, path1);

  return 0;
}

typedef int (*SYMLINK)(const char *path1, const char *path2);
int symlink(const char *path1, const char *path2) {
#if 0
  SYMLINK symlinkFunc;

  fprintf(stderr, "symlink start\n"); 
  symlinkFunc = (SYMLINK) dlsym(RTLD_NEXT, "symlink");
  return symlinkFunc(path1, path2);
#endif
  struct stat st;
  int fd = -1, r = 0;
  mode_t old_mask = umask(022);

  fprintf(stderr, "symlink start : %s\n", path2); 

  fd = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 00644);
  if (fd == -1)
    return -1;
  close(fd);

  __XSTAT stat_func = (__XSTAT) dlsym(RTLD_NEXT, "__xstat");
  r = stat_func(0, path2, &st);
  if (r)
    return -1;

  st.st_mode = ~old_mask;
  st.st_mode = (st.st_mode & ~S_IFMT) | S_IFLNK;
  sendStat(&st, FUNC_SYMLINK, path2, path1);

  return 0;
}

typedef int (*CHMOD)(const char *path, mode_t mode);
int chmod(const char *path, mode_t mode)
{
  int r = 0;
  struct stat st;

  __XSTAT stat_func = (__XSTAT) dlsym(RTLD_NEXT, "__xstat");
  r  = stat_func(0, path, &st);

  fprintf(stderr, "chmod path : %s %d\n", path, r);
  if (r)
    return -1;

  if (sendStat(&st, FUNC_STAT, path, NULL) != 0) {
    return 0;
  }

  st.st_mode = mode;
  sendStat(&st, FUNC_CHMOD, path, NULL);

  return 0;
}

typedef ssize_t (*READLINK)(const char *path, char *buf, size_t bufsiz);
ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
  int r, len = 0;
  __XSTAT xstat_func;
  struct stat st;
  char *templink = (char *) malloc(PATH_MAX);

  fprintf(stderr, "readlink : %s\n", path);

  xstat_func = (__XSTAT) dlsym(RTLD_NEXT,"__xstat");
  r = xstat_func(0, path, &st);
  if (r) {
    free(templink);
    return -1;
  }

  templink[0] = 0;
  if (sendStat(&st, FUNC_STAT, path, templink) != 0) {
    free(templink);
    READLINK rl_func = (READLINK) dlsym(RTLD_NEXT, "readlink");
    return rl_func(path, buf, bufsiz);
  }

  len = strnlen(templink, PATH_MAX);
  fprintf(stderr, "readlink : %s : %d\n", templink, len);
  strncpy(buf, templink, len);
  free(templink);

  return len;
}

typedef int (*MKDIR)(const char *path, mode_t mode);
int mkdir(const char *path, mode_t mode)
{
  int ret = 0;
  fprintf(stderr, "mkdir path : %s\n", path);

  MKDIR mkdir_func = (MKDIR) dlsym(RTLD_NEXT, "mkdir");
  ret = mkdir_func(path, mode);
  if (ret == 0) {
    struct stat st;
    __XSTAT stat_func = (__XSTAT) dlsym(RTLD_NEXT, "__xstat");
    int r = stat_func(0, path, &st);
    if (r)
      return -1;
    sendStat(&st, FUNC_MKDIR, path, NULL);
  }

  return ret;
}

int setuid(uid_t id)
{
  fprintf(stderr, "setuid\n"); 
  return 0;
}

uid_t getuid(void)
{
  fprintf(stderr, "getuid\n"); 
  return 0;
}

int seteuid(uid_t id)
{
  fprintf(stderr, "seteuid\n"); 
  return 0;
}

uid_t geteuid(void)
{
  fprintf(stderr, "geteuid\n"); 
  return 0;
}

int setgid(uid_t id)
{
  fprintf(stderr, "setgid\n"); 
  return 0;
}

uid_t getgid(void)
{
  fprintf(stderr, "getgid\n"); 
  return 0;
}

int setegid(uid_t id)
{
  fprintf(stderr, "setegid\n"); 
  return 0;
}

uid_t getegid(void)
{
  fprintf(stderr, "getegid\n"); 
  return 0;
}

#if 0
int __openat_2(int dirfd, const char *path, int flags);
int fcntl(int fd, int cmd, ...{struct flock *lock});
# just so we know the inums of symlinks
char *canonicalize_file_name(const char *filename);
int eaccess(const char *path, int mode);
int open64(const char *path, int flags, ...{mode_t mode}); /* flags=0 */
int openat64(int dirfd, const char *path, int flags, ...{mode_t mode}); /* flags=0 */
int __openat64_2(int dirfd, const char *path, int flags); /* flags=0 */
int creat64(const char *path, mode_t mode);
int stat(const char *path, struct stat *buf); /* real_func=pseudo_stat */
int lstat(const char *path, struct stat *buf); /* real_func=pseudo_lstat, flags=AT_SYMLINK_NOFOLLOW */
int fstat(int fd, struct stat *buf); /* real_func=pseudo_fstat */
int stat64(const char *path, struct stat64 *buf); /* real_func=pseudo_stat64 */
int lstat64(const char *path, struct stat64 *buf); /* real_func=pseudo_lstat64, flags=AT_SYMLINK_NOFOLLOW */
int fstat64(int fd, struct stat64 *buf); /* real_func=pseudo_fstat64 */
int __xstat64(int ver, const char *path, struct stat64 *buf);
int __lxstat64(int ver, const char *path, struct stat64 *buf); /* flags=AT_SYMLINK_NOFOLLOW */
int __fxstat64(int ver, int fd, struct stat64 *buf);
int __fxstatat64(int ver, int dirfd, const char *path, struct stat64 *buf, int flags);
FILE *fopen64(const char *path, const char *mode);
int nftw64(const char *path, int (*fn)(const char *, const struct stat64 *, int, struct FTW *), int nopenfd, int flag);
FILE *freopen64(const char *path, const char *mode, FILE *stream);
int ftw64(const char *path, int (*fn)(const char *, const struct stat64 *, int), int nopenfd);
int glob64(const char *pattern, int flags, int (*errfunc)(const char *, int), glob64_t *pglob);
int scandir64(const char *path, struct dirent64 ***namelist, int (*filter)(const struct dirent64 *), int (*compar)());
int truncate64(const char *path, off64_t length);
int mkstemp64(char *template); /* flags=AT_SYMLINK_NOFOLLOW */
int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);
int setgroups(size_t size, const gid_t *list);
int setfsgid(gid_t fsgid);
int setfsuid(uid_t fsuid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int scandir(const char *path, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)());
int getgroups(int size, gid_t *list);
int lckpwdf(void);
int ulckpwdf(void);
int euidaccess(const char *path, int mode);
int getpw(uid_t uid, char *buf);
int getpwent_r(struct passwd *pwbuf, char *buf, size_t buflen, struct passwd **pwbufp);
int getgrent_r(struct group *gbuf, char *buf, size_t buflen, struct group **gbufp);
int capset(cap_user_header_t hdrp, const cap_user_data_t datap); /* real_func=pseudo_capset */
#endif

