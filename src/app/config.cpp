#include "termchart/app/config.hpp"

#include <utility>

namespace termchart::app {

Config& Config::global() noexcept {
    static Config instance;
    return instance;
}

void Config::set(std::string_view key, ConfigValue value) {
    const std::lock_guard<std::mutex> lock(mutex_);
    values_[std::string(key)] = std::move(value);
}

ConfigValue Config::get(std::string_view key) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(std::string(key));
    return it != values_.end() ? it->second : ConfigValue{std::monostate{}};
}

bool Config::has(std::string_view key) const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return values_.find(std::string(key)) != values_.end();
}

bool Config::remove(std::string_view key) {
    const std::lock_guard<std::mutex> lock(mutex_);
    return values_.erase(std::string(key)) > 0;
}

void Config::clear() noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    values_.clear();
}

std::string Config::get_string(std::string_view key, std::string_view fallback) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(std::string(key));
    if (it == values_.end()) {
        return std::string(fallback);
    }
    const std::string* value = std::get_if<std::string>(&it->second);
    return value != nullptr ? *value : std::string(fallback);
}

std::int64_t Config::get_int(std::string_view key, std::int64_t fallback) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(std::string(key));
    if (it == values_.end()) {
        return fallback;
    }
    const std::int64_t* value = std::get_if<std::int64_t>(&it->second);
    return value != nullptr ? *value : fallback;
}

double Config::get_double(std::string_view key, double fallback) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(std::string(key));
    if (it == values_.end()) {
        return fallback;
    }
    const double* value = std::get_if<double>(&it->second);
    return value != nullptr ? *value : fallback;
}

bool Config::get_bool(std::string_view key, bool fallback) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto it = values_.find(std::string(key));
    if (it == values_.end()) {
        return fallback;
    }
    const bool* value = std::get_if<bool>(&it->second);
    return value != nullptr ? *value : fallback;
}

std::size_t Config::size() const noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    return values_.size();
}

void set_config(std::string_view key, ConfigValue value) {
    Config::global().set(key, std::move(value));
}

ConfigValue get_config(std::string_view key) { return Config::global().get(key); }

}  // namespace termchart::app
