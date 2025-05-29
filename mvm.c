#include <stdio.h>
#include "mvm.h"


#define STACK_MAX 2048
#define CALLSTACK_MAX 2048
#define PROC_MAX 1028
#define CHILD_MAX 1028
#define MAIL_MAX 1024
#define MSG_MAX 256

int mvm_errno = 0; // For error value
typedef enum {ERR_NOPID, ERR_MSGQFULL, ERR_TGTDEAD, ERR_STACKEMPTY, ERR_NOOPERAND, ERR_LARGEMSG, ERR_STACKOVERFLOW, ERR_NOSPACE, ERR_UNKNOWN, ERR_DIVZERO } mvm_err;
// Print errot to stderr
void mvm_perror(int err, int pid) {
	switch (err) {
		case ERR_NOPID:
			fprintf(stderr, "<%d>: No PID is free\n", pid);
			return;
		case ERR_MSGQFULL:
			fprintf(stderr, "<%d>: Message queue is full. Cannot send.\n", pid);
			return;
		case ERR_TGTDEAD:
			fprintf(stderr, "<%d>: Target process is dead\n", pid);
			return;
		case ERR_STACKEMPTY:
			fprintf(stderr, "<%d>: The stack is empty\n", pid);
			return;
		case ERR_NOOPERAND:
			fprintf(stderr, "<%d>: Insufficient operands in stack\n", pid);
			return;
		case ERR_LARGEMSG:
			fprintf(stderr, "<%d>: Message too large\n", pid);
			return;
		case ERR_STACKOVERFLOW:
			fprintf(stderr, "<%d>: Stack overflow\n", pid);
			return;
		case ERR_NOSPACE:
			fprintf(stderr, "<%d>: No space available in stack\n", pid);
			return;
		case ERR_UNKNOWN:
			fprintf(stderr, "<%d>: Unknown instruction\n", pid);
			return;
		case ERR_DIVZERO:
			fprintf(stderr, "<%d>: Division by zero\n", pid);
			return;
		default:
			fprintf(stderr, "<%d>: Unknown error occured\n", pid);
			return;
	}
}





	
		

typedef struct {
	int from;
	size_t len;
	int data[MSG_MAX];
} msg;
	
typedef struct {
	int pid;
	int ppid;
	int ip;
	int sp;
	int cp;
	int csp;
	int mhp; // Mail box head
	int mtp; // Mail box tail
	int stack[STACK_MAX]; // process memory stack
	int call_stack[CALLSTACK_MAX];
	int children[CHILD_MAX];
	msg mailbox[MAIL_MAX]; // The mail box queue
	int *prog; // The program code
	int active; // status: 1 if active, 0 if terminated
	int waiting; // waiting for message
} vm_proc;

vm_proc procs[PROC_MAX] = {0};
size_t process_count = 0;
int free_pid_list[PROC_MAX];
size_t free_pid_count = 0;

int get_free_pid() {
	int pid;
	if (free_pid_count > 0)
		pid = free_pid_list[--free_pid_count];
	else if (process_count < PROC_MAX)
		pid = process_count++;
	else
		pid = -1;
	return pid;	
}

void free_pid(int pid) {
	free_pid_list[free_pid_count++] = pid;
}

// Spawn a process
int spawn_proc(int *prog, int ppid) {
	int pid = get_free_pid();
	if (pid == -1) {
		mvm_errno = ERR_NOPID;
		return -1;
	}
	vm_proc *p = &procs[pid];
	p->pid = pid;
	p->ppid = ppid;
	p->ip = p->sp = 0;
	p->csp = 0;
	p->cp = 0;
	p->mhp = p->mtp = 0;
	p->prog = prog;
	p->active = 1;
	p->waiting = 0;
	return pid;
}	

// Spawn a root process
int spawn_process(int *prog) {
	return spawn_proc(prog, -1);
}

// Terminate process
void term_proc(vm_proc *p) {
	p->active = 0;
	int cpid;
	vm_proc *child;
	// make children orphans if they are alive
	for (int i = 0; i < p->cp; i++) {
		cpid = p->children[i];
		if (cpid == -1)
			continue;
		child = procs + p->children[i];
		child->ppid = -1;
	}
	// mark the child dead for parent if parent is alive
	int ppid = p->ppid;
	if (ppid != -1) {
		vm_proc *parent = procs + ppid;
		for (int i = 0; i < parent->cp; i++) {
			if (parent->children[i] == p->pid) {
				parent->children[i] = -1;
				break;
			}
		}
		while (parent->cp > 0 && parent->children[p->cp - 1] == -1) parent->cp--; // Shrink the parent's child process list
	}
	free_pid(p->pid);
}
int inc_qp(int *p, int limit) {
	int old = (*p)++;
	if (*p == limit)
		*p = 0;
	return old;
}

