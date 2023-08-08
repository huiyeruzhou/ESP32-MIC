#pragma once
#include <esp_log.h>

#include <cinttypes>
#include <ctime>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "speaker_driver.hpp"
enum Status {
  INIT = 0,
  STANDBY = 1,
  RUNNING = 2,
  SUSPEND = 3,
  TERMINATE = 4,
  UNKNOWN = 0x80000000
};
enum Cmd { START = 0, CONNECT = 1, DISCONNECT = 2, TERMINATE_CMD = 3 };
class AbilityContext {
 private:
  constexpr static const char *TAG = "ABILITY_CONTEXT";
  constexpr static const char *abilityName = "ESP32SPK";
  constexpr static const char *devicesList =
      "{\"speakerDevices\": "
      "[{\"channels\": 2,"
      "\"description\" : \"内置音频 模拟立体声\","
      "\"mute\" : false,"
      "\"name\" : \"alsa_output.pci-0000_00_1f.3.analog-stereo\","
      "\"sampleRate\" : 48000,"
      "\"volume\" : 65540}]}";

  constexpr static const char *abilityRunningStateFormat =
      "[{\"abilityName\": %s,"
      "\"abilityPort\" : %d,"
      "\"last_update\" : %" PRId64
      ","
      "\"port\" : %d,"
      "\"status\" : \"%s\"}]";

  constexpr static const char *abilitySupportFormat =
      "[{\"depends\":"
      " {\"abilities\": [\"none\"],"
      "\"devices\" : [%s]},"
      "\"level\": 0,"
      "\"name\" : %s}]";

  int abilityPort = 0;
  unsigned status = INIT;
  // 每行一种状态,每列一个动作
  unsigned status_transfer[5][4] = {
      // start connect  disconnect  terminate
      // init can start, terminate
      {STANDBY, UNKNOWN, UNKNOWN, TERMINATE},
      // standby can start, terminate
      {UNKNOWN, RUNNING, UNKNOWN, TERMINATE},
      // running can suspend, terminate
      {UNKNOWN, UNKNOWN, SUSPEND, TERMINATE},
      // suspend can restart, reconnect
      {STANDBY, RUNNING, UNKNOWN, TERMINATE},
      // nothing can be done to terminate
      {STANDBY, UNKNOWN, UNKNOWN, UNKNOWN}};
  int lifecyclePort = 0;
  std::string ip = "";
  struct timeval last_update;
  TaskHandle_t speaker_task_handle = NULL;

 public:
  AbilityContext() {
    gettimeofday((struct timeval *)&last_update, NULL);
    // convert to UNIX time_t
  }
  int getLifecyclePort() { return lifecyclePort; }
  const char *getAbilityName() { return abilityName; }
  bool check_cmd_legal(unsigned cmd) {
    assert(status < 5);
    assert(cmd < 4);
    return status_transfer[status][cmd] != UNKNOWN;
  }
  void do_cmd(unsigned cmd) {
    switch (cmd) {
      case START:
        start();
        break;
      case CONNECT:
        connect();
        break;
      case DISCONNECT:
        disconnect();
        break;
      case TERMINATE_CMD:
        terminate();
        break;
      default:
        ESP_LOGE(TAG, "cmd is illegal: %d", cmd);
        break;
    }
  }
  void start() {
    assert(status < 5);
    status = status_transfer[status][0];
    lifecyclePort = 1;
    gettimeofday((struct timeval *)&last_update, NULL);
  }
  void connect() {
    assert(status < 5);
    status = status_transfer[status][1];
    gettimeofday((struct timeval *)&last_update, NULL);
    xTaskCreate(speaker_task, "speaker", 32768, NULL, 5, &speaker_task_handle);
    abilityPort = SPEAKER_PORT;
    if (speaker_task_handle == NULL) {
      // 任务创建失败
      ESP_LOGI(TAG, "任务创建失败4");
    } else {
      // 任务创建成功
      ESP_LOGI(TAG, "任务创建成功4");
    }
  }
  void disconnect() {
    assert(status < 5);
    status = status_transfer[status][2];
    if (speaker_task_handle != NULL) {
      // vTaskDelete(speaker_task_handle);
    } else {
      ESP_LOGE(TAG, "speaker_task_handle is NULL");
    }
    gettimeofday((struct timeval *)&last_update, NULL);
  }
  void terminate() {
    assert(status < 5);
    status = status_transfer[status][3];
    gettimeofday((struct timeval *)&last_update, NULL);
  }
  const char *getDevicesList() { return devicesList; }
  const char *getStatusString() {
    switch (status) {
      case 0:
        return "INIT";
      case 1:
        return "STANDBY";
      case 2:
        return "RUNNING";
      case 3:
        return "SUSPEND";
      case 4:
        return "TERMINATE";
      default:
        return "UNKNOWN";
    }
  }
  const char *getAbilityRunningState() {
    int max_len = 1023;
    char *buf = reinterpret_cast<char *>(malloc(max_len + 1));
    snprintf(buf, max_len, abilityRunningStateFormat, abilityName, abilityPort,
             last_update.tv_sec, lifecyclePort, getStatusString());
    return buf;
  }
  const char *getAbilitySupport() {
    int max_len = 1023;
    char *buf = reinterpret_cast<char *>(malloc(max_len + 1));
    snprintf(buf, max_len, abilitySupportFormat, devicesList, abilityName);
    return buf;
  }
};
extern AbilityContext *const speakerContext;
