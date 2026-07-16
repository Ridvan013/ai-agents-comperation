#pragma once
#include "simulator.hpp"
#include <cassert>
#include <cstddef>
#include <vector>

namespace sjtu {

/*
 * Attention as a stream of rank-1 updates.
 *
 * The simulator charges MatMul(A, B) 5 * size(A) * size(B) cycles, i.e. by the
 * element count of the two operands rather than by the number of multiply-adds
 * it performs. Splitting a product along its contraction axis is therefore a
 * strict win: an (m x K) * (K x p) product costs 5*m*K*K*p as a single
 * instruction, but only 5*m*p per rank-1 update, so K of them cost 5*m*p*K.
 * Both products of the attention are evaluated that way:
 *
 *   Q*K^T = sum over j of (column j of Q) * (row j of K^T)   -- K = d = 512
 *   P*V   = sum over l of (column l of P) * (row l of V)      -- K = n
 *
 * The rank-1 chain accumulates the same terms in the same order as the dense
 * MatMul kernel does (a float32 accumulator visiting the contraction axis in
 * increasing order), so the results are bit-identical to the dense form.
 *
 * K^T and the stacked V are kept resident in HBM and their slices are streamed
 * into SRAM one at a time, so only the operands of the current rank-1 update
 * ever occupy the scored memory.
 */
class AttentionEngine {
public:
  AttentionEngine(GpuSimulator &gpu_sim, MatrixMemoryAllocator &allocator)
      : gpu_(gpu_sim), allocator_(allocator) {}

  /*!
   * \brief Append round `round`'s key/value and answer its query.
   * \return Attention(query, keys[0..round], values[0..round]), an
   *         (round+1) x d matrix in GPU HBM.
   */
  Matrix *Run(Matrix *key, Matrix *value, Matrix *query, size_t round) {
    const size_t n = round + 1;
    AppendKey(key);
    AppendValue(value);
    Matrix *scores = Scores(query);
    Matrix *probabilities = Softmax(scores, n);
    return WeightedValues(probabilities, n);
  }

private:
  static constexpr Position kHbm = Position::kInGpuHbm;
  static constexpr Position kSram = Position::kInSharedMemory;

  Matrix *New(const char *name) { return allocator_.Allocate(name); }

  /* Grow K^T (d x n, in HBM) by one column, so that its j-th row holds the
   * j-th component of every key seen so far. */
  void AppendKey(Matrix *key) {
    gpu_.Transpose(key, kHbm); // 1 x d -> d x 1
    if (keys_transposed_ == nullptr) {
      keys_transposed_ = key;
      return;
    }
    Matrix *grown = New("keys_transposed");
    gpu_.Concat(keys_transposed_, key, grown, 1, kHbm);
    gpu_.ReleaseMatrix(keys_transposed_);
    gpu_.ReleaseMatrix(key);
    keys_transposed_ = grown;
  }

  /* Grow the stacked V (n x d), kept resident in HBM. Every later round reads
   * all of it, but only the single row a rank-1 update needs is streamed into
   * SRAM at a time, so the bulk of V never counts against the scored memory. */
  void AppendValue(Matrix *value) {
    if (values_ == nullptr) {
      values_ = value;
      return;
    }
    Matrix *grown = New("values");
    gpu_.Concat(values_, value, grown, 0, kHbm);
    gpu_.ReleaseMatrix(values_);
    gpu_.ReleaseMatrix(value);
    values_ = grown;
  }

  /* Q*K^T (n x n, in SRAM) accumulated from d rank-1 updates. */
  Matrix *Scores(Matrix *query) {
    const size_t d = query->GetColumnNum();
    std::vector<Matrix *> query_columns(d), key_rows(d);
    for (size_t j = 0; j < d; ++j) {
      query_columns[j] = New("query_column");
      gpu_.GetColumn(query, j, query_columns[j], kHbm); // n x 1
      key_rows[j] = New("key_row");
      gpu_.GetRow(keys_transposed_, j, key_rows[j], kHbm); // 1 x n
      // Slicing happens in HBM, so these only reach SRAM once the calculation
      // queue has produced them; the IO queue stalls until then by itself.
      gpu_.MoveMatrixToSharedMem(query_columns[j]);
      gpu_.MoveMatrixToSharedMem(key_rows[j]);
    }

    Matrix *scores = nullptr;
    for (size_t j = 0; j < d; ++j) {
      Matrix *outer_product = New("scores_partial");
      gpu_.MatMul(query_columns[j], key_rows[j], outer_product); // 5*n*n cycles
      gpu_.ReleaseMatrix(query_columns[j]);
      gpu_.ReleaseMatrix(key_rows[j]);
      if (scores == nullptr) {
        scores = outer_product;
        continue;
      }
      Matrix *accumulated = New("scores");
      gpu_.MatAdd(scores, outer_product, accumulated);
      gpu_.ReleaseMatrix(scores);
      gpu_.ReleaseMatrix(outer_product);
      scores = accumulated;
    }
    return scores;
  }

