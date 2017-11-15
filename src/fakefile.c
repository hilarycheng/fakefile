#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

__attribute__((__constructor__,__used__)) static void lib_init();
static void lib_init()
{
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

typedef int (*orig__xstat)(int ver, const char *path, struct stat *buf);
int __xstat(int ver, const char *path, struct stat *buf)
{
  orig__xstat xstat_func;

  xstat_func = (orig__xstat) dlsym(RTLD_NEXT,"__xstat");
  return xstat_func(ver, path, buf);
}

typedef int (*orig__lxstat)(int ver, const char *path, struct stat *buf);
int __lxstat(int ver, const char *path, struct stat *buf)
{
  orig__lxstat xstat_func;

  xstat_func = (orig__lxstat) dlsym(RTLD_NEXT,"__lxstat");
  return xstat_func(ver, path, buf);
}

typedef int (*orig__fxstat)(int ver, int fd, struct stat *buf);
int __fxstat(int ver, int fd, struct stat *buf)
{
  orig__fxstat fxstat_func;

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
  LCHOWN lchown_func;

  fprintf(stderr, "lchown start\n");
  lchown_func = (LCHOWN) dlsym(RTLD_NEXT, "lchown");
  // return lchown_func(path, owner, group);
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

// typedef int (*XMKNOD)(int ver, const char *path, mode_t mode, dev_t *dev);
int __xmknod(int ver, const char *path, mode_t mode, dev_t *dev)
{
//  XMKNOD mknod_func;
  int fd = -1;
  fprintf(stderr, "__xmknod start\n");
  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 00644);
  if (fd == -1)
    return -1;

  close(fd);

  // mknod_func = (XMKNOD) dlsym(RTLD_NEXT, "__xmknod");
  // return mknod_func(ver, path, mode, dev);

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
  fprintf(stderr, "link start\n");
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
  int fd = -1;

  fprintf(stderr, "symlink start : %s\n", path2); 

  fd = open(path2, O_WRONLY | O_CREAT | O_TRUNC, 00644);
  if (fd == -1)
    return -1;
  close(fd);

  return 0;
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

