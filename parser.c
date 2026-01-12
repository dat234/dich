/* * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 */
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"
#include "scanner.h"
#include "parser.h"
#include "semantics.h"
#include "error.h"
#include "debug.h"
#include "codegen.h"

// --- KHAI BÁO BIẾN TOÀN CỤC ---
// currentToken: Giữ token hiện tại đang được xử lý.
Token *currentToken; 
// lookAhead: Giữ token "nhìn trước" (tiếp theo). Trình dịch sẽ kiểm tra token này để quyết định luồng xử lý.
Token *lookAhead;

// Các biến ngoại lai (extern) được định nghĩa ở các file khác (symtab.c)
extern Type* intType;    // Kiểu số nguyên chuẩn
extern Type* charType;   // Kiểu ký tự chuẩn
extern SymTab* symtab;   // Bảng ký hiệu toàn cục

// --- HÀM SCAN ---
// Chức năng: Đọc token tiếp theo từ nguồn vào biến lookAhead
void scan(void) {
  // Lưu địa chỉ của token cũ để giải phóng bộ nhớ sau này
  Token* tmp = currentToken;
  
  // Cập nhật token hiện tại bằng token nhìn trước
  currentToken = lookAhead;
  
  // Gọi Scanner để lấy token hợp lệ tiếp theo (bỏ qua khoảng trắng, comment...)
  lookAhead = getValidToken();
  
  // Giải phóng vùng nhớ của token cũ đã xử lý xong để tránh rò rỉ bộ nhớ
  free(tmp);
}

// --- HÀM EAT ---
// Chức năng: Kiểm tra token nhìn trước có đúng loại mong đợi không và di chuyển tiếp
// Tham số: tokenType - Loại token mong đợi (ví dụ: KW_PROGRAM, SB_SEMICOLON...)
void eat(TokenType tokenType) {
  // Kiểm tra nếu loại token nhìn trước khớp với loại mong đợi
  if (lookAhead->tokenType == tokenType) {
    // Nếu đúng, gọi scan() để tiến tới token kế tiếp
    scan();
  } else {
    // Nếu sai, báo lỗi "Thiếu token" tại dòng và cột tương ứng
    missingToken(tokenType, lookAhead->lineNo, lookAhead->colNo);
  }
}

// --- HÀM COMPILE PROGRAM ---
// Chức năng: Biên dịch toàn bộ chương trình (Root Rule)
// Ngữ pháp: PROGRAM <Tên> ; <Khối lệnh> .
void compileProgram(void) {
  Object* program; // Biến lưu đối tượng chương trình trong bảng ký hiệu

  eat(KW_PROGRAM); // 1. Kiểm tra từ khóa PROGRAM
  eat(TK_IDENT);   // 2. Kiểm tra tên định danh chương trình

  // Tạo đối tượng chương trình và đưa vào bảng ký hiệu
  program = createProgramObject(currentToken->string);
  // Lưu địa chỉ bắt đầu mã lệnh của chương trình (thường là 0)
  program->progAttrs->codeAddress = getCurrentCodeAddress();
  
  // Vào một phạm vi (scope) mới cho chương trình
  enterBlock(program->progAttrs->scope);

  eat(SB_SEMICOLON); // 3. Kiểm tra dấu chấm phẩy ;

  compileBlock();    // 4. Biên dịch khối lệnh chính (Khai báo + Thân)
  eat(SB_PERIOD);    // 5. Kiểm tra dấu chấm kết thúc file .

  genHL(); // Sinh lệnh HALT (Dừng chương trình)

  exitBlock(); // Thoát khỏi phạm vi chương trình
}

// --- HÀM COMPILE CONST DECLS ---
// Chức năng: Biên dịch phần khai báo hằng số
// Ngữ pháp: CONST <Tên> = <Giá trị>; ...
void compileConstDecls(void) {
  Object* constObj;
  ConstantValue* constValue;

  // Nếu token tiếp theo là từ khóa CONST thì mới xử lý
  if (lookAhead->tokenType == KW_CONST) {
    eat(KW_CONST);
    // Vòng lặp xử lý nhiều khai báo hằng liên tiếp (ví dụ: a=1; b=2;)
    do {
      eat(TK_IDENT); // Tên hằng
      
      // Kiểm tra xem tên này đã được dùng chưa trong scope hiện tại
      checkFreshIdent(currentToken->string);
      
      // Tạo đối tượng hằng mới
      constObj = createConstantObject(currentToken->string);
      // Khai báo nó vào bảng ký hiệu
      declareObject(constObj);
      
      eat(SB_EQ); // Dấu bằng =
      
      // Phân tích giá trị của hằng (số hoặc ký tự)
      constValue = compileConstant();
      constObj->constAttrs->value = constValue;
      
      eat(SB_SEMICOLON); // Dấu chấm phẩy ;
    } while (lookAhead->tokenType == TK_IDENT); // Lặp lại nếu còn gặp tên định danh
  }
}

