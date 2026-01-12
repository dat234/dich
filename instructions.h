/* * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 */

#ifndef __INSTRUCTIONS_H__
#define __INSTRUCTIONS_H__

#include <stdio.h>

#define TRUE 1
#define FALSE 0
#define DC_VALUE 0
#define INT_SIZE 1
#define CHAR_SIZE 1

typedef int WORD;

enum OpCode {
  OP_LA,   // Load Address
  OP_LV,   // Load Value
  OP_LC,   // load Constant
  OP_LI,   // Load Indirect
  OP_INT,  // Increment t
  OP_DCT,  // Decrement t
  OP_J,    // Jump
  OP_FJ,   // False Jump
  OP_HL,   // Halt
  OP_ST,   // Store
  OP_CALL, // Call
  OP_EP,   // Exit Procedure
  OP_EF,   // Exit Function
  OP_RC,   // Read Char
  OP_RI,   // Read Integer
  OP_WRC,  // Write Char
  OP_WRI,  // Write Int
  OP_WLN,  // WriteLN
  OP_AD,   // Add
  OP_SB,   // Substract
  OP_ML,   // Multiple
  OP_DV,   // Divide
  OP_NEG,  // Negative
  OP_CV,   // Copy Top
  OP_EQ,   // Equal
  OP_NE,   // Not Equal
  OP_GT,   // Greater
  OP_LT,   // Less
  OP_GE,   // Greater or Equal
  OP_LE,   // Less or Equal

  // [SỬA ĐỔI] Thêm mã lệnh máy ảo mới
  OP_MOD,  // Chia dư
  OP_AND,  // Logic AND
  OP_OR,   // Logic OR
  OP_NOT,  // Logic NOT
  // ---------------------------------

  OP_BP    // Break point
};

struct Instruction_ {
  enum OpCode op;
  WORD p;
  WORD q;
};

typedef struct Instruction_ Instruction;
typedef int CodeAddress;

struct CodeBlock_ {
  Instruction* code;
  int codeSize;
  int maxSize;
};

typedef struct CodeBlock_ CodeBlock;

CodeBlock* createCodeBlock(int maxSize);
void freeCodeBlock(CodeBlock* codeBlock);

int emitCode(CodeBlock* codeBlock, enum OpCode op, WORD p, WORD q);

int emitLA(CodeBlock* codeBlock, WORD p, WORD q);
int emitLV(CodeBlock* codeBlock, WORD p, WORD q);
int emitLC(CodeBlock* codeBlock, WORD q);
int emitLI(CodeBlock* codeBlock);
int emitINT(CodeBlock* codeBlock, WORD q);
int emitDCT(CodeBlock* codeBlock, WORD q);
int emitJ(CodeBlock* codeBlock, WORD q);
int emitFJ(CodeBlock* codeBlock, WORD q);
int emitHL(CodeBlock* codeBlock);
int emitST(CodeBlock* codeBlock);
int emitCALL(CodeBlock* codeBlock, WORD p, WORD q);
int emitEP(CodeBlock* codeBlock);
int emitEF(CodeBlock* codeBlock);
int emitRC(CodeBlock* codeBlock);
int emitRI(CodeBlock* codeBlock);
int emitWRC(CodeBlock* codeBlock);
int emitWRI(CodeBlock* codeBlock);
int emitWLN(CodeBlock* codeBlock);
int emitAD(CodeBlock* codeBlock);
int emitSB(CodeBlock* codeBlock);
int emitML(CodeBlock* codeBlock);
int emitDV(CodeBlock* codeBlock);
int emitNEG(CodeBlock* codeBlock);
int emitCV(CodeBlock* codeBlock);
int emitEQ(CodeBlock* codeBlock);
int emitNE(CodeBlock* codeBlock);
int emitGT(CodeBlock* codeBlock);
int emitLT(CodeBlock* codeBlock);
int emitGE(CodeBlock* codeBlock);
int emitLE(CodeBlock* codeBlock);

int emitBP(CodeBlock* codeBlock);

void printInstruction(Instruction* instruction);
void printCodeBlock(CodeBlock* codeBlock);

void loadCode(CodeBlock* codeBlock, FILE* f);
void saveCode(CodeBlock* codeBlock, FILE* f);

#endif