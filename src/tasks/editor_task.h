#pragma once

#include <algorithm>
#include <string>

/**
 * @brief Generic base class for long-running editor tasks with progress.
 *
 * Implement custom behavior by overriding `onStart()`, `onUpdate()`, and optionally `onCancel()`.
 */
class EditorTaskBase {
public:
    enum class State {
        Idle,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    explicit EditorTaskBase(std::string taskName)
        : name(std::move(taskName)) {}

    virtual ~EditorTaskBase() = default;

    bool start() {
        if (state == State::Running) return false;
        progress = 0.0f;
        status = "Starting...";
        state = State::Running;

        std::string error;
        if (!onStart(error)) {
            fail(error.empty() ? "Failed to start task." : error);
            return false;
        }
        return true;
    }

    void update() {
        if (state != State::Running) return;
        onUpdate();
    }

    void requestCancel() {
        if (state != State::Running) return;
        onCancel();
        markCancelled(status.empty() ? "Task cancelled." : status);
    }

    const std::string& getName() const { return name; }
    const std::string& getStatus() const { return status; }
    float getProgress() const { return progress; }
    State getState() const { return state; }

    bool isRunning() const { return state == State::Running; }
    bool isDone() const {
        return state == State::Completed || state == State::Failed || state == State::Cancelled;
    }

protected:
    virtual bool onStart(std::string& error) {
        (void)error;
        return true;
    }

    virtual void onUpdate() = 0;

    virtual void onCancel() {}

    void setProgress(float value, const std::string& message = "") {
        progress = std::clamp(value, 0.0f, 1.0f);
        if (!message.empty()) status = message;
    }

    void complete(const std::string& message = "Task completed.") {
        progress = 1.0f;
        status = message;
        state = State::Completed;
    }

    void fail(const std::string& message) {
        status = message;
        state = State::Failed;
    }

    void markCancelled(const std::string& message = "Task cancelled.") {
        status = message;
        state = State::Cancelled;
    }

private:
    std::string name;
    std::string status;
    float progress = 0.0f;
    State state = State::Idle;
};
