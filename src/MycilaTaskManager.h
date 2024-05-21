// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou and others
 */
#pragma once

#include <Arduino.h>
#include <Print.h>

#include <functional>
#include <vector>

#ifdef MYCILA_JSON_SUPPORT
#include <ArduinoJson.h>
#endif

#define MYCILA_TASK_MANAGER_VERSION          "3.0.4"
#define MYCILA_TASK_MANAGER_VERSION_MAJOR    3
#define MYCILA_TASK_MANAGER_VERSION_MINOR    0
#define MYCILA_TASK_MANAGER_VERSION_REVISION 4

namespace Mycila {
  namespace TaskDuration {
    constexpr int64_t MICROSECONDS = 1UL;
    constexpr int64_t MILLISECONDS = 1000UL;
    constexpr int64_t SECONDS = 1000000UL;
    constexpr int64_t MINUTES = 60 * SECONDS;
    constexpr int64_t HOURS = 60 * MINUTES;
  } // namespace TaskDuration

  enum class TaskTimeUnit {
    MICROSECONDS = (uint32_t)TaskDuration::MICROSECONDS,
    MILLISECONDS = (uint32_t)TaskDuration::MILLISECONDS,
    SECONDS = (uint32_t)TaskDuration::SECONDS,
  };

  enum class TaskType {
    // once enabled, the task will run once and then disable itself
    ONCE,
    // the task will run at the specified interval as long as it is enabled
    FOREVER
  };

  class Task;

  typedef std::function<void(void* params)> TaskFunction;
  typedef std::function<bool()> TaskPredicate;
  typedef std::function<int64_t()> TaskIntervalSupplier;
  typedef std::function<void(const Task& me, const uint32_t elapsed)> TaskDoneCallback;

  ///////////////////
  // TaskStatistics
  ///////////////////

  class TaskStatistics {
    public:
      // record the number of iterations in each bin.
      // bin sizing is bases on power of 2, so if nBins = 16, we will have 16 bins:
      // bin 0 : 0 <= elapsed < 2^1 (exception for lower bound)
      // bin 1 : 2^1 <= elapsed < 2^2
      // bin 2 : 2^2 <= elapsed < 2^3
      // bin 3 : 2^3 <= elapsed < 2^4
      // ...
      // bin 14 : 2^14 <= elapsed < 2^15
      // bin 15 : 2^15 <= elapsed (exception for upper bound)
      // The unit determines the unit of the elapsed time recorded in the bins.
      // It allows to be more precise depending on the expected task execution durations.
      explicit TaskStatistics(const uint8_t nBins = 16, TaskTimeUnit unit = TaskTimeUnit::MILLISECONDS);
      ~TaskStatistics();

      void record(uint32_t elapsed);
      void clear();

      // mark the stats object has beeing seen,
      // and next calls to isUpdated() will return false until new data is recorded
      void processed();

#ifdef MYCILA_JSON_SUPPORT
      // json output
      void toJson(const JsonObject& root) const;
#endif

      uint8_t getBinCount() const;
      TaskTimeUnit getUnit() const;
      uint32_t getIterations() const;
      uint16_t getBin(uint8_t index) const;
      bool isUpdated() const;

    private:
      const uint8_t _nBins;
      const TaskTimeUnit _unit;
      uint16_t* _bins;
      uint32_t _iterations = 0;
      bool _updated = false;
  };

  ///////////////////
  // TaskManager
  ///////////////////

  class TaskManager {
    public:
      explicit TaskManager(const char* name, const size_t taskCount = 0);
      ~TaskManager();

      const char* getName() const;

      // number of tasks
      size_t getSize() const;

      // must be called from main loop and will loop over all registered tasks
      // returns the number of executed tasks
      size_t loop();

      // call pause() on all tasks
      void pause();

      // call resume() on all tasks
      void resume();

      // for all tasks
      void enableProfiling(const uint8_t nBins = 10, TaskTimeUnit unit = TaskTimeUnit::MILLISECONDS);

      // for all tasks
      void disableProfiling();

      // log all tasks
      void log();

      // json output of the task manager
#ifdef MYCILA_JSON_SUPPORT
      void toJson(const JsonObject& root) const;
#endif

