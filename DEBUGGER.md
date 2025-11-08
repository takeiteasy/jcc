# JCC Interactive Debugger

The JCC VM includes an interactive, source-level debugger for step-by-step program execution and inspection.

**Enable with:** `-g` or `--debug` flags

When enabled, the debugger provides a powerful GDB-like interface for controlling program flow and inspecting state.

## Features

- **Source-Level Debugging**: The debugger maps bytecode instructions to their original source code locations. When you step through the code, it displays the current file, line number, and the corresponding source line, providing a seamless debugging experience.
- **Advanced Breakpoints**: Set breakpoints using multiple formats:
    - By line number in the current file (`break 42`).
    - By file and line number (`break test.c:42`).
    - At the entry point of a function (`break main`).
    - At a raw bytecode offset (legacy support).
- **Conditional Breakpoints**: Set breakpoints that only trigger when a specific condition is met. The expression can use local and global variables, arithmetic, comparison, and logical operators.
    - Syntax: `break <location> if <expression>`
    - Example: `break 22 if x > 5`
- **Watchpoints (Data Breakpoints)**: Break execution when memory is read or written. Watchpoints can be set on variables by name or on raw memory addresses.
    - `watch <var|addr>`: Break on write.
    - `rwatch <addr>`: Break on read.
    - `awatch <addr>`: Break on read or write.
- **Execution Control**: Full control over program flow with commands to step into (`step`), step over (`next`), and step out of (`finish`) functions.
- **State Inspection**: Inspect VM registers, the call stack, and raw memory at any address. The debugger tracks local and global variable names, allowing them to be used in expressions.

## Debugger Commands

| Command | Short | Description |
|---|---|---|
| `continue` | `c` | Continue execution until next breakpoint or watchpoint. |
| `step` | `s` | Single step, stepping into function calls. |
| `next` | `n` | Step over, executing function calls without stopping. |
| `finish` | `f` | Run until the current function returns. |
| `break <loc>` | `b <loc>` | Set a breakpoint at a specified location (`<line>`, `<file:line>`, `<func>`, or `<offset>`). |
| `break <loc> if <expr>` | `b <loc> if <expr>` | Set a conditional breakpoint. |
| `watch <var/addr>` | `w <var/addr>` | Set a watchpoint to break on writes to a variable or address. |
| `rwatch <addr>` | | Set a watchpoint to break on reads from an address. |
| `awatch <addr>` | | Set a watchpoint to break on reads or writes to an address. |
| `info watch` | | List all active watchpoints. |
| `delete <num>` | `d <num>` | Delete a breakpoint by its number. |
| `list` | `l` | List all breakpoints. |
| `registers` | `r` | Print all register values. |
| `stack [count]` | `st [count]` | Print the top `count` entries of the stack (default 10). |
| `disasm` | `dis` | Disassemble the current instruction. |
| `memory <addr>` | `m <addr>` | Inspect memory at a given address. |
| `help` | `h`, `?` | Show the help message. |
| `quit` | `q` | Exit the debugger and terminate the program. |

## Example Debugging Session

```bash
$ ./jcc -g test_debugger_enhanced.c

========================================
    JCC Debugger
========================================
Starting at entry point...
Type 'help' for commands, 'c' to continue

(jcc-dbg) break 20            # Set breakpoint at line 20
Breakpoint #0 set at test_debugger_enhanced.c:20

(jcc-dbg) continue            # Run to breakpoint
Breakpoint #0 hit at test_debugger_enhanced.c:20
At test_debugger_enhanced.c:20
    20:     int x = 10;
0xc33400018 (offset 24): LEA -4

(jcc-dbg) watch x             # Watch for writes to variable 'x'
Watchpoint #0: watch x

(jcc-dbg) step                # Execute one instruction (the assignment to x)
Watchpoint #0 hit: write to x at 0x7ffeea28d3f8
Old value: 0
New value: 10
At test_debugger_enhanced.c:21
    21:     int y = factorial(4);

(jcc-dbg) stack 3             # Check the stack
=== Stack (top 3 entries) ===
  sp[0] = 0x000000000000000a  (10)
  ...

(jcc-dbg) continue            # Run to completion
```
### Bytecode File Format

Binary format (little-endian):
```
[Magic: "JCC\0" (4 bytes)]
[Version: 1 (4 bytes)]
[Text size: bytes (8 bytes)]
[Data size: bytes (8 bytes)]
[Main offset: instruction offset (8 bytes)]
[Text segment: bytecode instructions]
[Data segment: global variables and constants]
```