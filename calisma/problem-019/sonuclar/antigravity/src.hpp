#pragma once
#include "simulator.hpp"
#include <string>
#include <vector>

namespace sjtu {

void Calculate(std::vector<Matrix *> keys, std::vector<Matrix *> values,
               Rater &rater, GpuSimulator &gpu_sim,
               MatrixMemoryAllocator matrix_memory_allocator) {
  assert(keys.size() == values.size());
  
  Matrix* K_concat = nullptr;
  Matrix* V_concat = nullptr;

  for (size_t i = 0; i < keys.size(); ++i) {
    auto current_query = rater.GetNextQuery();
    
    // Move to SRAM
    gpu_sim.MoveMatrixToSharedMem(keys[i]);
    gpu_sim.MoveMatrixToSharedMem(values[i]);
    gpu_sim.MoveMatrixToSharedMem(current_query);
    
    // Concat K and V
    if (i == 0) {
        K_concat = matrix_memory_allocator.Allocate("K_concat_0");
        gpu_sim.Copy(keys[0], K_concat, kInSharedMemory);
        
        V_concat = matrix_memory_allocator.Allocate("V_concat_0");
        gpu_sim.Copy(values[0], V_concat, kInSharedMemory);
    } else {
        Matrix* new_K_concat = matrix_memory_allocator.Allocate("K_concat_" + std::to_string(i));
        gpu_sim.Concat(K_concat, keys[i], new_K_concat, 0, kInSharedMemory);
        gpu_sim.ReleaseMatrix(K_concat);
        K_concat = new_K_concat;
        
        Matrix* new_V_concat = matrix_memory_allocator.Allocate("V_concat_" + std::to_string(i));
        gpu_sim.Concat(V_concat, values[i], new_V_concat, 0, kInSharedMemory);
        gpu_sim.ReleaseMatrix(V_concat);
        V_concat = new_V_concat;
    }
    
    // We can release keys[i] and values[i]
    gpu_sim.ReleaseMatrix(keys[i]);
    gpu_sim.ReleaseMatrix(values[i]);
    
    // Compute Q * K^T
    Matrix* Q_K_T = matrix_memory_allocator.Allocate("Q_K_T_" + std::to_string(i));
    gpu_sim.Transpose(K_concat, kInSharedMemory); 
    gpu_sim.MatMul(current_query, K_concat, Q_K_T);
    gpu_sim.Transpose(K_concat, kInSharedMemory); 
    
    // Softmax
    Matrix* exp_Q_K_T = matrix_memory_allocator.Allocate("exp_Q_K_T_" + std::to_string(i));
    gpu_sim.MatExp(Q_K_T, exp_Q_K_T);
    gpu_sim.ReleaseMatrix(Q_K_T);
    
    Matrix* A = nullptr;
    for (size_t r = 0; r <= i; ++r) {
        Matrix* row = matrix_memory_allocator.Allocate("row_" + std::to_string(i) + "_" + std::to_string(r));
        gpu_sim.GetRow(exp_Q_K_T, r, row, kInSharedMemory);
        
        Matrix* sum = matrix_memory_allocator.Allocate("sum_" + std::to_string(i) + "_" + std::to_string(r));
        gpu_sim.Sum(row, sum);
        
        Matrix* norm_row = matrix_memory_allocator.Allocate("norm_row_" + std::to_string(i) + "_" + std::to_string(r));
        gpu_sim.MatDiv(row, sum, norm_row);
        
        gpu_sim.ReleaseMatrix(row);
        gpu_sim.ReleaseMatrix(sum);
        
        if (r == 0) {
            A = norm_row;
        } else {
            Matrix* new_A = matrix_memory_allocator.Allocate("A_" + std::to_string(i) + "_" + std::to_string(r));
            gpu_sim.Concat(A, norm_row, new_A, 0, kInSharedMemory);
            gpu_sim.ReleaseMatrix(A);
            gpu_sim.ReleaseMatrix(norm_row);
            A = new_A;
        }
    }
    gpu_sim.ReleaseMatrix(exp_Q_K_T);
    
    // Multiply by V
    Matrix* Answer = matrix_memory_allocator.Allocate("Answer_" + std::to_string(i));
    gpu_sim.MatMul(A, V_concat, Answer);
    gpu_sim.ReleaseMatrix(A);
    
    gpu_sim.ReleaseMatrix(current_query);
    
    gpu_sim.MoveMatrixToGpuHbm(Answer);
    
    gpu_sim.Run(false, &matrix_memory_allocator);
    rater.CommitAnswer(*Answer);
  }

  if (K_concat != nullptr) {
    gpu_sim.ReleaseMatrix(K_concat);
  }
  if (V_concat != nullptr) {
    gpu_sim.ReleaseMatrix(V_concat);
  }
  gpu_sim.Run(false, &matrix_memory_allocator);
}

void Test(Rater &rater, GpuSimulator &gpu_sim,
          MatrixMemoryAllocator &matrix_memory_allocator) {
  Calculate(rater.keys_, rater.values_, rater, gpu_sim,
            matrix_memory_allocator);
  rater.PrintResult(gpu_sim);
}

} // namespace sjtu