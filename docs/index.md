# MycilaTaskManager

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Continuous Integration](https://github.com/mathieucarbou/MycilaTaskManager/actions/workflows/ci.yml/badge.svg)](https://github.com/mathieucarbou/MycilaTaskManager/actions/workflows/ci.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/mathieucarbou/library/MycilaTaskManager.svg)](https://registry.platformio.org/libraries/mathieucarbou/MycilaTaskManager)

Arduino / ESP32 Task Manager Library

This is a simple task manager for Arduino / ESP32, to schedule tasks at a given frequency.
Tasks are represented by anonymous function, so they must be small, non-blocking and cooperative.

- Support dynamic activation of task
- Support scheduling once and repeating tasks with or without intervals
- Ability to pass data to tasks
- Callback
- Debug and statistics to display execution times
- Pause and Resume a task or a complete set of tasks
- Async support! Start the loop manager in a background task as easy as calling `.asyncStart()`!

## Usage

**Please have a look at the API and examples.**

### Simple task

```c++
Mycila::Task sayHello("sayHello", [](void* params) { Serial.println("Hello"); });

void setup() {
  sayHello.setType(Mycila::TaskType::FOREVER); // this is the default
  sayHello.setEnabled(true);
  sayHello.setInterval(1 * Mycila::TaskDuration::SECONDS);
  sayHello.setCallback([](const Mycila::Task& me, const uint32_t elapsed) {
    ESP_LOGD("app", "Task '%s' executed in %d ms", me.getName(), elapsed / Mycila::TaskDuration::MILLISECONDS);
  });
}

void loop() {
  sayHello.tryRun();
}
```

### With a TaskManager

```c++
Mycila::TaskManager loopTaskManager("loop()", 2);

Mycila::Task sayHello("sayHello", [](void* params) { Serial.println("Hello"); });
Mycila::Task sayGoodbye("sayGoodbye", [](void* params) { Serial.println("Hello"); });

void setup() {
  sayHello.setType(Mycila::TaskType::FOREVER); // this is the default
  sayHello.setManager(&loopTaskManager);
  sayHello.setEnabled(true);
  sayHello.setInterval(1 * Mycila::TaskDuration::SECONDS);
  sayHello.setCallback([](const Mycila::Task& me, const uint32_t elapsed) { sayGoodbye.resume(); });

  sayGoodbye.setType(Mycila::TaskType::ONCE);
  sayGoodbye.setManager(&loopTaskManager);
}

void loop() {
  loopTaskManager.loop();
}
```

Have a look at the API for more!

### Async

Launch an async task with:

```c++
sayHello.asyncStart();
```

Launch an async task manager with:

```c++
loopTaskManager.asyncStart();
```
