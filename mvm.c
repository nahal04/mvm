#include <stdio.h>
#include "mvm.h"


#define STACK_MAX 2048
#define CALL_STACK_MAX 2048
#define PROC_MAX 1028
#define CHILD_MAX 1028
#define MAIL_MAX 1024
#define MSG_MAX 256

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
	int call_stack[CALL_STACK_MAX];
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

int spawn_proc(int *prog, int ppid) {
	int pid = get_free_pid();
	if (pid == -1) 
		return -1;
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

int spawn_process(int *prog) {
	return spawn_proc(prog, -1);
}

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
		while (parent->cp > 0 && parent->children[p->cp - 1] == -1) parent->cp--;
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
void send_msg(int from, int to, int *data, int len) {
	if (to == -1) return;
	vm_proc *target = procs + to;
	msg *m = new_msg(target);
	if (m == NULL) return;
	for (int i = 0; i < len; i++) {
		m->data[i] = data[i];
	}
	m->len = len;
	m->from = from;
	target->waiting = 0;
}

int mvm_atoi(char *s, int base) {
	int res = 0;
	while (*s) {
		if (*s >= '0' && *s <= '9')
			res = res * base + *s - '0';
		else if (*s == '#' && res)
			return mvm_atoi(s + 1, res);
		s++;
	}
	return res;
}

int mvm_strlen(char *s) {
	int i = 0;
	while (*s++) i++;
	return i;
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
			p->stack[p->sp++] = val;
		       	break;
		case OP_POP:
			p->stack[--p->sp];
			break;
		case OP_DUP:
			val = p->stack[p->sp - 1];
			p->stack[p->sp++] = val;
			break;
		case OP_SWAP:
			val = p->stack[--p->sp];
			val2 = p->stack[--p->sp];
			p->stack[p->sp++] = val;
			p->stack[p->sp++] = val2;
			break;
		case OP_SEND:
			int to = p->stack[--p->sp];
			int len = p->stack[--p->sp];
			int data[MSG_MAX];
			for (int i = 0;i < len; i++) {
				data[i] = p->stack[--p->sp];
			}
			if (to == -2)  		//send to parent
				send_msg(p->pid, p->ppid, data, len);
			else if (to == -1) // send to all children
				for (int i = 0; i < p->cp; i++)
					send_msg(p->pid, p->children[i], data, len);
			else if (sendable(p, to)) 
					send_msg(p->pid, to, data, len);
			break;
		case OP_RECV:
			if (p->mtp != p->mhp) {
				p->waiting = 0;
				m = p->mailbox + inc_qp(&p->mhp, MAIL_MAX);
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
			val = p->stack[--p->sp];
			printf("%d: %d\n",p->pid, val);
			p->stack[p->sp++] = val;
			break;
		case OP_SCAN:
			char buf[16];
			fgets(buf, 16, stdin);
			int n = mvm_atoi(buf, 10);
			p->stack[p->sp++] = n;
			break;
		case OP_SCANS:
			char s[256];
			fgets(s, 256, stdin);
			int slen = mvm_strlen(s);
			for (int i = slen - 1; i >= 0; i--) {
				p->stack[p->sp++] = s[i];
			}
			p->stack[p->sp++] = slen;
			break;
		case OP_PRINTS:
			slen = p->stack[--p->sp];
			for (int i = 0; i < slen; i++)
				putchar(p->stack[--p->sp]);
			break;
		case OP_ADD:
			val = p->stack[--p->sp];
			val2 = p->stack[--p->sp];
			p->stack[p->sp++] = val + val2;	
			break;
		case OP_SUB:
			val = p->stack[--p->sp];
			val2 = p->stack[--p->sp];
			p->stack[p->sp++] = val2 - val;
			break;
		case OP_MUL:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = a * b;
			break;
		case OP_DIV:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b / a;
			break;
		case OP_AND:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = a & b;
			break;
		case OP_OR:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b | a;
			break;
		case OP_XOR:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b ^ a;
			break;
		case OP_NOT:
			a = p->stack[--p->sp];
			p->stack[p->sp++] = ~a;
			break;
		case OP_LSHIFT:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b << a;
			break;
		case OP_RSHIFT:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = b >> a;
			break;
		case OP_MOD:
			a = p->stack[--p->sp];
			b = p->stack[--p->sp];
			p->stack[p->sp++] = a % b;
			break;
		case OP_CALL:
			int addr1 = p->prog[p->ip++];
			int addr2 = p->prog[p->ip++];
			int addr3 = p->prog[p->ip++];
			int c = p->stack[p->sp - 1];
			p->call_stack[p->csp++] = p->ip;
			if (c < 0 && addr1 != -1) p->ip = addr1;
			if (addr2 != -1 && c == 0 ) p->ip = addr2;
			if (addr3 != -1 && c > 0) p->ip = addr3;
			break;
		case OP_RET:
			arg = p->call_stack[--p->csp];
			p->ip = arg;
			break;
		case OP_FORK:
			arg = p->prog[p->ip++]; // read the target addr
			pid = spawn_proc(p->prog + arg, p->pid);
			p->children[p->cp++] = pid;
			break;	
		default:
			term_proc(p);
			return -1;
	}
}

#define TIME_SLICE 3
void exec() {
	vm_proc *p;
	int active = 1;
	while (active) {
		active = 0;
		for (size_t i = 0; i < process_count; i++) {
			p = procs + i;
			if (p->active && !p->waiting) active = 1;
			for (int j = 0; j < TIME_SLICE && p->active && !p->waiting; j++) {
				run_step(p);
			}
		}
	}
}

// Example Program
// 
// int main() {
//	int init[] = {
//		OP_SCANS,
//		OP_PUSH, -1,
//		OP_FORK, 7,
//		OP_SEND,
//		OP_HALT,
//	// fork:
//		OP_RECV,
//		OP_POP,
//		OP_PRINTC,
//		OP_HALT
//	};
//
//	spawn_proc(init, -1); // the init proc
//	exec();
// }
