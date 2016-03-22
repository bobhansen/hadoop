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

#include "logging.h"

#include <ctime>
#include <cstring>
#include <thread>
#include <iostream>
#include <sstream>

namespace hdfs
{

LogManager::LogManager() {}
std::unique_ptr<LoggerInterface> LogManager::logger_impl_(new StderrLogger());
std::mutex LogManager::impl_lock_;
#define GET_IMPL_LOCK std::lock_guard<std::mutex> impl_lock(impl_lock_);

void LogManager::DisableLogForComponent(LogSourceComponent c) {
  // AND with all bits other than one we want to unset
  GET_IMPL_LOCK
  if(logger_impl_)
    logger_impl_->DisableLoggingForComponent(c);
}

void LogManager::EnableLogForComponent(LogSourceComponent c) {
  // OR with bit to set
  GET_IMPL_LOCK
  if(logger_impl_)
    logger_impl_->EnableLoggingForComponent(c);
}

void LogManager::SetLogLevel(LogLevel level) {
  GET_IMPL_LOCK
  if(logger_impl_)
    logger_impl_->SetLogLevel(level);
}

bool LogManager::ShouldLog(LogLevel level, LogSourceComponent source) {
  GET_IMPL_LOCK
  if(logger_impl_)
    return logger_impl_->ShouldLog(level, source);
  return false;
}

void LogManager::Write(const LogMessageObj& msg) {
  GET_IMPL_LOCK
  if(logger_impl_)
    logger_impl_->Write(msg);
}

void LogManager::SetLoggerImplementation(std::unique_ptr<LoggerInterface> impl) {
  GET_IMPL_LOCK
  logger_impl_.reset(nullptr);
  logger_impl_.reset(impl.release());
}


/**
 *  Encapsulate all the nasty bitwise stuff that all derivitives of the LoggerInterface
 *  need to have in common.  Don't want this to be a virtual call.
 **/
bool LoggerInterface::ShouldLog(LogLevel level, LogSourceComponent component) {
  if(level < level_threshold_)
    return false;
  if(!(component & component_mask_))
    return false;
  return true;
}

void LoggerInterface::EnableLoggingForComponent(LogSourceComponent c) {
  component_mask_ |= c;
}

void LoggerInterface::DisableLoggingForComponent(LogSourceComponent c) {
  component_mask_ &= ~c;
}

void LoggerInterface::SetLogLevel(LogLevel level) {
  std::cerr << "SETLOGLEVEL_CALLED_" << std::endl;
  level_threshold_ = level;
}

/**
 *  Simple plugin to dump logs to stderr
 **/
void StderrLogger::Write(const LogMessageObj& msg) {
  if(!msg.is_worth_reporting())
    return;

  std::stringstream formatted;

  if(show_level_)
    formatted << msg.level_string();

  if(show_component_)
    formatted << msg.component_string();

  if(show_timestamp_) {
    time_t current_time = std::time(nullptr);
    //asctime appends a \n, so null that out
    char * timestr = std::asctime(std::localtime(&current_time));
    int len = strlen(timestr);
    if(len >=2)
      timestr[len-1] = 0;
    formatted << '[' << timestr << ']';
  }

  if(show_component_) {
    formatted << "[Thread id = " << std::this_thread::get_id() << ']';
  }

  std::cerr << formatted.str() << "    " << msg.MsgString() << std::endl;
}

void StderrLogger::set_show_timestamp(bool show) {
  show_timestamp_ = show;
}
void StderrLogger::set_show_level(bool show) {
  show_level_ = show;
}
void StderrLogger::set_show_thread(bool show) {
  show_thread_ = show;
}
void StderrLogger::set_show_component(bool show) {
  show_component_ = show;
}

/**
 *  Plugin to forward message to a C function pointer
 **/
void CForwardingLogger::Write(const LogMessageObj& msg) {
  if(!msg.is_worth_reporting())
    return;

  if(!callback_)
    return;

  const std::string text = msg.MsgString();

  LogData data;
  data.level = msg.level();
  data.component = msg.component();
  data.msg = text.c_str();
  callback_(&data);
}

void CForwardingLogger::SetCallback(void (*callback)(LogData*)) {
  callback_ = callback;
}

LogData *CForwardingLogger::CopyLogData(const LogData *orig) {
  if(!orig)
    return nullptr;

  LogData *copy = (LogData*)malloc(sizeof(LogData));
  if(!copy)
    return nullptr;

  copy->level = orig->level;
  copy->component = orig->component;
  if(orig->msg)
    copy->msg = strdup(orig->msg);
  return copy;
}

void CForwardingLogger::FreeLogData(LogData *data) {
  if(!data)
    return;
  if(data->msg)
    free((void*)data->msg);

  // Inexpensive way to help catch use-after-free
  memset(data, 0, sizeof(LogData));
  free(data);
}


LogMessageObj::~LogMessageObj() {
  if(worth_reporting_)
    LogManager::Write(*this);
}

LogMessageObj& LogMessageObj::operator<<(const std::string *str) {
  if(str && worth_reporting_)
    msg_buffer_ << str;
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(const std::string& str) {
  if(worth_reporting_)
    msg_buffer_ << str;
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(const char *str) {
  if(str && worth_reporting_) {
    msg_buffer_ << str;
  }
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(bool val) {
  if(worth_reporting_) {
    if(val)
      msg_buffer_ << "true";
    else
      msg_buffer_ << "false";
  }
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(int32_t val) {
  if(worth_reporting_)
    msg_buffer_ << val;
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(uint32_t val) {
  if(worth_reporting_)
    msg_buffer_ << val;
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(int64_t val) {
  if(worth_reporting_)
    msg_buffer_ << val;
  return *this;
}

LogMessageObj& LogMessageObj::operator<<(uint64_t val) {
  if(worth_reporting_)
    msg_buffer_ << val;
  return *this;
}


LogMessageObj& LogMessageObj::operator<<(void *ptr) {
  if(worth_reporting_)
    msg_buffer_ << ptr;
  return *this;
}

std::string LogMessageObj::MsgString() const {
  if(worth_reporting_)
    return msg_buffer_.str();
  return "";
}

const char * kLevelStrings[5] = {
  "[TRACE ]",
  "[DEBUG ]",
  "[INFO  ]",
  "[WARN  ]",
  "[ERROR ]"
};

const char * LogMessageObj::level_string() const {
  return kLevelStrings[level_];
}

const char * kComponentStrings[5] = {
  "[Unknown     ]",
  "[RPC         ]",
  "[BlockReader ]",
  "[FileHandle  ]",
  "[FileSystem  ]"
};

const char * LogMessageObj::component_string() const {
  switch(component_) {
    case kRPC: return kComponentStrings[1];
    case kBlockReader: return kComponentStrings[2];
    case kFileHandle: return kComponentStrings[3];
    case kFileSystem: return kComponentStrings[4];
    default: return kComponentStrings[0];
  }
}

}

