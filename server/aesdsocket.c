#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#define BUFFER_SIZE 4096

const int k_server_socket_failure = -1;
const int k_default_backlog = 32;
const int k_default_port = 9000;
const char k_write_file[] = "/var/tmp/aesdsocketdata";

/* Global flag for signal handling */
volatile sig_atomic_t keep_running = 1;

/* Signal handler */
void signal_handler(int signum) {
  syslog(LOG_INFO, "Caught signal, exiting");  // ADDED: Log message
  keep_running = 0;
}

/* SIGCHLD handler to reap zombie processes */
void sigchld_handler(int signum) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

int create_server_socket(unsigned short port, int backlog) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
    return k_server_socket_failure;
  }
  int k_reuse_addresses = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
        &k_reuse_addresses, sizeof(k_reuse_addresses)) < 0) {
    close(s);
    return k_server_socket_failure;
  }

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  if (bind(s, (struct sockaddr *)&address, sizeof(address)) == 0 &&
      listen(s, backlog) == 0) {
    return s;
  }

  close(s);
  return k_server_socket_failure;
}


int serve_client(int client_id) {
  int write_fd = open(k_write_file, O_RDWR | O_APPEND | O_CREAT, 0644);
  if (write_fd < 0) {
    syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
    return -1;
  }

  char *packet_buffer = NULL;
  size_t packet_size = 0;
  size_t packet_capacity = 0;

  char read_buffer[BUFFER_SIZE];

  // Read until we get a complete packet (ends with '\n')
  while (1) {
    ssize_t bytes_read = read(client_id, read_buffer, BUFFER_SIZE);

    if (bytes_read < 0) {
      if (errno == EINTR) continue;
      syslog(LOG_ERR, "Read failed: %s", strerror(errno));
      free(packet_buffer);
      close(write_fd);
      return -1;
    }

    if (bytes_read == 0) {
      // Client closed connection
      break;
    }

    // Check if we need to grow the packet buffer
    if (packet_size + bytes_read > packet_capacity) {
      packet_capacity = packet_size + bytes_read + BUFFER_SIZE;
      char *new_buffer = realloc(packet_buffer, packet_capacity);
      if (!new_buffer) {
        syslog(LOG_ERR, "Memory allocation failed");
        free(packet_buffer);
        close(write_fd);
        return -1;
      }
      packet_buffer = new_buffer;
    }

    // Append to packet buffer
    memcpy(packet_buffer + packet_size, read_buffer, bytes_read);
    packet_size += bytes_read;

    // Check if we received a newline (packet complete)
    if (memchr(read_buffer, '\n', bytes_read) != NULL) {
      // Found newline - packet is complete
      // Write the complete packet to file
      ssize_t written = 0;
      while (written < packet_size) {
        ssize_t w = write(write_fd, packet_buffer + written, 
            packet_size - written);
        if (w < 0) {
          syslog(LOG_ERR, "Write failed: %s", strerror(errno));
          free(packet_buffer);
          close(write_fd);
          return -1;
        }
        written += w;
      }


      if (lseek(write_fd, 0, SEEK_SET) < 0) {  
        syslog(LOG_ERR, "lseek failed: %s", strerror(errno));
        free(packet_buffer);
        close(write_fd);
        return -1;
      }

      // Send entire file back to client
      while (1) {
        ssize_t file_bytes = read(write_fd, read_buffer, BUFFER_SIZE);
        if (file_bytes < 0) {
          if (errno == EINTR) continue;
          syslog(LOG_ERR, "Read from file failed: %s", strerror(errno));
          break;
        }
        if (file_bytes == 0) break;

        ssize_t sent = 0;
        while (sent < file_bytes) {
          ssize_t s = write(client_id, read_buffer + sent, 
              file_bytes - sent);
          if (s < 0) {
            syslog(LOG_ERR, "Send to client failed: %s", strerror(errno));
            free(packet_buffer);
            close(write_fd);
            return -1;
          }
          sent += s;
        }
      }

      // Seek back to end for next append
      if (lseek(write_fd, 0, SEEK_END) < 0) {
        syslog(LOG_ERR, "lseek failed: %s", strerror(errno));
        free(packet_buffer);
        close(write_fd);
        return -1;
      }

      // Reset for next packet
      packet_size = 0;
    }
  }

  free(packet_buffer);
  close(write_fd);
  close(client_id);
  return 0;
}

int daemonize() {
  pid_t pid = fork();

  if (pid < 0) {
    return -1;  // Fork failed
  }

  if (pid > 0) {
    exit(EXIT_SUCCESS);  // Parent exits
  }

  // Child continues
  if (setsid() < 0) {
    return -1;  // Create new session
  }

  // Change working directory to root
  if (chdir("/") < 0) {
    return -1;
  }

  // Close standard file descriptors
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // Redirect to /dev/null
  open("/dev/null", O_RDONLY);  // stdin
  open("/dev/null", O_RDWR);    // stdout
  open("/dev/null", O_RDWR);    // stderr

  return 0;
}

int main(int argc, char *argv[]) {
  
  bool daemon_mode = false;

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
      daemon_mode = true;
  }

  openlog(NULL, 0, LOG_USER);

  // Set up signal handlers 
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, NULL);

  int server = create_server_socket(k_default_port, k_default_backlog);
  if (server == k_server_socket_failure) {
    perror("Failed to create server socket");
    return -1;
  }

    if (daemon_mode) {
      if (daemonize() < 0) {
          syslog(LOG_ERR, "Failed to daemonize");
          close(server);
          return -1;
      }
      syslog(LOG_INFO, "Running as daemon");
  }

  while (keep_running) {
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    socklen_t addr_len = sizeof(address); 
    int client = accept(server, (struct sockaddr *)&address, &addr_len);

    if (client < 0) {
      if (errno == EINTR) continue;
      syslog(LOG_ERR, "Failed to accept: %s", strerror(errno));
      continue;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip_str, sizeof(ip_str));
    syslog(LOG_INFO, "Accepted connection from %s", ip_str);  // CHANGED: LOG_DEBUG -> LOG_INFO

    pid_t pid = fork();
    if (pid == 0) {
      // Child process
      close(server);
      serve_client(client);
      syslog(LOG_INFO, "Closed connection from %s", ip_str);  // CHANGED: LOG_DEBUG -> LOG_INFO
      exit(EXIT_SUCCESS);
    } else if (pid > 0) {
      // Parent process
      close(client);
    } else {
      syslog(LOG_ERR, "Fork failed: %s", strerror(errno));
      close(client);
    }
  }

  close(server);
  while (wait(NULL) > 0);  // Wait for children

  /* Delete the temp file */
  if (unlink(k_write_file) < 0) {
    syslog(LOG_ERR, "Failed to delete %s: %s", k_write_file, strerror(errno));
  }

  return 0;
}
