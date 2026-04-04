#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>

/** @brief Log severity levels displayed by the console panel. */
enum class LogLevel {
    Info,
    Warning,
    Error
};

/** @brief Single console log entry. */
struct LogEntry {
    LogLevel level;
    std::string message;
    std::string timestamp;
};

/**
 * @brief ImGui console panel with log filtering and stream capture.
 */
class ConsolePanel {
public:
    /** @brief Constructs a console panel with default filters enabled. */
    ConsolePanel();
    /** @brief Releases panel resources. */
    ~ConsolePanel();
    
    /** @brief Draws the console UI. */
    void onImGuiRender();
    /** @brief Clears all log entries. */
    void clear();
    
    /** @brief Appends a message with a severity. */
    void addLog(LogLevel level, const std::string& message);
    /** @brief Convenience info log helper. */
    void info(const std::string& message);
    /** @brief Convenience warning log helper. */
    void warning(const std::string& message);
    /** @brief Convenience error log helper. */
    void error(const std::string& message);

private:
    std::vector<LogEntry> logs;
    mutable std::mutex logsMutex;
    bool autoScroll = true;
    bool showInfo = true;
    bool showWarnings = true;
    bool showErrors = true;
    char filterBuffer[256] = "";
    
    /** @brief Returns UI color for one log level. */
    ImVec4 getColorForLevel(LogLevel level);
    /** @brief Returns a formatted timestamp string. */
    std::string getCurrentTimestamp();
};

/**
 * @brief Redirects an output stream into the console panel.
 */
class StreamCapture {
public:
    /** @brief Begins capturing one stream. */
    StreamCapture(std::ostream& stream, ConsolePanel* console, LogLevel level);
    /** @brief Restores the original stream buffer. */
    ~StreamCapture();

private:
    /** @brief Stream buffer that forwards text to the console panel. */
    class CaptureBuffer : public std::streambuf {
    public:
        /** @brief Creates a capture buffer wrapper. */
        CaptureBuffer(ConsolePanel* console, LogLevel level, std::streambuf* original);
        
    protected:
        /** @brief Handles one character write. */
        int overflow(int c) override;
        /** @brief Handles block writes. */
        std::streamsize xsputn(const char* s, std::streamsize n) override;
        
    private:
        ConsolePanel* console;
        LogLevel level;
        std::streambuf* originalBuf;
        std::string buffer;
        std::mutex bufferMutex;
    };
    
    std::ostream& stream;
    std::streambuf* originalBuf;
    CaptureBuffer captureBuffer;
};