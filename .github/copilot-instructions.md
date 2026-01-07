# GitHub Copilot Instructions

## Project Overview

This repository contains a lightweight, embeddable shell module written in C for embedded systems. The shell provides a command-line interface (CLI) with features like command export macros, variable management, command history, tab completion, passthrough mode, and user authentication.

**Key Technologies:**
- Language: C (C11 standard)
- Target: Embedded systems
- Features: Command registration, variable export, history, completion, authentication

## Code Style and Formatting

### Formatting Tool
- Use `clang-format` (version 21+) for all C code formatting
- Configuration is defined in `.clang-format` at the repository root
- **Always run clang-format before committing code changes**

### Style Guidelines
- **Brace Style**: Allman style (braces on separate lines)
- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Maximum 120 characters
- **Comments**: Use Chinese comments in headers for function/module descriptions (existing convention)
- **Naming Conventions**:
  - Functions: `snake_case` (e.g., `shell_init`, `shell_process`)
  - Macros/Constants: `UPPER_SNAKE_CASE` (e.g., `SHELL_CMD_SIZE`)
  - Types: `snake_case_t` suffix (e.g., `shell_cmd_t`, `shell_var_type_t`)
  - Struct member names: `snake_case`

## Build Instructions

This is a library module designed to be integrated into embedded projects. There is no standalone build system in the repository root.

**Integration:**
- Include `shell.h` in your project
- Compile `shell.c` and `shell_port.c` with your project's build system
- Implement platform-specific functions in `shell_port.c` if needed
- Link the compiled objects with your application

**Example Compilation:**
```bash
# For testing/validation purposes
gcc -c shell.c -o shell.o -std=c11 -Wall -Wextra
gcc -c shell_port.c -o shell_port.o -std=c11 -Wall -Wextra
```

## Configuration

The shell module is highly configurable through `shell_cfg.h`:

- `SHELL_USING_CMD_EXPORT`: Enable macro-based command export (requires linker script support)
- `SHELL_USING_VAR`: Enable variable read/write functionality
- `SHELL_USING_HISTORY`: Enable command history
- `SHELL_USING_COMPLETION`: Enable tab completion
- `SHELL_USING_PASSTHROUGH`: Enable passthrough mode
- `SHELL_USING_AUTH`: Enable user authentication

**Do not modify configuration defaults without good reason.** These are carefully chosen for embedded use cases.

## Testing

Currently, there is no automated test infrastructure in this repository. When making changes:

1. Manually verify code compiles without warnings using GCC/Clang
2. Check that changes don't break existing functionality
3. Test edge cases for any new features
4. Validate changes work in both authenticated and non-authenticated modes (when applicable)

## Embedded Systems Considerations

This code is designed for resource-constrained embedded systems:

- **Memory Usage**: Be mindful of stack and heap allocation
- **Buffer Sizes**: All buffer sizes are configurable in `shell_cfg.h`
- **No Dynamic Allocation**: Avoid using `malloc`/`free` unless absolutely necessary
- **Thread Safety**: The shell is not thread-safe by design; document any concurrency requirements
- **Platform Abstraction**: Use `shell_port.c` for platform-specific implementations

## Special Rules

- **No Breaking Changes**: Maintain backward compatibility with existing API
- **Minimize Dependencies**: Keep external dependencies to a minimum (currently only standard C library)
- **Security**: Be cautious with buffer operations to avoid overflows
- **Password Handling**: Use the existing hash mechanism (`SHELL_USING_HASH_PWD`) for passwords
- **Documentation**: Update function comments when changing behavior
- **Error Handling**: Return appropriate error codes; use existing error handling patterns

## File Structure

- `shell.h` - Public API and type definitions
- `shell.c` - Core shell implementation
- `shell_cfg.h` - Configuration options
- `shell_port.h` - Platform-specific function declarations
- `shell_port.c` - Platform-specific implementations
- `.clang-format` - Code formatting rules

## Common Patterns

### Command Registration
Commands are registered using the `SHELL_EXPORT_CMD` macro:
```c
SHELL_EXPORT_CMD(command_name, command_description, command_function, permission);
```

### Variable Export
Variables are exported using the `SHELL_EXPORT_VAR` macro:
```c
SHELL_EXPORT_VAR(variable_name, variable_pointer, type, readonly_flag);
```

## Forbidden Patterns

- **Do not use `printf` directly** - Use `shell_printf` for output
- **Do not modify the README encoding** - It currently uses UTF-16 LE for Chinese characters; changing this may corrupt the file
- **Use C11 standard features judiciously** - Ensure compatibility with common embedded toolchains
- **Never hardcode passwords or credentials** in the code
- **Do not break the modular structure** - Keep platform-specific code in `shell_port.c`
