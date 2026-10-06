#ifndef PANS_INCLUDE_PANS_LOGGER_APPENDER_H
#define PANS_INCLUDE_PANS_LOGGER_APPENDER_H

#include <memory>
#include <string>

#include <pans/export.h>
#include <pans/logger/log_level.h>

namespace pans {

//用来代理我们的内部权限，严格控制谁能访问我们的Appender私有实现
namespace detail {
class AppenderAccess;
}

class PANS_API Appender final {
public:
    //声明一个内部实现类, 类外定义
    class Impl;
    ~Appender() = default;
    
    Appender(const Appender&) = delete;
    Appender& operator=(const Appender&) = delete;
    Appender(Appender&&) = delete;
    Appender& operator=(Appender&&) = delete;

    void setLevel(LogLevel::Level level) noexcept;
    [[nodiscard]] LogLevel::Level getLevel() const noexcept;

    void flush();
    void sync();

private:
    explicit Appender(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;
    friend class detail::AppenderAccess;
};

using AppenderPtr = std::shared_ptr<Appender>;

[[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();
[[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);

} // namespace pans

#endif // PANS_INCLUDE_PANS_LOGGER_APPENDER_H