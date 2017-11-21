#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sqlite3.h>
#include <string.h>
#include "message.h"

int mSocket = -1;
int mSocketList[1024];
int mSocketSize = 0;
unsigned char mTempBuf[4096];
sqlite3 *mem_db;
sqlite3 *file_db;
sqlite3_stmt *stmt;

void cleanup(int sig) {
  int rc = 0;
  sqlite3_backup *backup = NULL;

  printf("Clean up Handler\n");

  backup = sqlite3_backup_init(file_db, "main", mem_db, "main");
  if (backup) {
    sqlite3_backup_step(backup, -1);
    rc = sqlite3_backup_finish(backup);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "backup sqlite3 failed\n");
    }
    sqlite3_close(mem_db);
    sqlite3_close(file_db);
  }

  if (sig != -1)
    exit(0);
}

void socketListAdd(int sd) {
  mSocketList[mSocketSize] = sd;
  mSocketSize++;
}

void socketListRemove(int index) {
  for (index++; index < mSocketSize; index++)
    mSocketList[index - 1] = mSocketList[index];

  mSocketSize--;
}
  
void insertFile(FILE_MSG *msg) {
  int rc = 0;
  static sqlite3_stmt *insert = NULL;
  const char *sql = "INSERT INTO file (path, link, dev, ino, uid, gid, mode, rdev, deleted )"
    " VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0);";


  fprintf(stderr, "insert file : %p\n", insert);
  if (!insert) {
    rc = sqlite3_prepare_v2(mem_db, sql, strlen(sql), &insert, NULL);
    if (rc) {
      return;
    }
  }

  sqlite3_bind_text(insert, 1, msg->file, -1, SQLITE_STATIC);
  sqlite3_bind_text(insert, 2, msg->link, -1, SQLITE_STATIC);

  sqlite3_bind_int(insert, 3, msg->dev);
  sqlite3_bind_int64(insert, 4, msg->ino);
  sqlite3_bind_int(insert, 5, msg->uid);
  sqlite3_bind_int(insert, 6, msg->gid);
  sqlite3_bind_int(insert, 6, msg->mode);
  sqlite3_bind_int(insert, 6, msg->rdev);

  sqlite3_step(insert);
  sqlite3_reset(insert);
  sqlite3_clear_bindings(insert);

}

int readMsg(int sd, FILE_MSG *msg) {
  ssize_t len = 0;

  while (1) {
    len = read(sd, msg, sizeof(FILE_MSG));
    if (len > 0)
      break;

    if (len == 0)
      return -1;

    if (errno = EINTR)
      continue;

    exit(0);
  }
  printf("Read Msg : %d %d : %d\n", (int) len, (int) msg->ino, (int) sizeof(FILE_MSG));
  printf("Read Msg : %s\n", msg->file);

  insertFile(msg);

  return 0;
}

void processMsg(FILE_MSG *msg) {
  switch (msg->type) {
    case FUNC_CHOWD:
      break;
    case FUNC_CHOWN:
      break;
    case FUNC_MKNOD:
      break;
    case FUNC_UNLINK:
      break;
    case FUNC_SYMLINK:
      break;
  }
}

void waitForMsg() {
  fd_set readfds;
  int count, maxfd;
  int i;
  FILE_MSG msg;

  while(1) {
    FD_ZERO(&readfds);
    FD_SET(mSocket, &readfds);

    maxfd = mSocket;
    for (i = 0; i < mSocketSize; i++) {
      int sd = mSocketList[i];
      FD_SET(sd, &readfds);
      maxfd = MAX(sd, maxfd);
    }

    count = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if (count < 0) {
      if (errno = EINTR)
        continue;
      exit(0);
    }

    for (i = 0; i < mSocketSize;) {
      int sd = mSocketList[i];
      if (FD_ISSET(sd, &readfds)) {
        if (readMsg(sd, &msg) < 0) {
          close(sd);
          socketListRemove(i);
          continue;
        }
        processMsg(&msg);
      }
      i++;
    }

    if (FD_ISSET(mSocket, &readfds)) {
      struct sockaddr_in addr;
      socklen_t len = sizeof(addr);
      int sd = accept(mSocket, (struct sockaddr *) &addr, &len);
      if (sd < 0) {
        exit(0);
      }
      socketListAdd(sd);
    }
  }
}

int init_db(void) {
  int rc = 0, i = 0;
  int rows = 0, columns = 0;
  char *errmsg = NULL;
  char **results;
  int found = 0;
  sqlite3_backup *backup = NULL;

  sqlite3_open(":memory:", &mem_db);
  sqlite3_open("files.db", &file_db);

  backup = sqlite3_backup_init(mem_db, "main", file_db, "main");
  if (backup) {
    sqlite3_backup_step(backup, -1);
    rc = sqlite3_backup_finish(backup);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "cannot read db from disk\n");
      exit(0);
    }
  }

  // Check Tables Exists
  rc = sqlite3_get_table(mem_db, "select name from sqlite_master where type = 'table' order by name;",
    &results, &rows, &columns, &errmsg);
  found = 0;
  for (i = 1; i <= rows; i++) {
    if (strcmp(results[i], "file") == 0) {
      found = 1;
    }
  }
  sqlite3_free_table(results);

  if (found == 0) {
    const char *create_file = "create table file ( id INTEGER PRIMARY KEY, path VARCHAR, link VARCHAR, "
      "dev INTEGER, ino INTEGER, uid INTEGER, gid INTEGER, mode INTEGER, rdev INTEGER, deleted INTEGER "
      ");";
    rc = sqlite3_exec(mem_db, create_file, NULL, NULL, &errmsg);
    if (rc) {
      return 1;
    }
  }

  return 0;
}

int main(int argc, char **argv) {
  pid_t pid;
  struct sigaction sa;
  int val = 1;
  int port = 0;
  int i = 0;
  sockaddr_in addr;
  socklen_t addr_len;

  if (init_db()) {
    return 1;
  }

  mSocket = socket(PF_INET, SOCK_STREAM, 0);
  if (mSocket < 0)
    return 1;

  val = 1;
  if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    return 1;

  val = 1;
  if (setsockopt(mSocket, SOL_TCP, TCP_NODELAY, &val, sizeof(val)) < 0)
    return 1;

  if (listen(mSocket, SOMAXCONN) < 0)
    return 1;

  addr_len = sizeof(addr);
  if (getsockname(mSocket, (struct sockaddr *) &addr, &addr_len) < 0)
    return 1;

  sa.sa_handler = cleanup;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  for (i = 1; i < NSIG; i++) {
    switch (i) {
      case SIGKILL:
      case SIGTSTP:
      case SIGCONT:
        break;
      default:
        sigaction(i, &sa, NULL);
        break;
    }
  }

  port = ntohs(addr.sin_port);
  /*
  if ((pid = fork()) != 0) {
    printf("%i:%i\n", port, pid);
    exit(0);
  }
  */

  printf("%i:%i\n", port, pid);
  waitForMsg();

  return 0;
}
