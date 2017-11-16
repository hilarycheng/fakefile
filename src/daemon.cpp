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
#include "message.h"

int mSocket = -1;
int mSocketList[1024];
int mSocketSize = 0;
unsigned char mTempBuf[4096];

void cleanup(int sig) {
  printf("Clean up Handler\n");
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
  printf("Read Msg : %d %d\n", (int) len, (int) msg->ino);
  printf("Read Msg : %s\n", msg->file);

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

int main(int argc, char **argv) {
  pid_t pid;
  struct sigaction sa;
  int val = 1;
  int port = 0;
  int i = 0;
  sockaddr_in addr;
  socklen_t addr_len;

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
