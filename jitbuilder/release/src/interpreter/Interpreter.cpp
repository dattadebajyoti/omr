/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "Jit.hpp"
#include "ilgen/InterpreterBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/VirtualMachineInterpreterStack.hpp"
#include "ilgen/VirtualMachineInterpreterArray.hpp"
#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineRegisterInStruct.hpp"

#include "InterpreterTypes.h"

#include "InterpreterTypeDictionary.hpp"
#include "Interpreter.hpp"
#include "PushConstantBuilder.hpp"
#include "DupBuilder.hpp"
#include "MathBuilder.hpp"
#include "RetBuilder.hpp"
#include "ExitBuilder.hpp"
#include "CallBuilder.hpp"
#include "JumpIfBuilder.hpp"
#include "PopLocalBuilder.hpp"
#include "PushLocalBuilder.hpp"

using std::cout;
using std::cerr;

//#define PRINTSTRING(builder, value) ((builder)->Call("printString", 1, (builder)->ConstInt64((int64_t)value)))
//#define PRINTINT64(builder, value) ((builder)->Call("printInt64", 1, (builder)->ConstInt64((int64_t)value))
//#define PRINTINT64VALUE(builder, value) ((builder)->Call("printInt64", 1, value))

int
main(int argc, char *argv[])
   {

   cout << "Step 1: initialize JIT\n";
   bool initialized = initializeJit();
   if (!initialized)
      {
      cerr << "FAIL: could not initialize JIT\n";
      exit(-1);
      }

   cout << "Step 2: define type dictionary\n";
   InterpreterTypeDictionary types;

   cout << "Step 3: compile method builder\n";
   InterpreterMethod interpreterMethod(&types);
   uint8_t *entry = 0;
   int32_t rc = compileMethodBuilder(&interpreterMethod, &entry);
   if (rc != 0)
      {
      cerr << "fail: compilation error " << rc << "\n";
      exit(-2);
      }

   cout << "step 4: invoke compiled code and print results\n";
   InterpreterMethodFunction *interpreter = (InterpreterMethodFunction *) entry;

   Interpreter interp;
   interp.methods = interpreterMethod._methods;

   Frame frame;
   frame.locals = frame.loc;
   frame.sp = frame.stack;
   frame.bytecodes = interp.methods[0].bytecodes;
   frame.previous = NULL;
   frame.savedPC = 0;
   frame.frameType = INTERPRETER;

   memset(frame.stack, 0, sizeof(frame.stack));
   memset(frame.loc, 0, sizeof(frame.loc));

   interp.currentFrame = &frame;

   int64_t result = interpreter(&interp, &frame);

   cout << "interpreter(values) = " << result << "\n";

   cout << "Step 5: shutdown JIT\n";
   shutdownJit();
   }

InterpreterMethod::InterpreterMethod(InterpreterTypeDictionary *d)
   : InterpreterBuilder(d, "bytecodes", d->toIlType<uint8_t>(), "pc", "opcode"),
   _interpTypes(d)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   pInt8 = d->PointerTo(Int8);

   DefineName("Interpreter");

   DefineParameter("interp", _interpTypes->getTypes().pInterpreter);
   DefineParameter("frame", _interpTypes->getTypes().pFrame);

   DefineReturnType(Int64);

   for (int32_t i = 0; i < _methodCount; i++)
      {
      _methods[i].callsUntilJit = 10;
      _methods[i].compiledMethod = NULL;
      }

   _methods[0].bytecodes = _mainMethod;
   _methods[0].name = "MainMethod";
   _methods[0].bytecodeLength = sizeof(_mainMethod);
   _methods[0].argCount = 0;
   _methods[1].bytecodes = _testCallMethod;
   _methods[1].name = "TestCallMethod";
   _methods[1].bytecodeLength = sizeof(_testCallMethod);
   _methods[1].argCount = 0;
   _methods[2].bytecodes = _testDivMethod;
   _methods[2].name = "TestDivMethod";
   _methods[2].bytecodeLength = sizeof(_testDivMethod);
   _methods[2].argCount = 2;
   _methods[3].bytecodes = _testAddMethod;
   _methods[3].name = "TestAddMethod";
   _methods[3].bytecodeLength = sizeof(_testAddMethod);
   _methods[3].argCount = 2;
   _methods[4].bytecodes = _testJMPLMethod;
   _methods[4].name = "TestJMPLMethod";
   _methods[4].bytecodeLength = sizeof(_testJMPLMethod);
   _methods[4].argCount = 1;
   _methods[5].bytecodes = _fib;
   _methods[5].name = "Fib";
   _methods[5].bytecodeLength = sizeof(_fib);
   _methods[5].argCount = 1;
   _methods[6].bytecodes = _iterFib;
   _methods[6].name = "IterFib";
   _methods[6].bytecodeLength = sizeof(_iterFib);
   _methods[6].argCount = 1;
   _methods[7].bytecodes = _testJMPGMethod;
   _methods[7].name = "TestJMPGMethod";
   _methods[7].bytecodeLength = sizeof(_testJMPGMethod);
   _methods[7].argCount = 1;
   }

