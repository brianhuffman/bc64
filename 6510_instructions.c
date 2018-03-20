/* 6510_instructions.c - inline functions for 6510 instructions */
/* this file is included directly into 6510.c */

#include <unistd.h>

/* list of documented instructions:
   ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
   CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
   JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
   RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA
*/

/* opcode descriptions in C64 PRG, pp.235-253 */

inline static int test_z() { return (flag_nz <= 0x00); }
inline static int test_n() { return (flag_nz & 0x80); }
inline static int increment(int x) { return (x+1) & 0xff; }
inline static int decrement(int x) { return (x-1) & 0xff; }

inline static void update_p() {
	/* clear N, B, Z, and C flags */
	reg_p &= ~(N_FLAG|B_FLAG|Z_FLAG|C_FLAG);

	/* set 1 flag */
	reg_p |= 0x20;

	/* update negative, zero, and carry flags */
	if (test_n()) reg_p |= N_FLAG;
	else if (test_z()) reg_p |= Z_FLAG;
	if (flag_c) reg_p |= C_FLAG;
}

inline static void cpu6510_branch(int condition, int address) {
	if (condition) {
		signed char offset = mem_read(address);
		int new_pc = reg_pc + offset;
		/* add extra cycle if branching to another page */
		if ((reg_pc ^ new_pc) & 0xff00) clock_advance(2);
		else clock_advance(1);
		reg_pc = new_pc;
	}
}

inline static void opcode_compare(int reg, int data) {
	flag_c = (reg >= data);
	flag_nz = (reg - data) & 0xff;
}

inline static int opcode_add(int data) {
	int result, carry;

	if (reg_p & D_FLAG) {

		/* add lower nybble */
		carry = flag_c;
		result = (reg_a & 0xf) + (data & 0xf) + carry;

		/* do BCD fixup for lower nybble */
		if (result > 0x19) carry -= 10;
		else if (result > 0x09) carry += 6;

		/* add upper nybble */
		result = reg_a + data + carry;

		/* update negative and zero flags */
		flag_nz = result;
		if ((reg_a + data + flag_c) & 0xff == 0) flag_nz -= 0x100;

		/* set overflow if a7 != r7, AND d7 != r7*/
		reg_p &= ~V_FLAG;
		if ( ((reg_a ^ result) & (data ^ result)) & 0x80 )
			reg_p |= V_FLAG;

 		/* do BCD fixup for upper nybble */
		if (result > 0x9f) result += 0x60;

		/* set carry flag */
		flag_c = (result > 0xff);
		result &= 0xff;
	}

	/* non-decimal mode */
	else {
		result = reg_a + data + flag_c;

		flag_c = (result > 0xff);
		result &= 0xff;

		/* set overflow if a7 != r7, AND d7 != r7 */
		reg_p &= ~V_FLAG;
		if ( ((reg_a ^ result) & (data ^ result)) & 0x80 )
			reg_p |= V_FLAG;

		flag_nz = result;
	}
	return result;
}


inline static int opcode_sub(int data) {
	int result;

	if (reg_p & D_FLAG) {

		/* add lower nybble */
		result = 0xff + (reg_a & 0xf) - (data & 0xf) + flag_c;

		/* do BCD fixup for lower nybble */
		if (result < 0x100) result -= 0x06;
		if (result < 0xf0) result += 0x10;

		/* add upper nybble */
		result += (reg_a & 0xf0) - (data & 0xf0);

		/* update negative and zero flags */
		flag_nz = result;
		if ((reg_a - data - 1 + flag_c) & 0xff == 0) flag_nz -= 0x100;

		/* clear affected flags */
		reg_p &= ~(C_FLAG | V_FLAG);

		/* set overflow if a7 != r7, AND a7 != d7*/
		if ( ((reg_a ^ result) & (reg_a ^ data)) & 0x80 )
			reg_p |= V_FLAG;

		/* set carry flag */
		flag_c = (result & 0x100) ? 1 : 0;

		/* do BCD fixup for upper nybble */
		if (!(result & 0x100)) result -= 0x60;

		result &= 0xff;
	}

	/* non-decimal mode */
	else {
		result = reg_a + 0xff - data + flag_c;

		flag_c = (result > 0xff);
		result &= 0xff;

		/* set overflow if a7 != r7, AND a7 != d7 */
		reg_p &= ~V_FLAG;
		if ( ((reg_a ^ result) & (reg_a ^ data)) & 0x80 )
			reg_p |= V_FLAG;

		flag_nz = result;
	}
	return result;
}


/******************** ARITHMETIC INSTRUCTIONS ********************/

inline static void cpu6510_ORA(int address) {
	reg_a = flag_nz = reg_a | mem_read(address);
}
inline static void cpu6510_AND(int address) {
	reg_a = flag_nz = reg_a & mem_read(address);
}
inline static void cpu6510_EOR(int address) {
	reg_a = flag_nz = reg_a ^ mem_read(address);
}
inline static void cpu6510_ADC(int address) {
	reg_a = opcode_add( mem_read(address) );
}
inline static void cpu6510_CMP(int address) {
	opcode_compare(reg_a, mem_read(address));
}
inline static void cpu6510_SBC(int address) {
	reg_a = opcode_sub( mem_read(address) );
}

