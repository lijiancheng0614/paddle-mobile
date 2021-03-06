/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once
#include "../test_include.h"
#include "operators/fill_constant_op.h"

namespace paddle_mobile {
namespace framework {

template <typename Dtype>
class TestFillConstantOp {
 public:
  explicit TestFillConstantOp(const Program<Dtype> p) : program_(p) {
    if (use_optimize_) {
      to_predict_program_ = program_.optimizeProgram;
    } else {
      to_predict_program_ = program_.originProgram;
    }
    const std::vector<std::shared_ptr<BlockDesc>> blocks =
        to_predict_program_->Blocks();
    for (auto block_desc : blocks) {
      std::vector<std::shared_ptr<OpDesc>> ops = block_desc->Ops();
      for (auto op : ops) {
        if (op->Type() == "fill_constant") {
          DLOG << " attr size: " << op->GetAttrMap().size();
          std::unordered_map<std::string, Attribute> attrs = op->GetAttrMap();
          for (std::unordered_map<std::string, Attribute>::iterator it =
                   attrs.begin();
               it != attrs.end(); ++it) {
            DLOG << "  " << it->first << " " << it->second;
          }
          DLOG << " inputs size: " << op->GetInputs().size();
          DLOG << " outputs size: " << op->GetOutputs().size();
          DLOG << " output is : " << op->Output("Out")[0];
          output_var_name = op->Output("Out")[0];
          std::shared_ptr<operators::FillConstantOp<Dtype, float>> op_ptr =
              std::make_shared<operators::FillConstantOp<Dtype, float>>(
                  op->Type(), op->GetInputs(), op->GetOutputs(),
                  op->GetAttrMap(), program_.scope);
          ops_of_block_[*block_desc.get()].push_back(op_ptr);
        }
      }
    }
  }

  std::shared_ptr<Tensor> predict() {
    auto scope = program_.scope;

    Variable *output = scope->Var(output_var_name);
    auto *output_tensor = output->GetMutable<LoDTensor>();

    std::shared_ptr<Tensor> out_tensor = std::make_shared<LoDTensor>();
    out_tensor.reset(output_tensor);

    predict(0);

    return out_tensor;
  }

 private:
  const framework::Program<Dtype> program_;
  std::shared_ptr<ProgramDesc> to_predict_program_;
  std::map<framework::BlockDesc,
           std::vector<std::shared_ptr<OperatorBase<Dtype>>>>
      ops_of_block_;
  bool use_optimize_ = false;
  string output_var_name;

  void predict(int block_id) {
    std::shared_ptr<BlockDesc> to_predict_block =
        to_predict_program_->Block(block_id);
    for (int j = 0; j < ops_of_block_[*to_predict_block.get()].size(); ++j) {
      auto op = ops_of_block_[*to_predict_block.get()][j];
      op->Run();
    }
  }
};

template class TestFillConstantOp<CPU>;
}  // namespace framework
}  // namespace paddle_mobile

int main() {
  DLOG << "----------**********----------";
  DLOG << "begin to run FillConstant Test";
  paddle_mobile::Loader<paddle_mobile::CPU> loader;
  auto program = loader.Load(std::string(g_ocr) + "/model",
                             std::string(g_ocr) + "/params");

  paddle_mobile::framework::TestFillConstantOp<paddle_mobile::CPU>
      testFillConstantOp(program);

  auto output = testFillConstantOp.predict();
  auto *output_ptr = output->data<float>();

  DLOG << "output : ";
  for (int i = 0; i < output->numel(); ++i) {
    DLOG << " index " << i << " : " << output_ptr[i];
  }
  return 0;
}
