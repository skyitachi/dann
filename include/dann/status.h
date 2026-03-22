#pragma once

#include <string>
#include <algorithm>

namespace dann {

class Status {
public:
    enum Code {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kNotSupported = 3,
        kInvalidArgument = 4,
        kIOError = 5,
        kNotExist = 6,
        kAlreadyExist = 7,
        kInProgress = 8,
    };

    Status() : state_(nullptr) {}
    ~Status() { delete[] state_; }

    Status(const Status& rhs);
    Status& operator=(const Status& rhs);
    Status(Status&& rhs) noexcept : state_(rhs.state_) {
        rhs.state_ = nullptr;
    }
    Status& operator=(Status&& rhs) noexcept;

    bool ok() const { return (state_ == nullptr); }

    Code code() const {
        return ok() ? kOk : static_cast<Code>(state_[4]);
    }

    std::string ToString() const;

    static Status OK() { return Status(); }

    static Status NotFound(const std::string& msg, const std::string& msg2 = "") {
        return Status(kNotFound, msg, msg2);
    }

    static Status Corruption(const std::string& msg, const std::string& msg2 = "") {
        return Status(kCorruption, msg, msg2);
    }

    static Status NotSupported(const std::string& msg, const std::string& msg2 = "") {
        return Status(kNotSupported, msg, msg2);
    }

    static Status InvalidArgument(const std::string& msg, const std::string& msg2 = "") {
        return Status(kInvalidArgument, msg, msg2);
    }

    static Status IOError(const std::string& msg, const std::string& msg2 = "") {
        return Status(kIOError, msg, msg2);
    }

    static Status NotExist(const std::string& msg, const std::string& msg2 = "") {
        return Status(kNotExist, msg, msg2);
    }

    static Status AlreadyExist(const std::string& msg, const std::string& msg2 = "") {
        return Status(kAlreadyExist, msg, msg2);
    }

    static Status InProgress(const std::string& msg, const std::string& msg2 = "") {
        return Status(kInProgress, msg, msg2);
    }

private:
    Status(Code code, const std::string& msg, const std::string& msg2);

    const char* state_;
    static const char* CopyState(const char* s);
};

inline Status::Status(const Status& rhs) {
    state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
}

inline Status& Status::operator=(const Status& rhs) {
    if (this != &rhs) {
        delete[] state_;
        state_ = (rhs.state_ == nullptr) ? nullptr : CopyState(rhs.state_);
    }
    return *this;
}

inline Status& Status::operator=(Status&& rhs) noexcept {
    if (this != &rhs) {
        delete[] state_;
        state_ = rhs.state_;
        rhs.state_ = nullptr;
    }
    return *this;
}

} // namespace dann
