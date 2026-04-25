#include "command_history.h"

void CommandHistory::execute(std::unique_ptr<ICommand> command) {
    if (currentIndex < commands.size()) {
        commands.erase(commands.begin() + currentIndex, commands.end());
    }
    
    command->execute();
    commands.push_back(std::move(command));
    currentIndex++;
}

void CommandHistory::undo() {
    if (canUndo()) {
        currentIndex--;
        commands[currentIndex]->undo();
    }
}

void CommandHistory::redo() {
    if (canRedo()) {
        commands[currentIndex]->execute();
        currentIndex++;
    }
}

void CommandHistory::clear() {
    commands.clear();
    currentIndex = 0;
}