/**************** LOAD/STORE/TRANSFER INSTRUCTIONS ***************/

inline static void cpu6510_LDA(int address) {
	reg_a = flag_nz = mem_read(address);
}
inline static void cpu6510_LDX(int address) {
	reg_x = flag_nz = mem_read(address);
}
inline static void cpu6510_LDY(int address) {
	reg_y = flag_nz = mem_read(address);
}
inline static void cpu6510_STA(int address) {
	mem_write(address, reg_a);
}
inline static void cpu6510_STX(int address) {
	mem_write(address, reg_x);
}
inline static void cpu6510_STY(int address) {
	mem_write(address, reg_y);
}
inline static void cpu6510_TAX(void) {
	reg_x = flag_nz = reg_a;
}
inline static void cpu6510_TAY(void) {
	reg_y = flag_nz = reg_a;
}
inline static void cpu6510_TSX(void) {
	reg_x = flag_nz = reg_s;
}
inline static void cpu6510_TXA(void) {
	reg_a = flag_nz = reg_x;
}
inline static void cpu6510_TXS(void) {
	reg_s = reg_x;
}
inline static void cpu6510_TYA(void) {
	reg_a = flag_nz = reg_y;
}


/********************** BRANCH INSTRUCTIONS **********************/

inline static void cpu6510_BPL(int address) {
	cpu6510_branch( !test_n(), address);
}
inline static void cpu6510_BMI(int address) {
	cpu6510_branch( test_n(), address);
}
inline static void cpu6510_BVC(int address) {
	cpu6510_branch( !(reg_p & V_FLAG), address);
}
inline static void cpu6510_BVS(int address) {
	cpu6510_branch( reg_p & V_FLAG, address);
}
inline static void cpu6510_BCC(int address) {
	cpu6510_branch(!flag_c, address);
}
inline static void cpu6510_BCS(int address) {
	cpu6510_branch(flag_c, address);
}
inline static void cpu6510_BNE(int address) {
	cpu6510_branch( !test_z(), address);
}
inline static void cpu6510_BEQ(int address) {
	cpu6510_branch(test_z(), address);
}

/*********************** FLAG INSTRUCTIONS ***********************/

inline static void cpu6510_CLC(void) {
	flag_c = 0;
}
inline static void cpu6510_SEC(void) {
	flag_c = 1;
}
inline static void cpu6510_CLI(void) {
	reg_p &= ~I_FLAG;
}
inline static void cpu6510_SEI(void) {
	reg_p |= I_FLAG;
}
inline static void cpu6510_CLV(void) {
	reg_p &= ~V_FLAG;
}
inline static void cpu6510_CLD(void) {
	reg_p &= ~D_FLAG;
}
inline static void cpu6510_SED(void) {
	reg_p |= D_FLAG;
}

/*********************** STACK INSTRUCTIONS **********************/

inline static void cpu6510_PHP(void) {
	update_p();
	stack_write(reg_s--, reg_p);
}
inline static void cpu6510_PLP(void) {
	reg_p = stack_read(++reg_s);
	flag_nz = reg_p;
	if (reg_p & Z_FLAG) flag_nz -= 0x100;
	flag_c = reg_p & 0x01;
}
inline static void cpu6510_PHA(void) {
	stack_write(reg_s--, reg_a);
}
inline static void cpu6510_PLA(void) {
	reg_a = flag_nz = stack_read(++reg_s);
}

inline static void cpu6510_JSR(int address) {
	/* pre-decrement program counter */
	reg_pc--;
	/* push current reg_pc on stack, MSB first */
	stack_write(reg_s--, reg_pc >> 8);
	stack_write(reg_s--, reg_pc & 0xff);
	reg_pc = address;
}
inline static void cpu6510_RTS(void) {
	/* pull reg_pc from stack, LSB first */
	reg_s++;
	reg_pc = stack_read_16(reg_s);
	reg_s++;
	/* re-increment program counter */
	reg_pc++;
}
inline static void cpu6510_RTI(void) {
	/* pull P and PC from stack */
	cpu6510_PLP();
	cpu6510_RTS();
}
inline static void cpu6510_BRK() {
	/* jump to IRQ vector */
	cpu6510_JSR( mem_read_16(0xfffe) );
	/* push P onto stack, with B flag set */
	update_p();
	stack_write(reg_s--, reg_p | B_FLAG);
	/* disable further interrupts */
	cpu6510_SEI();
}


/***************** READ-MODIFY-WRITE INSTRUCTIONS ****************/

inline static void cpu6510_ASL(int address) {
	int data = mem_read(address);
	flag_c = (data >> 7);
	mem_write(address, flag_nz = (data << 1) & 0xff);
}
inline static void cpu6510_ASL_a(void) {
	flag_c = (reg_a >> 7);
	reg_a = flag_nz = (reg_a << 1) & 0xff;
}

