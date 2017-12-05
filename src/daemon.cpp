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
#include <time.h>
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

void queryFile(int sd, FILE_MSG *msg) {
  static sqlite3_stmt *select = NULL;
  int rc = 0;
  const char *sql = "SELECT * FROM file WHERE ino = ?;";

  if (!select) {
    rc = sqlite3_prepare_v2(mem_db, sql, strlen(sql), &select, NULL);
    if (rc) 
      return;
  }

  rc = sqlite3_bind_int64(select, 1, msg->ino);
  rc = sqlite3_step(select);

  printf("Query File : %s %d = %d\n", msg->file, rc, SQLITE_ROW);
  if (rc == SQLITE_ROW) {
    if (sqlite3_column_text(select, 2) != NULL) {
      strncpy(msg->link, (char *) sqlite3_column_text(select, 2), PATH_MAX);
    }
    msg->dev   = sqlite3_column_int(select, 3);
    msg->ino   = (unsigned long) sqlite3_column_int64(select, 4);
    msg->uid   = sqlite3_column_int(select, 5);
    msg->gid   = sqlite3_column_int(select, 6);
    msg->mode  = sqlite3_column_int(select, 7);
    msg->rdev  = sqlite3_column_int(select, 8);
    msg->nlink = sqlite3_column_int(select, 9);
    printf("Query File : %s , Dev : %d, Ino : %d, uid : %d, gid : %d, mode : %d, rdev : %d, nlink : %d\n",
        msg->file, msg->dev, msg->ino, msg->uid, msg->gid, msg->mode, msg->rdev , msg->nlink);
  } else {
    msg->type |= 0x8000;
  }

  write(sd, msg, sizeof(FILE_MSG));
  sqlite3_reset(select);
  sqlite3_clear_bindings(select);
}

void updateFile(FILE_MSG *msg) {
  int rc = 0;
  static sqlite3_stmt *update = NULL;
  const char *sql = "UPDATE file SET dev = ?, file = ?, uid = ?, gid = ?, mode = ?, rdev = ?, nlink = ? WHERE ino = ?";

  if (!update) {
    rc = sqlite3_prepare_v2(mem_db, sql, strlen(sql), &update, NULL);
    if (rc) {
      return; 
    }
  }

  // sqlite3_bind_text(update, 1, msg->link, -1, SQLITE_STATIC);
  sqlite3_bind_int(update, 1, msg->dev);
  sqlite3_bind_text(update, 2, msg->file, -1, SQLITE_STATIC);
  sqlite3_bind_int(update, 3, msg->uid);
  sqlite3_bind_int(update, 4, msg->gid);
  sqlite3_bind_int(update, 5, msg->mode);
  sqlite3_bind_int(update, 6, msg->rdev);
  sqlite3_bind_int(update, 7, msg->nlink);

  sqlite3_bind_int64(update, 8, msg->ino);

  sqlite3_step(update);
  sqlite3_reset(update);
  sqlite3_clear_bindings(update);
}

void insertFile(FILE_MSG *msg) {
  int rc = 0;
  static sqlite3_stmt *insert = NULL;
  const char *sql = "INSERT INTO file (path, link, dev, ino, uid, gid, mode, rdev, nlink, deleted )"
    " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0);";


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
  sqlite3_bind_int(insert, 7, msg->mode);
  sqlite3_bind_int(insert, 8, msg->rdev);
  sqlite3_bind_int(insert, 9, msg->nlink);

  rc = sqlite3_step(insert);
  fprintf(stderr, "insert sql : %d %s\n", rc, msg->file);
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

    if (errno == EINTR)
      continue;

    exit(0);
  }

  if (msg->type != FUNC_CHOWN && msg->type != FUNC_STAT && msg->type != FUNC_CHMOD) {
    printf("Insert Msg Type : %d %d\n", msg->type, FUNC_SYMLINK);
    insertFile(msg);
  } else if (msg->type == FUNC_STAT) {
    queryFile(sd, msg);
  } else if (msg->type == FUNC_CHOWN || msg->type == FUNC_CHMOD) {
    updateFile(msg);
  }

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

  fprintf(stderr, "Found Tables : %d\n", found);
  if (found == 0) {
    const char *create_file = "create table file ( id INTEGER PRIMARY KEY, path VARCHAR, link VARCHAR, "
      "dev INTEGER, ino INTEGER, uid INTEGER, gid INTEGER, mode INTEGER, rdev INTEGER, nlink INTEGER, "
      " deleted INTEGER "
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
