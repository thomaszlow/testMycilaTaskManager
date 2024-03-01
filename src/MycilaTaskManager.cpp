// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#include <MycilaTaskManager.h>

#include <Arduino.h>

#define TAG "TASK-MAN"

#define NOW() esp_timer_get_time()

const Mycila::TaskPredicate Mycila::Task::ALWAYS_TRUE = []() {
  return true;
};

const Mycila::TaskPredicate Mycila::Task::ALWAYS_FALSE = []() {
  return false;
};

////////////////
// TaskStatistics
////////////////

Mycila::TaskStatistics::TaskStatistics(const uint8_t nBins, TaskTimeUnit unit) : _nBins(nBins), _unit(unit) {
  if (nBins)
    _bins = new uint16_t[nBins];
  clear();
}

Mycila::TaskStatistics::~TaskStatistics() {
  if (_nBins)
    delete[] _bins;
}

void Mycila::TaskStatistics::record(uint32_t elapsed) {
  if (_iterations == UINT32_MAX)
    clear();
  _iterations++;
  _updated = true;
  if (!_nBins)
    return;
  uint8_t bin = 0;
  elapsed = elapsed / (uint32_t)_unit;
  while (elapsed >>= 1 && bin < _nBins - 1)
    bin++;
  if (_bins[bin] < UINT16_MAX) {
    _bins[bin]++;
  }
}

void Mycila::TaskStatistics::clear() {
  _iterations = 0;
  for (size_t i = 0; i < _nBins; i++)
    _bins[i] = 0;
}

void Mycila::TaskStatistics::processed() { _updated = false; }

#ifdef MYCILA_TASK_MANAGER_JSON_SUPPORT
void Mycila::TaskStatistics::toJson(const JsonObject& root) const {
  root["count"] = _iterations;
  root["unit"] = _unit == TaskTimeUnit::MICROSECONDS ? "us" : (_unit == TaskTimeUnit::MILLISECONDS ? "ms" : "s");
  for (size_t i = 0; i < _nBins; i++)
    root["bins"][i] = _bins[i];
}
#endif

uint8_t Mycila::TaskStatistics::getBinCount() const { return _nBins; }
Mycila::TaskTimeUnit Mycila::TaskStatistics::getUnit() const { return _unit; }
uint32_t Mycila::TaskStatistics::getIterations() const { return _iterations; }
uint16_t Mycila::TaskStatistics::getBin(uint8_t index) const { return _bins[index]; }
bool Mycila::TaskStatistics::isUpdated() const { return _updated; }

////////////////
// TASK MANAGER
////////////////

Mycila::TaskManager::TaskManager(const char* name, const size_t maxTaskCount) : _name(name),
                                                                                _capacity(maxTaskCount),
                                                                                _tasks(new Task*[maxTaskCount]) {
  for (size_t i = 0; i < _capacity; i++)
    _tasks[i] = nullptr;
}
Mycila::TaskManager::~TaskManager() { delete[] _tasks; }

const char* Mycila::TaskManager::getName() const { return _name; }

size_t Mycila::TaskManager::getSize() const {
  size_t count = 0;
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      count++;
  return count;
}

size_t Mycila::TaskManager::loop() {
  size_t executed = 0;
  for (size_t i = 0; i < _capacity; i++) {
    if (_tasks[i] && _tasks[i]->tryRun()) {
      executed++;
      yield();
    }
  }
  return executed;
}

void Mycila::TaskManager::pause() {
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      _tasks[i]->pause();
}

void Mycila::TaskManager::resume() {
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      _tasks[i]->resume();
}

void Mycila::TaskManager::enableProfiling(const uint8_t nBins, TaskTimeUnit unit) {
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      _tasks[i]->enableProfiling(nBins, unit);
}

void Mycila::TaskManager::disableProfiling() {
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      _tasks[i]->disableProfiling();
}

void Mycila::TaskManager::log(const size_t maxNameWidth) {
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      _tasks[i]->log(maxNameWidth);
}

#ifdef MYCILA_TASK_MANAGER_JSON_SUPPORT
void Mycila::TaskManager::toJson(const JsonObject& root) const {
  root["name"] = _name;
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i])
      _tasks[i]->toJson(root["tasks"][i].to<JsonObject>());
}
#endif

#ifdef MYCILA_TASK_MANAGER_ASYNC_SUPPORT
bool Mycila::TaskManager::asyncStart(const uint32_t stackSize, const UBaseType_t priority, const BaseType_t coreID, uint32_t delay) {
  if (_taskManagerHandle)
    return false;
  _delay = delay;
  bool b = xTaskCreateUniversal(_asyncTaskManager, _name, stackSize, this, priority, &_taskManagerHandle, coreID) == pdPASS;
  if (b)
    ESP_LOGD(TAG, "Started async task manager '%s' with handle: %p", _name, _taskManagerHandle);
  return b;
}

