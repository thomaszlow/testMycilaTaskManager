#include <Arduino.h>
#include <MycilaTaskManager.h>

Mycila::TaskManager loopTaskManager("loop()", 4);

Mycila::Task sayHello("sayHello", [](void* params) { Serial.println("Hello"); });
Mycila::Task sayGoodbye("sayGoodbye", [](void* params) { Serial.println("Hello"); });
Mycila::Task ping("ping", [](void* params) { Serial.println((const char*)params); });
Mycila::Task output("output", [](void* params) { loopTaskManager.log(10); });

char* params = "Pong";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  sayHello.setType(Mycila::TaskType::FOREVER);
  sayHello.setManager(&loopTaskManager);
  sayHello.setInterval(1 * Mycila::TaskDuration::SECONDS);
  sayHello.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %d ms", me.getName(), elapsed / Mycila::TaskDuration::MILLISECONDS);
  });
  
  // Requires -D MYCILA_TASK_MANAGER_DEBUG
  // sayHello.setDebug(true);

  sayGoodbye.setType(Mycila::TaskType::FOREVER);
  sayGoodbye.setManager(&loopTaskManager);
  sayGoodbye.setInterval(3 * Mycila::TaskDuration::SECONDS);
  sayGoodbye.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %llu ms", me.getName(), elapsed / Mycila::TaskDuration::MILLISECONDS);
    ping.setData(params);
    ping.resume();
  });
  // sayGoodbye.setDebug(true);

  ping.setType(Mycila::TaskType::ONCE);
  ping.setManager(&loopTaskManager);
  ping.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %llu ms", me.getName(), elapsed / Mycila::TaskDuration::MILLISECONDS);
  });
  // ping.setDebug(true);

  output.setType(Mycila::TaskType::FOREVER);
  output.setManager(&loopTaskManager);
  output.setInterval(5 * Mycila::TaskDuration::SECONDS);

  loopTaskManager.enableProfiling(6, Mycila::TaskTimeUnit::MICROSECONDS);
}

void loop() {
  loopTaskManager.loop();
}