TR::VirtualMachineState *
InterpreterMethod::createVMState()
   {
   TR::VirtualMachineRegisterInStruct *stackRegister = new TR::VirtualMachineRegisterInStruct(this, "Frame", "frame", "sp", "SP");
   TR::VirtualMachineInterpreterStack *stack = new TR::VirtualMachineInterpreterStack(this, stackRegister, STACKVALUEILTYPE);

   TR::VirtualMachineRegisterInStruct *localsRegister = new TR::VirtualMachineRegisterInStruct(this, "Frame", "frame", "locals", "LOCALS");
   TR::VirtualMachineInterpreterArray *localsArray = new TR::VirtualMachineInterpreterArray(this, 10, STACKVALUEILTYPE, localsRegister);

   InterpreterVMState *vmState = new InterpreterVMState(stack, stackRegister, localsArray, localsRegister);
   return vmState;
   }

void
InterpreterMethod::loadBytecodes(TR::IlBuilder *builder)
   {
   TR::IlValue *frame = builder->Load("frame");
   TR::IlValue *bytecodesAddress = builder->StructFieldInstanceAddress("Frame", "bytecodes", frame);
   TR::IlType *ppInt8 = _types->PointerTo(pInt8);
   TR::IlValue *bytecodes = builder->LoadAt(ppInt8, bytecodesAddress);

   setBytecodes(builder, bytecodes);
   }

void
InterpreterMethod::loadPC(TR::IlBuilder *builder)
   {
   TR::IlValue *frame = builder->Load("frame");
   TR::IlValue *pcAddress = builder->StructFieldInstanceAddress("Frame", "savedPC", frame);
   TR::IlType *pInt32 = _types->PointerTo(Int32);
   TR::IlValue *pc = builder->LoadAt(pInt32, pcAddress);

   setPC(builder, pc);
   }

void
InterpreterMethod::registerBytecodeBuilders()
   {
   CallBuilder::DefineFunctions(this, _interpTypes->getTypes().pInterpreter, _interpTypes->getTypes().pFrame);
   RetBuilder::DefineFunctions(this, _interpTypes->getTypes().pInterpreter, _interpTypes->getTypes().pFrame);

   registerBytecodeBuilder(PushConstantBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::PUSH_CONSTANT));
   registerBytecodeBuilder(DupBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::DUP));
   registerBytecodeBuilder(MathBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::ADD, &MathBuilder::add));
   registerBytecodeBuilder(MathBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::SUB, &MathBuilder::sub));
   registerBytecodeBuilder(MathBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::MUL, &MathBuilder::mul));
   registerBytecodeBuilder(MathBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::DIV, &MathBuilder::div));
   registerBytecodeBuilder(RetBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::RET, _interpTypes->getTypes().pFrame));
   registerBytecodeBuilder(CallBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::CALL, _interpTypes->getTypes().pInterpreter, _interpTypes->getTypes().pFrame));
   registerBytecodeBuilder(JumpIfBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::JMPL, &JumpIfBuilder::lessThan));
   registerBytecodeBuilder(JumpIfBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::JMPG, &JumpIfBuilder::greaterThan));
   registerBytecodeBuilder(PopLocalBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::POP_LOCAL));
   registerBytecodeBuilder(PushLocalBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::PUSH_LOCAL));
   registerBytecodeBuilder(ExitBuilder::OrphanBytecodeBuilder(this, interpreter_opcodes::EXIT));
   }

void
InterpreterMethod::handleInterpreterExit(TR::IlBuilder *builder)
   {
   builder->Return(
   builder->   ConstInt64(-1));
   }
