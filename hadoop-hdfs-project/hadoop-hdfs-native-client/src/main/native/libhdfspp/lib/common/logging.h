/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIB_COMMON_LOGGING_H_
#define LIB_COMMON_LOGGING_H_

#include "hdfspp/hdfs_ext.h"

#include <iostream>
#include <sstream>
#include <mutex>
#include <memory>

namespace hdfs {

/**
 *  Logging mechanism to provide lightweight logging to stderr as well as
 *  as a callback mechanism to allow C clients and larger third party libs
 *  to be used to handle logging.
 **/

enum LogLevel {
  kTrace     = 0,
  kDebug     = 1,
  kInfo      = 2,
  kWarning   = 3,
  kError     = 4,
};

enum LogSourceComponent {
  kUnknown     = 1 << 0,
  kRPC         = 1 << 1,
  kBlockReader = 1 << 2,
  kFileHandle  = 1 << 3,
  kFileSystem  = 1 << 4,
};

class LogMessageObj;

#define LogMessage(LEVEL, COMPONENT) (LogMessageObj(LEVEL,COMPONENT) << "[this=" << ((void*)this) << "] ")
#define LOG_DEBUG() LogMessage(kDebug, kUnknown)
#define LOG_INFO() LogMessage(kInfo, kUnknown)
#define LOG_WARN() LogMessage(kWarning, kUnknown)
#define LOG_ERROR() LogMessage(kError, kUnknown)

class LoggerInterface {
 public:
  LoggerInterface() : component_mask_(0xFFFFFFFF), level_threshold_(kTrace) {};
  virtual ~LoggerInterface() {};
  /**
   *  Allow log manager to determine if something is worth logging.
   *  Don't want these to be virtual for performance reasons.
   **/
  bool ShouldLog(LogLevel level, LogSourceComponent component);
  void EnableLoggingForComponent(LogSourceComponent c);
  void DisableLoggingForComponent(LogSourceComponent c);
  void SetLogLevel(LogLevel level);

  /**
   *  User defined handling messages, common case would be printing somewhere.
   **/
  virtual void Write(const LogMessageObj& msg) = 0;
 private:
  /**
   * Derived classes should not access directly.
   **/
  uint32_t component_mask_;
  uint32_t level_threshold_;
};


class StderrLogger : public LoggerInterface {
 public:
  StderrLogger() : show_timestamp_(true), show_level_(true),
                   show_thread_(true), show_component_(true) {}
  void Write(const LogMessageObj& msg);
  void set_show_timestamp(bool show);
  void set_show_level(bool show);
  void set_show_thread(bool show);
  void set_show_component(bool show);
 private:
  bool show_timestamp_;
  bool show_level_;
  bool show_thread_;
  bool show_component_;
};

class CForwardingLogger : public LoggerInterface {
 public:
  CForwardingLogger() : callback_(nullptr) {};

  // Converts LogMessage into LogData, a POD type,
  // and invokes callback_ if it's not null.
  void Write(const LogMessageObj& msg);

  // pass in NULL to clear the hook
  void SetCallback(void (*callback)(LogData*));

  //return a copy, or null on failure.
  static LogData *CopyLogData(const LogData*);
  //free LogData allocated with CopyLogData
  static void FreeLogData(LogData*);
 private:
  void (*callback_)(LogData*);
};


/**
 *  LogManager provides a thread safe static interface to the underlying
 *  logger implementation.
 **/
class LogManager {
 friend class LogMessageObj;
 public:
  static bool ShouldLog(LogLevel level, LogSourceComponent source);
  static void Write(const LogMessageObj & msg);
  static void EnableLogForComponent(LogSourceComponent c);
  static void DisableLogForComponent(LogSourceComponent c);
  static void SetLogLevel(LogLevel level);
  static void SetLoggerImplementation(std::unique_ptr<LoggerInterface> impl);

 private:
  // don't create instances of this
  LogManager();
  // synchronize all unsafe plugin calls
  static std::mutex impl_lock_;
  static std::unique_ptr<LoggerInterface> logger_impl_;
};

class LogMessageObj {
 friend class LogManager;
 public:
  LogMessageObj(const LogLevel &l, LogSourceComponent component = kUnknown) :
             worth_reporting_(LogManager::ShouldLog(l,component)),
             level_(l), component_(component) {
    //std::cerr << "msg ctor called, worth_reporting_=" << worth_reporting_
    //          << "level = " << level_string() << ", component=" << component_string() << std::endl;
  }

  ~LogMessageObj();

  bool is_worth_reporting() const { return worth_reporting_; };
  const char *level_string() const;
  const char *component_string() const;
  LogLevel level() const {return level_; }
  LogSourceComponent component() const {return component_; }

  //print as-is, no-op if pointer params are null
  LogMessageObj& operator<<(const char *);
  LogMessageObj& operator<<(const std::string&);
  LogMessageObj& operator<<(const std::string*);

  //convert to a string "true"/"false"
  LogMessageObj& operator<<(bool);

  LogMessageObj& operator<<(int32_t);
  LogMessageObj& operator<<(uint32_t);
  LogMessageObj& operator<<(int64_t);
  LogMessageObj& operator<<(uint64_t);

  //print address as hex
  LogMessageObj& operator<<(void *);

  std::string MsgString() const;

 private:
  // true if level/component settings say this msg should be printed
  const bool worth_reporting_;
  LogLevel level_;
  LogSourceComponent component_;
  std::stringstream msg_buffer_;
};

}

#endif
