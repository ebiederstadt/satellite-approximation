#include "analysis/utils.h"

#include <fmt/format.h>
#include <magic_enum.hpp>

namespace analysis {
// https://stackoverflow.com/questions/69453972/get-all-non-zero-values-of-a-dense-eigenmatrix-object
VecX<f64> selectMatrixElements(MatX<f64> const& matrix, f64 removalValue)
{
    auto size = matrix.size();
    // create 1D view
    auto view = matrix.reshaped().transpose();
    // create boolean markers for nonzeros
    auto mask = view.array() != removalValue;
    // create index list and set useless elements to sentinel value
    int constexpr sentinel = std::numeric_limits<int>::lowest();
    auto idxs = mask.select(Eigen::RowVectorXi::LinSpaced(size, 0, (int)size), sentinel).eval();
    // sort to remove sentinel values
    std::partial_sort(idxs.begin(), idxs.begin() + size, idxs.end(), std::greater {});
    idxs.conservativeResize(mask.count());
    VecX<f64> newVector = view(idxs.reverse()).eval();
    return newVector;
}
}
