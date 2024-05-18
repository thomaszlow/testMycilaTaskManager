#include <Arduino.h>
#include <ArduinoJson.h>
#include <MycilaTaskManager.h>

Mycila::TaskManager loopTaskManager("loop()", 4);

Mycila::Task sayHello("sayHello", [](void* params) { Serial.println("Hello"); });
Mycila::Task sayGoodbye("sayGoodbye", [](void* params) { Serial.println("Hello"); });
Mycila::Task ping("ping", [](void* params) { Serial.println((const char*)params); });
Mycila::Task output("output", [](void* params) {
  JsonDocument doc;
  loopTaskManager.toJson(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();
  loopTaskManager.log();
});

char* params = "Pong";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  sayHello.setType(Mycila::TaskType::FOREVER);
  sayHello.setManager(loopTaskManager);
  sayHello.setInterval(1 * Mycila::TaskDuration::SECONDS);
  sayHello.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %" PRIu32 " us", me.getName(), elapsed);
  });

  sayGoodbye.setType(Mycila::TaskType::FOREVER);
  sayGoodbye.setManager(loopTaskManager);
  sayGoodbye.setInterval(3 * Mycila::TaskDuration::SECONDS);
  sayGoodbye.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %" PRIu32 " us", me.getName(), elapsed);
    ping.setData(params);
    ping.resume();
  });

  ping.setType(Mycila::TaskType::ONCE);
  ping.setManager(loopTaskManager);
  ping.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %" PRIu32 " us", me.getName(), elapsed);
  });

  output.setType(Mycila::TaskType::FOREVER);
  output.setManager(loopTaskManager);
  output.setInterval(5 * Mycila::TaskDuration::SECONDS);

  loopTaskManager.enableProfiling(6, Mycila::TaskTimeUnit::MICROSECONDS);
}

void loop() {
  loopTaskManager.loop();
}
