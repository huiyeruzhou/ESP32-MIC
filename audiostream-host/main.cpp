#include <arpa/inet.h>
#include <mpg123.h>
#include <opus/opus.h>
#include <out123.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#define MAX_PACKET 1500
#define MAX_FRAME_SIZE 6 * 960
#define MAX_PACKET_SIZE (3840000)
#define COUNTERLEN 4000

#define RATE 48000
#define BITS 16
#define FRAMELEN 2.5
#define CHANNELS 2
#define FRAMESIZE (RATE * CHANNELS * 2 * FRAMELEN / 1000)

OpusEncoder *encoder_init(opus_int32 sampling_rate, int channels,
                          int application);

int main() {
  // 初始化mpg123解码器
  mpg123_init();

  // 获得解码器句柄
  int err;
  mpg123_handle *mh = mpg123_new(NULL, &err);

  // 根据播放器句柄获得buffer大小参数，这里在创建时没有定制参数
  unsigned char *buffer;
  size_t buffer_size;
  size_t done;
  buffer_size = mpg123_outblock(mh);
  std::cout << "Get buffer size " << buffer_size << std::endl;
  buffer = (unsigned char *)malloc(buffer_size * sizeof(unsigned char));

  // 从mp3文件中获得通道数和编码方法
  int channels, encoding;
  int64_t rate;
  mpg123_open(mh, "./test.mp3");
  mpg123_getformat(mh, &rate, &channels, &encoding);
  std::cout << "rate is " << rate << ","
            << "channels is " << channels << ", "
            << "encoding is " << encoding << std::endl;

  unsigned int counter = 0;

  // 这里简单计算，其实就是2.5ms的framesize对应多少采样点
  // must be multiple times of bits
  // 2.5MS -> 48000 samples per second / 1000 MS per second * 2 Channels *
  // 2bytes per sample * 2.5MS = 480 bytes per 2.5MS 2.5MS 120 per channel
  // samples, 240 per channel size, 240 samples per 2.5MS

  int max_payload_bytes = MAX_PACKET;
  int len_opus;
  // int frame_duration_ms = 2.5;
  int frame_size = rate / 1000 * 20;
  buffer_size = frame_size * channels * 2;
  unsigned char cbits[MAX_PACKET_SIZE];
  unsigned char *cbits_vtmp = cbits;

  // 初始化Opus编码器
  OpusEncoder *enc = encoder_init(rate, channels, OPUS_APPLICATION_AUDIO);
  timeval start, end, start1, end1;
  timeval start4, end4;
  int totalBytes;
  // decoding to buffer address with buffersize, one frame size.
  // 循环到解码mp3文件完成，done表示实际得到解码长度，buffer_size表示目标解码长度
  // 只在最后一次读取时，done可能会小于buffer_size

  // 1.创建通信的套接字
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket error");
    return -1;
  }

  time_t now;
  char strftime_buf[64];
  struct tm timeinfo;

  time(&now);
  // Set timezone to China Standard Time
  setenv("TZ", "CST-8", 1);
  tzset();

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  printf("The current date/time in Shanghai is: %s\n", strftime_buf);

  int recvbuf = 0;
  int sendbuf = 0;
  socklen_t optlen = sizeof(sendbuf);
  int er = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &optlen);
  if (er) {
    printf("获取发送缓冲区大小错误\n");
  }
  er = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, &optlen);
  if (er) {
    printf("获取接收缓冲区大小错误\n");
  }

  printf(" 发送缓冲区原始大小为: %d 字节\n", sendbuf);
  printf(" 接收缓冲区原始大小为: %d 字节\n", recvbuf);

  recvbuf = 300;
  sendbuf = 500;

  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(int));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(int));

  recvbuf = 0;
  sendbuf = 0;

  er = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuf, &optlen);
  if (er) {
    printf("获取发送缓冲区大小错误\n");
  }
  er = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, &optlen);
  if (er) {
    printf("获取接收缓冲区大小错误\n");
  }

  printf(" 发送缓冲区大小为: %d 字节\n", sendbuf);
  printf(" 接收缓冲区大小为: %d 字节\n", recvbuf);

  // 2.连接服务器的IP port
  struct sockaddr_in saddr;
  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(1028);
  inet_pton(AF_INET, "192.168.101.8", &saddr.sin_addr.s_addr);
  int ret = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (ret == -1) {
    perror("connect error");
    return -1;
  }
  char buff[10];
  int a = 0;

  std::cout << "buffersize=" << buffer_size << std::endl;
  for (totalBytes = 0;
       (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) &&
       (counter < COUNTERLEN);) {
    int16_t *tst = reinterpret_cast<int16_t *>(buffer);

    totalBytes += done;
    if (buffer_size != done) {
      std::cout << "last done size : " << done << std::endl;
    }
    std::cout << "Total sent size is: " << totalBytes << std::endl;

    // std::cout << "FRAMESIZE is " << FRAMESIZE << std::endl;

    // 使用opus编码器对pcm音频进行编码，得到opus编码结果存到cbits_vtmp数组
    // len_opus数组中存放每次编码得到的字节长度
    // int tv=start.tv_usec;

    // 编码
    gettimeofday(&start1, NULL);
    len_opus = opus_encode(enc, tst, frame_size, cbits_vtmp, MAX_PACKET_SIZE);
    gettimeofday(&end1, NULL);
    std::cout << "opus_encode " << (end1.tv_usec - start1.tv_usec) << "us"
              << std::endl;

    // 打印编码长度
    printf("len_opus[]=%d\n", len_opus);
    if (len_opus < 0) {
      std::cout << "failed to encode: " << opus_strerror(len_opus) << std::endl;
    }

    // 发送
    // *(int*)cbits_vtmp =tv;
    gettimeofday(&start1, NULL);
    int len = send(fd, cbits_vtmp, len_opus, 0);
    gettimeofday(&end1, NULL);
    std::cout << "send is" << (end1.tv_usec - start1.tv_usec) << "us"
              << std::endl;
    if (len > 0) {
      std::cout << "len = " << len << std::endl;
      a += 1;
      std::cout << "a= " << a << std::endl;
      // usleep(8000);
    } else {
      std::cout << "errno is " << strerror(errno) << std::endl;
      send(fd, cbits_vtmp, len_opus, 0);
    }
    // int b=strlen(buff);
    // printf("b=%d\n",b);

    // usleep(9350);
    gettimeofday(&start1, NULL);
    len = recv(fd, buff, 10, 0);
    gettimeofday(&end1, NULL);
    while (len < 0) {
      std::cout << "errno is " << strerror(errno) << std::endl;
      len = recv(fd, buff, 10, 0);
      usleep(1000);
    }

    std::cout << "       recv is" << (end1.tv_usec - start1.tv_usec) << "us, "
              << len << " bytes" << std::endl;
    /*
    if(a==2){
        len=recv(fd,buff,sizeof(buff),0);
        if (len<0){//recv返回值小于0时，接收数据错误
            perror("recv error!");
            break;
        }
        else if (len == 0){//recv返回值等于0时，说明对端(客户端)关闭
            perror("the client  process is stopped!");
            break;
        }
        else{
            std::cout << "ok"<<std::endl;
            a=0;
        }
    }*/
    // 运行到此处说明接收成功，接受的数据存放在buff中
    // 运行到此处说明send成功

    // 数组地址向前移动编码长度
    cbits_vtmp = cbits_vtmp + len_opus;
    counter = counter + 1;
  }

  std::cout << "total pcm bytes is " << totalBytes << std::endl;
  /* gettimeofday(&end, NULL);
   std::cout << 1000 * (end.tv_sec - start.tv_sec) +
                    (end.tv_usec - start.tv_usec) / 1000
             << std::endl;
*/

  free(buffer);
  mpg123_close(mh);
  mpg123_delete(mh);
  mpg123_exit();
  return 0;
}

