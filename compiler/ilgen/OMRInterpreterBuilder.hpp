/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef OMR_INTERPRETERBUILDER_INCL
#define OMR_INTERPRETERBUILDER_INCL

#include "ilgen/IlBuilder.hpp"

namespace TR { class InterpreterBuilder; }
namespace TR { class MethodBuilder; }
namespace TR { class VirtualMachineRegister; }
namespace TR { class VirtualMachineInterpreterStack; }

namespace OMR
{

class InterpreterBuilder : public TR::IlBuilder
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

enum OPCODES
   {
   BC_00,
   BC_01,
   BC_02,
   BC_03,
   BC_04,
   BC_05,
   BC_06,
   BC_07,
   BC_08,
   BC_09,
   BC_10,
   BC_11,
   BC_12,
   BC_13,
   BC_14,
   BC_15,
   BC_COUNT = BC_15
   };

   InterpreterBuilder(TR::MethodBuilder *methodBuilder, TR::TypeDictionary *d, TR::VirtualMachineInterpreterStack *interpStack, const char *bytecodePtrName, TR::IlType *bytecodeElementType, const char *pcName, const char *opcodeName);

   TR::IlBuilder *registerOpcodeHandler(int32_t opcode);
   void execute(TR::IlBuilder *builder);

   TR::VirtualMachineInterpreterStack *getState() {return _stack;}

protected:
   void getNextOpcode(TR::IlBuilder *builder);

   void setPC(TR::IlBuilder *builder, int32_t value);
   void setPC(TR::IlBuilder *builder, TR::IlValue *value);
   void incrementPC(TR::IlBuilder *builder, int32_t increment);
   TR::IlValue *getPC(TR::IlBuilder *builder);

   void handleUnusedOpcodes();

private:
   TR::VirtualMachineInterpreterStack *_stack;
   const char *_bytecodePtrName;
   TR::IlType *_bytecodeElementType;
   TR::IlType *_bytecodePtrType;
   const char *_pcName;
   const char *_opcodeName;
   TR::IlBuilder *_defaultHandler;
   TR::IlBuilder *_opcodeBuilders[OPCODES::BC_COUNT];
   bool _opcodeHasBeenRegistered[OPCODES::BC_COUNT];

   };

} // namespace OMR

#endif // !defined(OMR_INTERPRETERBUILDER_INCL)