      // start the task manager in a separate task.
      // You can add a delay in microseconds when no task is executed in order to avoid triggering the watchdog
      bool asyncStart(const uint32_t stackSize = 4096, const UBaseType_t priority = 0, const BaseType_t coreID = 0, uint32_t delay = 10);

      // kill the async task
      void asyncStop();

    private:
      const char* _name;
      std::vector<Task*> _tasks;
      void _addTask(Task* task);
      void _removeTask(Task* task);
      // async
      TaskHandle_t _taskManagerHandle = NULL;
      uint32_t _delay = 0;
      static void _asyncTaskManager(void* params);

    private:
      friend class Task;
  };

  ///////////////////
  // Task
  ///////////////////

  class Task {
    public:
      Task(const char* name, TaskFunction fn);
      ~Task();

      ///////////////////
      // task information
      ///////////////////

      const char* getName() const;
      TaskType getType() const;
      int64_t getInterval() const;
      // get remaining time before next run
      int64_t getRemainingTme() const;
      // check if a task is enabled as per the enabled predicate. By default a task is enabled.
      bool isEnabled() const;
      // check is the task is temporary paused
      bool isPaused() const;
      bool isRunning() const;
      bool isEarlyRunRequested() const;
      // check if the task should run, meaning it is enabled, not paused and the interval has been reached
      bool shouldRun() const;
      bool isManaged() const;
      bool isProfiled() const;

      ///////////////////
      // task creation
      ///////////////////

      // change task type.
      // ONCE will start paused, run once, and be paused again.
      // FOREVER will start active and will run at the specified interval.
      // In both cases the enable predicate will be checked to see of the task is enabled when not paused.
      void setType(TaskType type);

      // change the enabled state
      void setEnabled(bool enabled);

      // enable the task if the predicate returns true
      void setEnabledWhen(TaskPredicate predicate);

      // change the interval of execution
      void setInterval(int64_t intervalMicros);

      // dynamically provide the interval
      void setIntervalSupplier(TaskIntervalSupplier supplier);

      // callback when the task is done
      void setCallback(TaskDoneCallback doneCallback);

      // have this task managed by a task manager
      void setManager(TaskManager& manager); // NOLINT

      ///////////////////
      // task management
      ///////////////////

      // pass some data to the task
      void setData(void* params);

      // pause a task
      void pause();

      // resume a paused task
      void resume(int64_t delayMicros = 0);

      // try to run the task if it should run
      bool tryRun();

      // force the task to run
      void forceRun();

      // request an early run of the task and do not wait for the interval to be reached
      void requestEarlyRun();

      ///////////////////
      // stats
      ///////////////////

      bool enableProfiling(const uint8_t nBins = 10, TaskTimeUnit unit = TaskTimeUnit::MILLISECONDS);
      bool disableProfiling();
      void log();
      const TaskStatistics& getStatistics() const;

      ///////////////////
      // optional
      ///////////////////

#ifdef MYCILA_JSON_SUPPORT
      // json output
      void toJson(const JsonObject& root) const;
#endif

#ifdef MYCILA_TASK_MANAGER_ASYNC_SUPPORT
      // start the task in a separate task.
      // You can add a delay in microseconds when the task is not executed in order to avoid triggering the watchdog
      bool asyncStart(const uint32_t stackSize = 4096, const UBaseType_t priority = 0, const BaseType_t coreID = 0, uint32_t delay = 10);

      // kill the async task
      void asyncStop();
#endif

    public:
      static const Mycila::TaskPredicate ALWAYS_TRUE;
      static const Mycila::TaskPredicate ALWAYS_FALSE;

      ///////////////////
      // private
      ///////////////////

    private:
      const char* _name;
      const TaskFunction _fn;

      TaskType _type = TaskType::FOREVER;
      TaskManager* _manager = nullptr;
      TaskStatistics* _stats = nullptr;
      TaskPredicate _enabledPredicate = nullptr;
      TaskIntervalSupplier _intervalSupplier = nullptr;
      TaskDoneCallback _onDone = nullptr;
      bool _running = false;
      bool _paused = false;
      int64_t _lastEnd = 0;
      void* _params = nullptr;

      void _run(const int64_t& now);
  };
} // namespace Mycila
