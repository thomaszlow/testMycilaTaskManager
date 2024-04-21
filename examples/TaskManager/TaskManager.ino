#include <Arduino.h>
#include <MycilaTaskManager.h>

Mycila::TaskManager loopTaskManager("loop()", 5);

Mycila::Task sayHello("sayHello", [](void* params) { Serial.println("Hello"); });
Mycila::Task sayGoodbye("sayGoodbye", [](void* params) { Serial.println("Hello"); });
Mycila::Task ping("ping", [](void* params) { Serial.println((const char*)params); });
Mycila::Task output("output", [](void* params) { loopTaskManager.log(10); });
Mycila::Task delayed("delayed", [](void* params) { Serial.println("Delayed!"); });

char* params = "Pong";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  sayHello.setType(Mycila::TaskType::FOREVER);
  sayHello.setManager(&loopTaskManager);
  sayHello.setInterval(1 * Mycila::TaskDuration::SECONDS);
  sayHello.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %" PRIu32 " us", me.getName(), elapsed);
  });

  // Requires -D MYCILA_TASK_MANAGER_DEBUG
  // sayHello.setDebug(true);

  sayGoodbye.setType(Mycila::TaskType::FOREVER);
  sayGoodbye.setManager(&loopTaskManager);
  sayGoodbye.setInterval(3 * Mycila::TaskDuration::SECONDS);
  sayGoodbye.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %" PRIu32 " us", me.getName(), elapsed);
    ping.setData(params);
    ping.resume();
  });
  // sayGoodbye.setDebug(true);

  ping.setType(Mycila::TaskType::ONCE);
  ping.setManager(&loopTaskManager);
  ping.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %" PRIu32 " us", me.getName(), elapsed);
  });
  // ping.setDebug(true);

  output.setType(Mycila::TaskType::FOREVER);
  output.setManager(&loopTaskManager);
  output.setInterval(5 * Mycila::TaskDuration::SECONDS);

  delayed.setType(Mycila::TaskType::ONCE);
  delayed.setManager(&loopTaskManager);

  loopTaskManager.enableProfiling(6, Mycila::TaskTimeUnit::MICROSECONDS);

  delayed.resume(10 * Mycila::TaskDuration::SECONDS);
}

void loop() {
  loopTaskManager.loop();
}