void Mycila::TaskManager::asyncStop() {
  if (!_taskManagerHandle)
    return;
  ESP_LOGD(TAG, "Stopping async task manager with handle: %p", _taskManagerHandle);
  vTaskDelete(_taskManagerHandle);
  _taskManagerHandle = NULL;
}

void Mycila::TaskManager::_asyncTaskManager(void* params) {
  TaskManager* taskManager = reinterpret_cast<TaskManager*>(params);
  while (true) {
    if (!taskManager->loop()) {
      if (taskManager->_delay)
        delay(taskManager->_delay);
      else
        yield();
    }
  }
  vTaskDelete(NULL);
}
#endif

void Mycila::TaskManager::_addTask(Task* task) {
  for (size_t i = 0; i < _capacity; i++)
    if (!_tasks[i]) {
      _tasks[i] = task;
      ESP_LOGD(TAG, "Task '%s' added to task manager '%s'", task->getName(), _name);
      return;
    }
  ESP_LOGE(TAG, "Task '%s' cannot be added to task manager '%s': increase capacity!", task->getName(), _name);
  assert(false); // full
}

void Mycila::TaskManager::_removeTask(Task* task) {
  for (size_t i = 0; i < _capacity; i++)
    if (_tasks[i] == task) {
      _tasks[i] = nullptr;
      ESP_LOGD(TAG, "Task '%s' removed from task manager '%s'", task->getName(), _name);
      return;
    }
}

////////////////
// TASK
////////////////

Mycila::Task::Task(const char* name, TaskFunction fn) : _name(name), _fn(fn) { assert(_fn); }

Mycila::Task::~Task() {
  if (_manager)
    _manager->_removeTask(this);
  if (_stats)
    delete _stats;
}

///////////////////
// task information
///////////////////

const char* Mycila::Task::getName() const { return _name; }
Mycila::TaskType Mycila::Task::getType() const { return _type; }
int64_t Mycila::Task::getInterval() const { return _intervalSupplier ? _intervalSupplier() : 0; }
bool Mycila::Task::isEnabled() const { return !_enabledPredicate || _enabledPredicate(); }
bool Mycila::Task::isPaused() const { return _paused; }
bool Mycila::Task::isRunning() const { return _running; }
bool Mycila::Task::isManaged() const { return _manager; }
bool Mycila::Task::isProfiled() const { return _stats; }
bool Mycila::Task::isEarlyRunRequested() const { return _lastEnd == 0; }
int64_t Mycila::Task::getRemainingTme() const {
  if (!_intervalSupplier)
    return 0;
  const int64_t itvl = _intervalSupplier();
  if (itvl == 0)
    return 0;
  const int64_t next = _lastEnd + itvl;
  const int64_t now = NOW();
  return next > now ? next - now : 0;
}
bool Mycila::Task::shouldRun() const {
  if (_paused)
    return false;
  if (_enabledPredicate && !_enabledPredicate())
    return false;
  if (_lastEnd == 0 || !_intervalSupplier)
    return true;
  const uint32_t itvl = _intervalSupplier();
  return itvl == 0 || NOW() - _lastEnd >= itvl;
}

///////////////////
// task creation
///////////////////

void Mycila::Task::setType(Mycila::TaskType type) {
  _type = type;
  _paused = type == TaskType::ONCE;
}

void Mycila::Task::setEnabled(bool enabled) {
  if (enabled)
    _enabledPredicate = nullptr;
  else
    _enabledPredicate = ALWAYS_FALSE;
}

void Mycila::Task::setEnabledWhen(TaskPredicate predicate) {
  _enabledPredicate = predicate;
}

void Mycila::Task::setInterval(int64_t intervalMicros) {
  if (intervalMicros == 0)
    _intervalSupplier = nullptr;
  else
    _intervalSupplier = [intervalMicros]() {
      return intervalMicros;
    };
}

void Mycila::Task::setIntervalSupplier(TaskIntervalSupplier supplier) { _intervalSupplier = supplier; }

void Mycila::Task::setCallback(TaskDoneCallback doneCallback) { _onDone = doneCallback; }

void Mycila::Task::setManager(TaskManager* manager) {
  if (manager == _manager)
    return;
  // cannot replace a manager
  if (manager)
    assert(!_manager);
  _manager = manager;
  if (_manager)
    _manager->_addTask(this);
}

///////////////////
// task management
///////////////////

void Mycila::Task::setData(void* params) { _params = params; }
void Mycila::Task::pause() { _paused = true; }
void Mycila::Task::resume() { _paused = false; }
bool Mycila::Task::tryRun() {
  if (_paused)
    return false;
  if (_enabledPredicate && !_enabledPredicate())
    return false;
  if (_lastEnd == 0 || !_intervalSupplier) {
    _run(NOW());
    return true;
  }
  const int64_t itvl = _intervalSupplier();
  const int64_t now = NOW();
  if (itvl == 0 || now - _lastEnd >= itvl) {
    _run(now);
    return true;
  }
  return false;
}
void Mycila::Task::forceRun() { _run(NOW()); }
void Mycila::Task::requestEarlyRun() { _lastEnd = 0; }

