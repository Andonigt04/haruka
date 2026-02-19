#include "console.h"
#include <ctime>
#include <iomanip>

ConsolePanel::ConsolePanel() {
    addLog(LogLevel::Info, "Console initialized");
}

ConsolePanel::~ConsolePanel() {}

std::string ConsolePanel::getCurrentTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

void ConsolePanel::addLog(LogLevel level, const std::string& message) {
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.timestamp = getCurrentTimestamp();
    logs.push_back(entry);
    
    // Limitar a 1000 logs
    if (logs.size() > 1000) {
        logs.erase(logs.begin());
    }
}

void ConsolePanel::info(const std::string& message) {
    addLog(LogLevel::Info, message);
}

void ConsolePanel::warning(const std::string& message) {
    addLog(LogLevel::Warning, message);
}

void ConsolePanel::error(const std::string& message) {
    addLog(LogLevel::Error, message);
}

void ConsolePanel::clear() {
    logs.clear();
}

ImVec4 ConsolePanel::getColorForLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Info:    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        case LogLevel::Warning: return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
        case LogLevel::Error:   return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
        default:                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

void ConsolePanel::onImGuiRender() {
    ImGui::Begin("Console");
    
    // Toolbar
    if (ImGui::Button("Clear")) {
        clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warnings", &showWarnings);
    ImGui::SameLine();
    ImGui::Checkbox("Errors", &showErrors);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Filter", filterBuffer, sizeof(filterBuffer));
    
    ImGui::Separator();
    
    // Log list
    ImGui::BeginChild("LogList", ImVec2(0, 0), true);
    
    std::string filter(filterBuffer);
    
    for (const auto& log : logs) {
        if (log.level == LogLevel::Info && !showInfo) continue;
        if (log.level == LogLevel::Warning && !showWarnings) continue;
        if (log.level == LogLevel::Error && !showErrors) continue;
        
        if (!filter.empty() && log.message.find(filter) == std::string::npos) {
            continue;
        }
        
        // Show Log
        ImVec4 color = getColorForLevel(log.level);
        ImGui::TextColored(color, "[%s]", log.timestamp.c_str());
        ImGui::SameLine();
        
        const char* levelStr = "";
        switch (log.level) {
            case LogLevel::Info:    levelStr = "INFO"; break;
            case LogLevel::Warning: levelStr = "WARN"; break;
            case LogLevel::Error:   levelStr = "ERROR"; break;
        }
        
        ImGui::TextColored(color, "[%s]", levelStr);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", log.message.c_str());
    }
    
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    ImGui::End();
}

StreamCapture::CaptureBuffer::CaptureBuffer(ConsolePanel* console, LogLevel level, std::streambuf* original)
    : console(console), level(level), originalBuf(original) {}

int StreamCapture::CaptureBuffer::overflow(int c) {
    if (c != EOF) {
        buffer += static_cast<char>(c);
        if (c == '\n') {
            if (!buffer.empty() && buffer.back() == '\n') {
                buffer.pop_back();
            }
            if (!buffer.empty()) {
                console->addLog(level, buffer);
            }
            buffer.clear();
        }
    }
    return originalBuf->sputc(c);
}

std::streamsize StreamCapture::CaptureBuffer::xsputn(const char* s, std::streamsize n) {
    for (std::streamsize i = 0; i < n; ++i) {
        overflow(s[i]);
    }
    return n;
}

StreamCapture::StreamCapture(std::ostream& stream, ConsolePanel* console, LogLevel level)
    : stream(stream), originalBuf(stream.rdbuf()), captureBuffer(console, level, originalBuf) {
    stream.rdbuf(&captureBuffer);
}

StreamCapture::~StreamCapture() {
    stream.rdbuf(originalBuf);
}