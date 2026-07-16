#pragma once
#include "simulator.hpp"
#include <string>

namespace sjtu {

namespace {

Matrix *Alloc(MatrixMemoryAllocator &alloc, const std::string &name) {
  return alloc.Allocate(name);
}

// Softmax over a vector stored as (n x 1) or (1 x n) in SRAM.
Matrix *SoftmaxVec(GpuSimulator &gpu_sim, MatrixMemoryAllocator &alloc,
                   Matrix *vec) {
  Matrix *exp_v = Alloc(alloc, "exp_v");
  gpu_sim.MatExp(vec, exp_v);

  Matrix *denom = Alloc(alloc, "denom");
  gpu_sim.Sum(exp_v, denom);

  Matrix *soft = Alloc(alloc, "soft");
  gpu_sim.MatDiv(exp_v, denom, soft);
  gpu_sim.ReleaseMatrix(exp_v);
  gpu_sim.ReleaseMatrix(denom);
  return soft;
}

// scores (n x 1) = q_row (1 x d) dotted with every key row of K (n x d).
Matrix *ScoresForQueryRow(GpuSimulator &gpu_sim, MatrixMemoryAllocator &alloc,
                          Matrix *q_row, Matrix *keys_hbm, size_t dims) {
  Matrix *scores = nullptr;
  for (size_t k = 0; k < dims; ++k) {
    Matrix *q_scalar = Alloc(alloc, "q_scalar");
    Matrix *k_col = Alloc(alloc, "k_col");
    gpu_sim.GetColumn(q_row, k, q_scalar, kInSharedMemory);
    gpu_sim.GetColumn(keys_hbm, k, k_col, kInGpuHbm);
    gpu_sim.MoveMatrixToSharedMem(k_col);

    // (n x 1) @ (1 x 1) scales the key-column by this query feature.
    Matrix *scaled = Alloc(alloc, "k_scaled");
    gpu_sim.MatMul(k_col, q_scalar, scaled);
    gpu_sim.ReleaseMatrix(q_scalar);
    gpu_sim.ReleaseMatrix(k_col);

    if (scores == nullptr) {
      scores = scaled;
    } else {
      Matrix *sum = Alloc(alloc, "score_sum");
      gpu_sim.MatAdd(scores, scaled, sum);
      gpu_sim.ReleaseMatrix(scores);
      gpu_sim.ReleaseMatrix(scaled);
      scores = sum;
    }
  }
  return scores;
}

// out (1 x d) = soft^T @ V, soft is (rows x 1).
Matrix *WeightedValues(GpuSimulator &gpu_sim, MatrixMemoryAllocator &alloc,
                       Matrix *soft, Matrix *values_hbm, size_t rows) {
  Matrix *out = nullptr;
  for (size_t j = 0; j < rows; ++j) {
    Matrix *weight = Alloc(alloc, "weight");
    Matrix *v_row = Alloc(alloc, "v_row");
    gpu_sim.GetRow(soft, j, weight, kInSharedMemory);
    gpu_sim.GetRow(values_hbm, j, v_row, kInGpuHbm);
    gpu_sim.MoveMatrixToSharedMem(v_row);

    // (1 x 1) @ (1 x d) scales the value row
    Matrix *scaled = Alloc(alloc, "v_scaled");
    gpu_sim.MatMul(weight, v_row, scaled);
    gpu_sim.ReleaseMatrix(weight);
    gpu_sim.ReleaseMatrix(v_row);

    if (out == nullptr) {
      out = scaled;
    } else {
      Matrix *sum = Alloc(alloc, "out_sum");
      gpu_sim.MatAdd(out, scaled, sum);
      gpu_sim.ReleaseMatrix(out);
      gpu_sim.ReleaseMatrix(scaled);
      out = sum;
    }
  }
  return out;
}

} // namespace

void Calculate(std::vector<Matrix *> keys, std::vector<Matrix *> values,
               Rater &rater, GpuSimulator &gpu_sim,
               MatrixMemoryAllocator matrix_memory_allocator) {
  assert(keys.size() == values.size());

  Matrix *k_all = nullptr;
  Matrix *v_all = nullptr;

  for (size_t i = 0; i < keys.size(); ++i) {
    Matrix *query = rater.GetNextQuery();
    const size_t rows = i + 1;
    const size_t dims = keys[i]->GetColumnNum();

    if (i == 0) {
      k_all = Alloc(matrix_memory_allocator, "k_all");
      v_all = Alloc(matrix_memory_allocator, "v_all");
      gpu_sim.Copy(keys[0], k_all, kInGpuHbm);
      gpu_sim.Copy(values[0], v_all, kInGpuHbm);
    } else {
      Matrix *k_next = Alloc(matrix_memory_allocator, "k_all");
      Matrix *v_next = Alloc(matrix_memory_allocator, "v_all");
      gpu_sim.Concat(k_all, keys[i], k_next, 0, kInGpuHbm);
      gpu_sim.Concat(v_all, values[i], v_next, 0, kInGpuHbm);
      gpu_sim.ReleaseMatrix(k_all);
      gpu_sim.ReleaseMatrix(v_all);
      k_all = k_next;
      v_all = v_next;
    }

    // Keep full Q in HBM; stream one query row at a time into SRAM.
    Matrix *answer = nullptr;
    for (size_t r = 0; r < rows; ++r) {
      Matrix *q_row = Alloc(matrix_memory_allocator, "q_row");
      gpu_sim.GetRow(query, r, q_row, kInGpuHbm);
      gpu_sim.MoveMatrixToSharedMem(q_row);

      Matrix *scores = ScoresForQueryRow(gpu_sim, matrix_memory_allocator,
                                           q_row, k_all, dims);
      gpu_sim.ReleaseMatrix(q_row);

      Matrix *soft = SoftmaxVec(gpu_sim, matrix_memory_allocator, scores);
      gpu_sim.ReleaseMatrix(scores);

      Matrix *out_row =
          WeightedValues(gpu_sim, matrix_memory_allocator, soft, v_all, rows);
      gpu_sim.ReleaseMatrix(soft);

      // Park partial answers in HBM so SRAM only holds the active row tile.
      gpu_sim.MoveMatrixToGpuHbm(out_row);
      if (answer == nullptr) {
        answer = out_row;
      } else {
        Matrix *merged = Alloc(matrix_memory_allocator, "ans_cat");
        gpu_sim.Concat(answer, out_row, merged, 0, kInGpuHbm);
        gpu_sim.ReleaseMatrix(answer);
        gpu_sim.ReleaseMatrix(out_row);
        answer = merged;
      }
    }

    gpu_sim.ReleaseMatrix(query);

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