// --- HÀM COMPILE TYPE DECLS ---
// Chức năng: Biên dịch phần khai báo kiểu dữ liệu
// Ngữ pháp: TYPE <Tên> = <Kiểu>; ...
void compileTypeDecls(void) {
  Object* typeObj;
  Type* actualType;

  if (lookAhead->tokenType == KW_TYPE) {
    eat(KW_TYPE);
    do {
      eat(TK_IDENT); // Tên kiểu mới
      
      checkFreshIdent(currentToken->string);
      typeObj = createTypeObject(currentToken->string);
      declareObject(typeObj);
      
      eat(SB_EQ); // Dấu bằng =
      
      // Phân tích kiểu dữ liệu thực sự (ví dụ: ARRAY OF INTEGER)
      actualType = compileType();
      typeObj->typeAttrs->actualType = actualType;
      
      eat(SB_SEMICOLON); // Dấu chấm phẩy ;
    } while (lookAhead->tokenType == TK_IDENT);
  } 
}

// --- HÀM COMPILE VAR DECLS ---
// Chức năng: Biên dịch phần khai báo biến
// Ngữ pháp: VAR <Tên> : <Kiểu>; ...
void compileVarDecls(void) {
  Object* varObj;
  Type* varType;

  if (lookAhead->tokenType == KW_VAR) {
    eat(KW_VAR);
    do {
      eat(TK_IDENT); // Tên biến
      
      checkFreshIdent(currentToken->string);
      varObj = createVariableObject(currentToken->string);
      
      eat(SB_COLON); // Dấu hai chấm :
      
      // Xác định kiểu của biến
      varType = compileType();
      varObj->varAttrs->type = varType;
      
      // Đưa biến vào bảng ký hiệu (cấp phát offset trên stack)
      declareObject(varObj);      
      
      eat(SB_SEMICOLON); // Dấu chấm phẩy ;
    } while (lookAhead->tokenType == TK_IDENT);
  } 
}

// --- HÀM COMPILE BLOCK ---
// Chức năng: Biên dịch một khối (Block) gồm khai báo và thân lệnh
void compileBlock(void) {
  Instruction* jmp;
  
  // Sinh lệnh nhảy (Jump) giả định.
  // Lý do: Khi chương trình chạy, nó cần nhảy qua vùng chứa mã khai báo hàm/thủ tục con
  // để đến ngay lệnh BEGIN của khối hiện tại.
  jmp = genJ(DC_VALUE);

  // Lần lượt biên dịch các phần khai báo
  compileConstDecls();
  compileTypeDecls();
  compileVarDecls();
  compileSubDecls();

  // "Backpatching": Cập nhật địa chỉ đích cho lệnh Jump ở trên.
  // Bây giờ ta đã biết mã lệnh của BEGIN nằm ở đâu (getCurrentCodeAddress).
  updateJ(jmp,getCurrentCodeAddress());
  
  // Sinh lệnh INT (Increment Stack Pointer) để dành chỗ cho các biến cục bộ trên stack.
  // frameSize là tổng kích thước các biến đã khai báo.
  genINT(symtab->currentScope->frameSize);

  // Bắt đầu phần thân
  eat(KW_BEGIN);
  compileStatements(); // Biên dịch danh sách câu lệnh
  eat(KW_END);
}

// --- HÀM COMPILE SUB DECLS ---
// Chức năng: Biên dịch các hàm và thủ tục con
void compileSubDecls(void) {
  // Lặp lại nếu gặp từ khóa FUNCTION hoặc PROCEDURE
  while ((lookAhead->tokenType == KW_FUNCTION) || (lookAhead->tokenType == KW_PROCEDURE)) {
    if (lookAhead->tokenType == KW_FUNCTION)
      compileFuncDecl();
    else compileProcDecl();
  }
}

