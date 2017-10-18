/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "JitTest.hpp"
#include "jitbuilder_compiler.hpp"

#include <string>

class IllformedTrees : public TRTest::JitTest, public ::testing::WithParamInterface<std::string> {};

TEST_P(IllformedTrees, FailCompilation) {
    auto inputTrees = GetParam();
    auto trees = parseString(inputTrees.c_str());

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EXIT(compiler.compile(), ::testing::KilledBySignal(6), "VALIDATION ERROR")
            << "Compilation did not fail due to ill-formed input trees";
}

INSTANTIATE_TEST_CASE_P(ILValidatorDeathTest, IllformedTrees, ::testing::Values(
    "(method return=Int32 (block (ireturn (iadd (iconst 1) (sconst 3)))))",
    "(method return=Int32 (block (ireturn (sadd (iconst 1) (iconst 3)))))",
    "(method return=Address (block (areturn (aiadd (aconst 4) (lconst 1)))))",
    "(method return=Address (block (areturn (aladd (aconst 4) (iconst 1)))))",
    "(method return=Address (block (areturn (aiadd (iconst 1) (aconst 4)))))",
    "(method return=Address (block (areturn (aladd (lconst 1) (aconst 4)))))",
    "(method return=Int32 (block (ireturn (acmpeq (iconst 4) (aconst 4)))))",
    "(method return=Int32 (block (ireturn (acmpge (lconst 4) (aconst 4)))))",
    "(method return=NoType (block (return (GlRegDeps))))",
    "(method return=Int32 (block (ireturn (GlRegDeps) (iconst 3))))",
    "(method return=Int32 (block (ireturn (GlRegDeps) (iadd (iconst 1) (iconst 3)))))",
    "(method return=Int32 (block (ireturn (iconst 3 (GlRegDeps)))))",
    "(method return=Int32 (block (ireturn (iadd (GlRegDeps) (iconst 1) (iconst 3)))))"
    ));

class WellformedTrees : public TRTest::JitTest, public ::testing::WithParamInterface<std::string> {};

TEST_P(WellformedTrees, CompileOnly) {
    auto inputTrees = GetParam();
    auto trees = parseString(inputTrees.c_str());

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly";
}

INSTANTIATE_TEST_CASE_P(ILValidatorTest, WellformedTrees, ::testing::Values(
    "(method return=Int32 (block (ireturn (iconst 3))))",
    "(method return=Int32 (block (ireturn (sconst 3))))",
    "(method return=Int32 (block (ireturn (iadd (iconst 1) (iconst 3)))))",
    "(method return=Address (block (areturn (aladd (aconst 4) (lconst 1)))))",
    "(method return=Int32 (block (ireturn (acmpge (aconst 4) (aconst 4)))))",
    "(method return=Int32 (block (ireturn (scmpeq (sconst 1) (sconst 3)))))",
    "(method return=Int32 (block (ireturn (lcmpeq (lconst 1) (lconst 3)))))"
    ));

class CommoningTest : public TRTest::JitTest, public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {};

TEST_P(CommoningTest, CommoningUnderSameTree)
   {
   auto param = GetParam();
   auto tril = TRIL((method return=Int32 args=[Int32, Int32]
                       (block
                           (ireturn
                               (icmpeq
                                  (imul
                                     (iadd (iload parm=0 id="loadParm0") (iload parm=1 id="loadParm1"))
                                     (isub (@id "loadParm0") (@id "loadParm1")))
                                  (isub
                                     (imul (@id "loadParm0") (@id "loadParm0"))
                                     (imul (@id "loadParm1") (@id "loadParm1")))
                                  )))));

   auto ast = parseString(tril);
   ASSERT_NOTNULL(ast) << "Parsing failed unexpectedly";

   Tril::JitBuilderCompiler compiler{ast};
   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly";

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
   ASSERT_NOTNULL(entry_point);

   ASSERT_EQ(1, entry_point(std::get<0>(param), std::get<1>(param)));
   }

TEST_P(CommoningTest, CommoningWithinBlock)
   {
   auto param = GetParam();
   auto tril = TRIL((method return=Int32 args=[Int32, Int32]
                       (block
                           (iload parm=0 id="loadParm0")
                           (iload parm=1 id="loadParm1")
                           (ireturn
                               (icmpeq
                                  (imul
                                     (iadd (@id "loadParm0") (@id "loadParm1"))
                                     (isub (@id "loadParm0") (@id "loadParm1")))
                                  (isub
                                     (imul (@id "loadParm0") (@id "loadParm0"))
                                     (imul (@id "loadParm1") (@id "loadParm1")))
                                  )))));

   auto ast = parseString(tril);
   ASSERT_NOTNULL(ast) << "Parsing failed unexpectedly";

   Tril::JitBuilderCompiler compiler{ast};
   ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly";

   auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
   ASSERT_NOTNULL(entry_point);

   ASSERT_EQ(1, entry_point(std::get<0>(param), std::get<1>(param)));
   }

INSTANTIATE_TEST_CASE_P(CommingValidationTest, CommoningTest,
  ::testing::ValuesIn(TRTest::const_value_pairs<int32_t, int32_t>()));

class CommoningDeathTest : public TRTest::JitTest {};

TEST_F(CommoningDeathTest, CommoningAcrossBlock)
   {
   //Ensure that the ILValidator is capable of catching 
   //invalid commoning.
   auto tril = TRIL((method return=Int32 args=[Int32]
                     (block name="first"
                      (iload parm=0 id="parm0")
                      (imul id="multwo"
                       (iconst 2)
                       (@id "parm0")
                      ))
                     (block name="second"
                      (ireturn
                       (@id "multwo")
                      )
                     )
                    ));

   auto ast = parseString(tril);
   ASSERT_NOTNULL(ast) << "Parsing failed unexpectedly";

   Tril::JitBuilderCompiler compiler{ast};
   ASSERT_EXIT(compiler.compile(), ::testing::KilledBySignal(6), "VALIDATION ERROR")
      << "Compilation did not fail due to ill-formed input trees";
   }
