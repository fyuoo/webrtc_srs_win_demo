// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A "timer" takes care of invoking a callback in the future, once or
// repeatedly. The callback is invoked:
// - OneShotTimer: Once after a `TimeDelta` delay has elapsed.
// - RetainingOneShotTimer: Same as OneShotTimer, but the callback is retained
//    after being executed, allowing another invocation to be scheduled with
//    Reset() without specifying the callback again.
// - DeadlineTimer: Once at the specified `TimeTicks` time.
// - RepeatingTimer: Repeatedly, with a specified `TimeDelta` delay before the
//    first invocation and between invocations.

// Scheduled invocations can be cancelled with Stop() or by deleting the
// Timer. The latter makes it easy to ensure that an object is not accessed by a
// Timer after it has been deleted: just make the Timer a member of the object
// which receives Timer events (see example below).
//
// Sample RepeatingTimer usage:
//
//   class MyClass {
//    public:
//     void StartDoingStuff() {
//       timer_.Start(FROM_HERE, Seconds(1),
//                    this, &MyClass::DoStuff);
//     }
//     void StopDoingStuff() {
//       timer_.Stop();
//     }
//    private:
//     void DoStuff() {
//       // This method is called every second to do stuff.
//       ...
//     }
//     base::RepeatingTimer timer_;
//   };
//
// These APIs are not thread safe. When a method is called (except the
// constructor), all further method calls must be on the same sequence until
// Stop(). Once stopped, it may be destroyed or restarted on another sequence.
//
// By default, the scheduled tasks will be run on the same sequence that the
// Timer was *started on*. To mock time in unit tests, some old tests used
// SetTaskRunner() to schedule the delay on a test-controlled TaskRunner. The
// modern and preferred approach to mock time is to use TaskEnvironment's
// MOCK_TIME mode.

#ifndef BASE_TIMER_TIMER_H_
#define BASE_TIMER_TIMER_H_

// IMPORTANT: If you change timer code, make sure that all tests (including
// disabled ones) from timer_unittests.cc pass locally. Some are disabled
// because they're flaky on the buildbot, but when you run them locally you
// should be able to tell the difference.

#include "base/base_export.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"

namespace base {

class TickClock;

using ExactDeadline = base::StrongAlias<class ExactDeadlineTag, bool>;

namespace internal {

class TaskDestructionDetector;

// This class wraps logic shared by all timers.
class BASE_EXPORT TimerBase {
 public:
  // Initializes the state of all the timer features. Must be invoked after
  // FeatureList initialization and while Chrome is still single-threaded.
  static void InitializeFeatures();

  TimerBase(const TimerBase&) = delete;
  TimerBase& operator=(const TimerBase&) = delete;

  virtual ~TimerBase();

  // Returns true if the timer is running (i.e., not stopped).
  bool IsRunning() const;

  // Sets the task runner on which the delayed task should be scheduled when
  // this Timer is running. This method can only be called while this Timer
  // isn't running. If this is used to mock time in tests, the modern and
  // preferred approach is to use TaskEnvironment::TimeSource::MOCK_TIME. To
  // avoid racy usage of Timer, |task_runner| must run tasks on the same
  // sequence which this Timer is bound to (started from). TODO(gab): Migrate
  // callers using this as a test seam to
  // TaskEnvironment::TimeSource::MOCK_TIME.
  virtual void SetTaskRunner(scoped_refptr<SequencedTaskRunner> task_runner);

  // Call this method to stop and cancel the timer. It is a no-op if the timer
  // is not running.
  virtual void Stop();

 protected:
  // Constructs a timer. Start must be called later to set task info.
  explicit TimerBase(const Location& posted_from = Location());

  virtual void RunUserTask() = 0;
  virtual void OnStop() = 0;

  // Cancels the scheduled task and abandon it so that it no longer refers back
  // to this object.
  void AbandonScheduledTask();

  // Returns the task runner on which the task should be scheduled. If the
  // corresponding |task_runner_| field is null, the task runner for the current
  // sequence is returned.
  scoped_refptr<SequencedTaskRunner> GetTaskRunner();

  // The task runner on which the task should be scheduled. If it is null, the
  // task runner for the current sequence will be used.
  scoped_refptr<SequencedTaskRunner> task_runner_;

  // Timer isn't thread-safe and while it is running, it must only be used on
  // the same sequence until fully Stop()'ed. Once stopped, it may be destroyed
  // or restarted on another sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Location in user code.
  Location posted_from_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Detects when the scheduled task is deleted before being executed. Null when
  // there is no scheduled task.
  // `task_destruction_detector_` is not a raw_ptr<...> for performance reasons
  // (based on analysis of sampling profiler data).
  TaskDestructionDetector* task_destruction_detector_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If true, |user_task_| is scheduled to run sometime in the future.
  bool is_running_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The handle to the posted delayed task.
  DelayedTaskHandle delayed_task_handle_ GUARDED_BY_CONTEXT(sequence_checker_);

 private:
  friend class TaskDestructionDetector;

