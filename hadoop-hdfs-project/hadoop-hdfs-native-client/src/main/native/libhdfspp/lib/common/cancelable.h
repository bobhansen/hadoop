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
#ifndef LIB_COMMON_CANCELABLE_H_
#define LIB_COMMON_CANCELABLE_H_

#include <memory>

namespace hdfs {

class Cancelable {
public:
  virtual void cancel() = 0;
};

class NullCancelable : public Cancelable {
public:
  NullCancelable() {};
  virtual void cancel() {};
  
};

class CancelHandle : public Cancelable {
public:
  CancelHandle(std::shared_ptr<Cancelable> target) : target_(target) {}
  
  void cancel() override { target_->cancel(); }
private:
  std::shared_ptr<Cancelable> target_;
};

}

#endif
