#pragma once
#include "simulator.hpp"
#include <vector>

namespace sjtu {

void Calculate(std::vector<Matrix *> keys, std::vector<Matrix *> values,
               Rater &rater, GpuSimulator &gpu_sim,
               MatrixMemoryAllocator matrix_memory_allocator) {
  assert(keys.size() == values.size());
  if (keys.empty()) {
    return;
  }

  auto allocate_matrix = [&]() { return matrix_memory_allocator.Allocate(); };

  for (size_t i = 0; i < keys.size(); ++i) {
    gpu_sim.MoveMatrixToSharedMem(keys[i]);
    gpu_sim.MoveMatrixToSharedMem(values[i]);
  }
  for (size_t i = 0; i < keys.size(); ++i) {
    gpu_sim.Transpose(keys[i], kInSharedMemory);
  }

  std::vector<Matrix *> key_prefix_transposed(keys.size());
  key_prefix_transposed[0] = keys[0];
  for (size_t i = 1; i < keys.size(); ++i) {
    key_prefix_transposed[i] = allocate_matrix();
    gpu_sim.Concat(key_prefix_transposed[i - 1], keys[i], key_prefix_transposed[i],
                   1, kInSharedMemory);
    gpu_sim.ReleaseMatrix(keys[i]);
  }

  gpu_sim.Run(false, &matrix_memory_allocator);

  for (size_t i = 0; i < keys.size(); ++i) {
    auto current_query = rater.GetNextQuery();
    gpu_sim.MoveMatrixToSharedMem(current_query);

    Matrix *scores = allocate_matrix();
    gpu_sim.MatMul(current_query, key_prefix_transposed[i], scores);

    Matrix *answer = nullptr;
    for (size_t row = 0; row <= i; ++row) {
      Matrix *score_row = allocate_matrix();
      gpu_sim.GetRow(scores, row, score_row, kInSharedMemory);

      Matrix *exp_row = allocate_matrix();
      gpu_sim.MatExp(score_row, exp_row);

      Matrix *row_sum = allocate_matrix();
      gpu_sim.Sum(exp_row, row_sum);

      Matrix *weights = allocate_matrix();
      gpu_sim.MatDiv(exp_row, row_sum, weights);

      Matrix *row_answer = nullptr;
      for (size_t value_index = 0; value_index <= i; ++value_index) {
        Matrix *weight_scalar = allocate_matrix();
        gpu_sim.GetColumn(weights, value_index, weight_scalar, kInSharedMemory);

        Matrix *weighted_value = allocate_matrix();
        gpu_sim.MatMul(weight_scalar, values[value_index], weighted_value);
        gpu_sim.ReleaseMatrix(weight_scalar);

        if (row_answer == nullptr) {
          row_answer = weighted_value;
        } else {
          Matrix *next_row_answer = allocate_matrix();
          gpu_sim.MatAdd(row_answer, weighted_value, next_row_answer);
          gpu_sim.ReleaseMatrix(row_answer);
          gpu_sim.ReleaseMatrix(weighted_value);
          row_answer = next_row_answer;
        }
      }

      gpu_sim.ReleaseMatrix(score_row);
      gpu_sim.ReleaseMatrix(exp_row);
      gpu_sim.ReleaseMatrix(row_sum);
      gpu_sim.ReleaseMatrix(weights);

      if (answer == nullptr) {
        answer = row_answer;
      } else {
        Matrix *next_answer = allocate_matrix();
        gpu_sim.Concat(answer, row_answer, next_answer, 0, kInSharedMemory);
        gpu_sim.ReleaseMatrix(answer);
        gpu_sim.ReleaseMatrix(row_answer);
        answer = next_answer;
      }
    }

    gpu_sim.ReleaseMatrix(scores);
    gpu_sim.ReleaseMatrix(current_query);
    gpu_sim.MoveMatrixToGpuHbm(answer);
    gpu_sim.Run(false, &matrix_memory_allocator);
    rater.CommitAnswer(*answer);
  }
}

void Test(Rater &rater, GpuSimulator &gpu_sim,
          MatrixMemoryAllocator &matrix_memory_allocator) {
  Calculate(rater.keys_, rater.values_, rater, gpu_sim,
            matrix_memory_allocator);
  rater.PrintResult(gpu_sim);
}

} // namespace sjtu
