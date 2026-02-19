#pragma once

#include "command.h"
#include <memory>
#include <vector>

class CommandHistory {
public:
    void execute(std::unique_ptr<ICommand> command);
    void undo();
    void redo();
    
    bool canUndo() const { return currentIndex > 0; }
    bool canRedo() const { return currentIndex < commands.size(); }
    
    void clear();

private:
    std::vector<std::unique_ptr<ICommand>> commands;
    size_t currentIndex = 0;
};