#pragma once

#include "command.h"
#include <memory>
#include <vector>

/**
 * @brief Linear undo/redo command history.
 */
class CommandHistory {
public:
    /** @brief Executes and stores a new command, truncating redo tail. */
    void execute(std::unique_ptr<ICommand> command);
    /** @brief Reverts the last executed command if available. */
    void undo();
    /** @brief Reapplies the next command in the redo chain. */
    void redo();
    
    /** @brief Returns true when an undo step is available. */
    bool canUndo() const { return currentIndex > 0; }
    /** @brief Returns true when a redo step is available. */
    bool canRedo() const { return currentIndex < commands.size(); }
    
    /** @brief Clears all stored commands and resets the index. */
    void clear();

private:
    std::vector<std::unique_ptr<ICommand>> commands;
    size_t currentIndex = 0;
};