///////////////////
// stats
///////////////////

bool Mycila::Task::enableProfiling(const uint8_t nBins, TaskTimeUnit unit) {
  if (_stats)
    return false;
  _stats = new TaskStatistics(nBins, unit);
  ESP_LOGD(TAG, "Enabled profiling on task '%s'", _name);
  return true;
}

bool Mycila::Task::disableProfiling() {
  if (_stats) {
    delete _stats;
    _stats = nullptr;
    ESP_LOGD(TAG, "Disabled profiling on task '%s'", _name);
    return true;
  }
  return false;
}

void Mycila::Task::log(const size_t maxNameWidth) {
  if (!_stats)
    return;
  if (!_stats->isUpdated())
    return;
  int c;
  String line;
  line.reserve(256);
  line += "| ";
  c = strlen(_name);
  if (c == maxNameWidth)
    line += _name;
  else if (c > maxNameWidth)
    line += String(_name).substring(0, maxNameWidth);
  else {
    line += _name;
    while (c++ < maxNameWidth)
      line += " ";
  }
  const uint8_t nBins = _stats->getBinCount();
  if (nBins) {
    const char* unit = _stats->getUnit() == TaskTimeUnit::MICROSECONDS ? "us" : (_stats->getUnit() == TaskTimeUnit::MILLISECONDS ? "ms" : "s");
    for (uint8_t i = 0; i < nBins; i++) {
      const String val = String(_stats->getBin(i));
      line += " | ";
      c = 5 - val.length();
      while (c-- > 0)
        line += " ";
      line += val;
      line += i < nBins - 1 ? " < 2^" : " >= 2^";
      line += i < nBins - 1 ? (i + 1) : i;
      line += " ";
      line += unit;
    }
    line += " | count: ";
    line += _stats->getIterations();
  }
  ESP_LOGD(TAG, "%s", line.c_str());
  _stats->processed();
}

const Mycila::TaskStatistics* Mycila::Task::getStatistics() const { return _stats; }

///////////////////
// optional
///////////////////

#ifdef MYCILA_TASK_MANAGER_JSON_SUPPORT
void Mycila::Task::toJson(const JsonObject& root) const {
  root["name"] = _name;
  root["type"] = _type == TaskType::ONCE ? "ONCE" : "FOREVER";
  root["paused"] = _paused;
  root["enabled"] = isEnabled();
  root["interval"] = getInterval();
  if (_stats)
    _stats->toJson(root["stats"].to<JsonObject>());
}
#endif

#ifdef MYCILA_TASK_MANAGER_ASYNC_SUPPORT
bool Mycila::Task::asyncStart(const uint32_t stackSize, const UBaseType_t priority, const BaseType_t coreID, uint32_t delay) {
  if (_taskHandle)
    return false;
  _delay = delay;
  bool b = xTaskCreateUniversal(_asyncTask, _name, stackSize, this, priority, &_taskHandle, coreID) == pdPASS;
  if (b)
    ESP_LOGD(TAG, "Started async task '%s' with handle: %p", _name, _taskHandle);
  return b;
}

void Mycila::Task::asyncStop() {
  if (!_taskHandle)
    return;
  ESP_LOGD(TAG, "Stopping async task with handle: %p", _taskHandle);
  vTaskDelete(_taskHandle);
  _taskHandle = NULL;
}

void Mycila::Task::_asyncTask(void* params) {
  Task* task = reinterpret_cast<Task*>(params);
  while (true) {
    if (!task->tryRun()) {
      if (task->_delay)
        delay(task->_delay);
      else
        yield();
    }
  }
  vTaskDelete(NULL);
}
#endif

#ifdef MYCILA_TASK_MANAGER_DEBUG
bool Mycila::Task::isDebug() const { return _debugPredicate && _debugPredicate(); }
void Mycila::Task::setDebug(bool debug) { _debugPredicate = debug ? ALWAYS_TRUE : nullptr; }
void Mycila::Task::setDebugWhen(TaskPredicate predicate) { _debugPredicate = predicate; }
#endif

///////////////////
// private
///////////////////

void Mycila::Task::_run(const int64_t& now) {
  _running = true;
  _fn(_params);
  _running = false;
  _lastEnd = esp_timer_get_time();
  if (_type == TaskType::ONCE)
    _paused = true;

  const uint32_t elapsed = _lastEnd - now;

#ifdef MYCILA_TASK_MANAGER_DEBUG
  if (_debugPredicate && _debugPredicate())
    ESP_LOGD(TAG, "%s ended in %u us", _name, elapsed);
#endif

  if (_stats)
    _stats->record(elapsed);

  if (_onDone)
    _onDone(*this, elapsed);
}