msg *new_msg(vm_proc *p) {
	int old = inc_qp(&p->mtp, MAIL_MAX);
	if (p->mtp == p->mhp) {
		p->mtp = old;
		mvm_errno = ERR_MSGQFULL;
		return NULL;
	}
	return p->mailbox + old;
}

int sendable(vm_proc *from, int to) {
	if (from->ppid == to) return 1;
	for (int i = 0; i < from->cp; i++) {
		if (from->children[i] == to) return 1;
	}
	return 0;
}

// check if alive and send a message
int send_msg(int from, int to, int *data, int len) {
	if (to == -1) {
		mvm_errno = ERR_TGTDEAD; // Target dead
		return -1;
	}
	vm_proc *target = procs + to;
	msg *m = new_msg(target);
	if (m == NULL) {
		mvm_errno = ERR_MSGQFULL;
		return -1;
	}
	for (int i = 0; i < len; i++) {
		m->data[i] = data[i];
	}
	m->len = len;
	m->from = from;
	target->waiting = 0;
	return 0;
}

// The following two functions are a re-implementation of two lib functions, just wanted to keep it simple with no additional headers just for two functions and was unsure if it could break portability. Not sure if it's a good idea. Anyway, there is additional functionality in mvm_atoi(). 

// scan integers in a format like <base>#<number> e.g: 16#0FAF4C, 2#1010110. Base upto 35 is supported.
int mvm_atoi(char *s, int base) {
	int res = 0;
	while (*s) {
		if (*s >= '0' && *s <= '9')
			res = res * base + *s - '0';
		else if (*s >= 'a' && *s <= 'z')
			res = res * base + *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'Z')
			res = res * base + *s - 'A' + 10;
		else if (*s == '#' && res)
			return mvm_atoi(s + 1, res);
		s++;
	}
	return res;
}

// like strlen() from <string.h>
int mvm_strlen(char *s) {
	int i = 0;
	while (*s++) i++;
	return i;
}	

int *pop_stack(int *s, int *sp) {
	if (*sp > 0) {
		(*sp)--;
		return &s[(*sp)];
	}
	mvm_errno = ERR_STACKEMPTY;
	return NULL;
}

int push_stack(int *s, int *sp, int value, int limit) {
	if (*sp < limit) {
		s[*sp] = value;
		(*sp)++;
		return 0;
	}
	mvm_errno = ERR_STACKOVERFLOW;
	return -1;
}	

