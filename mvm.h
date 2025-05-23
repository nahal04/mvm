
#ifndef MVM_H
#define MVM_H

typedef enum {
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
} op_code;

// Spawn a root process to execute.
int spawn_process(int *program);

// Execute existing processes.
void exec(void);

#endif
