#include <stdio.h>
#include <uv.h>
#include <stdlib.h>

uv_loop_t *loop;
#define SERVER_PORT 10002
#define CLIENT_PORT 10008

//连接队列最大长度
#define DEFAULT_BACKLOG 128

/*
 * @param:  server  libuv的tcp server对象
 * @param:  status  状态，小于0表示新连接有误
 * @author: sherlock
 */
void on_connected(uv_connect_t *req, int status)
{
    if(status) {
        printf("connect error");
        return;
    }
    
    struct sockaddr addr;
    struct sockaddr_in addrin;
    int addrlen = sizeof(addr);
    char sockname[17] = {0};

    struct sockaddr addrpeer;
    struct sockaddr_in addrinpeer;
    int addrlenpeer = sizeof(addrpeer);
    char socknamepeer[17] = {0};
    
    if(0 == uv_tcp_getsockname((uv_tcp_t*)req->handle,&addr,&addrlen) && 
       0 == uv_tcp_getpeername((uv_tcp_t*)req->handle,&addrpeer,&addrlenpeer))
    {
        addrin     = *((struct sockaddr_in*)&addr);
        addrinpeer = *((struct sockaddr_in*)&addrpeer);
        uv_ip4_name(&addrin,sockname,addrlen);
        uv_ip4_name(&addrinpeer,socknamepeer,addrlenpeer);
        
	printf("%s:%d sendto %s:%d\r\n",sockname, ntohs(addrin.sin_port),socknamepeer, ntohs(addrinpeer.sin_port));  
    }
    else
        printf("get socket fail!\n");
    
    printf(" connect succeed!\r\n");  
}


uv_tcp_t server;
struct sockaddr_in dest;
uv_buf_t writebuf[] = {{.base = "1",.len = 1},{.base = "2",.len = 1}};

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

#if 0
    uv_write_t* req = (uv_write_t*)malloc(sizeof(uv_write_t));

    uv_buf_t uvBuf = uv_buf_init(buf->base, nread);//初始化write的uv_buf_t

    //发送buffer数组，第四个参数表示数组大小
    uv_write(req, client, &uvBuf, 1, echo_write);
#endif
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


void write_cb(uv_write_t *req, int status)
{
   if(status){
       printf("write error");
       return;
   }

   printf("write succeed!\r\n");
}

void timer_cb(uv_timer_t* handle) {
  printf("timer_cb time: %lu\r\n", time(NULL));
}

void* tcpclient_thread(void* param)
{
  uv_tcp_t client;
  uv_tcp_init(loop, &client);//初始化tcp server对象

  struct sockaddr_in addr;
  struct sockaddr_in dest;
  uv_ip4_addr("127.0.0.1", CLIENT_PORT, &addr);//将ip和port数据填充到sockaddr_in结构体中
  uv_ip4_addr("127.0.0.1", SERVER_PORT, &dest);

  uv_tcp_bind(&client, (const struct sockaddr *)&addr, 0);//bind

  int connected = 0;
  uv_connect_t connect;
  if(0 == uv_tcp_connect(&connect, &client, (const struct sockaddr*)&dest, on_connected)){
    uv_write_t write_req;
    uv_write(&write_req, (uv_stream_t*)&client, writebuf, 2, write_cb);

    uv_read_start((uv_stream_t *) &client, alloc_buffer, read_cb);
    connected = 1;
  }
  else {
    uv_close((uv_handle_t*)&client, NULL);
    printf("close socket!\r\n");
  }
  
  uv_run(loop, UV_RUN_DEFAULT);

  return NULL;
}

int main(int argc, char **argv) {
  loop = uv_default_loop();

#if 0
  uv_tcp_t client;
  uv_tcp_init(loop, &client);//初始化tcp server对象

  struct sockaddr_in addr;
  struct sockaddr_in dest;
  uv_ip4_addr("127.0.0.1", CLIENT_PORT, &addr);//将ip和port数据填充到sockaddr_in结构体中
  uv_ip4_addr("127.0.0.1", SERVER_PORT, &dest);

  uv_tcp_bind(&client, (const struct sockaddr *)&addr, 0);//bind

  int connected = 0;
  uv_connect_t connect;
  if(0 == uv_tcp_connect(&connect, &client, (const struct sockaddr*)&dest, on_connected)){
    uv_write_t write_req;
    uv_write(&write_req, (uv_stream_t*)&client, writebuf, 2, write_cb);

    uv_read_start((uv_stream_t *) &client, alloc_buffer, read_cb);
    connected = 1;
  }	
  else {
    uv_close((uv_handle_t*)&client, NULL);
    printf("close socket!\r\n");
  }

//  uv_run(loop, UV_RUN_DEFAULT);
//  uv_run(loop, UV_RUN_DEFAULT);
#endif
  int ret = 0;
  pthread_t pid;

  if((ret = pthread_create(&pid, NULL, tcpclient_thread, NULL)) != 0){
    printf("tcp client thread create fail!\r\n");
    return 0;
  }

  uv_timer_t uvTimer;
  
  uv_timer_init(loop, &uvTimer);
  uv_timer_start(&uvTimer, timer_cb, 1000, 1000);
  
  printf("call uv_timer_start time: %ld\r\n",time(NULL));

//  uv_run(loop, UV_RUN_DEFAULT);

  int i = 0;
  do {
   
#if 0  
  if(0 == i && connected){	  
      uv_write_t write_req;
      uv_write(&write_req, (uv_stream_t*)&client, writebuf, 2, write_cb);
    }
#endif
    i++;
    if(i== 1000) i = 0;
    if(i == 100) uv_timer_stop(&uvTimer);
    //uv_run(loop, UV_RUN_DEFAULT);	  
    uv_sleep(1000);
  }while(1);

  return 0;
  //return uv_run(loop, UV_RUN_DEFAULT);
}
