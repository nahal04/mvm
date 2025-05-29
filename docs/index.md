---
title: MVM
nav_order: 1
---
# Contents
1. TOC
{:toc}

# MVM

MVM (Minimal Virtual Machine) is a stack based minimalistic virtual machine with a simple instruction set interface.

# Example program

```c
// Example Program
#include "mvm.h"

int main() {
   // define init program as an array of integers
   int init[] = {
   	OP_SCANS,        // Scan stdin and store string in stack
   	OP_FORK, 7,      // Fork a child process that start running at address 7
   	OP_PUSH, -1,     // Push -1 to stack to for next instruction
   	OP_SEND,         // Send the stack to all child processes (-1)
   	OP_HALT,         // Terminate the parent program
   // fork:
   	OP_RECV,         // Receive the message from parent and store it in stack
   	OP_POP,          // Pop out the from address from stack
   	OP_PRINTS,       // Print the string from stack
   	OP_HALT          // Terminate the child process
   };

   spawn_process(init); // spawn a root process
   exec(); // Execute all spawned processes
}
```

# Machine specs

The VM contains a single processor with each process owning its own stack memory. Yes, there is no heap memory in MVM. If you want to receive an input from the user, the input is restricted to 256 bytes. All the memory operations are done by manipulating the stack. Each instructions are explained in detail later.

# Instruction set

Here is a breif summary of all the instructions available in MVM

```c
    OP_HALT, // Terminate the process
	OP_PUSH, // Push an item to the stack
	OP_POP, // Pop an item from the stack and discard
	OP_DUP, // Duplicate the topmost layer in stack
	OP_SWAP, // Swap the top two elements in stack
	OP_SCAN, // Scan the stdin for an integer
	OP_SCANS, // Scan the stdin for a string
	OP_PRINT, // Print the integer on top of stack to stdout
	OP_PRINTS, // Print the string on top of stack to stdout
	OP_ADD, // Add two integers on top of stack and push the result
	OP_SUB, // Subtract two integers on top of stack and push the result (second - first)
	OP_MUL, // Multiply two integers on top of stack and push the result
	OP_DIV, // Divide two integers on top of stack and push the result (second - first)
	OP_MOD, // Modulo two integers on top of stack and push the result
	OP_AND, // AND two integers on top of stack and push the result
	OP_OR, // OR two integers on top of stack and push the result
	OP_XOR, // XOR two integers on top of stack and push the result
	OP_NOT, // NOT the integer on top of the stack and push the result
	OP_LSHIFT, // second << first
	OP_RSHIFT, // second >> first
	OP_CALL, // Call function based on top of stack
	OP_RET, // Return from function
	OP_FORK, // Fork a child process
	OP_SEND, // Send a message from the stack to designated process
	OP_RECV // Recieve a message or wait for a message
```

# Stack Manipulation

Use `OP_PUSH` to push an integer to the stack. The integer has to be specified as an argument in the program. Similarly `OP_POP` removes the top of the stack and discards it.

`OP_DUP` takes no argument. It duplicates the top of the stack and pushes it onto the stack. 

`OP_SWAP` swaps the uppermost two integers on the stack.

# Data types

Everything is an integer in MVM. Floats, chars, booleans are all eventually cast to aninteger. While accepting user input use `OP_SCAN` for integers and `OP_SCANS` for string and characters. The scanned input is pushed automatically to the stack. The input limit is 256 bytes. `OP_SCAN` converts the input into in an integer like so `"21" -> 21`. However, you can also input numbers of different base using `#`. For example: `16#0F1CAF`, `2#1001101`. Any base from 1 to 35 are allowed.

# User I/O

`OP_SCANS` reads the input in reverse order, so that the first letter is on the top of the stack. This instruction also pushes the length of the string onto the stack after reading the input. Note that `OP_SCAN` does not store any length info because there is no length for integers. So, after a successful call to `OP_SCAN`, the stack contains the length of the stack followed by the string starting with the first letter.
 
Similarly use `OP_PRINT` to print the integer on top of the stack and `OP_PRINTS` to print a string from the top of the stack. Note that the top of stack should containt the length of the string. Also note that print group of instructions pops the printing character and length information off the stack.

