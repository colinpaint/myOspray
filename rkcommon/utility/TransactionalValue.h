// Copyright 2009 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mutex>

namespace rkcommon {
  namespace utility {

    /* This implements a 1-to-1 value fence. One thread can set (or "queue") a
     * value for another thread to later get. This is conceptually similar to
     * "doublebuffering" a single value. Note that all values from the producer
     * thread overwrite the "queued" value, where the consumer thread will
     * always get the last value set by the producer thread.
     */
    template <typename T>
    class TransactionalValue
    {
     public:
      TransactionalValue()  = default;
      ~TransactionalValue() = default;

      template <typename OtherType>
      TransactionalValue(const OtherType &ot);

      template <typename OtherType>
      TransactionalValue &operator=(const OtherType &ot);

      TransactionalValue<T> &operator=(const TransactionalValue<T> &fp);

      T &ref();
      T get();

      bool update();

     private:
      bool newValue{false};
      T queuedValue;
      T currentValue;

      std::mutex mutex;
    };

    // Inlined TransactionalValue Members /////////////////////////////////////

    template <typename T>
    template <typename OtherType>
    inline TransactionalValue<T>::TransactionalValue(const OtherType &ot)
    {
      currentValue = ot;
    }

    template <typename T>
    template <typename OtherType>
    inline TransactionalValue<T> &TransactionalValue<T>::operator=(
        const OtherType &ot)
    {
      std::lock_guard<std::mutex> lock{mutex};
      queuedValue = ot;
      newValue    = true;
      return *this;
    }

    template <typename T>
    inline TransactionalValue<T> &TransactionalValue<T>::operator=(
        const TransactionalValue<T> &fp)
    {
      std::lock_guard<std::mutex> lock{mutex};
      queuedValue = fp.ref();
      newValue    = true;
      return *this;
    }

    template <typename T>
    inline T &TransactionalValue<T>::ref()
    {
      return currentValue;
    }

    template <typename T>
    inline T TransactionalValue<T>::get()
    {
      return currentValue;
    }

    template <typename T>
    inline bool TransactionalValue<T>::update()
    {
      bool didUpdate = false;
      if (newValue) {
        std::lock_guard<std::mutex> lock{mutex};
        currentValue = std::move(queuedValue);
        newValue     = false;
        didUpdate    = true;
      }

      return didUpdate;
    }

  }  // namespace utility
}  // namespace rkcommon
