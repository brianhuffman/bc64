/* 6510_addressing.c - inline functions for 6510 addressing modes */
/* this file is included directly into 6510.c */

/* immediate mode */
inline int addr_imm() {
  return reg_pc++;
}

/* zero page */
inline int addr_zpg() {
  return mem_read(reg_pc++);
}

/* absolute */
inline int addr_abs() {
  int address = mem_read_16(reg_pc);
  reg_pc+=2;
  return address;
}

/* zero page, x */
inline int addr_zpx() {
  int address = mem_read(reg_pc++) + reg_x;
  return (address &= 0xff);
}

/* zero page, y */
inline int addr_zpy() {
  int address = mem_read(reg_pc++) + reg_y;
  return (address &= 0xff);
}

/* absolute, x */
inline int addr_abx() {
  int address = mem_read(reg_pc++) + reg_x;

  /* test for page boundary crossing */
  if (address > 0xff) clock_advance(1);

  return address + (mem_read(reg_pc++) << 8);
}

/* absolute, y */
inline int addr_aby() {
  int address = mem_read(reg_pc++) + reg_y;

  /* test for page boundary crossing */
  if (address > 0xff) clock_advance(1);

  return address + (mem_read(reg_pc++) << 8);
}

/* indexed indirect */
inline int addr_inx() {
  int index = mem_read(reg_pc++) + reg_x;
  index &= 0xff;
  return mem_read_16(index);
}

/* indirect indexed */
inline int addr_iny() {
  int index, address;

  index = mem_read(reg_pc++);
  address = mem_read(index) + reg_y;

  /* test for page boundary crossing */
  if (address > 0xff) clock_advance(1);

  return address + (mem_read(index + 1) << 8);
}

/* absolute indirect */
inline int addr_ind() {
  int index = mem_read_16(reg_pc);
  reg_pc += 2;
  return mem_read_16(index);
}

