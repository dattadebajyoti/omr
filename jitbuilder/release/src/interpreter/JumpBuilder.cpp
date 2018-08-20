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

#include <new>

#include "ilgen/RuntimeBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineInterpreterStack.hpp"
#include "InterpreterTypes.h"
#include "JumpBuilder.hpp"

JumpBuilder::JumpBuilder(TR::RuntimeBuilder *runtimeBuilder, int32_t bcIndex)
   : BytecodeBuilder(runtimeBuilder, bcIndex, "JUMP"),
   _runtimeBuilder(runtimeBuilder)
   {
   }

JumpBuilder *
JumpBuilder::OrphanBytecodeBuilder(TR::RuntimeBuilder *runtimeBuilder, int32_t bcIndex)
   {
   JumpBuilder *orphan = new JumpBuilder(runtimeBuilder, bcIndex);
   runtimeBuilder->InitializeBytecodeBuilder(orphan);
   return orphan;
   }

void
JumpBuilder::execute()
   {
   InterpreterVMState *state = (InterpreterVMState*)vmState();
   TR::VirtualMachineStack *stack = state->_stack;
   TR::IlValue *pc = Load("pc");
   TR::IlValue *value = _runtimeBuilder->GetImmediate(this, 1);

   TR::IlValue *right = stack->Pop(this);
   TR::IlValue *left = stack->Pop(this);

   state->Commit(this);

   TR::IlBuilder *jump = NULL;
   TR::IlBuilder *doNotJump = NULL;
   IfThenElse(&jump, &doNotJump,
      LessThan(left, right));

   jump->Store("pc",
   jump->   ConvertTo(Int32, value));

   doNotJump->Store("pc",
   doNotJump->   Add(pc,
   doNotJump->      ConstInt32(2)));
   }

