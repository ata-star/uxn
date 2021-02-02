#include <stdio.h>

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define FLAG_HALT 0x01
#define FLAG_ZERO 0x02
#define FLAG_CARRY 0x04
#define FLAG_TRAPS 0x08

#define STACK_DEPTH 256

typedef unsigned char Uint8;
typedef unsigned short Uint16;

typedef struct {
	Uint8 literal, status;
	Uint8 mptr, sptr, rsptr;
	Uint8 memory[STACK_DEPTH];
	Uint8 stack[STACK_DEPTH];
	Uint8 rstack[STACK_DEPTH];
	Uint16 counter;
} Computer;

Computer cpu;

#pragma mark - Helpers

void
setflag(char flag, int b)
{
	if(b)
		cpu.status |= flag;
	else
		cpu.status &= (~flag);
}

int
getflag(char flag)
{
	return cpu.status & flag;
}

void
echo(Uint8 *s, Uint8 len, char *name)
{
	int i;
	printf("%s\n", name);
	for(i = 0; i < len; ++i) {
		if(i % 16 == 0)
			printf("\n");
		printf("%02x ", s[i]);
	}
	printf("\n\n");
}

void
spush(Uint8 v)
{
	cpu.stack[cpu.sptr++] = v;
}

Uint8
spop(void)
{
	return cpu.stack[--cpu.sptr];
}

void
rspush(Uint8 v)
{
	cpu.rstack[cpu.rsptr++] = v;
}

Uint8
rspop(void)
{
	return cpu.rstack[--cpu.rsptr];
}

#pragma mark - Operations

/* clang-format off */

void op_brk() { setflag(FLAG_HALT, 1); }
void op_rts() {	cpu.mptr = rspop(); }
void op_lit() { cpu.literal += cpu.memory[cpu.mptr++]; }
void op_drp() { spop(); }
void op_dup() { spush(cpu.stack[cpu.sptr - 1]); }
void op_swp() { Uint8 b = spop(), a = spop(); spush(b); spush(a); }
void op_ovr() { spush(cpu.stack[cpu.sptr - 2]); }
void op_rot() { Uint8 c = spop(),b = spop(),a = spop(); spush(b); spush(c); spush(a); }
void op_jmp() { cpu.mptr = spop(); }
void op_jsr() { rspush(cpu.mptr); cpu.mptr = spop(); }
void op_jmq() { Uint8 a = spop(); if(getflag(FLAG_ZERO)){ cpu.mptr = a; } setflag(FLAG_ZERO,0); }
void op_jsq() { Uint8 a = spop(); if(getflag(FLAG_ZERO)){ rspush(cpu.mptr); cpu.mptr = a; } setflag(FLAG_ZERO,0); }
void op_equ() { setflag(FLAG_ZERO, spop() == spop()); }
void op_neq() { setflag(FLAG_ZERO, spop() != spop()); }
void op_lth() {	setflag(FLAG_ZERO, spop() < spop()); }
void op_gth() {	setflag(FLAG_ZERO, spop() > spop()); }
void op_and() {	spush(spop() & spop()); }
void op_ora() {	spush(spop() | spop()); }
void op_rol() { spush(spop() << 1); }
void op_ror() { spush(spop() >> 1); }
void op_add() { spush(spop() + spop()); }
void op_sub() { spush(spop() - spop()); }
void op_mul() { spush(spop() * spop()); }
void op_div() { spush(spop() / spop()); }

void (*ops[])(void) = {
	op_brk, op_rts, op_lit, op_drp, op_dup, op_swp, op_ovr, op_rot, 
	op_jmp, op_jsr, op_jmq, op_jsq, op_equ, op_neq, op_gth, op_lth, 
	op_and, op_ora, op_rol, op_ror, op_add, op_sub, op_mul, op_div};

Uint8 opr[][2] = {
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {2,1}, {0,0}, {0,0}, {0,0},
	{0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0},
};

/* clang-format on */

void
reset(void)
{
	int i;
	cpu.status = 0x00;
	cpu.counter = 0x00;
	cpu.mptr = 0x00;
	cpu.sptr = 0x00;
	cpu.literal = 0x00;
	for(i = 0; i < 256; i++)
		cpu.stack[i] = 0x00;
}

int
error(char *name)
{
	printf("Error: %s\n", name);
	return 0;
}

void
load(FILE *f)
{
	fread(cpu.memory, sizeof(cpu.memory), 1, f);
}

int
eval()
{
	Uint8 instr = cpu.memory[cpu.mptr++];
	if(cpu.literal > 0) {
		spush(instr);
		cpu.literal--;
		return 1;
	}
	if(instr < 24) {
		if(cpu.sptr < opr[instr][0])
			return error("Stack underflow");
		/* TODO stack overflow */
		(*ops[instr])();
	}
	if(instr > 0x10)
		setflag(FLAG_ZERO, 0);
	cpu.counter++;
	return 1;
}

void
run(void)
{
	while(!(cpu.status & FLAG_HALT) && eval(cpu))
		;
	/* debug */
	printf("ended @ %d steps | ", cpu.counter);
	printf("hf: %x zf: %x cf: %x tf: %x\n",
		getflag(FLAG_HALT) != 0,
		getflag(FLAG_ZERO) != 0,
		getflag(FLAG_CARRY) != 0,
		getflag(FLAG_TRAPS) != 0);
	printf("\n");
}

int
main(int argc, char *argv[])
{
	FILE *f;
	if(argc < 2)
		return error("No input.");
	if(!(f = fopen(argv[1], "rb")))
		return error("Missing input.");
	reset();
	load(f);
	run();
	/* print result */
	echo(cpu.stack, 0x40, "stack");
	echo(cpu.memory, 0x40, "memory");
	return 0;
}