# Arithmetic and Bitwise operations

See the instructions section to see the supported arithmetic and bitwise operations. Almost all the basic operations are available including modulo and bitwise operators. All the logic operations -  `OP_AND`, `OP_OR`, `OP_XOR`, `OP_NOT` - are all bitwise operators and not logic operators.

The required number of operands are popped from the top of the stack and the result is pushed into the stack. All operations, except `OP_NOT`, requires two operands, while `OP_NOT` requires a single argument. Note that all operands are discarded after oparation. If you want to preserve them, then duplicate before operation using `OP_DUP` (See above).

If `a` is the top most integer on stack and `b` is the second integer, then all operations with two operands follows the order - `<b> <operator> <a>`.
 
# Control flow

There is no comparison operators in MVM and there is no conditional jump instructions. But, that doesn't mean there is no control flow. 

The `OP_CALL` instruction jumps to a function in the address specified as an argument in the program. In fact, this instruction requires 3 arguments, each containing a specific address, like so: `OP_CALL, <addr1>, <addr2>, <addr3>`.

When the `OP_CALL` instruction is called, it pops the top element in the stack. If this integer is less than 0, equal to 0 or greater than 0, the program calls the function at `<addr1>`, `<addr2>`, or `<addr3>` respectively. If you don't want any function to be called on certain conditions, the addr parameter can be specified as `-1`. This causes the program to move on to the next instruction. In all the cases the top most conditional integer is popped.

`OP_RET` causes the function to return and the program jumps back to its initial location when it was called. You can always call functions within functions and a call stack will be maintained internally and each `OP_RET` jumps one step back in the call stack.

As you might have guessed, there is no loops with a jump instructions. Loops can be acheived by recursive function calls and returns.

# Processes and Concurrency

Before spinning up the VM, one can create as many *root processes* as the VM allows. See limits section for more details. Each root process runs concurrently in the VM. However, sibling processes cannot communicate with each other.

*Root processes* can be created with the `spawn_process()` function. It expects an integer array containing the program instructions as an argument.

A process on the fly can create a child process using the `OP_FORK` instruction. This spawns a new child process at the address specified as an argument in the program. Remember that this new child process has its own fresh stack memory and the starting address is reset to 0 in the child process, while that address remains as it is in the parent process. 

# Interprocess Communication

Processes can communicate with their parents or children, not beyond that. Use `OP_SEND` and `OP_RECV` instructions for sending and receiving messages between processes.

`OP_SEND` expects a target PID from the stack. If the target PID is -2, then the message will be sent to the parent. If -1, the message will be broadcasted to all children. If any other PID is provided, the message will only be sent if the target process is either the sending process' child or parent. Otherwise, the message is not sent.

The message to be sent is taken from the stack. The message should have the length in the top of the stack followed by the integers. The message along with length is placed beneath the target PID in the stack. After a call to `OP_SEND`, the message is removed from the stack.

The target process can call `OP_RECV` instruction to receive the process or wait for the process, if there is no message sent yet. The received message is pushed to the stack followed by the length of the message followed by the sender's PID. Consequently, the sender's PID ends up at the top of the stack.

If there is no message to be received in the process' mailbox, it enters a state of waiting, and, is only woken up by an `OP_SEND` targeting that process.

# Process termination

A process upon meeting the `OP_HALT` instruction reaches its end of life. Once a process terminates, all its children, if alive, become *orphan processes*. All *orphan processes* are converted to *root processes* automatically. In other words, the termination of parent process does not cause automatic termination of its child processes and vice-versa. Once a child terminates, its PID will be removed from its parent's list of children.

# Spinning up the VM

This is acheived through the `exec()` function call. This causes all the existing active processes to run concurrently. A round-robin pattern of scheduling is followed with each process receiving its own time slice. As of current version, there is no process prioritizing and each process receive an equal time slice. After the time slice expires, the next process will be pre-emptively scheduled automatically. No manual pre-emptive scheduling is supported at present. Any process who is waiting for a message will not be scheduled until it is woken up by another process. Situations causing deadlocks are simply ignored and the system shutdowns terminating all deadlocking processes.


