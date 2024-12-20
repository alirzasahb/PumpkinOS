#include "sys.h"
#include "script.h"
#include "pit_io.h"
#include "secure.h"
#include "httpd.h"
#include "mime.h"
#include "thread.h"
#include "plist.h"
#include "ts.h"
#include "timeutc.h"
#include "mutex.h"
#include "util.h"
#include "debug.h"
#include "xalloc.h"

#define KEEP_ALIVE    "keep-alive"
#define CLOSE         "close"
#define REALM         "HTTP"
#define MAX_PATH      256
#define MAX_HEADERS   64

#define TAG_HTTPD   "HTTPD"
#define TAG_WORKER  "WORKER"

typedef struct httpd_server_t {
  mutex_t *mutex;
  int sock;
  int multithreaded;
  char *system;
  char *home;
  char *user;
  char *password;
  node_t *types;
  secure_provider_t *secure;
  secure_config_t *sc;
  secure_t *s;
  int (*callback)(http_connection_t *con);
  void *data;
} httpd_server_t;

static int conn_action(void *arg);
static int unescape_url(char *url);
static void plus2space(char *s);

static char *weekday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Ago", "Sep", "Oct", "Nov", "Dec"};

static int httpd_init(char *host, int port) {
  int sock, p;

  p = port;
  if ((sock = sys_socket_bind(host, &p, IP_STREAM)) == -1) {
    return -1;
  }

  debug(DEBUG_INFO, "WEB", "HTTP server initialized on port %d", p);

  return sock;
}

static int httpd_spawn(int sock, char *host, int port, httpd_server_t *server, int timeout, ext_type_t *types, int num_types) {
  http_connection_t *con;
  int handle;

  if ((con = xcalloc(1, sizeof(http_connection_t))) == NULL) {
    return -1;
  }

  if (server->secure && (con->s = server->secure->connect(server->sc, host, port, sock)) == NULL) {
    xfree(con);
    return -1;
  }

  con->tag = TAG_CONN;
  con->sock = sock;
  con->secure = server->secure;
  con->timeout = timeout;
  con->system = server->system;
  con->home = server->home;
  con->callback = server->callback;
  con->data = server->data;
  con->user = server->user;
  con->password = server->password;
  con->types = types;
  con->num_types = num_types;

  con->remote_port = port;
  sys_strncpy(con->host, host, MAX_HOST-1);
  debug(DEBUG_INFO, "WEB", "connection from client %s:%d", con->host, con->remote_port);

  if (server->multithreaded) {
    if ((handle = thread_begin(TAG_WORKER, conn_action, con)) == -1) {
      if (server->secure) server->secure->close(con->s);
      xfree(con);
    }
  } else {
    conn_action(con);
    handle = 0;
  }

  return handle;
}