inline static void cpu6510_ROL(int address) {
	int data = mem_read(address);
	int result = ((data << 1) | flag_c) & 0xff;
	flag_c = (data >> 7);
	mem_write(address, flag_nz = result);
}
inline static void cpu6510_ROL_a(void) {
	int result = ((reg_a << 1) | flag_c) & 0xff;
	flag_c = (reg_a >> 7);
	reg_a = flag_nz = result;
}

inline static void cpu6510_LSR(int address) {
	int data = mem_read(address);
	flag_c = (data & 0x01);
	mem_write(address, flag_nz = data >> 1);
}
inline static void cpu6510_LSR_a(void) {
	flag_c = (reg_a & 0x01);
	reg_a = flag_nz = reg_a >> 1;
}

inline static void cpu6510_ROR(int address) {
	int data = mem_read(address);
	int result = (data >> 1) | (flag_c << 7);
	flag_c = (data & 0x01);
	mem_write(address, flag_nz = result);
}
inline static void cpu6510_ROR_a(void) {
	int result = (reg_a >> 1) | (flag_c << 7);
	flag_c = (reg_a & 0x01);
	reg_a = flag_nz = result;
}

inline static void cpu6510_DEC(int address) {
	mem_write(address, flag_nz = decrement(mem_read(address)));
}

inline static void cpu6510_INC(int address) {
	mem_write(address, flag_nz = increment(mem_read(address)));
}


/****************** INDEX REGISTER INSTRUCTIONS ******************/

inline static void cpu6510_CPX(int address) {
	opcode_compare(reg_x, mem_read(address));
}
inline static void cpu6510_CPY(int address) {
	opcode_compare(reg_y, mem_read(address));
}
inline static void cpu6510_DEX(void) {
	reg_x = flag_nz = decrement(reg_x);
}
inline static void cpu6510_DEY(void) {
	reg_y = flag_nz = decrement(reg_y);
}
inline static void cpu6510_INX(void) {
	reg_x = flag_nz = increment(reg_x);
}
inline static void cpu6510_INY(void) {
	reg_y = flag_nz = increment(reg_y);
}


/******************* MISCELLANEOUS INSTRUCTIONS ******************/

inline static void cpu6510_JMP(int address) {
	reg_pc = address;
}
inline static void cpu6510_NOP(int address) {
	/* no operation, do nothing */
}
inline static void cpu6510_BIT(int address) {
	int data = mem_read(address);
	/* bit 6 goes to V flag */
	reg_p &= ~V_FLAG;
	reg_p |= data & V_FLAG;
	/* bit 7 goes to N flag */
	flag_nz = data;
	if (reg_a & data == 0) flag_nz -= 0x100;
}
inline static void cpu6510_JAM(void) {
	int opcode = mem_read(--reg_pc);
	fprintf(stderr, "Instruction %02x at $%04x crashed the CPU!\n",
		opcode, reg_pc);
#ifndef VICELOG
	exit(1);
#endif
}

/******************** UNDOCUMENTED INSTRUCTIONS ******************/

inline static void cpu6510_ANC(int address) {
	cpu6510_AND(address);
	flag_c = (reg_a & 0x80) ? 0 : 1;
}
inline static void cpu6510_ASR(int address) {
	cpu6510_AND(address);
	cpu6510_LSR_a();
}
inline static void cpu6510_DCP(int address) {
	cpu6510_DEC(address);
	cpu6510_CMP(address);
}
inline static void cpu6510_ISB(int address) {
	cpu6510_INC(address);
	cpu6510_SBC(address);
}
inline static void cpu6510_LAX(int address) {
	cpu6510_LDA(address);
	cpu6510_LDX(address);
}
inline static void cpu6510_RLA(int address) {
	cpu6510_ROL(address);
	cpu6510_AND(address);
}
inline static void cpu6510_RRA(int address) {
	cpu6510_ROR(address);
	cpu6510_ADC(address);
}
inline static void cpu6510_SAX(int address) {
	mem_write(address, reg_a & reg_x);
}
inline static void cpu6510_SBX(int address) {
	int data = mem_read(address);
	flag_c = ((reg_a & reg_x) >= data);
	reg_x = flag_nz = ((reg_a & reg_x) - data) & 0xff;
}
inline static void cpu6510_SHA(int address) {
	mem_write(address, reg_a & reg_x & ((address >> 8) + 1));
}
inline static void cpu6510_SHX(int address) {
	mem_write(address, reg_x & ((address >> 8) + 1));
}
inline static void cpu6510_SHY(int address) {
	mem_write(address, reg_y & ((address >> 8) + 1));
}
inline static void cpu6510_SLO(int address) {
	cpu6510_ASL(address);
	cpu6510_ORA(address);
}
inline static void cpu6510_SRE(int address) {
	cpu6510_LSR(address);
	cpu6510_EOR(address);
}