  // Indicates that the scheduled task was destroyed from inside the queue.
  // Stops the timer if it was running.
  void OnTaskDestroyed();
};

//-----------------------------------------------------------------------------
// This class wraps logic shared by (Retaining)OneShotTimer and RepeatingTimer.
class BASE_EXPORT DelayTimerBase : public TimerBase {
 public:
  DelayTimerBase(const DelayTimerBase&) = delete;
  DelayTimerBase& operator=(const DelayTimerBase&) = delete;

  ~DelayTimerBase() override;

  // Returns the current delay for this timer.
  TimeDelta GetCurrentDelay() const;

  // Call this method to reset the timer delay. The user task must be set. If
  // the timer is not running, this will start it by posting a task.
  virtual void Reset();

  void Stop() override;

  // Abandons the scheduled task (if any) and stops the timer (if running). Use
  // this instead of Stop() only if the timer will need to be used or destroyed
  // on another sequence.
  // TODO(1262205): Remove once kAlwaysAbandonScheduledTask is gone.
  void AbandonAndStop();

  TimeTicks desired_run_time() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return desired_run_time_;
  }

 protected:
  // Constructs a timer. Start must be called later to set task info.
  // If |tick_clock| is provided, it is used instead of TimeTicks::Now() to get
  // TimeTicks when scheduling tasks.
  explicit DelayTimerBase(const TickClock* tick_clock = nullptr);

  // Construct a timer with task info.
  // If |tick_clock| is provided, it is used instead of TimeTicks::Now() to get
  // TimeTicks when scheduling tasks.
  DelayTimerBase(const Location& posted_from,
                 TimeDelta delay,
                 const TickClock* tick_clock = nullptr);

  // Schedules |OnScheduledTaskInvoked()| to run on the current sequence with
  // the given |delay|. |scheduled_run_time_| and |desired_run_time_| are reset
  // to Now() + delay.
  void ScheduleNewTask(TimeDelta delay);

  void StartInternal(const Location& posted_from, TimeDelta delay);

 private:
  // Returns the current tick count.
  TimeTicks Now() const;

  // Called when the scheduled task is invoked. Will run the  |user_task| if the
  // timer is still running and |desired_run_time_| was reached.
  // |task_destruction_detector| is owned by the callback to detect when the
  // scheduled task is deleted before being executed.
  void OnScheduledTaskInvoked(
      std::unique_ptr<TaskDestructionDetector> task_destruction_detector);

  // Delay requested by user.
  TimeDelta delay_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The time at which the scheduled task is expected to fire. This time can be
  // null if the task must be run immediately.
  TimeTicks scheduled_run_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The desired run time of |user_task_|. The user may update this at any time,
  // even if their previous request has not run yet. If |desired_run_time_| is
  // greater than |scheduled_run_time_|, a continuation task will be posted to
  // wait for the remaining time. This allows us to reuse the pending task so as
  // not to flood the delayed queues with orphaned tasks when the user code
  // excessively Stops and Starts the timer. This time can be a "zero" TimeTicks
  // if the task must be run immediately.
  TimeTicks desired_run_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The tick clock used to calculate the run time for scheduled tasks.
  const raw_ptr<const TickClock> tick_clock_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace internal

//-----------------------------------------------------------------------------
// A simple, one-shot timer.  See usage notes at the top of the file.
class BASE_EXPORT OneShotTimer : public internal::DelayTimerBase {
 public:
  OneShotTimer();
  explicit OneShotTimer(const TickClock* tick_clock);

  OneShotTimer(const OneShotTimer&) = delete;
  OneShotTimer& operator=(const OneShotTimer&) = delete;

  ~OneShotTimer() override;

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call the given |user_task|.
  virtual void Start(const Location& posted_from,
                     TimeDelta delay,
                     OnceClosure user_task);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call a task formed from
  // |receiver->*method|.
  template <class Receiver>
  void Start(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)()) {
    Start(posted_from, delay, BindOnce(method, Unretained(receiver)));
  }

  // Run the scheduled task immediately, and stop the timer. The timer needs to
  // be running.
  void FireNow();

 private:
  void OnStop() final;
  void RunUserTask() final;

  OnceClosure user_task_;
};

//-----------------------------------------------------------------------------
// A simple, repeating timer.  See usage notes at the top of the file.
class BASE_EXPORT RepeatingTimer : public internal::DelayTimerBase {
 public:
  RepeatingTimer();
  explicit RepeatingTimer(const TickClock* tick_clock);

  RepeatingTimer(const RepeatingTimer&) = delete;
  RepeatingTimer& operator=(const RepeatingTimer&) = delete;

  ~RepeatingTimer() override;

  RepeatingTimer(const Location& posted_from,
                 TimeDelta delay,
                 RepeatingClosure user_task);
  RepeatingTimer(const Location& posted_from,
                 TimeDelta delay,
                 RepeatingClosure user_task,
                 const TickClock* tick_clock);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call the given |user_task|.
  virtual void Start(const Location& posted_from,
                     TimeDelta delay,
                     RepeatingClosure user_task);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call a task formed from
  // |receiver->*method|.
  template <class Receiver>
  void Start(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)()) {
    Start(posted_from, delay, BindRepeating(method, Unretained(receiver)));
  }

