/*
Copyright (c) 2018 - present Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include"llvm/ADT/ArrayRef.h"
#include"llvm/IR/LLVMContext.h"
#include"llvm/IR/Module.h"
#include"llvm/IR/Function.h"
#include"llvm/IR/BasicBlock.h"
#include"llvm/IR/IRBuilder.h"
#include"llvm/IR/Constants.h"

#include"llvm/Support/TargetRegistry.h"
#include"llvm/Target/TargetOptions.h"
#include"llvm/Support/CodeGen.h"
#include"llvm/Target/TargetMachine.h"
#include"llvm/IR/LegacyPassManager.h"
#include"llvm/IR/PassManager.h"
#include"llvm/Support/FileSystem.h"
#include"llvm/Support/raw_ostream.h"
#include<system_error>

#include<iostream>
#include<vector>
#include<string>
#include<stdint.h>
#include<memory>
#include<map>
#include<fstream>

#include "globals.h"

namespace llvm {
  extern "C" void LLVMInitializeAMDGPUTargetInfo();
  extern "C" void LLVMInitializeAMDGPUTargetInfo();
  extern "C" void LLVMInitializeAMDGPUTarget();
  extern "C" void LLVMInitializeAMDGPUTargetMC();
  extern "C" void LLVMInitializeAMDGPUAsmParser();
  extern "C" void LLVMInitializeAMDGPUAsmPrinter();
}

using namespace llvm::sys;
using namespace llvm;

llvm::LLVMContext context;
llvm::Module *module = new llvm::Module("top", context);
llvm::IRBuilder<> builder(context);

std::map<std::string, llvm::Value*> NamedValues;

void generateFuncAST(){
  std::string funcName = "main";

  std::vector<std::string> Args;
  Args.push_back("a");
  Args.push_back("b");
  Args.push_back("c");

  std::vector<llvm::Type*> argPointers(Args.size());

  for(uint32_t i=0;i<Args.size();i++){
    argPointers[i] = llvm::PointerType::get(llvm::VectorType::get(builder.getFloatTy(), 4), 1);
  }

  llvm::FunctionType *funcType = llvm::FunctionType::get(builder.getVoidTy(), argPointers, false);

  llvm::Function *mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, funcName.c_str(), module);

  mainFunc->setCallingConv(llvm::CallingConv::AMDGPU_KERNEL);

  llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "", mainFunc);
  builder.SetInsertPoint(entry);

  uint32_t Idx = 0;
  for(auto &Arg: mainFunc->args()){
    std::string str = Args[Idx++];
    Arg.setName(str);
    NamedValues[str] = &Arg;
  }

  auto intrin_id = llvm::Intrinsic::amdgcn_workitem_id_x;
  auto f = llvm::Intrinsic::getDeclaration(module, intrin_id);
  auto tid_x = builder.CreateCall(f, {});
  intrin_id = llvm::Intrinsic::amdgcn_workitem_id_y;
  f = llvm::Intrinsic::getDeclaration(module, intrin_id);
  auto tid_y = builder.CreateCall(f, {});
  intrin_id = llvm::Intrinsic::amdgcn_workgroup_id_x;
  f = llvm::Intrinsic::getDeclaration(module, intrin_id);
  auto bid_x = builder.CreateCall(f, {});
  intrin_id = llvm::Intrinsic::amdgcn_workgroup_id_y;
  f = llvm::Intrinsic::getDeclaration(module, intrin_id);
  auto bid_y = builder.CreateCall(f, {});

  auto x0 = getInt(context, 0);
  auto x16 = getInt(context, 16);
  auto x2 = getInt(context, 2);
  auto x1024 = getInt(context, 1024);
  auto x32 = getInt(context, 32);
  auto x64 = getInt(context, 64);
  auto x4096 = getInt(context, 4096);
  auto x8k = getInt(context, 8192);
  auto x128k = getInt(context, 128 * 1024);
  auto x64k = getInt(context, 64*1024);

  auto c_global_id =doUAdd(builder, doUAdd(builder, doUAdd(builder, tid_x, doUMul(builder, tid_y, x4096)), doUMul(builder, bid_x, x32)), doUMul(builder, bid_y, x128k));

  auto A = NamedValues[Args[0]];
  auto B = NamedValues[Args[1]];
  auto C = NamedValues[Args[2]];

  auto c0 = getGlobalLoad(builder, C, c_global_id);
  auto c4 = getGlobalLoad(builder, C, c_global_id, 256);

  auto c_global_id1 = doUAdd(builder, c_global_id, x1024);
  auto c1 = getGlobalLoad(builder, C, c_global_id1);
  auto c5 = getGlobalLoad(builder, C, c_global_id1, 256);

  auto c_global_id2 = doUAdd(builder, c_global_id1, x1024);
  auto c2 = getGlobalLoad(builder, C, c_global_id2);
  auto c6 = getGlobalLoad(builder, C, c_global_id2, 256);

  auto c_global_id3 = doUAdd(builder, c_global_id2, x1024);
  auto c3 = getGlobalLoad(builder, C, c_global_id3);
  auto c7 = getGlobalLoad(builder, C, c_global_id3, 256);

  auto c_global_id4 = doUAdd(builder, c_global_id, x64k);
  auto c8 = getGlobalLoad(builder, C, c_global_id4);
  auto c12 = getGlobalLoad(builder, C, c_global_id4, 256);

  auto c_global_id5 = doUAdd(builder, c_global_id4, x1024);
  auto c9 = getGlobalLoad(builder, C, c_global_id5);
  auto c13 = getGlobalLoad(builder, C, c_global_id5, 256);

  auto c_global_id6 = doUAdd(builder, c_global_id5, x1024);
  auto c10 = getGlobalLoad(builder, C, c_global_id6);
  auto c14 = getGlobalLoad(builder, C, c_global_id6, 256);

  auto c_global_id7 = doUAdd(builder, c_global_id6, x1024);
  auto c11 = getGlobalLoad(builder, C, c_global_id7);
  auto c15 = getGlobalLoad(builder, C, c_global_id7, 256);

  std::vector<unsigned> ids0(1, 0);
  std::vector<unsigned> ids1(1, 1);
  std::vector<unsigned> ids2(1, 2);
  std::vector<unsigned> ids3(1, 3);

  auto a_global_id = doUAdd(builder, doUAdd(builder, doUAdd(builder, tid_x, doUMul(builder, doURem(builder, tid_y, x2), x16)), doUMul(builder, doUDiv(builder, tid_y, x2), x1024)), doUMul(builder, bid_y, x32));
  auto b_global_id = doUAdd(builder, doUAdd(builder, doUAdd(builder, tid_x, doUMul(builder, doURem(builder, tid_y, x2), x16)), doUMul(builder, doUDiv(builder, tid_y, x2), x1024)), doUMul(builder, bid_x, x32));

  auto ra = getGlobalLoad(builder, A, a_global_id);
  auto rb = getGlobalLoad(builder, B, b_global_id);

  a_global_id = doUAdd(builder, a_global_id, x8k);
  b_global_id = doUAdd(builder, b_global_id, x8k);

  getVmCnt(builder, context, 0);
  doBarrier(builder, context);

  auto id = doUMul(builder, doUAdd(builder, doUAdd(builder, tid_x, doUMul(builder, doURem(builder, tid_y, x2), x16)), doUMul(builder, doUDiv(builder, tid_y, x2), x32)), x16);

  auto ldsA = x0;
  auto ldsB = x4096;

  auto ldsReadIdA = doUMul(builder, tid_y, x16);
  auto ldsReadIdB = doUMul(builder, tid_x, x16);

  llvm::Value *a0, *a1, *a2, *a3;
  llvm::Value *b0, *b1, *b2, *b3;
  llvm::Value *microTile0, *microTile1;

// shared_write<0>, shared_write<4096>
  doDsWrite(builder, context, ldsA, id, ra);
  doDsWrite(builder, context, ldsB, id, rb);

// lgkmcnt<0>
  getLgkmCnt(builder, context, 0);
  doBarrier(builder, context);

  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 0);

// for j = 1 -> (4096/8) 512

    auto Start = getInt(context, 1);
    auto TheFunction = builder.GetInsertBlock()->getParent();
    auto PreHeaderBB = builder.GetInsertBlock();

    auto LoopBB = llvm::BasicBlock::Create(context, "loop", TheFunction);
    builder.CreateBr(LoopBB);

    builder.SetInsertPoint(LoopBB);

    auto Variable = builder.CreatePHI(llvm::Type::getInt32Ty(context), 2, "");
    Variable->addIncoming(Start, PreHeaderBB);
    auto SetVal = getInt(context, 1);
    auto NextVar = builder.CreateAdd(Variable, SetVal);

// global_load<0>
  ra = getGlobalLoad(builder, A, a_global_id);
  rb = getGlobalLoad(builder, B, b_global_id);
  a_global_id = doUAdd(builder, a_global_id, x8k);
  b_global_id = doUAdd(builder, b_global_id, x8k);

// shared_read_b128<512>
  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

//
  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

// shared_read_b128<2*512>
  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 2*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);


// shared_read_b128<3*512>
  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 3*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

//
  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

// shared_read_b128<4*512>
  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 4*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);


  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);


// shared_read_b128<5*512>
  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 5*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

//
  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

// shared_read_b128<6*512>
  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 6*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);


  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);


  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 7*512);
  ldsA = builder.CreateXor(ldsA, 0x2000);
  ldsB = builder.CreateXor(ldsB, 0x2000);

  doDsWrite(builder, context, ldsA, id, ra);
  doDsWrite(builder, context, ldsB, id, rb);

  ldsReadIdA = doUMul(builder, tid_y, x16);
  ldsReadIdB = doUMul(builder, tid_x, x16);

  getVmCnt(builder, context, 0);

  doDsWrite(builder, context, ldsA, id, ra);
  doDsWrite(builder, context, ldsB, id, rb);

  getLgkmCnt(builder, context, 2);

  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

  getLgkmCnt(builder, context, 0);
  doBarrier(builder, context);

  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 0);
 
  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);

// loop ends

    auto EndCond = builder.CreateICmpULT(NextVar, getInt(context, 512));
    auto LoopEndBB = builder.GetInsertBlock();
    auto AfterBB = llvm::BasicBlock::Create(context, "", TheFunction);
    builder.CreateCondBr(EndCond, LoopBB, AfterBB);

    builder.SetInsertPoint(AfterBB);

    Variable->addIncoming(NextVar, LoopEndBB);

// shared_read_b128<512>
  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

// shared_read_b128<2*512>
  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 2*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);


// shared_read_b128<3*512>
  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 3*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

// shared_read_b128<4*512>
  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 4*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);


// shared_read_b128<5*512>
  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 5*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

// shared_read_b128<6*512>
  microTile0 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 6*512);
// lgkmcnt<4>
  getLgkmCnt(builder, context, 4);

  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);


  microTile1 = doDsRead(builder, context, ldsA, ldsReadIdA, ldsB, ldsReadIdB, 7*512);
  getLgkmCnt(builder, context, 4);

  a0 = builder.CreateExtractValue(microTile0, ids0);
  a1 = builder.CreateExtractValue(microTile0, ids1);
  b0 = builder.CreateExtractValue(microTile0, ids2);
  b1 = builder.CreateExtractValue(microTile0, ids3);
  doMac4x4(builder, context, a0, b0, c0, c1, c2, c3);
  doMac4x4(builder, context, a0, b1, c4, c5, c6, c7);
  doMac4x4(builder, context, a1, b0, c8, c9, c10, c11);
  doMac4x4(builder, context, a1, b1, c12, c13, c14, c15);

  getLgkmCnt(builder, context, 0);

  a2 = builder.CreateExtractValue(microTile1, ids0);
  a3 = builder.CreateExtractValue(microTile1, ids1);
  b2 = builder.CreateExtractValue(microTile1, ids2);
  b3 = builder.CreateExtractValue(microTile1, ids3);
  doMac4x4(builder, context, a2, b2, c0, c1, c2, c3);
  doMac4x4(builder, context, a2, b3, c4, c5, c6, c7);
  doMac4x4(builder, context, a3, b2, c8, c9, c10, c11);
  doMac4x4(builder, context, a3, b3, c12, c13, c14, c15);

 

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id, c0);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id, c4, 256);

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id1, c1);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id1, c5, 256);

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id2, c2);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id2, c6, 256);

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id3, c3);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id3, c7, 256);


  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id4, c8);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id4, c12, 256);

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id5, c9);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id5, c13, 256);

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id6, c10);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id6, c14, 256);

  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id7, c11);
  getGlobalStore(builder, context, NamedValues[Args[2]], c_global_id7, c15, 256);

  getVmCnt(builder, context, 0);


  builder.CreateRetVoid();

  llvm::SmallString<8> data_ll;
  llvm::raw_svector_ostream dest_ll(data_ll);
  dest_ll.SetUnbuffered();
  std::string print_ll(data_ll.begin(), data_ll.end());
  module->print(errs(), nullptr);
  std::cout<<print_ll<<std::endl;

  auto TargetTriple = std::string("amdgcn-amd-amdhsa-hcc");

  llvm::LLVMInitializeAMDGPUTargetInfo();
  llvm::LLVMInitializeAMDGPUTarget();
  llvm::LLVMInitializeAMDGPUTargetMC();
  llvm::LLVMInitializeAMDGPUAsmParser();
  llvm::LLVMInitializeAMDGPUAsmPrinter();

  std::string Error;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

  if(!Target){
    std::cout << Error<<std::endl;
    return;
  }

  auto GPU = "gfx900";
  auto Features = "";

  llvm::TargetOptions opt;
  auto RM = llvm::Optional<llvm::Reloc::Model>();
  auto targetMachine = Target->createTargetMachine(TargetTriple, GPU, Features, opt, RM);

  module->setDataLayout(targetMachine->createDataLayout());
  module->setTargetTriple(TargetTriple);

  auto FileName = "output.s";
  std::error_code EC;
  raw_fd_ostream dest(FileName, EC, sys::fs::F_None);

  llvm::legacy::PassManager pass;
  auto FileType = llvm::TargetMachine::CGFT_AssemblyFile;

  if(targetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    errs()<< "TargetMachine can't emit a file of this type";
  }

  pass.run(*module);
  dest.flush();

}

int main(){
  std::cout<<"; Generating AMDGPU LLVM IR"<<std::endl;
  generateFuncAST();
}

