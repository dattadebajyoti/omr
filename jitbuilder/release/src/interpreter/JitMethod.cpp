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
#include "ilgen/VirtualMachineOperandArray.hpp"
#include "ilgen/VirtualMachineOperandStack.hpp"
#include "ilgen/VirtualMachineState.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineRegisterInStruct.hpp"

#include "InterpreterTypes.h"

#include "InterpreterTypeDictionary.hpp"
#include "JitMethod.hpp"
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

static void debug(Interpreter *interp, Frame *frame)
   {
#define DEBUG_LINE LINETOSTR(__LINE__)
   fprintf(stderr, "debug interp %p frame %p frame->stack[0] %lld \n", interp, frame, frame->stack[0]);
   }

JitMethod::JitMethod(InterpreterTypeDictionary *d, Method *method)
   : RuntimeBuilder(d),
   _interpTypes(d),
   _method(method)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName(method->name);

   TR::IlType *pInt8 = d->PointerTo(Int8);

   DefineParameter("interp", _interpTypes->getTypes().pInterpreter);
   DefineParameter("frame", _interpTypes->getTypes().pFrame);

   DefineLocal("pc", Int32);
   DefineLocal("opcode", Int32);

   TR::IlType *voidType = d->toIlType<void>();
   DefineFunction((char *)"debug2", (char *)__FILE__, (char *)DEBUG_LINE, (void *)&debug, voidType, 2, _interpTypes->getTypes().pInterpreter, _interpTypes->getTypes().pFrame);

   DefineReturnType(Int64);
   }

TR::IlValue *
JitMethod::GetImmediate(TR::BytecodeBuilder *builder, int32_t pcOffset)
   {
   int8_t immediate = _method->bytecodes[builder->bcIndex() + pcOffset];

   return builder->ConstInt8(immediate);
   }

void
JitMethod::DefaultFallthroughTarget(TR::BytecodeBuilder *builder)
   {
   builder->AddFallThroughBuilder(_builders[builder->bcIndex() + builder->bcLength()]);
   }

void
JitMethod::SetJumpIfTarget(TR::BytecodeBuilder *builder, TR::IlValue *condition, TR::IlValue *jumpTarget)
   {
   TR::BytecodeBuilder *target = _builders[jumpTarget->getConstValue()];
   builder->IfCmpNotEqualZero(target, condition);
   builder->AddFallThroughBuilder(_builders[builder->bcIndex() + builder->bcLength()]);
   }

void
JitMethod::ReturnTarget(TR::BytecodeBuilder *builder)
   {
   builder->Return(builder->ConstInt64(-1));
   }

bool
JitMethod::buildIL()
   {
   int32_t bytecodeLength = _method->bytecodeLength;
   //fprintf(stderr, "JitMethod::buildIL() %s %ld\n", _method->name, bytecodeLength);
   _builders = (TR::BytecodeBuilder **)malloc(sizeof(TR::BytecodeBuilder *) * bytecodeLength);
   if (NULL == _builders)
      {
      return false;
      }

   CallBuilder::DefineFunctions(this, _interpTypes->getTypes().pInterpreter, _interpTypes->getTypes().pFrame);
   RetBuilder::DefineFunctions(this, _interpTypes->getTypes().pInterpreter, _interpTypes->getTypes().pFrame);

   int32_t i = 0;
   while (i < bytecodeLength)
      {
      interpreter_opcodes opcode = (interpreter_opcodes)_method->bytecodes[i];
      _builders[i] = createBuilder(opcode, i);
      i += _builders[i]->bcLength();
      }

   TR::VirtualMachineRegisterInStruct *stackRegister = new TR::VirtualMachineRegisterInStruct(this, "Frame", "frame", "sp", "SP");
   TR::VirtualMachineOperandStack *stack = new TR::VirtualMachineOperandStack(this, 10, STACKVALUEILTYPE, stackRegister, true, 0);

   TR::VirtualMachineRegisterInStruct *localsRegister = new TR::VirtualMachineRegisterInStruct(this, "Frame", "frame", "locals", "LOCALS");
   TR::VirtualMachineOperandArray *localsArray = new TR::VirtualMachineOperandArray(this, 10, STACKVALUEILTYPE, localsRegister);

   InterpreterVMState *vmState = new InterpreterVMState(stack, stackRegister, localsArray, localsRegister);
   setVMState(vmState);

   //TODO fix this..... sp is too far along.... need to move sp back by param count then Drop(-paramCount) then Reload
   //Also retopcode needs to be fixed to push the ret value properly on the returning frame
   stack->Drop(this, -_method->argCount);
   stack->Reload(this);

   //Call("debug2", 2, Load("interp"), Load("frame"));

   AppendBuilder(_builders[0]);

   int32_t bytecodeIndex = GetNextBytecodeFromWorklist();
   bool canHandle = true;
   while (canHandle && (-1 != bytecodeIndex))
      {
      interpreter_opcodes opcode = (interpreter_opcodes)_method->bytecodes[bytecodeIndex];
      TR::BytecodeBuilder *builder = _builders[bytecodeIndex];
      int32_t nextBCIndex = bytecodeIndex + builder->bcLength();

      builder->Store("pc", builder->ConstInt32(bytecodeIndex));

      switch(opcode)
         {
         case CALL:
            builder->execute();
            if (nextBCIndex < bytecodeLength)
               builder->AddFallThroughBuilder(_builders[nextBCIndex]);
            break;
         case PUSH_CONSTANT:
         case DUP:
         case ADD:
         case SUB:
         case MUL:
         case DIV:
         case PUSH_LOCAL:
         case POP_LOCAL:
         case JMPL:
         case JMPG:
         case RET:
            builder->execute();
            break;
         case EXIT:
         default:
            canHandle = false;
         }

      bytecodeIndex = GetNextBytecodeFromWorklist();
      }

   free(_builders);
   return canHandle;
   }