  /* Row-wise softmax producing the full n x n probability matrix in SRAM.
   * Each row is exponentiated, summed, and divided by its own total, then the
   * rows are stacked so that P*V can slice out whole columns. */
  Matrix *Softmax(Matrix *scores, size_t n) {
    Matrix *exponentials = New("exponentials");
    gpu_.MatExp(scores, exponentials);
    gpu_.ReleaseMatrix(scores);

    Matrix *probabilities = nullptr;
    for (size_t i = 0; i < n; ++i) {
      Matrix *row = New("exponentials_row");
      gpu_.GetRow(exponentials, i, row, kSram);
      Matrix *total = New("exponentials_row_sum");
      gpu_.Sum(row, total);
      Matrix *normalized = New("probabilities_row");
      gpu_.MatDiv(row, total, normalized);
      gpu_.ReleaseMatrix(row);
      gpu_.ReleaseMatrix(total);
      if (probabilities == nullptr) {
        probabilities = normalized;
        continue;
      }
      Matrix *grown = New("probabilities");
      gpu_.Concat(probabilities, normalized, grown, 0, kSram);
      gpu_.ReleaseMatrix(probabilities);
      gpu_.ReleaseMatrix(normalized);
      probabilities = grown;
    }
    gpu_.ReleaseMatrix(exponentials);
    return probabilities;
  }

  /* P*V (n x d) accumulated from n rank-1 updates, each the outer product of a
   * column of P with the matching row of V. The finished block is moved to HBM
   * as the committed answer. */
  Matrix *WeightedValues(Matrix *probabilities, size_t n) {
    Matrix *answer = nullptr;
    for (size_t l = 0; l < n; ++l) {
      Matrix *weights = New("weights_column");
      gpu_.GetColumn(probabilities, l, weights, kSram); // n x 1
      Matrix *value_row = New("value_row");
      gpu_.GetRow(values_, l, value_row, kHbm); // 1 x d, sliced in HBM
      gpu_.MoveMatrixToSharedMem(value_row);
      Matrix *outer_product = New("answer_partial");
      gpu_.MatMul(weights, value_row, outer_product); // 5*n*d cycles, n x d
      gpu_.ReleaseMatrix(weights);
      gpu_.ReleaseMatrix(value_row);
      if (answer == nullptr) {
        answer = outer_product;
        continue;
      }
      Matrix *accumulated = New("answer");
      gpu_.MatAdd(answer, outer_product, accumulated);
      gpu_.ReleaseMatrix(answer);
      gpu_.ReleaseMatrix(outer_product);
      answer = accumulated;
    }
    gpu_.ReleaseMatrix(probabilities);
    gpu_.MoveMatrixToGpuHbm(answer);
    return answer;
  }

  GpuSimulator &gpu_;
  MatrixMemoryAllocator &allocator_;
  Matrix *keys_transposed_ = nullptr; // d x n, in HBM
  Matrix *values_ = nullptr;          // n x d, in HBM
};

void Calculate(std::vector<Matrix *> keys, std::vector<Matrix *> values,
               Rater &rater, GpuSimulator &gpu_sim,
               MatrixMemoryAllocator matrix_memory_allocator) {
  assert(keys.size() == values.size());
  AttentionEngine engine(gpu_sim, matrix_memory_allocator);
  for (size_t i = 0; i < keys.size(); ++i) {
    auto current_query = rater.GetNextQuery();
    Matrix *answer = engine.Run(keys[i], values[i], current_query, i);
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