static unsigned char code64(char c) {
  int k;
  static char code[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  if (c == '=') {
    return 0;
  }
  for (k = 0; code[k]; k++) {
    if (c == code[k]) break;
  }
  if (!code[k]) {
    return 255;
  }

  return k;
}

static int authenticate(char *s, char *correct_user, char *correct_password) {
  char r[256], user[256], password[256];
  unsigned char c0, c1, c2, c3;
  int i, j;
 
  if (s == NULL || s[0] == '\0') {
    return -1;
  }
 
  for (i = 0, j = 0; s[i] && j < 255; ) {
    c0 = code64(s[i++]);
    c1 = code64(s[i++]);
    c2 = code64(s[i++]);
    c3 = code64(s[i++]);
    if (c0 == 255 || c1 == 255 || c2 == 255 || c3 == 255)
      return -1;
    r[j++] = (c0 << 2) | (c1 >> 4);
    r[j++] = (c1 << 4) | (c2 >> 2);
    r[j++] = (c2 << 6) | c3;
  }

  i = sys_strlen((char *)s)-1;
  if (s[i] == '=') {
    j--;
  }
  if (s[i-1] == '=') {
    j--;
  }
 
  r[j] = '\0';
 
  if (sys_strlen(r) > 128 || sys_sscanf(r, "%255[^:]:%255s", user, password) != 2) {
    return -1;
  }
 
  if (!sys_strcmp(user, correct_user) && !sys_strcmp(password, correct_password)) {
    return 0;
  }

  return -1;
}

static uint64_t parse_date(char *date) {
  int day, year, hour, min, sec, i, j, n;
  char wday[4], mon[4], tz[4];
  sys_tm_t tm;
  uint64_t t;

  // Wed, 23 Mar 2016 14:19:36 GMT
  
  wday[3] = 0;
  mon[3] = 0;
  tz[3] = 0;
  t = 0;
  n = 0;

  if (date && (n = sys_sscanf(date, "%c%c%c, %d %c%c%c %d %02d:%02d:%02d %c%c%c", wday, wday+1, wday+2, &day, mon, mon+1, mon+2,
    &year, &hour, &min, &sec, tz, tz+1, tz+2)) == 14) {

    if (!sys_strcmp(tz, "GMT")) {
      for (i = 0; i < 7; i++) {
        if (!sys_strcmp(wday, weekday[i])) break;
      }
      if (i < 7) {
        for (j = 0; j < 12; j++) {
          if (!sys_strcmp(mon, month[j])) break;
        }
        if (j < 12) {
          tm.tm_year = year - 1900;
          tm.tm_mon = j;
          tm.tm_mday = day;
          tm.tm_hour = hour;
          tm.tm_min = min;
          tm.tm_sec = sec;
          t = timeutc(&tm);
        } else {
          debug(DEBUG_ERROR, "WEB", "invalid date \"%s\" (invalid month)", date);
        }
      } else {
        debug(DEBUG_ERROR, "WEB", "invalid date \"%s\" (invalid week day)", date);
      }
    } else {
      debug(DEBUG_ERROR, "WEB", "invalid date \"%s\" (not GNT)", date);
    }
  } else {
    debug(DEBUG_ERROR, "WEB", "invalid date \"%s\" (only %d fields)", date, n);
  }

  return t;
}

static int httpd_handle(http_connection_t *con) {
  char *path, *p, *q, *e;
  sys_stat_t statbuf;
  uint64_t if_modified_since;
  char *value;
  int n, i, isget, r;

  isget = 0;

  if (!sys_strncmp(con->buffer, "GET /", 5)) {
    con->buffer[3] = 0;
    con->uri = &con->buffer[4];
    isget = 1;

  } else if (!sys_strncmp(con->buffer, "POST /", 6)) {
    con->buffer[4] = 0;
    con->uri = &con->buffer[5];

  } else if (!sys_strncmp(con->buffer, "PUT /", 5)) {
    con->buffer[3] = 0;
    con->uri = &con->buffer[4];

  } else if (!sys_strncmp(con->buffer, "DELETE /", 8)) {
    con->buffer[6] = 0;
    con->uri = &con->buffer[7];

  } else {
    debug(DEBUG_ERROR, "WEB", "invalid method");
    return httpd_reply(con, 400);
  }

  con->method = con->buffer;
  debug(DEBUG_TRACE, "WEB", "method %s", con->method);

  if ((p = sys_strchr(con->uri, ' ')) == NULL) {
    debug(DEBUG_ERROR, "WEB", "no space in uri");
    return httpd_reply(con, 400);
  }
  p[0] = 0;
  con->protocol = p+1;

  unescape_url(con->uri);
  debug(DEBUG_INFO, "WEB", "%s \"%s\"", con->method, con->uri);

  if (sys_strchr(con->uri, '\\') || sys_strstr(con->uri, "..")) {
    debug(DEBUG_ERROR, "WEB", "\\ or .. in uri");
    return httpd_reply(con, 400);
  }

  if ((p = sys_strstr(con->protocol, "\r\n")) == NULL) {
    debug(DEBUG_ERROR, "WEB", "\\r\\n not found after protocol");
    return httpd_reply(con, 400);
  }
  p[0] = 0;
  con->headers = p+2;
  debug(DEBUG_TRACE, "WEB", "protocol %s", con->protocol);

  if (sys_strlen(con->protocol) != 8 || sys_strncmp(con->protocol, "HTTP/1.", 7) ||
      (con->protocol[7] != '0' && con->protocol[7] != '1')) {
    debug(DEBUG_ERROR, "WEB", "invalid protocol");
    return httpd_reply(con, 400);
  }

  con->num_params = 0;

  if ((p = sys_strchr(con->uri, '?')) != NULL) {
    p[0] = 0;
    p++;
    plus2space(p);

    for (; p[0] && con->num_params < MAX_REQ_PARAMS;) {
      con->param_name[con->num_params] = p;

      q = sys_strchr(p, '&');
      e = sys_strchr(p, '=');

      if (q && e) {
        if (q < e) {
          e = NULL;
        } else {
          q = NULL;
        }
      }

      if (q) {
        q[0] = 0;
        con->param_value[con->num_params] = q;
        p = q+1;

      } else if (e) {
        e[0] = 0;
        con->param_value[con->num_params] = e+1;
        p = e+1;

        if ((q = sys_strchr(p, '&')) != NULL) {
          q[0] = 0;
          p = q+1;

        } else {
          p += sys_strlen(p);
        }

      } else {
        p += sys_strlen(p);
      }

      con->num_params++;
    }
  }

  debug(DEBUG_TRACE, "WEB", "uri \"%s\"", con->uri);

  if_modified_since = 0;

  con->num_headers = 0;
  for (p = con->headers; p && p[0] && con->num_headers < MAX_HEADERS;) {
    if ((q = sys_strstr(p, "\r\n")) == NULL) break;
    q[0] = 0;

    if ((value = sys_strstr(p, ": ")) == NULL) {
      debug(DEBUG_ERROR, "WEB", "\":\" not found in header ");
      return httpd_reply(con, 400);
    }
    value[0] = 0;
    value += 2;
    con->header_name[con->num_headers] = p;
    con->header_value[con->num_headers] = value;
    p = q+2;

    if (!sys_strcmp(con->header_name[con->num_headers], "Authorization") &&
        !sys_strncmp(con->header_value[con->num_headers], "Basic ", 6)) {
      con->authorization = con->header_value[con->num_headers] + 6;

    } else if (!sys_strcmp(con->header_name[con->num_headers], "Content-Length")) {
      con->content_length = sys_atoi(con->header_value[con->num_headers]);

    } else if (!sys_strcmp(con->header_name[con->num_headers], "Content-Type")) {
      con->content_type = con->header_value[con->num_headers];

    } else if (!sys_strcmp(con->header_name[con->num_headers], "Connection")) {
      con->keepalive = !sys_strcasecmp(con->header_value[con->num_headers], "keep-alive");
      debug(DEBUG_TRACE, "WEB", "keep alive %d", con->keepalive);

    } else if (!sys_strcmp(con->header_name[con->num_headers], "If-Modified-Since")) {
      if_modified_since = parse_date(con->header_value[con->num_headers]);
      debug(DEBUG_TRACE, "WEB", "if modified since (%s)", con->header_value[con->num_headers]);
    }

    con->num_headers++;
  }

  for (i = 0; i < con->num_headers; i++) {
    debug(DEBUG_TRACE, "WEB", "header %d \"%s\" = \"%s\"", i, con->header_name[i], con->header_value[i]);
  }

  for (i = 0; i < con->num_params; i++) {
    debug(DEBUG_TRACE, "WEB", "param %d \"%s\" = \"%s\"", i, con->param_name[i], con->param_value[i]);
  }

  if (con->user && con->password) {
    debug(DEBUG_TRACE, "WEB", "authorization \"%s\"", con->authorization);

    if (authenticate(con->authorization, con->user, con->password) != 0) {
      return httpd_reply(con, 401);
    }
  }

  if ((path = xcalloc(MAX_PATH, 1)) == NULL) {
    return httpd_reply(con, 500);
  }

  if (con->home) {
    sys_strncpy(path, con->home, MAX_PATH-1);
  }

  if (!sys_strcmp(con->uri, "/")) {
    sys_strncat(path, "/index.html", MAX_PATH-sys_strlen(path)-1);
  } else {
    sys_strncat(path, con->uri, MAX_PATH-sys_strlen(path)-1);
  }

  n = sys_strlen(path);
  if (n > 0 && path[n-1] == '/') {
    path[n-1] = 0;
  }

  debug(DEBUG_TRACE, "WEB", "path \"%s\"", path);

  if (sys_stat(path, &statbuf) == -1 || (statbuf.mode & SYS_IFDIR)) {
    debug(DEBUG_TRACE, "WEB", "calling callback");
    con->status = HTTPD_ACTION;
    if ((r = con->callback(con)) != 0) {
      debug(DEBUG_ERROR, "WEB", "callback failed (%d)", r);
      xfree(path);
      return -1;
    }
    xfree(path);
    return 0;
  }

  if (!isget) {
    xfree(path);
    debug(DEBUG_ERROR, "WEB", "invalid method on file");
    return httpd_reply(con, 400);
  }

  if (if_modified_since && statbuf.mtime <= if_modified_since) {
    debug(DEBUG_TRACE, "WEB", "\"%s\" not modified", path);
    xfree(path);
    httpd_reply(con, 304);
    return 0;
  }

  debug(DEBUG_INFO, "WEB", "sending file \"%s\"", path);
  r = httpd_file(con, path);
  xfree(path);

  return r;
}

static char *make_date(uint64_t t, char *s, int n) {
  int day, mon, year, wday, hour, min, sec;

  ts2time(t, &day, &mon, &year, &wday, &hour, &min, &sec);
  sys_snprintf(s, n, "%s, %02d %s %02d %02d:%02d:%02d GMT", weekday[wday], day, month[mon-1], year, hour, min, sec);

  return s;
}

static void make_reply(char *header, int hlen, int code, char *msg, char *system) {
  char date[80];

  make_date(sys_time(), date, sizeof(date)-1);
  sys_snprintf(header, hlen, "HTTP/1.1 %d %s\r\nServer: %s\r\nDate: %s\r\n", code, msg, system, date);
}

static void make_header(char *header, int hlen, int code, char *status, char *type, int length, uint64_t modified, int cache, http_connection_t *con) {
  char data_mod[80], op_mod[80], op_length[80], op_cache[80];
  int n, i;

  make_reply(header, hlen, code, status, con->system);
  n = sys_strlen(header);

  if (type) {
    if (length >= 0) {
      sys_snprintf(op_length, sizeof(op_length)-1, "\r\nContent-Length: %d", length);
    } else {
      op_length[0] = '\0';
    }

    if (modified) {
      make_date(modified, data_mod, sizeof(data_mod)-1);
      sys_strncpy(op_mod, "\r\nLast-modified: ", sizeof(op_mod)-1);
      sys_strncat(op_mod, data_mod, sizeof(op_mod)-sys_strlen(op_mod)-1);
    } else {
      op_mod[0] = '\0';
    }

    if (!cache) {
      sys_strncpy(op_cache, "\r\nPragma: no-cache", sizeof(op_cache)-1);
    } else {
      op_cache[0] = '\0';
    }

    sys_snprintf(header+n, hlen-n, "MIME-version: 1.0%s\r\nContent-Type: %s%s%s\r\n", op_cache, type, op_length, op_mod);
    n = sys_strlen(header);
  }

  for (i = 0; i < con->num_res_headers; i++) {
    if (con->res_header_name[i] && con->res_header_value[i]) {
      sys_snprintf(header+n, hlen-n, "%s: %s\r\n", con->res_header_name[i], con->res_header_value[i]);
      n = sys_strlen(header);
    }
  }

  sys_snprintf(header+n, hlen-n, "Connection: %s\r\n\r\n", con->keepalive ? KEEP_ALIVE : CLOSE);
}

int httpd_set_header(http_connection_t *con, char *name, char *value) {
  if (con->num_res_headers < MAX_REQ_HEADERS) {
    con->res_header_name[con->num_res_headers] = xstrdup(name);
    con->res_header_value[con->num_res_headers] = xstrdup(value);
    con->num_res_headers++;
    return 0;
  }

  return -1;
}

// return -1: error
// return  0: nothing to read from fd
// return  1, nread = 0: nothing was read from fd
// return  1, nread > 0: read nread bytes from fd

static int httpd_read(http_connection_t *con, uint8_t *buffer, int len, int *nread, uint32_t us) {
  int r;

  if (con->secure) {
    r = con->secure->read(con->s, (char *)buffer, len);
    if (r > 0) {
      *nread = r;
      r = 1;
    }
    return r;
  }

  return sys_read_timeout(con->sock, buffer, len, nread, us);
}

static int httpd_write(http_connection_t *con, uint8_t *buffer, int len) {
  if (con->secure) {
    return con->secure->write(con->s, (char *)buffer, len);
  }

  return sys_write(con->sock, buffer, len);
}

static char *status_msg(int code) {
  char *msg;

  switch (code) {
    case 200: msg = "OK"; break;
    case 304: msg = "Not Modified"; break;
    case 400: msg = "Bad Request"; break;
    case 401: msg = "Unauthorized"; break;
    case 403: msg = "Forbidden"; break;
    case 404: msg = "Not Found"; break;
    case 500: msg = "Internal Server Error"; break;
    case 501: msg = "Not Implemented"; break;
    case 503: msg = "Service Unavailable"; break;
    default : msg = "Service Unavailable"; break;
  }

  return msg;
}

int httpd_string(http_connection_t *con, int code, char *str, char *mime) {
  int len, n;
  char *buffer;

  len = sys_strlen(str);

  n = len;
  if (n < MAX_PACKET) n = MAX_PACKET;
  else if (n > 65536) n = 65536;

  if ((buffer = xmalloc(n)) == NULL) {
    return httpd_reply(con, 500);
  }

  make_header(buffer, n-1, code, status_msg(code), mime, len, 0, 1, con);
  httpd_write(con, (uint8_t *)buffer, sys_strlen(buffer));
  httpd_write(con, (uint8_t *)str, len);
  xfree(buffer);

  return 0;
}

int httpd_file_stream(http_connection_t *con, int fd, char *mime, uint64_t mtime) {
  int len, n, i;
  char *buffer;

  if (sys_seek(fd, 0, SYS_SEEK_END) == -1) {
    return httpd_reply(con, 500);
  }
  len = sys_seek(fd, 0, SYS_SEEK_CUR);

  if (sys_seek(fd, 0, SYS_SEEK_SET) == -1) {
    return httpd_reply(con, 500);
  }

  n = len;
  if (n < MAX_PACKET) n = MAX_PACKET;
  else if (n > 65536) n = 65536;

  if ((buffer = xmalloc(n)) == NULL) {
    return httpd_reply(con, 500);
  }

  make_header(buffer, n-1, 200, "OK", mime, len, mtime, 1, con);
  httpd_write(con, (uint8_t *)buffer, sys_strlen(buffer));
  debug(DEBUG_TRACE, "WEB", "sending header \"%s\"", buffer);

  for (; !thread_must_end();) {
    if ((i = sys_read(fd, (uint8_t *)buffer, n)) <= 0) break;
    httpd_write(con, (uint8_t *)buffer, i);
  }

  xfree(buffer);

  return 0;
}

int httpd_file(http_connection_t *con, char *filename) {
  sys_stat_t statbuf;
  char *ext, *mimetype;
  int i, f, r;

  if (sys_stat(filename, &statbuf) == -1) {
    return httpd_reply(con, 404);
  }

  if ((f = sys_open(filename, SYS_READ)) == -1) {
    return httpd_reply(con, 404);
  }

  ext = getext(filename);
  mimetype = NULL;

  if (ext) {
    for (i = 0; i < con->num_types; i++) {
      if (!sys_strcmp(ext, con->types[i].ext)) {
        mimetype = con->types[i].mimetype;
        break;
      }
    }

    if (mimetype == NULL) {
      if (!sys_strcmp(ext, "jpg")) {
        mimetype = MIME_TYPE_JPEG;
      } else if (!sys_strcmp(ext, "png")) {
        mimetype = MIME_TYPE_PNG;
      } else if (!sys_strcmp(ext, "html")) {
        mimetype = MIME_TYPE_HTML;
      } else if (!sys_strcmp(ext, "js")) {
        mimetype = MIME_TYPE_JS;
      } else if (!sys_strcmp(ext, "css")) {
        mimetype = MIME_TYPE_CSS;
      } else if (!sys_strcmp(ext, "txt")) {
        mimetype = MIME_TYPE_TEXT;
      }
    }
  }

  r = httpd_file_stream(con, f, mimetype ? mimetype : MIME_TYPE_BINARY, statbuf.mtime);
  sys_close(f);

  return r;
}

int httpd_reply(http_connection_t *con, int code) {
  char buffer[MAX_PACKET];
  char body[256];
  char *msg;
  int r;

  msg = status_msg(code);
  r = (code == 200) ? 0 : -1;

  debug(code < 400 ? DEBUG_INFO : DEBUG_ERROR, "WEB", "reply %d (%s)", code, msg);
  make_reply(buffer, sizeof(buffer)-1, code, msg, con->system);

  body[0] = 0;
  if (code >= 400 && code < 600) {
    sys_snprintf(body, sizeof(body)-1, "<head><title>%d %s</title></head>\n<body><h1>%d %s</h1></body></html>", code, msg, code, msg);
  }

  if (code == 401) {
    sys_snprintf(&buffer[sys_strlen(buffer)], sizeof(buffer)-sys_strlen(buffer)-1, "WWW-Authenticate: Basic realm=\"%s\"\r\n", REALM);
  }

  if (body[0]) {
    sys_snprintf(&buffer[sys_strlen(buffer)], sizeof(buffer)-sys_strlen(buffer)-1, "MIME-version: 1.0\r\nContent-Type: text/html\r\nContent-Length: %d\r\n", (int)sys_strlen(body));
  }

  sys_strncat(buffer, "Connection: close\r\n\r\n", sizeof(buffer)-sys_strlen(buffer)-1);
  httpd_write(con, (uint8_t *)buffer, sys_strlen(buffer));

  if (body[0]) {
    httpd_write(con, (uint8_t *)body, sys_strlen(body));
  }

  return r;
}

static int httpd_closesock(secure_provider_t *secure, secure_t *s, int sock) {
  int r = -1;

  if (secure && s) {
    secure->close(s);
  }

  if (sock != -1) {
    r = sys_close(sock);
    debug(DEBUG_INFO, "WEB", "HTTP server closed");
  }

  return r;
}

static int con_save(http_connection_t *con, uint32_t us) {
  unsigned char buf[1024];
  int nread, n, r = 0;

  for (; !thread_must_end();) {
    n = httpd_read(con, buf, sizeof(buf), &nread, us);
    if (n == 0) break;
    if (n < 0) {
      r = -1;
      break;
    }
    if (nread == 0) break;

    if (sys_write(con->body_fd, buf, nread) != nread) {
      r = -1;
      break;
    }
  }

  return r;
}

static int conn_action(void *arg) {
  http_connection_t *con;
  int i, r, n, nread, err, timeouts;
  char *p;

  con = (http_connection_t *)arg;
  timeouts = 0;
  n = 0;

  for (; !thread_must_end() && n < MAX_PACKET-1 && (con->timeout < 0 || timeouts < con->timeout);) {
    r = httpd_read(con, (unsigned char *)&con->buffer[n], MAX_PACKET-1-n, &nread, 100000);
    debug(DEBUG_TRACE, "WEB", "read r=%d n=%d ", r, nread);
    if (r < 0) break;
    if (r == 0 || nread == 0) {
      timeouts++;
      continue;
    }
    if (nread == 0) break;

    timeouts = 0;
    n += nread;
    con->buffer[n] = 0;
    con->body_fd = 0;
    debug(DEBUG_TRACE, "WEB", "buffer \"%s\"", con->buffer);

    if ((p = sys_strstr(con->buffer, "\r\n\r\n")) == NULL) {
      continue;
    }

    // XXX
    if ((p = sys_strstr(con->buffer, "\r\n\r\n")) != NULL) {
      if ((con->body_fd = sys_mkstemp()) != -1) {
        n -= (p + 4 - con->buffer);
        p[2] = 0;
        err = 0;

        if (n > 0) {
          if (sys_write(con->body_fd, (uint8_t *)(p+4), n) != n) {
            err = -1;
          } else {
            err = con_save(con, 100000);
          }
        }

        if (err) {
          httpd_reply(con, 500);
          r = -1;
        } else {
          sys_seek(con->body_fd, 0, SYS_SEEK_SET);
          r = httpd_handle(con);
        }

        sys_close(con->body_fd);
        con->body_fd = 0;
        n = 0;
        // can't handle more than one request because the callback
        // inside httpd_handle may have freed resources
        //if (!con->keepalive || r != 0) break;
        break;

      } else {
        httpd_reply(con, 500);
        break;
      }

    } else {
      debug(DEBUG_ERROR, "WEB", "end of request header not found in \"%s\"", con->buffer);
      httpd_reply(con, 400);
      break;
    }
  }

  debug(DEBUG_INFO, "WEB", "client %s:%d disconnected", con->host, con->remote_port);

  thread_end(TAG_WORKER, thread_get_handle());
  httpd_closesock(con->secure, con->s, con->sock);

  for (i = 0; i < con->num_res_headers; i++) {
    if (con->res_header_name[i]) xfree(con->res_header_name[i]);
    if (con->res_header_value[i]) xfree(con->res_header_value[i]);
  }

  if (con->types) xfree(con->types);
  xfree(con);

  return 0;
}

static void plus2space(char *s) {
  int i;

  if (!s) return;
  for (i = 0; s[i]; i++) {
    if (s[i] == '+') s[i] = ' ';
  }
}

static char x2c(char *code) {
  char digit;

  digit = (code[0] >= 'A' ? ((code[0] & 0xdf) - 'A')+10 : (code[0] - '0'));
  digit *= 16;
  digit += (code[1] >= 'A' ? ((code[1] & 0xdf) - 'A')+10 : (code[1] - '0'));

  return digit;
}

static int unescape_url(char *url) {
  int i, j;
  char aux[1024];

  sys_memset(aux, 0, sizeof(aux));
  sys_strncpy(aux, url, sizeof(aux)-1);

  for (i = 0, j = 0; aux[j]; i++, j++) {
    url[i] = aux[j];
    if (url[i] == '%') {
      url[i] = x2c(&aux[j+1]);
      j += 2;
    }
  }

  url[i] = '\0';
  return 0;
}

static int httpd_action(void *arg) {
  httpd_server_t *server;
  http_connection_t *con;
  sys_timeval_t tv;
  char host[MAX_HOST];
  int sock, port;

  server = (httpd_server_t *)arg;
  con = xmalloc(sizeof(http_connection_t));
  con->status = HTTPD_START;
  con->data = server->data;
  server->callback(con);

  for (; !thread_must_end();) {
    tv.tv_sec = 0;
    tv.tv_usec = server->multithreaded ? 100000 : 0;
    if ((sock = sys_socket_accept(server->sock, host, MAX_HOST, &port, &tv)) == -1) break;

    if (sock > 0) {
      if (httpd_spawn(sock, host, port, server, 10, NULL, 0) == -1) {
        sys_close(sock);
      }
    } else {
      con->status = HTTPD_IDLE;
      if (server->callback(con) != 0) break;
    }
  }

  httpd_closesock(server->secure, server->s, server->sock);

  mutex_destroy(server->mutex);
  if (server->home) xfree(server->home);
  if (server->user) xfree(server->user);
  if (server->password) xfree(server->password);

  con->status = HTTPD_STOP;
  server->callback(con);
  xfree(con);
  xfree(server);

  return 0;
}

int httpd_create(char *host, int port, char *system, char *home, char *user, char *password, secure_provider_t *secure, char *cert, char *key, int (*callback)(http_connection_t *con), void *data, int multithreaded) {
  httpd_server_t *server;
  int handle;

  if (system == NULL || home == NULL) {
    return -1;
  }

  if ((server = xcalloc(1, sizeof(httpd_server_t))) == NULL) {
    return -1;
  }

  if ((server->mutex = mutex_create("httpd")) == NULL) {
    xfree(server);
    return -1;
  }

  if ((server->sock = httpd_init(host, port)) == -1) {
    mutex_destroy(server->mutex);
    xfree(server);
    return -1;
  }

  if (secure) {
    if ((server->sc = secure->new(cert, key)) == NULL) {
      mutex_destroy(server->mutex);
      xfree(server);
      return -1;
    }
    if ((server->s = secure->connect(server->sc, host, port, server->sock)) == NULL) {
      mutex_destroy(server->mutex);
      xfree(server);
      return -1;
    }
  }

  server->multithreaded = multithreaded;
  server->system = xstrdup(system);
  server->home = xstrdup(home);
  server->user = user && user[0] ? xstrdup(user) : NULL;
  server->password = password && password[0] ? xstrdup(password) : NULL;
  server->secure = secure;
  server->callback = callback;
  server->data = data;

  if (multithreaded) {
    if ((handle = thread_begin(TAG_HTTPD, httpd_action, server)) == -1) {
      mutex_destroy(server->mutex);
      httpd_closesock(server->secure, server->s, server->sock);
      xfree(server);
    }
  } else {
    httpd_action(server);
    handle = 0;
  }

  return handle;
}

int httpd_close(int handle) {
  return thread_end(TAG_HTTPD, handle);
}
