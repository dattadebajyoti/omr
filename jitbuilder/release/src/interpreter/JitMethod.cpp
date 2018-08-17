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
#include "JumpBuilder.hpp"
#include "PopLocalBuilder.hpp"
#include "PushLocalBuilder.hpp"

using std::cout;
using std::cerr;

//#define PRINTSTRING(builder, value) ((builder)->Call("printString", 1, (builder)->ConstInt64((int64_t)value)))
//#define PRINTINT64(builder, value) ((builder)->Call("printInt64", 1, (builder)->ConstInt64((int64_t)value))
//#define PRINTINT64VALUE(builder, value) ((builder)->Call("printInt64", 1, value))

JitMethod::JitMethod(InterpreterTypeDictionary *d, Method *method)
   : MethodBuilder(d),
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

   DefineReturnType(Int64);
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
      i += getBytecodeLength(opcode);
      }

   //TODO create / set proper vmstate
   TR::VirtualMachineRegisterInStruct *stackRegister = new TR::VirtualMachineRegisterInStruct(this, "Frame", "frame", "sp", "SP");
   TR::VirtualMachineInterpreterStack *stack = new TR::VirtualMachineInterpreterStack(this, stackRegister, STACKVALUEILTYPE);
   setVMState(stack);

   TR::IlValue *bytecodesAddress = StructFieldInstanceAddress("Frame", "bytecodes", Load("frame"));
   TR::IlValue *bytecodes = LoadAt(_types->PointerTo(_types->PointerTo(Int8)), bytecodesAddress);
   Store("bytecodes", bytecodes);

   AppendBuilder(_builders[0]);

   int32_t bytecodeIndex = GetNextBytecodeFromWorklist();
   bool canHandle = true;
   while (canHandle && (-1 != bytecodeIndex))
      {
      interpreter_opcodes opcode = (interpreter_opcodes)_method->bytecodes[bytecodeIndex];
      int32_t nextBCIndex = bytecodeIndex + getBytecodeLength(opcode);
      TR::BytecodeBuilder *builder = _builders[bytecodeIndex];
      TR::BytecodeBuilder *fallThroughBuilder = nullptr;

      if (nextBCIndex < bytecodeLength)
         {
         fallThroughBuilder = _builders[nextBCIndex];
         }

      builder->Store("pc", builder->ConstInt32(bytecodeIndex));

      switch(opcode)
         {
         case PUSH_CONSTANT:
            builder->execute();
            break;
         case DUP:
            builder->execute();
            break;
         case ADD:
            builder->execute();
            break;
         case SUB:
            builder->execute();
            break;
         case MUL:
            builder->execute();
            break;
         case DIV:
            builder->execute();
            break;
         case RET:
            builder->execute();
            builder->Return(builder->ConstInt64(-1));
            fallThroughBuilder = NULL;
            break;
         case CALL:
            builder->execute();
            break;
         case PUSH_LOCAL:
            builder->execute();
            break;
         case POP_LOCAL:
            builder->execute();
            break;
         case JMPL:
         {
            TR::IlValue *pc = builder->Load("pc");
            pc = builder->Add(builder->Load("pc"), builder->ConstInt32(2));
            builder->execute();
            int8_t index = _method->bytecodes[bytecodeIndex + 1];
            builder->IfCmpNotEqual(_builders[index], builder->Load("pc"), pc);
            break;
         }
         case EXIT:
         default:
            canHandle = false;
         }
      if (NULL != fallThroughBuilder)
         builder->AddFallThroughBuilder(fallThroughBuilder);

      bytecodeIndex = GetNextBytecodeFromWorklist();
      }

   free(_builders);
   return canHandle;
   }

