/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "ilgen/VirtualMachineInterpreterArray.hpp"

#include "compile/Compilation.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/VirtualMachineState.hpp"

#define TraceEnabled    (TR::comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(TR::comp(), m, ##__VA_ARGS__);}}

OMR::VirtualMachineInterpreterArray::VirtualMachineInterpreterArray(TR::MethodBuilder *mb, int32_t numOfElements, TR::IlType *elementType, TR::VirtualMachineRegister *arrayBaseRegister)
   : TR::VirtualMachineArray(),
   _mb(mb),
   _elementType(elementType),
   _arrayBaseRegister(arrayBaseRegister)
   {
   init();
   }

OMR::VirtualMachineInterpreterArray::VirtualMachineInterpreterArray(TR::VirtualMachineInterpreterArray *other)
   : TR::VirtualMachineArray(),
   _mb(other->_mb),
   _elementType(other->_elementType),
   _arrayBaseRegister(other->_arrayBaseRegister),
   _arrayBaseName(other->_arrayBaseName)
   {
   }


// commits the simulated operand array of values to the virtual machine state
// the given builder object is where the operations to commit the state will be inserted
// into the array which is assumed to be managed independently, most likely
void
OMR::VirtualMachineInterpreterArray::Commit(TR::IlBuilder *b)
   {
   }

void
OMR::VirtualMachineInterpreterArray::Reload(TR::IlBuilder* b)
   {
   b->Store(_arrayBaseName, _arrayBaseRegister->Load(b));
   }

void
OMR::VirtualMachineInterpreterArray::MergeInto(TR::VirtualMachineState *o, TR::IlBuilder *b)
   {
   }

// Update the OperandArray_base after the Virtual Machine moves the array.
// This call will normally be followed by a call to Reload if any of the array values changed in the move
void
OMR::VirtualMachineInterpreterArray::UpdateArray(TR::IlBuilder *b, TR::IlValue *array)
   {
   // TODO implement?  Is this even useful for anything?
   }

// Allocate a new operand array and copy everything in this state
// If VirtualMachineOperandArray is subclassed, this function *must* also be implemented in the subclass!
TR::VirtualMachineState *
OMR::VirtualMachineInterpreterArray::MakeCopy()
   {
   return static_cast<TR::VirtualMachineState *>(this);
   }

TR::IlValue *
OMR::VirtualMachineInterpreterArray::Get(TR::IlBuilder *b, int32_t index)
   {
   return Get(b, b->ConstInt32(index));
   }

TR::IlValue *
OMR::VirtualMachineInterpreterArray::Get(TR::IlBuilder *b, TR::IlValue *index)
   {
   TR::IlType *pElementType = _mb->typeDictionary()->PointerTo(_elementType);
   TR::IlValue *arrayBase = b->Load(_arrayBaseName);

   TR::IlValue *arrayValue =
   b->LoadAt(pElementType,
   b->   IndexAt(pElementType,
            arrayBase,
            index));

   return arrayValue;
   }

void
OMR::VirtualMachineInterpreterArray::Set(TR::IlBuilder *b, int32_t index, TR::IlValue *value)
   {
   Set(b, b->ConstInt32(index), value);
   }

void
OMR::VirtualMachineInterpreterArray::Set(TR::IlBuilder *b, TR::IlValue *index, TR::IlValue *value)
   {
   TR::IlType *pElementType = _mb->typeDictionary()->PointerTo(_elementType);
   TR::IlValue *arrayBase = b->Load(_arrayBaseName);

   b->StoreAt(
   b->   IndexAt(pElementType,
            arrayBase,
            index),
         value);
   }

void
OMR::VirtualMachineInterpreterArray::Move(TR::IlBuilder *b, int32_t dstIndex, int32_t srcIndex)
   {

   }

void
OMR::VirtualMachineInterpreterArray::init()
   {
   TR::Compilation *comp = TR::comp();
   // Create a temp for the OperandArray base
   TR::SymbolReference *symRef = comp->getSymRefTab()->createTemporary(_mb->methodSymbol(), _mb->typeDictionary()->PointerTo(_elementType)->getPrimitiveType());
   symRef->getSymbol()->setNotCollected();
   char *name = (char *) comp->trMemory()->allocateHeapMemory((11+10+1) * sizeof(char)); // 11 ("_ArrayBase_") + max 10 digits + trailing zero
   sprintf(name, "_ArrayBase_%u", symRef->getCPIndex());
   symRef->getSymbol()->getAutoSymbol()->setName(name);
   _mb->defineSymbol(name, symRef);

   _arrayBaseName = symRef->getSymbol()->getAutoSymbol()->getName();

   // store current operand stack pointer base address so we can use it whenever we need
   // to recreate the stack as the interpreter would have
   _mb->Store(_arrayBaseName, _arrayBaseRegister->Load(_mb));
   }
