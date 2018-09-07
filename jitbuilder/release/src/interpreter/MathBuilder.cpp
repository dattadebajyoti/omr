/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "ilgen/RuntimeBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineStack.hpp"

#include "InterpreterTypes.h"
#include "MathBuilder.hpp"

MathBuilder::MathBuilder(TR::MethodBuilder *methodBuilder, int32_t bcIndex, char *name)
   : BytecodeBuilder(methodBuilder, bcIndex, name, 1),
   _runtimeBuilder((TR::RuntimeBuilder *)methodBuilder)
   {
   }

void
MathBuilder::execute()
   {
   TR::VirtualMachineStack *state = ((InterpreterVMState*)vmState())->_stack;

   TR::IlValue *right = state->Pop(this);
   TR::IlValue *left = state->Pop(this);

   TR::IlValue *value = (*_mathFunction)(this, left, right);

   state->Push(this, value);

   _runtimeBuilder->DefaultFallthroughTarget(this);
   }

TR::IlValue *
MathBuilder::add(TR::IlBuilder *builder, TR::IlValue *left, TR::IlValue *right)
   {
   return builder->Add(left, right);
   }

TR::IlValue *
MathBuilder::sub(TR::IlBuilder *builder, TR::IlValue *left, TR::IlValue *right)
   {
   return builder->Sub(left, right);
   }

TR::IlValue *
MathBuilder::mul(TR::IlBuilder *builder, TR::IlValue *left, TR::IlValue *right)
   {
   return builder->Mul(left, right);
   }

TR::IlValue *
MathBuilder::div(TR::IlBuilder *builder, TR::IlValue *left, TR::IlValue *right)
   {
   return builder->Div(left, right);
   }

