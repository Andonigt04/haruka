#pragma once

#pragma once

/**
 * @brief Undoable editor command contract.
 */
class ICommand {
public:
    /** @brief Virtual destructor for polymorphic deletion. */
    virtual ~ICommand() = default;
    /** @brief Executes the command. */
    virtual void execute() = 0;
    /** @brief Reverts the command effects. */
    virtual void undo() = 0;
};