/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_SHARED_MODEL_STRING_BUILDER_HPP
#define IROHA_SHARED_MODEL_STRING_BUILDER_HPP

#include <string>

namespace shared_model {
  namespace detail {
    /**
     * A simple string builder class for building pretty looking strings
     */
    class PrettyStringBuilder {
     public:
      /**
       * Initializes new string with a provided name
       * @param name - name to initialize
       */
      PrettyStringBuilder &init(const std::string &name);

      /**
       * Inserts new level marker
       */
      PrettyStringBuilder &insertLevel();

      /**
       * Closes new level marker
       */
      PrettyStringBuilder &removeLevel();

      /**
       * Appends new field to string as a "name=value" pair
       * @param name - field name to append
       * @param value - field value
       */
      PrettyStringBuilder &append(const std::string &name,
                                  const std::string &value);

      /**
       * Appends new single value to string
       * @param value - value to append
       */
      PrettyStringBuilder &append(const std::string &value);

      /**
       * Appends a new collection to string
       * @tparam Collection - type of collection
       * @tparam Transform - type of transformation function
       * @param c - collection to append
       * @param t - transformation function
       */
      template <typename Collection, typename Transform>
      PrettyStringBuilder &appendAll(Collection &&c, Transform &&t) {
        insertLevel();
        for (auto &val : c) {
          append(t(val));
        }
        removeLevel();
        return *this;
      }

      /**
       * Appends a new named collection to string
       * @tparam Collection - type of collection
       * @tparam Transform - type of transformation function
       * @param name - field name to append
       * @param c - collection to append
       * @param t - transformation function
       */
      template <typename Collection, typename Transform>
      PrettyStringBuilder &appendAll(const std::string &name,
                                     Collection &&c,
                                     Transform &&t) {
        result_.append(name);
        result_.append(keyValueSeparator);
        appendAll(c, t);
        result_.append(singleFieldsSeparator);
        result_.append(spaceSeparator);
        return *this;
      }

      /**
       * Appends a new named collection to string
       * @param c - iterable collection to append using toString method
       */
      template <typename Collection>
      std::enable_if_t<
          std::is_same<
              typename std::decay<decltype(
                  std::declval<Collection>().begin()->toString())>::type,
              std::string>::value,
          PrettyStringBuilder &>
      appendAll(Collection &&c) {
        appendAll(c, [](const auto &o) { return o.toString(); });
        return *this;
      }

      /**
       * Appends a new named collection to string
       * @param c - iterable collection of pointers
       */
      template <typename Collection>
      std::enable_if_t<std::is_same<typename std::decay<decltype(
                                        (*std::declval<Collection>().begin())
                                            ->toString())>::type,
                                    std::string>::value,
                       PrettyStringBuilder &>
      appendAll(Collection &&c) {
        appendAll(c, [](const auto &o) { return o->toString(); });
        return *this;
      }

      /**
       * Finalizes appending and returns constructed string.
       * @return resulted string
       */
      std::string finalize();

     private:
      std::string result_;
      static const std::string beginBlockMarker;
      static const std::string endBlockMarker;
      static const std::string keyValueSeparator;
      static const std::string singleFieldsSeparator;
      static const std::string initSeparator;
      static const std::string spaceSeparator;
    };
  }  // namespace detail
}  // namespace shared_model

#endif  // IROHA_SHARED_MODEL_STRING_BUILDER_HPP
