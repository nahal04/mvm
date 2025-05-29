# MVM
MVM (Minimal Virtual Machine) is a stack based virtual machine with a minimal instruction set interface. It's not a complete virtual machine but rather a compiler backend. A set of instructions that languages can compile into.

# Documentation
See [MVM](https://nahal04.github.io/mvm/) web site for a detailed documentation and usage guide.

# Installation
There is no build tool with MVM. Its just a single .c file. Compile it with any modern C compiler and use it anywhere. Don't forget to `#include "mvm.h"` in your project.

# Usage
Here is an example program

```c
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

Refer to the documentation above to learn more and see all instructions available.

# LICENSE

The code is licensed under the [MIT LICENSE](LICENSE).