  const RepeatingClosure& user_task() const { return user_task_; }

 private:
  // Mark this final, so that the destructor can call this safely.
  void OnStop() final;

  void RunUserTask() override;

  RepeatingClosure user_task_;
};

//-----------------------------------------------------------------------------
// A simple, one-shot timer with the retained |user_task| which is reused when
// Reset() is invoked. See usage notes at the top of the file.
class BASE_EXPORT RetainingOneShotTimer : public internal::DelayTimerBase {
 public:
  RetainingOneShotTimer();
  explicit RetainingOneShotTimer(const TickClock* tick_clock);

  RetainingOneShotTimer(const RetainingOneShotTimer&) = delete;
  RetainingOneShotTimer& operator=(const RetainingOneShotTimer&) = delete;

  ~RetainingOneShotTimer() override;

  RetainingOneShotTimer(const Location& posted_from,
                        TimeDelta delay,
                        RepeatingClosure user_task);
  RetainingOneShotTimer(const Location& posted_from,
                        TimeDelta delay,
                        RepeatingClosure user_task,
                        const TickClock* tick_clock);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call the given |user_task|.
  virtual void Start(const Location& posted_from,
                     TimeDelta delay,
                     RepeatingClosure user_task);

  // Start the timer to run at the given |delay| from now. If the timer is
  // already running, it will be replaced to call a task formed from
  // |receiver->*method|.
  template <class Receiver>
  void Start(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)()) {
    Start(posted_from, delay, BindRepeating(method, Unretained(receiver)));
  }

  const RepeatingClosure& user_task() const { return user_task_; }

 private:
  // Mark this final, so that the destructor can call this safely.
  void OnStop() final;

  void RunUserTask() override;

  RepeatingClosure user_task_;
};

//-----------------------------------------------------------------------------
// A Delay timer is like The Button from Lost. Once started, you have to keep
// calling Reset otherwise it will call the given method on the sequence it was
// initially Reset() from.
//
// Once created, it is inactive until Reset is called. Once |delay| seconds have
// passed since the last call to Reset, the callback is made. Once the callback
// has been made, it's inactive until Reset is called again.
//
// If destroyed, the timeout is canceled and will not occur even if already
// inflight.
class DelayTimer {
 public:
  template <class Receiver>
  DelayTimer(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)())
      : DelayTimer(posted_from, delay, receiver, method, nullptr) {}

  template <class Receiver>
  DelayTimer(const Location& posted_from,
             TimeDelta delay,
             Receiver* receiver,
             void (Receiver::*method)(),
             const TickClock* tick_clock)
      : timer_(posted_from,
               delay,
               BindRepeating(method, Unretained(receiver)),
               tick_clock) {}

  DelayTimer(const DelayTimer&) = delete;
  DelayTimer& operator=(const DelayTimer&) = delete;

  void Reset() { timer_.Reset(); }

 private:
  RetainingOneShotTimer timer_;
};

//-----------------------------------------------------------------------------
// A one-shot timer that attempts to run |user_task| some time near specified
// deadline. See usage notes at the top of the file.
class BASE_EXPORT DeadlineTimer : public internal::TimerBase {
 public:
  DeadlineTimer();
  ~DeadlineTimer() override;

  DeadlineTimer(const DeadlineTimer&) = delete;
  DeadlineTimer& operator=(const DeadlineTimer&) = delete;

  // Start the timer to run |user_task| near the specified |deadline|;
  // preferably as close as possible to the specified time if |exact|, or
  // preferably a little bit before than after otherwise. If the timer is
  // already running, it will be replaced to call the given |user_task|.
  void Start(const Location& posted_from,
             TimeTicks deadline,
             OnceClosure user_task,
             ExactDeadline exact = ExactDeadline(false));

  // Start the timer to run |user_task| near the specified |deadline|. If the
  // timer is already running, it will be replaced to call a task formed from
  // |receiver->*method|.
  template <class Receiver>
  void Start(const Location& posted_from,
             TimeTicks deadline,
             Receiver* receiver,
             void (Receiver::*method)(),
             ExactDeadline exact = ExactDeadline(false)) {
    Start(posted_from, deadline, BindOnce(method, Unretained(receiver)), exact);
  }

 protected:
  void OnStop() override;
  void RunUserTask() override;

  // Schedules |OnScheduledTaskInvoked()| to run on the current sequence at
  // the given |deadline|.
  void ScheduleNewTask(TimeTicks deadline, subtle::DelayPolicy delay_policy);

 private:
  // Called when the scheduled task is invoked to run the |user_task|.
  // |task_destruction_detector| is owned by the callback to detect when the
  // scheduled task is deleted before being executed.
  void OnScheduledTaskInvoked(std::unique_ptr<internal::TaskDestructionDetector>
                                  task_destruction_detector);

  OnceClosure user_task_;
};

}  // namespace base

#endif  // BASE_TIMER_TIMER_H_