// --- HÀM COMPILE FUNC DECL ---
// Chức năng: Biên dịch khai báo hàm
void compileFuncDecl(void) {
  Object* funcObj;
  Type* returnType;

  eat(KW_FUNCTION);
  eat(TK_IDENT); // Tên hàm

  checkFreshIdent(currentToken->string);
  funcObj = createFunctionObject(currentToken->string);
  // Lưu địa chỉ bắt đầu mã của hàm
  funcObj->funcAttrs->codeAddress = getCurrentCodeAddress();
  declareObject(funcObj);

  enterBlock(funcObj->funcAttrs->scope); // Vào scope mới của hàm
  
  compileParams(); // Biên dịch tham số

  eat(SB_COLON);
  returnType = compileBasicType(); // Kiểu trả về
  funcObj->funcAttrs->returnType = returnType;

  eat(SB_SEMICOLON);

  compileBlock(); // Biên dịch thân hàm

  genEF(); // Sinh lệnh EF (Exit Function) để thoát khỏi hàm và trả về giá trị
  eat(SB_SEMICOLON);

  exitBlock(); // Thoát scope hàm
}

// --- HÀM COMPILE PROC DECL ---
// Chức năng: Biên dịch khai báo thủ tục
void compileProcDecl(void) {
  Object* procObj;

  eat(KW_PROCEDURE);
  eat(TK_IDENT);

  checkFreshIdent(currentToken->string);
  procObj = createProcedureObject(currentToken->string);
  procObj->procAttrs->codeAddress = getCurrentCodeAddress();
  declareObject(procObj);

  enterBlock(procObj->procAttrs->scope);

  compileParams();

  eat(SB_SEMICOLON);
  compileBlock();

  genEP(); // Sinh lệnh EP (Exit Procedure)
  eat(SB_SEMICOLON);

  exitBlock();
}

// --- HÀM COMPILE UNSIGNED CONSTANT ---
// Chức năng: Biên dịch hằng số không dấu (số hoặc tên hằng)
ConstantValue* compileUnsignedConstant(void) {
  ConstantValue* constValue;
  Object* obj;

  switch (lookAhead->tokenType) {
  case TK_NUMBER: // Là số nguyên
    eat(TK_NUMBER);
    constValue = makeIntConstant(currentToken->value);
    break;
  case TK_IDENT: // Là tên một hằng khác
    eat(TK_IDENT);
    obj = checkDeclaredConstant(currentToken->string); // Kiểm tra xem đã khai báo chưa
    constValue = duplicateConstantValue(obj->constAttrs->value); // Copy giá trị
    break;
  case TK_CHAR: // Là ký tự
    eat(TK_CHAR);
    constValue = makeCharConstant(currentToken->string[0]);
    break;
  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return constValue;
}

// --- HÀM COMPILE CONSTANT ---
// Chức năng: Biên dịch hằng số (có thể có dấu + -)
ConstantValue* compileConstant(void) {
  ConstantValue* constValue;

  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    constValue = compileConstant2();
    break;
  case SB_MINUS: // Xử lý số âm
    eat(SB_MINUS);
    constValue = compileConstant2();
    constValue->intValue = - constValue->intValue; // Đảo dấu
    break;
  case TK_CHAR:
    eat(TK_CHAR);
    constValue = makeCharConstant(currentToken->string[0]);
    break;
  default:
    constValue = compileConstant2();
    break;
  }
  return constValue;
}