int run_step(vm_proc *p) {
	op_code op = p->prog[p->ip++];
	int val;
	int val2;
	int arg;
	int pid;
	int cpid;
	int a, b;
	msg *m;

	switch (op) {
		case OP_HALT:
			term_proc(p);
			return 0;
		case OP_PUSH:
			val = p->prog[p->ip++];
			if (push_stack(p->stack, &p->sp, val, STACK_MAX) == -1)
				return -1;
		       	break;
		case OP_POP:
			pop_stack(p->stack, &p->sp);
			break;
		case OP_DUP:
			if (p->sp == 0) {
				mvm_errno = ERR_STACKEMPTY;
				return -1;
			}
			val = p->stack[p->sp - 1];
			if (push_stack(p->stack, &p->sp, val, STACK_MAX) == -1)
				return -1;
			break;
		case OP_SWAP:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			val = p->stack[--p->sp];
			val2 = p->stack[--p->sp];
			p->stack[p->sp++] = val;
			p->stack[p->sp++] = val2;
			break;
		case OP_SEND:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			int to = p->stack[--p->sp];
			int len = p->stack[--p->sp];
			if (len > MSG_MAX) {
				mvm_errno = ERR_LARGEMSG;
				return -1;
			}
			if (p->sp < len) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			int data[MSG_MAX];
			for (int i = 0;i < len; i++) {
				data[i] = p->stack[--p->sp];
			}
			if (to == -2) { 		//send to parent
				if (send_msg(p->pid, p->ppid, data, len) == -1)
					return -1;
			}
			else if (to == -1) { // send to all children
				for (int i = 0; i < p->cp; i++) {
					if (send_msg(p->pid, p->children[i], data, len) == -1)
						return -1;
				}
			}
			else if (sendable(p, to)) {
					if (send_msg(p->pid, to, data, len) == -1)
						return -1;
			}
			break;
		case OP_RECV:
			if (p->mtp != p->mhp) {
				p->waiting = 0;
				m = p->mailbox + inc_qp(&p->mhp, MAIL_MAX);
				if (p->sp + m->len + 2 > STACK_MAX) {
					mvm_errno = ERR_NOSPACE;
					return -1;
				}
				for (int i = m->len - 1; i >= 0; i--) {
					p->stack[p->sp++] = m->data[i];
				}
				p->stack[p->sp++] = m->len;
				p->stack[p->sp++] = m->from;
			} else {
				p->waiting = 1;
				p->ip--;
			}
			break;
		case OP_PRINT:
			if (p->sp <= 0) {
				mvm_errno = ERR_STACKEMPTY;
				return -1;
			}
			val = p->stack[--p->sp];
			printf("<%d>: %d\n",p->pid, val);
			break;
		case OP_SCAN:
			char buf[16];
			printf("<%d>: ", p->pid);
			fgets(buf, 16, stdin);
			int n = mvm_atoi(buf, 10);
			if (push_stack(p->stack, &p->sp, n, STACK_MAX) == -1)
				return -1;
			break;
		case OP_SCANS:
			char s[256];
			printf("<%d>: ", p->pid);
			fgets(s, 256, stdin);
			int slen = mvm_strlen(s);
			if (p->sp + slen + 1 > STACK_MAX) {
				mvm_errno = ERR_NOSPACE;
				return -1;
			}
			for (int i = slen - 1; i >= 0; i--) {
				p->stack[p->sp++] = s[i];	
			}
			p->stack[p->sp++] = slen;
			break;
		case OP_PRINTS:
			if (p->sp < 1) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			slen = p->stack[--p->sp];
			if (p->sp < slen) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			printf("<%d>: ", p->pid);
			for (int i = 0; i < slen; i++)
				putchar(p->stack[--p->sp]);
			break;
		case OP_ADD:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			val = p->stack[--p->sp];
			val2 = p->stack[--p->sp];
			p->stack[p->sp++] = val + val2;	
			break;
		case OP_SUB:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			val = p->stack[--p->sp];
			val2 = p->stack[--p->sp];
			p->stack[p->sp++] = val2 - val;
			break;
		case OP_MUL:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = a * b;
			break;
		case OP_DIV:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			if (a == 0) {
				mvm_errno = ERR_DIVZERO;
				return -1;
			}
			p->stack[p->sp++] = b / a;
			break;
		case OP_AND:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = a & b;
			break;
		case OP_OR:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b | a;
			break;
		case OP_XOR:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b ^ a;
			break;
		case OP_NOT:
			if (p->sp < 1) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			p->stack[p->sp++] = ~a;
			break;
		case OP_LSHIFT:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b << a;
			break;
		case OP_RSHIFT:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b >> a;
			break;
		case OP_MOD:
			if (p->sp < 2) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			if (a == 0) {
				mvm_errno = ERR_DIVZERO;
				return -1;
			}
			p->stack[p->sp++] = a % b;
			break;
		case OP_CALL:
			int addr1 = p->prog[p->ip++];
			int addr2 = p->prog[p->ip++];
			int addr3 = p->prog[p->ip++];
			if (p->sp < 1) {
				mvm_errno = ERR_NOOPERAND;
				return -1;
			}
			int c = p->stack[--p->sp];
			if (p->csp >= CALLSTACK_MAX) {
				mvm_errno = ERR_STACKOVERFLOW;
				return -1;
			}
			p->call_stack[p->csp++] = p->ip;
			if (c < 0 && addr1 != -1) p->ip = addr1;
			if (addr2 != -1 && c == 0 ) p->ip = addr2;
			if (addr3 != -1 && c > 0) p->ip = addr3;
			break;
		case OP_RET:
			if (p->csp < 1) {
				mvm_errno = ERR_STACKEMPTY;
				return -1;
			}
			arg = p->call_stack[--p->csp];
			p->ip = arg;
			break;
		case OP_FORK:
			arg = p->prog[p->ip++]; // read the target addr
			pid = spawn_proc(p->prog + arg, p->pid);
			if (p->cp >= CHILD_MAX) {
				mvm_errno = ERR_NOSPACE;
				return -1;
			}
			p->children[p->cp++] = pid;
			break;	
		default:
			term_proc(p);
			mvm_errno = ERR_UNKNOWN;
			return -1;
	}
}

#define TIME_SLICE 3
void exec() {
	vm_proc *p;
	int status;
	int active = 1;
	while (active) {
		active = 0;
		for (size_t i = 0; i < process_count; i++) {
			p = procs + i;
			if (p->active && !p->waiting) active = 1;
			for (int j = 0; j < TIME_SLICE && p->active && !p->waiting; j++) {
				status = run_step(p);
				if (status == -1) {
					mvm_perror(mvm_errno, p->pid);
					return;
				}
			}
		}
	}
}

// Example Program
//
// int main() {
// 	int init[] = {
// 		OP_SCANS,
// 		OP_PUSH, -1,
// 		OP_FORK, 7,
// 		OP_SEND,
// 		OP_HALT,
// 		// fork:
// 		OP_RECV,
// 		OP_POP,
// 		OP_PRINTS,
// 		OP_HALT
// 	};
// 
// 	spawn_proc(init, -1); // the init proc
// 	exec();
// }
