#include <stdio.h>
#include <uv.h>
#include <stdlib.h>

uv_loop_t *loop;
#define DEFAULT_PORT 10002

//连接队列最大长度
#define DEFAULT_BACKLOG 128

//负责为新来的消息申请空间
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->len = suggested_size;
  buf->base = static_cast<char *>(malloc(suggested_size));
}

void on_close(uv_handle_t *handle) {
  if (handle != NULL)
    free(handle);
}

void echo_write(uv_write_t *req, int status) {
  if (status) {
    fprintf(stderr, "Write error %s\n", uv_strerror(status));
  }

  free(req);
}

/**
 * @brief: 负责处理新来的消息
 * @param: client
 * @param: nread>0表示有数据就绪，nread<0表示异常，nread是有可能为0的，但是这并不是异常或者结束
 * @author: sherlock
 */
void read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
  if (nread > 0) {
//    buf->base[nread] = 0;
    fprintf(stdout, "recv:%s\n", buf->base);
    fflush(stdout);

    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));

    uv_buf_t uvBuf = uv_buf_init(buf->base, nread);//初始化write的uv_buf_t

    //发送buffer数组，第四个参数表示数组大小
    uv_write(req, client, &uvBuf, 1, echo_write);

    return;
  } else if (nread < 0) {
    if (nread != UV_EOF) {
      fprintf(stderr, "Read error %s\n", uv_err_name(nread));
    } else {
      fprintf(stderr, "client disconnect\n");
    }
    uv_close((uv_handle_t *) client, on_close);
  }

  //释放之前申请的资源
  if (buf->base != NULL) {
    free(buf->base);
  }
}

/**
 *
 * @param:  server  libuv的tcp server对象
 * @param:  status  状态，小于0表示新连接有误
 * @author: sherlock
 */
void on_new_connection(uv_stream_t *server, int status) {
  if (status < 0) {
    fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    return;
  }

  uv_tcp_t *client = (uv_tcp_t *) malloc(sizeof(uv_tcp_t));//为tcp client申请资源

  uv_tcp_init(loop, client);//初始化tcp client句柄

  //判断accept是否成功
  if (uv_accept(server, (uv_stream_t *) client) == 0) {
    //从传入的stream中读取数据，read_cb会被多次调用，直到数据读完，或者主动调用uv_read_stop方法停止
    uv_read_start((uv_stream_t *) client, alloc_buffer, read_cb);
  } else {
    uv_close((uv_handle_t *) client, NULL);
  }
}

int main(int argc, char **argv) {
  loop = uv_default_loop();

  uv_tcp_t server;
  uv_tcp_init(loop, &server);//初始化tcp server对象

  struct sockaddr_in addr;

  uv_ip4_addr("127.0.0.1", DEFAULT_PORT, &addr);//将ip和port数据填充到sockaddr_in结构体中

  uv_tcp_bind(&server, (const struct sockaddr *) &addr, 0);//bind

  int r = uv_listen((uv_stream_t * ) & server, DEFAULT_BACKLOG, on_new_connection);//listen

  if (r) {
    fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    return 1;
  }

  printf("i am here!\r\n");

  return uv_run(loop, UV_RUN_DEFAULT);
}