ConstantValue* compileConstant2(void) {
  ConstantValue* constValue;
  Object* obj;

  switch (lookAhead->tokenType) {
  case TK_NUMBER:
    eat(TK_NUMBER);
    constValue = makeIntConstant(currentToken->value);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    obj = checkDeclaredConstant(currentToken->string);
    if (obj->constAttrs->value->type == TP_INT)
      constValue = duplicateConstantValue(obj->constAttrs->value);
    else
      error(ERR_UNDECLARED_INT_CONSTANT,currentToken->lineNo, currentToken->colNo);
    break;
  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return constValue;
}

// --- HÀM COMPILE TYPE ---
// Chức năng: Xác định kiểu dữ liệu
Type* compileType(void) {
  Type* type;
  Type* elementType;
  int arraySize;
  Object* obj;

  switch (lookAhead->tokenType) {
  case KW_INTEGER: 
    eat(KW_INTEGER);
    type =  makeIntType();
    break;
  case KW_CHAR: 
    eat(KW_CHAR); 
    type = makeCharType();
    break;
  case KW_ARRAY: // Xử lý mảng: ARRAY [.size.] OF Type
    eat(KW_ARRAY);
    eat(SB_LSEL);
    eat(TK_NUMBER);

    arraySize = currentToken->value; // Lấy kích thước mảng

    eat(SB_RSEL);
    eat(KW_OF);
    elementType = compileType(); // Kiểu phần tử mảng
    type = makeArrayType(arraySize, elementType);
    break;
  case TK_IDENT: // Kiểu định nghĩa trước (TYPE A = ...)
    eat(TK_IDENT);
    obj = checkDeclaredType(currentToken->string);
    type = duplicateType(obj->typeAttrs->actualType);
    break;
  default:
    error(ERR_INVALID_TYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return type;
}

Type* compileBasicType(void) {
  Type* type;

  switch (lookAhead->tokenType) {
  case KW_INTEGER: 
    eat(KW_INTEGER); 
    type = makeIntType();
    break;
  case KW_CHAR: 
    eat(KW_CHAR); 
    type = makeCharType();
    break;
  default:
    error(ERR_INVALID_BASICTYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return type;
}

// --- HÀM COMPILE PARAMS ---
// Chức năng: Biên dịch danh sách tham số hàm/thủ tục
void compileParams(void) {
  if (lookAhead->tokenType == SB_LPAR) { // Dấu mở ngoặc (
    eat(SB_LPAR);
    compileParam(); // Tham số đầu tiên
    while (lookAhead->tokenType == SB_SEMICOLON) { // Các tham số tiếp theo cách nhau bởi ;
      eat(SB_SEMICOLON);
      compileParam();
    }
    eat(SB_RPAR); // Dấu đóng ngoặc )
  }
}

void compileParam(void) {
  Object* param;
  Type* type;
  enum ParamKind paramKind = PARAM_VALUE; // Mặc định là tham trị

  if (lookAhead->tokenType == KW_VAR) { // Nếu có VAR -> Tham biến
    paramKind = PARAM_REFERENCE;
    eat(KW_VAR);
  }

  eat(TK_IDENT); // Tên tham số
  checkFreshIdent(currentToken->string);
  param = createParameterObject(currentToken->string, paramKind);
  eat(SB_COLON);
  type = compileBasicType(); // Kiểu tham số
  param->paramAttrs->type = type;
  declareObject(param); // Đưa tham số vào bảng ký hiệu
}

// --- HÀM COMPILE STATEMENTS ---
// Chức năng: Biên dịch chuỗi các câu lệnh
void compileStatements(void) {
  compileStatement();
  while (lookAhead->tokenType == SB_SEMICOLON) { // Cách nhau bởi ;
    eat(SB_SEMICOLON);
    compileStatement();
  }
}

// --- HÀM COMPILE STATEMENT ---
// Chức năng: Phân loại và gọi hàm xử lý cho từng loại câu lệnh
void compileStatement(void) {
  switch (lookAhead->tokenType) {
  case TK_IDENT: // Bắt đầu bằng tên -> Lệnh gán
    compileAssignSt();
    break;
  case KW_CALL: // Lệnh gọi thủ tục
    compileCallSt();
    break;
  case KW_BEGIN: // Khối lệnh con
    compileGroupSt();
    break;
  case KW_IF: // Câu lệnh điều kiện IF
    compileIfSt();
    break;
  case KW_WHILE: // Vòng lặp WHILE
    compileWhileSt();
    break;
  case KW_FOR: // Vòng lặp FOR
    compileForSt();
    break;
    // Các token này báo hiệu kết thúc câu lệnh hoặc khối lệnh rỗng
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
    break;
  default:
    error(ERR_INVALID_STATEMENT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
}

// --- HÀM COMPILE L-VALUE ---
// Chức năng: Xử lý vế trái của lệnh gán (Biến nhận giá trị)
Type* compileLValue(void) {
  Object* var;
  Type* varType;

  eat(TK_IDENT); // Tên biến
  
  var = checkDeclaredLValueIdent(currentToken->string); // Kiểm tra đã khai báo chưa

  switch (var->kind) {
  case OBJ_VARIABLE:
    genVariableAddress(var); // Sinh lệnh đẩy ĐỊA CHỈ của biến lên stack (LA)

    if (var->varAttrs->type->typeClass == TP_ARRAY) {
      // Nếu là mảng, tính toán địa chỉ phần tử
      varType = compileIndexes(var->varAttrs->type);
    }
    else
      varType = var->varAttrs->type;
    break;
  case OBJ_PARAMETER:
    // Nếu là tham số hàm
    if (var->paramAttrs->kind == PARAM_VALUE)
      genParameterAddress(var); // Tham trị
    else genParameterValue(var); // Tham biến (bản thân giá trị của nó là địa chỉ)

    varType = var->paramAttrs->type;
    break;
  case OBJ_FUNCTION:
    // Nếu gán giá trị cho tên hàm (trong Pascal nghĩa là return)
    genReturnValueAddress(var);
    varType = var->funcAttrs->returnType;
    break;
  default: 
    error(ERR_INVALID_LVALUE,currentToken->lineNo, currentToken->colNo);
  }

  return varType;
}

// --- HÀM COMPILE ASSIGN STATEMENT ---
// Chức năng: Biên dịch lệnh gán (Variable := Expression)
void compileAssignSt(void) {
  Type* varType;
  Type* expType;

  varType = compileLValue(); // Xử lý vế trái (đẩy địa chỉ lên stack)
  
  eat(SB_ASSIGN); // Dấu :=
  expType = compileExpression(); // Xử lý vế phải (tính giá trị, đẩy kết quả lên stack)
  
  checkTypeEquality(varType, expType); // Kiểm tra 2 vế có cùng kiểu không

  genST(); // Sinh lệnh ST (Store): Lấy giá trị ở đỉnh stack lưu vào địa chỉ ngay dưới nó
}

// --- HÀM COMPILE CALL STATEMENT ---
// Chức năng: Biên dịch lệnh gọi thủ tục CALL Procedure(...)
void compileCallSt(void) {
  Object* proc;

  eat(KW_CALL);
  eat(TK_IDENT); // Tên thủ tục

  proc = checkDeclaredProcedure(currentToken->string);
  
  // Xử lý các trường hợp: hàm dựng sẵn hoặc hàm người dùng
  if (proc == NULL) {
    // Không tìm thấy trong symbol table -> Kiểm tra xem có phải hàm hệ thống không
    if (isPredefinedFunction(proc)){
      compileArguments(proc->funcAttrs->paramList);
      genPredefinedFunctionCall(proc);
    } 
  }
  else{
    if (isPredefinedProcedure(proc)) {
      compileArguments(proc->procAttrs->paramList);
      genPredefinedProcedureCall(proc);
    } else {
      // Thủ tục người dùng định nghĩa
      genINT(RESERVED_WORDS); // Tăng stack frame cho các thông tin quản lý (RA, DL, SL)
      compileArguments(proc->procAttrs->paramList); // Đẩy các tham số lên stack
      genDCT( RESERVED_WORDS + proc->procAttrs->paramCount); // Reset stack pointer (trừ hao đi)
      genProcedureCall(proc); // Sinh lệnh CALL tới địa chỉ code của thủ tục
    }
  }
}

void compileGroupSt(void) {
  eat(KW_BEGIN);
  compileStatements();
  eat(KW_END);
}

// --- HÀM COMPILE IF STATEMENT ---
// Chức năng: Biên dịch câu lệnh điều kiện IF ... THEN ... ELSE
void compileIfSt(void) {
  Instruction* fjInstruction; // Lệnh nhảy sai (False Jump)
  Instruction* jInstruction;  // Lệnh nhảy không điều kiện (Jump)

  eat(KW_IF);
  compileCondition(); // Tính giá trị điều kiện (True/False)
  eat(KW_THEN);

  // Sinh lệnh False Jump: Nếu điều kiện sai (đỉnh stack = 0), nhảy tới nhãn... (chưa biết)
  fjInstruction = genFJ(DC_VALUE);
  
  compileStatement(); // Lệnh thực hiện khi đúng

  if (lookAhead->tokenType == KW_ELSE) { // Nếu có nhánh ELSE
    // Sinh lệnh Jump: Nhảy qua nhánh ELSE sau khi làm xong nhánh THEN
    jInstruction = genJ(DC_VALUE);
    
    // Cập nhật đích nhảy cho lệnh False Jump -> nhảy tới đầu nhánh ELSE
    updateFJ(fjInstruction, getCurrentCodeAddress());
    
    eat(KW_ELSE);
    compileStatement(); // Lệnh thực hiện khi sai
    
    // Cập nhật đích nhảy cho lệnh Jump -> nhảy tới cuối lệnh IF
    updateJ(jInstruction, getCurrentCodeAddress());
  } else {
    // Nếu không có ELSE, cập nhật đích nhảy cho False Jump -> nhảy tới cuối lệnh IF
    updateFJ(fjInstruction, getCurrentCodeAddress());
  }
}

// --- HÀM COMPILE WHILE STATEMENT ---
// Chức năng: Biên dịch vòng lặp WHILE
void compileWhileSt(void) {
  CodeAddress beginWhile;
  Instruction* fjInstruction;

  beginWhile = getCurrentCodeAddress(); // Lưu địa chỉ đầu vòng lặp
  eat(KW_WHILE);
  compileCondition(); // Tính điều kiện
  
  // Nếu sai -> Nhảy thoát vòng lặp
  fjInstruction = genFJ(DC_VALUE);
  
  eat(KW_DO);
  compileStatement(); // Thân vòng lặp
  
  genJ(beginWhile); // Quay lại đầu kiểm tra điều kiện
  
  // Cập nhật địa chỉ thoát
  updateFJ(fjInstruction, getCurrentCodeAddress());
}

// --- HÀM COMPILE FOR STATEMENT ---
// Chức năng: Biên dịch vòng lặp FOR
void compileForSt(void) {
  CodeAddress beginLoop;
  Instruction* fjInstruction;
  Type* varType;
  Type *type;

  eat(KW_FOR);

  // Xử lý khởi tạo: i := 1
  varType = compileLValue(); // Địa chỉ biến đếm
  eat(SB_ASSIGN);

  genCV(); // Copy địa chỉ biến đếm (để dùng lại)
  type = compileExpression(); // Giá trị khởi đầu
  checkTypeEquality(varType, type);
  genST(); // Lưu giá trị khởi đầu vào biến đếm
  
  genCV(); 
  genLI(); // Lấy giá trị biến đếm lên stack
  
  beginLoop = getCurrentCodeAddress(); // Đầu vòng lặp (kiểm tra điều kiện)
  eat(KW_TO);

  type = compileExpression(); // Giá trị đích
  checkTypeEquality(varType, type);
  genLE(); // So sánh Biến đếm <= Giá trị đích
  
  fjInstruction = genFJ(DC_VALUE); // Nếu sai -> Thoát

  eat(KW_DO);
  compileStatement(); // Thân vòng lặp

  // Tăng biến đếm: i = i + 1
  genCV();  
  genCV();
  genLI();  // Lấy giá trị i
  genLC(1); // Load 1
  genAD();  // Cộng
  genST();  // Lưu lại vào i

  genCV();
  genLI(); // Lấy giá trị i mới để so sánh tiếp

  genJ(beginLoop); // Quay lại đầu
  updateFJ(fjInstruction, getCurrentCodeAddress()); // Cập nhật đích thoát
  genDCT(1); // Dọn dẹp stack
}

void compileArgument(Object* param) {
  Type* type;

  if (param->paramAttrs->kind == PARAM_VALUE) {
    type = compileExpression();
    checkTypeEquality(type, param->paramAttrs->type);
  } else {
    type = compileLValue();
    checkTypeEquality(type, param->paramAttrs->type);
  }
}

void compileArguments(ObjectNode* paramList) {
  ObjectNode* node = paramList;

  switch (lookAhead->tokenType) {
  case SB_LPAR:
    eat(SB_LPAR);
    if (node == NULL)
      error(ERR_PARAMETERS_ARGUMENTS_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
    compileArgument(node->object);
    node = node->next;

    while (lookAhead->tokenType == SB_COMMA) {
      eat(SB_COMMA);
      if (node == NULL)
	error(ERR_PARAMETERS_ARGUMENTS_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
      compileArgument(node->object);
      node = node->next;
    }

    if (node != NULL)
      error(ERR_PARAMETERS_ARGUMENTS_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
    
    eat(SB_RPAR);
    break;
    // Check FOLLOW set 
  case SB_TIMES:
  case SB_SLASH:
  case SB_PLUS:
  case SB_MINUS:
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
  // [BỔ SUNG] Thêm các toán tử mới vào Follow Set của Arguments
  case KW_MOD:
  case KW_AND:
  case KW_OR:
  // ----------------------------------------------------------
    break;
  default:
    error(ERR_INVALID_ARGUMENTS, lookAhead->lineNo, lookAhead->colNo);
  }
}

// --- HÀM COMPILE CONDITION ---
// Chức năng: Biên dịch biểu thức điều kiện (so sánh)
void compileCondition(void) {
  Type* type1;
  Type* type2;
  TokenType op;

  type1 = compileExpression(); // Vế trái
  checkBasicType(type1);

  op = lookAhead->tokenType; // Toán tử so sánh
  switch (op) {
  case SB_EQ: eat(SB_EQ); break;
  case SB_NEQ: eat(SB_NEQ); break;
  case SB_LE: eat(SB_LE); break;
  case SB_LT: eat(SB_LT); break;
  case SB_GE: eat(SB_GE); break;
  case SB_GT: eat(SB_GT); break;
  default:
    error(ERR_INVALID_COMPARATOR, lookAhead->lineNo, lookAhead->colNo);
  }

  type2 = compileExpression(); // Vế phải
  checkTypeEquality(type1,type2);

  // Sinh mã máy so sánh tương ứng
  switch (op) {
  case SB_EQ: genEQ(); break; // Equal
  case SB_NEQ: genNE(); break; // Not Equal
  case SB_LE: genLE(); break; // Less or Equal
  case SB_LT: genLT(); break; // Less Than
  case SB_GE: genGE(); break; // Greater or Equal
  case SB_GT: genGT(); break; // Greater Than
  default: break;
  }
}

// --- HÀM COMPILE EXPRESSION ---
// Chức năng: Biên dịch biểu thức, xử lý cộng trừ (độ ưu tiên thấp)
Type* compileExpression(void) {
  Type* type;
  
  switch (lookAhead->tokenType) {
  case SB_PLUS: // Dấu cộng một ngôi (+5)
    eat(SB_PLUS);
    type = compileExpression2();
    checkIntType(type);
    break;
  case SB_MINUS: // Dấu trừ một ngôi (-5)
    eat(SB_MINUS);
    type = compileExpression2();
    checkIntType(type);
    genNEG(); // Sinh lệnh đảo dấu
    break;
  default:
    type = compileExpression2();
  }
  return type;
}

Type* compileExpression2(void) {
  Type* type;

  type = compileTerm(); // Xử lý các phép nhân/chia trước
  type = compileExpression3(type); // Sau đó mới cộng trừ

  return type;
}

Type* compileExpression3(Type* argType1) {
  Type* argType2;
  Type* resultType;

  switch (lookAhead->tokenType) {
  case SB_PLUS:
    eat(SB_PLUS);
    checkIntType(argType1);
    argType2 = compileTerm();
    checkIntType(argType2);

    genAD(); // Sinh lệnh ADD

    resultType = compileExpression3(argType1); // Đệ quy tiếp
    break;
  case SB_MINUS:
    eat(SB_MINUS);
    checkIntType(argType1);
    argType2 = compileTerm();
    checkIntType(argType2);

    genSB(); // Sinh lệnh SUBTRACT

    resultType = compileExpression3(argType1);
    break;
    
  // [BỔ SUNG] Thêm trường hợp cho toán tử OR (Độ ưu tiên thấp nhất, ngang với + -)
  case KW_OR:
    eat(KW_OR);
    checkIntType(argType1);
    argType2 = compileTerm();
    checkIntType(argType2);
    
    genOR(); // Sinh lệnh máy ảo OR

    resultType = compileExpression3(argType1);
    break;
  // -----------------------------------------------------------------------------

    // Follow sets
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
    resultType = argType1;
    break;
  default:
    error(ERR_INVALID_EXPRESSION, lookAhead->lineNo, lookAhead->colNo);
  }
  return resultType;
}

// --- HÀM COMPILE TERM ---
// Chức năng: Biên dịch số hạng, xử lý nhân chia (độ ưu tiên cao)
Type* compileTerm(void) {
  Type* type;
  type = compileFactor(); // Xử lý nhân tố (số, biến)
  type = compileTerm2(type); // Xử lý nhân/chia

  return type;
}

Type* compileTerm2(Type* argType1) {
  Type* argType2;
  Type* resultType;

  switch (lookAhead->tokenType) {
  case SB_TIMES:
    eat(SB_TIMES);
    checkIntType(argType1);
    argType2 = compileFactor();
    checkIntType(argType2);

    genML(); // Sinh lệnh MULTIPLY

    resultType = compileTerm2(argType1);
    break;
  case SB_SLASH:
    eat(SB_SLASH);
    checkIntType(argType1);
    argType2 = compileFactor();
    checkIntType(argType2);

    genDV(); // Sinh lệnh DIVIDE

    resultType = compileTerm2(argType1);
    break;

  // [BỔ SUNG] Thêm trường hợp cho toán tử MOD và AND (Độ ưu tiên cao hơn OR)
  case KW_MOD:
    eat(KW_MOD);
    checkIntType(argType1);
    argType2 = compileFactor();
    checkIntType(argType2);

    genMOD(); // Sinh lệnh máy ảo MOD (chia lấy dư)

    resultType = compileTerm2(argType1);
    break;

  case KW_AND:
    eat(KW_AND);
    checkIntType(argType1);
    argType2 = compileFactor();
    checkIntType(argType2);

    genAND(); // Sinh lệnh máy ảo AND

    resultType = compileTerm2(argType1);
    break;
  // ------------------------------------------------------------------------

    // Follow sets
  case SB_PLUS:
  case SB_MINUS:
  case KW_OR: // [QUAN TRỌNG] Thêm OR vào đây vì nó có độ ưu tiên thấp hơn (nằm ở Expression)
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
    resultType = argType1;
    break;
  default:
    error(ERR_INVALID_TERM, lookAhead->lineNo, lookAhead->colNo);
  }
  return resultType;
}

// --- HÀM COMPILE FACTOR ---
// Chức năng: Biên dịch nhân tố - đơn vị nhỏ nhất (Số, Biến, Biểu thức con)
Type* compileFactor(void) {
  Type* type;
  Object* obj;

  switch (lookAhead->tokenType) {
  case TK_NUMBER: // Số nguyên
    eat(TK_NUMBER);
    type = intType;
    genLC(currentToken->value); // Load Constant
    break;
  case TK_CHAR: // Ký tự
    eat(TK_CHAR);
    type = charType;
    genLC(currentToken->value);
    break;
  case TK_IDENT: // Tên định danh
    eat(TK_IDENT);
    obj = checkDeclaredIdent(currentToken->string); // Tra cứu trong bảng ký hiệu

    switch (obj->kind) {
    case OBJ_CONSTANT: // Là hằng
      switch (obj->constAttrs->value->type) {
      case TP_INT:
	type = intType;
	genLC(obj->constAttrs->value->intValue);
	break;
      case TP_CHAR:
	type = charType;
	genLC(obj->constAttrs->value->charValue);
	break;
      default:
	break;
      }
      break;
    case OBJ_VARIABLE: // Là biến
      if (obj->varAttrs->type->typeClass == TP_ARRAY) {
        // Nếu là mảng
	genVariableAddress(obj);
	type = compileIndexes(obj->varAttrs->type); // Tính chỉ số
	genLI(); // Load Indirect (lấy giá trị)
      } else {
	type = obj->varAttrs->type;
	genVariableValue(obj); // Load Value
      }
      break;
    case OBJ_PARAMETER: // Là tham số
      type = obj->paramAttrs->type;
      genParameterValue(obj);
      if (obj->paramAttrs->kind == PARAM_REFERENCE)
	genLI(); // Nếu là tham biến thì phải load indirect thêm 1 lần
      break;
    case OBJ_FUNCTION: // Là hàm
      if (isPredefinedFunction(obj)) {
	compileArguments(obj->funcAttrs->paramList);
	genPredefinedFunctionCall(obj);
      } else {
	genINT(4);
	compileArguments(obj->funcAttrs->paramList);
	genDCT(4+obj->funcAttrs->paramCount);
	genFunctionCall(obj);
      }
      type = obj->funcAttrs->returnType;
      break;
    default: 
      error(ERR_INVALID_FACTOR,currentToken->lineNo, currentToken->colNo);
      break;
    }
    break;
  case SB_LPAR: // Biểu thức trong ngoặc (...)
    eat(SB_LPAR);
    type = compileExpression();
    eat(SB_RPAR);
    break;

  // [BỔ SUNG] Thêm trường hợp cho toán tử NOT (Độ ưu tiên cao nhất, xử lý ngay tại Factor)
  case KW_NOT:
    eat(KW_NOT);
    type = compileFactor(); // Gọi đệ quy để xử lý chuỗi (ví dụ: NOT NOT x)
    checkIntType(type);
    genNOT(); // Sinh lệnh máy ảo NOT (đảo bit/logic)
    break;
  // -------------------------------------------------------------------------------------

  default:
    error(ERR_INVALID_FACTOR, lookAhead->lineNo, lookAhead->colNo);
  }
  
  return type;
}

// --- HÀM COMPILE INDEXES ---
// Chức năng: Tính toán chỉ số mảng
Type* compileIndexes(Type* arrayType) {
  Type* type;

  // Lặp xử lý mảng đa chiều
  while (lookAhead->tokenType == SB_LSEL) {
    eat(SB_LSEL); // [
    type = compileExpression(); // Tính biểu thức index
    checkIntType(type);
    checkArrayType(arrayType);

    // Công thức địa chỉ: Base + Index * ElementSize
    genLC(sizeOfType(arrayType->elementType));
    genML(); // Nhân
    genAD(); // Cộng

    arrayType = arrayType->elementType;
    eat(SB_RSEL); // ]
  }
  checkBasicType(arrayType);
  return arrayType;
}

// --- HÀM CHÍNH (COMPILE) ---
// Chức năng: Khởi động quá trình biên dịch từ tên file
int compile(char *fileName) {
  if (openInputStream(fileName) == IO_ERROR)
    return IO_ERROR;

  currentToken = NULL;
  lookAhead = getValidToken(); // Lấy token đầu tiên

  initSymTab(); // Khởi tạo bảng ký hiệu

  compileProgram(); // Bắt đầu phân tích cú pháp

  cleanSymTab(); // Dọn dẹp
  free(currentToken);
  free(lookAhead);
  closeInputStream();
  return IO_SUCCESS;
}