OpusEncoder *encoder_init(opus_int32 sampling_rate, int channels,
                          int application) {
  int enc_err;
  std::cout << "Here the rate is" << sampling_rate << std::endl;
  OpusEncoder *enc =
      opus_encoder_create(sampling_rate, channels, application, &enc_err);
  if (enc_err != OPUS_OK) {
    fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(enc_err));
    return NULL;
  }

  int bitrate_bps = 120000;
  int bandwidth = OPUS_BANDWIDTH_FULLBAND;
  int use_vbr = 1;
  int cvbr = 0;
  int complexity = 5;
  int use_inbandfec = 0;
  int forcechannels = CHANNELS;
  int use_dtx = 0;
  int packet_loss_perc = 0;

  opus_encoder_ctl(
      enc, OPUS_SET_BITRATE(bitrate_bps));  // 自动比特率，依据采样率和声道数
  /*
  opus_int32 a=0;
  opus_encoder_ctl(enc,OPUS_GET_BITRATE(&a));
  std::cout<<"complexity="<<a<<std::endl;	*/	//获取到的默认OPUS_AUTO是120000，OPUS_BITRATE_MAX是4083200
  opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(bandwidth));  // 带宽FB全频带20kHz
  opus_encoder_ctl(
      enc,
      OPUS_SET_SIGNAL(
          OPUS_SIGNAL_MUSIC));  // 选择 MDCT
                                // 模式的偏差阈值，这只是提示编码器，不是设置
  opus_encoder_ctl(enc, OPUS_SET_VBR(use_vbr));  // 使用动态比特率
  opus_encoder_ctl(
      enc, OPUS_SET_VBR_CONSTRAINT(cvbr));  // 不启用约束VBR，启用可以降低延迟
  /*
  opus_int32 a=0;
  opus_encoder_ctl(enc,OPUS_GET_COMPLEXITY(&a));
  std::cout<<"complexity="<<a<<std::endl;*/	//获取到的是9
  opus_encoder_ctl(
      enc,
      OPUS_SET_COMPLEXITY(
          complexity));  // 复杂度0-10，在 CPU 复杂性和质量/比特率之间进行取舍
  opus_encoder_ctl(
      enc, OPUS_SET_INBAND_FEC(use_inbandfec));  // 不使用前向纠错，只适用于LPC
  opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(forcechannels));  // 强制双声道
  opus_encoder_ctl(
      enc,
      OPUS_SET_DTX(
          use_dtx));  // 不使用不连续传输
                      // (DTX)，在静音或背景噪音期间降低比特率，主要适用于voip
  opus_encoder_ctl(enc,
                   OPUS_SET_PACKET_LOSS_PERC(
                       packet_loss_perc));  // 预期丢包，用降低比特率，来防丢包
  // opus_encoder_ctl(OPUS_SET_PREDICTION_DISABLED	(0))
  // //默认启用预测，LPC线性预测？不启用好像每一帧都有帧头，且会降低质量

  // opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&skip));
  opus_encoder_ctl(
      enc,
      OPUS_SET_LSB_DEPTH(
          BITS));  // 被编码信号的深度，是一个提示，低于该数量的信号包含可忽略的量化或其他噪声，帮助编码器识别静音和接近静音

  // IMPORTANT TO CONFIGURE DELAY
  int variable_duration = OPUS_FRAMESIZE_20_MS;
  opus_encoder_ctl(
      enc, OPUS_SET_EXPERT_FRAME_DURATION(variable_duration));  // 帧时长

  return enc;
}
