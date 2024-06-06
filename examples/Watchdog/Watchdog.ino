#include <Arduino.h>
#include <MycilaTaskManager.h>

Mycila::TaskManager taskManager1("tm-1");
Mycila::TaskManager taskManager2("tm-2");

Mycila::Task slowdown1("slowdown1", [](void* params) {
  long d = random(1000, 6000);
  Serial.printf("[slowdown1] Delaying for %ld ms\n", d);
  delay(d);
});

Mycila::Task slowdown2("slowdown2", [](void* params) {
  long d = random(1000, 6000);
  Serial.printf("[slowdown2] Delaying for %ld ms\n", d);
  delay(d);
});

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  slowdown1.setType(Mycila::TaskType::FOREVER);
  slowdown1.setManager(taskManager1);

  slowdown2.setType(Mycila::TaskType::FOREVER);
  slowdown2.setManager(taskManager2);

  Mycila::TaskManager::configureWDT(5, false);

  taskManager1.asyncStart(MYCILA_TASK_MANAGER_ASYNC_STACK_SIZE, MYCILA_TASK_MANAGER_ASYNC_PRIORITY, MYCILA_TASK_MANAGER_ASYNC_CORE, MYCILA_TASK_MANAGER_ASYNC_DELAY, true);
}

void loop() {
  taskManager2.loop();
}
