// #pragma once

#include <algorithm>
#include <boost/random/linear_feedback_shift.hpp>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include "cpdd/cpdd.h"

#include "common/geometryIO.h"
#include "common/parse_command_line.h"
#include "common/time_loop.h"
#include "parlay/internal/group_by.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/slice.h"

// using coord = long;
using coord = double;
using Typename = coord;
using namespace cpdd;

// NOTE: KNN size
static constexpr double batchQueryRatio = 0.01;
static constexpr size_t batchQueryOsmSize = 10000000;
static constexpr size_t sliding_window_len = 5;
// NOTE: rectangle numbers
static constexpr int rangeQueryNum = 100;
static constexpr int singleQueryLogRepeatNum = 100;

// NOTE: rectangle numbers for inba ratio
static constexpr int rangeQueryNumInbaRatio = 50000;
// NOTE: insert batch ratio for inba ratio
static constexpr double insertBatchInbaRatio = 0.001;
// NOTE: knn batch ratio for inba ratio
static constexpr double knnBatchInbaRatio = 0.001;

// NOTE: Insert Ratio when summary
static constexpr double batchInsertRatio = 0.01;
// NOTE: rectange type used in summary
static constexpr int summaryRangeQueryType = 2;
// NOTE: range query num in summary
static constexpr int summaryRangeQueryNum = 10000;
// NOTE: range query num in real-world
static constexpr int realworldRangeQueryNum = 1000;

//* [a,b)
inline size_t get_random_index(size_t a, size_t b, int seed) {
    return size_t((rand() % (b - a)) + a);
    // return size_t( ( parlay::hash64( static_cast<uint64_t>( seed ) ) % ( b - a
    // ) ) + a );
}

template<typename point>
size_t recurse_box(parlay::slice<point*, point*> In, parlay::sequence<std::pair<std::pair<point, point>, size_t>>& boxs,
                   int DIM, std::pair<size_t, size_t> range, int& idx, int recNum, int type) {
    using tree = ParallelKDtree<point>;
    using box = typename tree::box;

    size_t n = In.size();
    if (idx >= recNum || n < range.first || n == 0) return 0;

    size_t mx = 0;
    bool goon = false;
    if (n <= range.second) {
        // LOG << idx << " " << n << " " << ENDL;
        boxs[idx++] = std::make_pair(tree::get_box(In), In.size());
        // NOTE: handle the cose that all points are the same then become un-divideable
        // NOTE: Modify the coefficient to make the rectangle size distribute as uniform as possible within the range
        if ((type == 0 && n < 40 * range.first) || (type == 1 && n < 10 * range.first) ||
            (type == 2 && n < 2 * range.first) || parlay::all_of(In, [&](const point& p) { return p == In[0]; })) {
            return In.size();
        } else {
            goon = true;
            mx = n;
        }
    }

    int dim = get_random_index(0, DIM, rand());
    size_t pos = get_random_index(0, n, rand());
    parlay::sequence<bool> flag(n, 0);
    parlay::parallel_for(0, n, [&](size_t i) {
        if (cpdd::Num_Comparator<coord>::Gt(In[i].pnt[dim], In[pos].pnt[dim]))
            flag[i] = 1;
        else
            flag[i] = 0;
    });
    auto [Out, m] = parlay::internal::split_two(In, flag);

    assert(Out.size() == n);
    // LOG << dim << " " << Out[0] << Out[m] << ENDL;
    size_t l, r;
    l = recurse_box<point>(Out.cut(0, m), boxs, DIM, range, idx, recNum, type);
    r = recurse_box<point>(Out.cut(m, n), boxs, DIM, range, idx, recNum, type);

    if (goon) {
        return mx;
    } else {
        return std::max(l, r);
    }
}

template<typename point>
std::pair<parlay::sequence<std::pair<std::pair<point, point>, size_t>>, size_t> gen_rectangles(
    int recNum, const int type, const parlay::sequence<point>& WP, int DIM) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;
    using boxs = parlay::sequence<std::pair<box, size_t>>;

    size_t n = WP.size();
    std::pair<size_t, size_t> range;
    if (type == 0) {  //* small bracket
        range.first = 1;
        range.second = size_t(std::sqrt(std::sqrt(n)));
    } else if (type == 1) {  //* medium bracket
        range.first = size_t(std::sqrt(std::sqrt(n)));
        range.second = size_t(std::sqrt(n));
    } else if (type == 2) {  //* large bracket
        range.first = size_t(std::sqrt(n));

        if (n <= 1000000)
            range.second = n - 1;
        else if (n <= 10000000)
            range.second = n / 10 - 1;
        else if (n <= 100000000)
            range.second = n / 100 - 1;
        else if (n <= 1000000000)
            range.second = n / 1000 - 1;
    }
    boxs bxs(recNum);
    int cnt = 0;
    points wp(n);

    srand(10);

    // LOG << " " << range.first << " " << range.second << ENDL;

    size_t maxSize = 0;
    while (cnt < recNum) {
        parlay::copy(WP, wp);
        auto r = recurse_box<point>(parlay::make_slice(wp), bxs, DIM, range, cnt, recNum, type);
        maxSize = std::max(maxSize, r);
        // LOG << cnt << " " << maxSize << ENDL;
    }
    // LOG << "finish generate " << ENDL;
    return std::make_pair(bxs, maxSize);
}

template<typename tree>
void checkTreeSameSequential(typename tree::node* T, int dim, const int& DIM) {
    if (T->is_leaf) {
        assert(T->dim == dim);
        return;
    }
    typename tree::interior* TI = static_cast<typename tree::interior*>(T);
    assert(TI->split.second == dim && TI->dim == dim);
    dim = (dim + 1) % DIM;
    parlay::par_do_if(
        T->size > 1000, [&]() { checkTreeSameSequential<tree>(TI->left, dim, DIM); },
        [&]() { checkTreeSameSequential<tree>(TI->right, dim, DIM); });
    return;
}

template<typename tree>
size_t checkTreesSize(typename tree::node* T) {
    if (T->is_leaf) {
        return T->size;
    }
    typename tree::interior* TI = static_cast<typename tree::interior*>(T);
    size_t l = checkTreesSize<tree>(TI->left);
    size_t r = checkTreesSize<tree>(TI->right);
    assert(l + r == T->size);
    return T->size;
}

template<typename point, int print = 1>
void buildTree(const int& Dim, const parlay::sequence<point>& WP, const int& rounds, ParallelKDtree<point>& pkd) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;

    double loopLate = rounds > 1 ? 1.0 : -0.1;
    size_t n = WP.size();
    points wp = points::uninitialized(n);
    pkd.delete_tree();
    double aveBuild = time_loop(
        rounds, loopLate, [&]() { parlay::copy(WP.cut(0, n), wp.cut(0, n)); }, [&]() { pkd.build(wp.cut(0, n), Dim); },
        [&]() { pkd.delete_tree(); });

    //* return a built tree
    parlay::copy(WP.cut(0, n), wp.cut(0, n));
    pkd.build(wp.cut(0, n), Dim);

    if (print == 1) {
        LOG << aveBuild << " " << std::flush;
        auto deep = pkd.getAveTreeHeight();
        LOG << deep << " " << std::flush;
    } else if (print == 2) {
        size_t max_deep = 0;
        LOG << aveBuild << " " << pkd.getMaxTreeDepth(pkd.get_root(), max_deep) << " " << pkd.getAveTreeHeight() << " "
            << std::flush;
    }

    return;
}

template<typename point, int print = 1>
void incrementalBuild(const int Dim, const parlay::sequence<point>& WP, const int rounds, ParallelKDtree<point>& pkd,
                      double stepRatio) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    size_t n = WP.size();
    size_t step = n * stepRatio;
    points wp = points::uninitialized(n);

    pkd.delete_tree();
    double delay = 1.0;
    if (pkd.get_imbalance_ratio() == 1) {
        delay = -0.1;
    }

    double aveIncreBuild = time_loop(
        rounds, delay, [&]() { parlay::copy(WP, wp); },
        [&]() {
            size_t l = 0, r = 0;
            while (l < n) {
                r = std::min(l + step, n);
                pkd.batchInsert(wp.cut(l, r), Dim);
                l = r;
            }
        },
        [&]() { pkd.delete_tree(); });

    parlay::copy(WP, wp);
    size_t l = 0, r = 0;
    while (l < n) {
        r = std::min(l + step, n);
        pkd.batchInsert(wp.cut(l, r), Dim);
        l = r;
    }

    if (print == 1) {
        auto deep = pkd.getAveTreeHeight();
        LOG << aveIncreBuild << " " << deep << " " << std::flush;
    } else if (print == 2) {  // NOTE: print the maxtree height and avetree height
        size_t max_deep = 0;
        LOG << aveIncreBuild << " " << pkd.getMaxTreeDepth(pkd.get_root(), max_deep) << " " << pkd.getAveTreeHeight()
            << " " << std::flush;
    }
    return;
}

template<typename point, bool print = 1>
void incrementalDelete(const int Dim, const parlay::sequence<point>& WP, const parlay::sequence<point>& WI, int rounds,
                       ParallelKDtree<point>& pkd, double stepRatio) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    size_t n = WP.size();
    size_t step = n * stepRatio;
    points wp = points::uninitialized(2 * n);
    points wi = points::uninitialized(n);

    pkd.delete_tree();

    double aveIncreDelete = time_loop(
        rounds, 1.0,
        [&]() {
            parlay::copy(WP.cut(0, n), wp.cut(0, n));
            parlay::copy(WI.cut(0, n), wp.cut(n, 2 * n));
            parlay::copy(WI.cut(0, n), wi.cut(0, n));

            pkd.build(wp.cut(0, 2 * n), Dim);
        },
        [&]() {
            size_t l = 0, r = 0;
            while (l < n) {
                r = std::min(l + step, n);
                pkd.batchDelete(wi.cut(l, r), Dim);
                l = r;
            }
        },
        [&]() { pkd.delete_tree(); });

    parlay::copy(WP.cut(0, n), wp.cut(0, n));
    parlay::copy(WI.cut(0, n), wp.cut(n, 2 * n));
    parlay::copy(WI.cut(0, n), wi.cut(0, n));
    pkd.build(wp.cut(0, 2 * n), Dim);
    size_t l = 0, r = 0;
    while (l < n) {
        r = std::min(l + step, n);
        pkd.batchDelete(wi.cut(l, r), Dim);
        l = r;
    }

    if (print) {
        auto deep = pkd.getAveTreeHeight();
        LOG << aveIncreDelete << " " << deep << " " << std::flush;
    }
    return;
}

template<typename point, bool serial = false>
void batchInsert(ParallelKDtree<point>& pkd, const parlay::sequence<point>& WP, const parlay::sequence<point>& WI,
                 const uint_fast8_t& DIM, const int& rounds, double ratio = 1.0) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    points wp = points::uninitialized(WP.size());
    points wi = points::uninitialized(WI.size());
    double loopLate = rounds > 1 ? 1.0 : -0.1;

    pkd.delete_tree();

    double aveInsert = time_loop(
        rounds, loopLate,
        [&]() {
            parlay::copy(WP, wp), parlay::copy(WI, wi);
            pkd.build(parlay::make_slice(wp), DIM);
        },
        [&]() {
            if (!serial) {
                pkd.batchInsert(wi.cut(0, size_t(wi.size() * ratio)), DIM);
            } else {
                for (size_t i = 0; i < wi.size() * ratio; i++) {
                    pkd.pointInsert(wi[i], DIM);
                }
            }
        },
        [&]() { pkd.delete_tree(); });

    //* set status to be finish insert
    parlay::copy(WP, wp), parlay::copy(WI, wi);
    pkd.build(parlay::make_slice(wp), DIM);
    if (!serial) {
        pkd.batchInsert(wi.cut(0, size_t(wi.size() * ratio)), DIM);
    } else {
        for (size_t i = 0; i < wi.size() * ratio; i++) {
            pkd.pointInsert(wi[i], DIM);
        }
    }

    LOG << aveInsert << " " << std::flush;

    return;
}

template<typename point, bool serial = false>
void batchDelete(ParallelKDtree<point>& pkd, const parlay::sequence<point>& WP, const parlay::sequence<point>& WI,
                 const uint_fast8_t& DIM, const int& rounds, bool afterInsert = 1, double ratio = 1.0) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    points wp = points::uninitialized(WP.size());
    points wi = points::uninitialized(WP.size());  //! warnning need to adjust space if necessary

    pkd.delete_tree();
    size_t batchSize = static_cast<size_t>(WP.size() * ratio);
    double loopLate = rounds > 1 ? 1.0 : -0.1;

    double aveDelete = time_loop(
        rounds, loopLate,
        [&]() {
            if (afterInsert) {  //* first insert wi then delete wi
                parlay::copy(WP, wp), parlay::copy(WI, wi);
                pkd.build(parlay::make_slice(wp), DIM);
                pkd.batchInsert(wi.cut(0, size_t(wi.size() * ratio)), DIM);
                parlay::copy(WP, wp), parlay::copy(WI, wi);
            } else {  //* only build wp and then delete from wp
                // LOG << " after insert " << ENDL;
                parlay::copy(WP, wp);
                parlay::copy(WP, wi);
                pkd.build(parlay::make_slice(wp), DIM);
            }
        },
        [&]() {
            if (!serial) {
                pkd.batchDelete(wi.cut(0, batchSize), DIM);
            } else {
                for (size_t i = 0; i < batchSize; i++) {
                    pkd.pointDelete(wi[i], DIM);
                }
            }
        },
        [&]() { pkd.delete_tree(); });

    //* set status to be finish delete

    if (afterInsert) {
        parlay::copy(WP, wp), parlay::copy(WI, wi);
        pkd.build(parlay::make_slice(wp), DIM);
        pkd.batchInsert(wi.cut(0, size_t(wi.size() * ratio)), DIM);
        parlay::copy(WP, wp), parlay::copy(WI, wi);
        if (!serial) {
            pkd.batchDelete(wi.cut(0, batchSize), DIM);
        } else {
            for (size_t i = 0; i < batchSize; i++) {
                pkd.pointDelete(wi[i], DIM);
            }
        }
    } else {
        parlay::copy(WP, wp);
        pkd.build(parlay::make_slice(wp), DIM);
    }

    std::cout << aveDelete << " " << std::flush;

    return;
}

template<typename point, bool insert>
void batchUpdateByStep(ParallelKDtree<point>& pkd, const parlay::sequence<point>& WP, const parlay::sequence<point>& WI,
                       const uint_fast8_t& DIM, const int& rounds, double ratio = 1.0, double max_ratio = 1e-2) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    points wp = points::uninitialized(WP.size());
    points wi = points::uninitialized(WI.size());
    size_t n = static_cast<size_t>(max_ratio * wi.size());

    pkd.delete_tree();

    double aveInsert = time_loop(
        rounds, 1.0,
        [&]() {
            parlay::copy(WP, wp), parlay::copy(WI, wi);
            pkd.build(parlay::make_slice(wp), DIM);
        },
        [&]() {
            size_t l = 0, r = 0;
            size_t step = static_cast<size_t>(wi.size() * ratio);
            while (l < n) {
                r = std::min(l + step, n);
                // LOG << l << ' ' << r << ENDL;
                if (insert) {
                    pkd.batchInsert(parlay::make_slice(wi.begin() + l, wi.begin() + r), DIM);
                } else {
                    pkd.batchDelete(parlay::make_slice(wi.begin() + l, wi.begin() + r), DIM);
                }
                l = r;
            }
            // LOG << l << ENDL;
        },
        [&]() { pkd.delete_tree(); });

    // WARN: not reset status

    // parlay::copy(WP, wp), parlay::copy(WI, wi);
    // pkd.build(parlay::make_slice(wp), DIM);
    // size_t l = 0, r = 0;
    // size_t step = wi.size() * ratio;
    // while (l < n) {
    //     r = std::min(l + step, n);
    //     if (insert) {
    //         pkd.batchInsert(parlay::make_slice(wi.begin() + l, wi.begin() + r), DIM);
    //     } else {
    //         pkd.batchDelete(parlay::make_slice(wi.begin() + l, wi.begin() + r), DIM);
    //     }
    //     l = r;
    // }

    LOG << aveInsert << " " << std::flush;

    return;
}

template<typename point, bool printHeight = 1, bool printVisNode = 1>
void queryKNN(const uint_fast8_t& Dim, const parlay::sequence<point>& WP, const int& rounds, ParallelKDtree<point>& pkd,
              Typename* kdknn, const int K, const bool do_not_flatten) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using coord = typename point::coord;
    using nn_pair = std::pair<std::reference_wrapper<point>, coord>;
    // using nn_pair = std::pair<point, coord>;
    size_t n = WP.size();
    int LEAVE_WRAP = 32;
    double loopLate = rounds > 1 ? 1.0 : -0.1;
    node* KDParallelRoot = pkd.get_root();
    points wp = points::uninitialized(n);
    parlay::copy(WP, wp);

    parlay::sequence<nn_pair> Out(K * n, nn_pair(std::ref(wp[0]), 0));
    // parlay::sequence<nn_pair> Out(K * n);
    parlay::sequence<kBoundedQueue<point, nn_pair>> bq =
        parlay::sequence<kBoundedQueue<point, nn_pair>>::uninitialized(n);
    parlay::parallel_for(0, n, [&](size_t i) { bq[i].resize(Out.cut(i * K, i * K + K)); });
    parlay::sequence<size_t> visNum(n);

    double aveQuery = time_loop(
        rounds, loopLate, [&]() { parlay::parallel_for(0, n, [&](size_t i) { bq[i].reset(); }); },
        [&]() {
            if (!do_not_flatten) {  // WARN: Need ensure pkd.size() == wp.size()
                pkd.flatten(pkd.get_root(), parlay::make_slice(wp));
            }
            auto bx = pkd.get_root_box();
            double aveVisNum = 0.0;
            parlay::parallel_for(0, n, [&](size_t i) {
                size_t visNodeNum = 0;
                pkd.k_nearest(KDParallelRoot, wp[i], Dim, bq[i], bx, visNodeNum);
                kdknn[i] = bq[i].top().second;
                visNum[i] = visNodeNum;
            });
        },
        [&]() {});

    LOG << aveQuery << " " << std::flush;
    if (printHeight) {
        auto deep = pkd.getAveTreeHeight();
        LOG << deep << " " << std::flush;
    }
    if (printVisNode) {
        LOG << parlay::reduce(visNum.cut(0, n)) / n << " " << std::flush;
    }

    return;
}

template<typename point>
void rangeCount(const parlay::sequence<point>& wp, ParallelKDtree<point>& pkd, Typename* kdknn, const int& rounds,
                const int& queryNum) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    int n = wp.size();

    parlay::sequence<size_t> visLeafNum(queryNum, 0);
    parlay::sequence<size_t> visInterNum(queryNum, 0);

    double aveCount = time_loop(
        rounds, 1.0, [&]() {},
        [&]() {
            parlay::parallel_for(0, queryNum, [&](size_t i) {
                box queryBox = pkd.get_box(box(wp[i], wp[i]), box(wp[(i + n / 2) % n], wp[(i + n / 2) % n]));
                kdknn[i] = pkd.range_count(queryBox, visLeafNum[i], visInterNum[i]);
            });
        },
        [&]() {});

    LOG << aveCount << " " << std::flush;

    return;
}

template<typename point>
void rangeCountRadius(const parlay::sequence<point>& wp, ParallelKDtree<point>& pkd, Typename* kdknn, const int& rounds,
                      const int& queryNum) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;
    using circle = typename tree::circle;

    int n = wp.size();

    double aveCount = time_loop(
        rounds, 1.0, [&]() {},
        [&]() {
            parlay::parallel_for(0, queryNum, [&](size_t i) {
                //   box queryBox = pkd.get_box(
                //   box( wp[i], wp[i] ), box( wp[( i + n / 2 ) % n], wp[( i + n / 2 )
                //   % n] ) );
                auto d = cpdd::ParallelKDtree<point>::p2p_distance(wp[i], wp[(i + n / 2) % n], wp[i].get_dim());
                d = static_cast<coord>(std::sqrt(d));
                circle cl = circle(wp[i], d);
                kdknn[i] = pkd.range_count(cl);
            });
        },
        [&]() {});

    LOG << aveCount << " " << std::flush;

    return;
}

template<typename point>
void rangeQuery(const parlay::sequence<point>& wp, ParallelKDtree<point>& pkd, Typename* kdknn, const int& rounds,
                const int& queryNum, parlay::sequence<point>& Out) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    int n = wp.size();
    size_t step = Out.size() / queryNum;
    using ref_t = std::reference_wrapper<point>;
    parlay::sequence<ref_t> out_ref(Out.size(), std::ref(Out[0]));
    // parlay::sequence<double> preTime( queryNum, 0 );

    double aveQuery = time_loop(
        rounds, 1.0, [&]() {},
        [&]() {
            parlay::parallel_for(0, queryNum, [&](size_t i) {
                box queryBox = pkd.get_box(box(wp[i], wp[i]), box(wp[(i + n / 2) % n], wp[(i + n / 2) % n]));
                kdknn[i] = pkd.range_query_serial(queryBox, out_ref.cut(i * step, (i + 1) * step));
            });
        },
        [&]() {});

    parlay::parallel_for(0, queryNum, [&](size_t i) {
        for (int j = 0; j < kdknn[i]; j++) {
            Out[i * step + j] = out_ref[i * step + j];
        }
    });

    LOG << aveQuery << " " << std::flush;
    return;
}

//* test range count for fix rectangle
template<typename point>
void rangeCountFix(const parlay::sequence<point>& WP, ParallelKDtree<point>& pkd, Typename* kdknn, const int& rounds,
                   int recType, int recNum, int DIM) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    int n = WP.size();

    auto [queryBox, maxSize] = gen_rectangles(recNum, recType, WP, DIM);
    parlay::sequence<size_t> visLeafNum(recNum, 0), visInterNum(recNum, 0);

    double aveCount = time_loop(
        rounds, 1.0, [&]() {},
        [&]() {
            parlay::parallel_for(0, recNum, [&](size_t i) {
                visInterNum[i] = 0;
                visLeafNum[i] = 0;
                kdknn[i] = pkd.range_count(queryBox[i].first, visLeafNum[i], visInterNum[i]);
            });
            // for (int i = 0; i < recNum; i++) {
            //     visInterNum[i] = 0;
            //     visLeafNum[i] = 0;
            //     kdknn[i] = pkd.range_count(queryBox[i].first, visLeafNum[i], visInterNum[i]);
            // }
        },
        [&]() {});

    LOG << aveCount << " " << std::flush;

    return;
}

template<typename point>
void rangeCountFixWithLog(const parlay::sequence<point>& WP, ParallelKDtree<point>& pkd, Typename* kdknn,
                          const int& rounds, int recType, int recNum, int DIM) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    int n = WP.size();

    auto [queryBox, maxSize] = gen_rectangles(recNum, recType, WP, DIM);
    parlay::sequence<size_t> visLeafNum(recNum, 0), visInterNum(recNum, 0);
    parlay::internal::timer t;
    for (int i = 0; i < recNum; i++) {
        double aveQuery = time_loop(
            rounds, -1.0,
            [&]() {
                visInterNum[i] = 0;
                visLeafNum[i] = 0;
            },
            [&]() { kdknn[i] = pkd.range_count(queryBox[i].first, visLeafNum[i], visInterNum[i]); }, [&]() {});
        if (queryBox[i].second != kdknn[i]) LOG << "wrong" << ENDL;
        LOG << queryBox[i].second << " " << std::scientific << aveQuery << ENDL;
    }

    return;
}

//* test range query for fix rectangle
template<typename point>
void rangeQueryFix(const parlay::sequence<point>& WP, ParallelKDtree<point>& pkd, Typename* kdknn, const int& rounds,
                   parlay::sequence<point>& Out, int recType, int recNum, int DIM) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    auto [queryBox, maxSize] = gen_rectangles(recNum, recType, WP, DIM);
    // LOG << maxSize << ENDL;
    Out.resize(recNum * maxSize);

    int n = WP.size();
    size_t step = Out.size() / recNum;
    using ref_t = std::reference_wrapper<point>;
    parlay::sequence<ref_t> out_ref(Out.size(), std::ref(Out[0]));
    double loopLate = rounds > 1 ? 1.0 : -0.1;

    double aveQuery = time_loop(
        rounds, loopLate, [&]() {},
        [&]() {
            parlay::parallel_for(0, recNum, [&](size_t i) {
                kdknn[i] = pkd.range_query_serial(queryBox[i].first, Out.cut(i * step, (i + 1) * step));
            });
            // for (size_t i = 0; i < recNum; i++) {
            //     kdknn[i] = pkd.range_query_serial(queryBox[i].first, Out.cut(i * step, (i + 1) * step));
            //     // LOG << kdknn[i] << " " << std::flush;
            // }
        },
        [&]() {});

    LOG << aveQuery << " " << std::flush;
    return;
}

template<typename point>
void rangeQuerySerialWithLog(const parlay::sequence<point>& WP, ParallelKDtree<point>& pkd, Typename* kdknn,
                             const int& rounds, parlay::sequence<point>& Out, int recType, int recNum, int DIM) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    auto [queryBox, maxSize] = gen_rectangles(recNum, recType, WP, DIM);
    Out.resize(recNum * maxSize);

    size_t step = Out.size() / recNum;
    // using ref_t = std::reference_wrapper<point>;
    // parlay::sequence<ref_t> out_ref( Out.size(), std::ref( Out[0] ) );

    for (int i = 0; i < recNum; i++) {
        double aveQuery = time_loop(
            rounds, -1.0, [&]() {},
            [&]() { kdknn[i] = pkd.range_query_serial(queryBox[i].first, Out.cut(i * step, (i + 1) * step)); },
            [&]() {});
        if (queryBox[i].second != kdknn[i]) LOG << "wrong" << ENDL;
        LOG << queryBox[i].second << " " << std::scientific << aveQuery << ENDL;
        // LOG << queryBox[i].second << " " << std::setprecision(7) << aveQuery << ENDL;
    }

    return;
}

template<typename point>
void generate_knn(const uint_fast8_t& Dim, const parlay::sequence<point>& WP, const int K, const char* outFile) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using coord = typename point::coord;
    using nn_pair = std::pair<point, coord>;
    // using nn_pair = std::pair<std::reference_wrapper<point>, coord>;
    using ID_type = uint;

    size_t n = WP.size();

    tree pkd;
    points wp = points(n);
    parlay::copy(WP.cut(0, n), wp.cut(0, n));

    pkd.build(parlay::make_slice(wp), Dim);

    parlay::sequence<nn_pair> Out(K * n);
    parlay::sequence<kBoundedQueue<point, nn_pair>> bq =
        parlay::sequence<kBoundedQueue<point, nn_pair>>::uninitialized(n);
    parlay::parallel_for(0, n, [&](size_t i) {
        bq[i].resize(Out.cut(i * K, i * K + K));
        bq[i].reset();
    });

    std::cout << "begin query" << std::endl;
    parlay::copy(WP.cut(0, n), wp.cut(0, n));
    node* KDParallelRoot = pkd.get_root();
    auto bx = pkd.get_root_box();
    parlay::parallel_for(0, n, [&](size_t i) {
        size_t visNodeNum = 0;
        pkd.k_nearest(KDParallelRoot, wp[i], Dim, bq[i], bx, visNodeNum);
    });
    std::cout << "finish query" << std::endl;

    std::ofstream ofs(outFile);
    if (!ofs.is_open()) {
        throw("file not open");
        abort();
    }
    size_t m = n * K;
    ofs << "WeightedAdjacencyGraph" << '\n';
    ofs << n << '\n';
    ofs << m << '\n';
    parlay::sequence<uint64_t> offset(n + 1);
    parlay::parallel_for(0, n + 1, [&](size_t i) { offset[i] = i * K; });
    // parlay::parallel_for( 0, n, [&]( size_t i ) {
    //   for ( size_t j = 0; j < K; j++ ) {
    //     if ( Out[i * K + j].first == wp[i] ) {
    //       printf( "%d, self-loop\n", i );
    //       exit( 0 );
    //     }
    //   }
    // } );

    parlay::sequence<ID_type> edge(m);
    parlay::parallel_for(0, m, [&](size_t i) { edge[i] = Out[i].first.id; });
    parlay::sequence<double> weight(m);
    parlay::parallel_for(0, m, [&](size_t i) { weight[i] = Out[i].second; });
    for (size_t i = 0; i < n; i++) {
        ofs << offset[i] << '\n';
    }
    for (size_t i = 0; i < m; i++) {
        ofs << edge[i];
        ofs << "\n";
        // ofs << edge[i] << '\n';
    }
    for (size_t i = 0; i < m; i++) {
        ofs << weight[i] << '\n';
    }
    ofs.close();
    return;
}

template<typename point>
void insertOsmByTime(const int Dim, const parlay::sequence<parlay::sequence<point>>& node_by_time, const int rounds,
                     ParallelKDtree<point>& pkd, const int K, Typename* kdknn) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using box = typename tree::box;

    pkd.delete_tree();
    const int time_period_num = node_by_time.size();
    const size_t input_offset = batchQueryOsmSize / 10;
    parlay::sequence<points> wp(time_period_num), wi(time_period_num);
    points query_points(batchQueryOsmSize);
    for (int i = 0; i < time_period_num; i++) {
        parlay::copy(node_by_time[i].cut(0, input_offset), query_points.cut(i * input_offset, (i + 1) * input_offset));
        wp[i] = node_by_time[i];
        wi[i] = node_by_time[i];
    }

    // NOTE: begin revert
    LOG << ENDL;
    int within_num = 0;
    for (int i = 0; i < time_period_num; i++) {
        LOG << wp[i].size() << " ";
        parlay::internal::timer t;
        t.reset(), t.start();
        pkd.batchInsert(wp[i].cut(input_offset, wp[i].size()), Dim);
        t.stop();
        LOG << t.total_time() << " ";
        if (within_num == sliding_window_len) {
            // LOG << "begin delete " << wi[i - within_num].size() << " ";
            t.reset(), t.start();
            pkd.batchDelete(wi[i - within_num].cut(input_offset, wi[i - within_num].size()), Dim);
            t.stop();
            LOG << t.total_time() << " ";
        } else {
            within_num++;
            LOG << "-1 ";
        }

        if (time_period_num < 12) {
            // points tmp(node_by_time[0].begin(), node_by_time[0].begin() + batchQueryOsmSize);
            queryKNN(Dim, query_points, rounds, pkd, kdknn, K, true);
        }

        LOG << ENDL;
    }

    size_t max_deep = 0;
    LOG << " " << pkd.getMaxTreeDepth(pkd.get_root(), max_deep) << " " << pkd.getAveTreeHeight() << " " << std::flush;

    return;
}

template<typename point, bool insertBuild = 1>
void incrementalBuildAndQuery(const int Dim, const parlay::sequence<point>& WP, const int rounds,
                              ParallelKDtree<point>& pkd, double stepRatio,
                              const parlay::sequence<point>& query_points) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    size_t n = WP.size();
    size_t step = n * stepRatio;
    points wp = points::uninitialized(n);

    pkd.delete_tree();

    parlay::copy(WP, wp);
    size_t l = 0, r = 0;

    size_t batchSize = query_points.size();
    Typename* kdknn = new Typename[batchSize];
    /*const int k[3] = {1, 5, 100};*/
    const int k[3] = {1};

    /*LOG << "begin insert: " << batchSize << ENDL;*/
    size_t cnt = 0;
    while (l < n) {
        parlay::internal::timer t;
        r = std::min(l + step, n);
        if (insertBuild) {
            t.reset(), t.start();
            pkd.batchInsert(wp.cut(l, r), Dim);
        } else {
            pkd.delete_tree();
            parlay::copy(WP.cut(0, r), wp);
            t.reset(), t.start();
            pkd.build(wp.cut(0, r), Dim);
        }
        t.stop();

        // NOTE: print info

        if (cnt % 10 == 0) {
            size_t max_deep = 0;
            LOG << l << " " << r << " " << t.total_time() << " " << pkd.getMaxTreeDepth(pkd.get_root(), max_deep) << " "
                << pkd.getAveTreeHeight() << " " << std::flush;

            // NOTE: add additional query phase
            /*for (int i = 0; i < 3; i++) {*/
            for (int i = 0; i < 1; i++) {
                queryKNN<point, 0, 1>(Dim, query_points, 1, pkd, kdknn, k[i], true);
            }
            LOG << ENDL;
        }
        cnt++;
        l = r;
    }
    delete[] kdknn;

    return;
}

template<typename T>
class counter_iterator {
   private:
    struct accept_any {
        template<typename U>
        accept_any& operator=(const U&) {
            return *this;
        }
    };

   public:
    typedef std::output_iterator_tag iterator_category;

    counter_iterator(T& counter) : counter(counter) {}
    counter_iterator(const counter_iterator& other) : counter(other.counter) {}

    bool operator==(const counter_iterator& rhs) const { return counter == rhs.counter; }
    bool operator!=(const counter_iterator& rhs) const { return counter != rhs.counter; }

    accept_any operator*() const {
        ++counter.get();
        return {};
    }

    counter_iterator& operator++() {  // ++a
        return *this;
    }
    counter_iterator operator++(int) {  // a++
        return *this;
    }

   protected:
    std::reference_wrapper<T> counter;
};

//*---------- generate points within a 0-box_size --------------------
template<typename point>
void generate_random_points(parlay::sequence<point>& wp, coord _box_size, long n, int Dim) {
    coord box_size = _box_size;

    std::random_device rd;      // a seed source for the random number engine
    std::mt19937 gen_mt(rd());  // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<int> distrib(1, box_size);

    parlay::random_generator gen(distrib(gen_mt));
    std::uniform_int_distribution<int> dis(0, box_size);

    wp.resize(n);
    // generate n random points in a cube
    parlay::parallel_for(
        0, n,
        [&](long i) {
            auto r = gen[i];
            for (int j = 0; j < Dim; j++) {
                wp[i].pnt[j] = dis(r);
            }
        },
        1000);
    return;
}

template<typename point>
std::pair<size_t, int> read_points(const char* iFile, parlay::sequence<point>& wp, int K, bool withID = false) {
    using coord = typename point::coord;
    using coords = typename point::coords;
    static coords samplePoint;
    parlay::sequence<char> S = readStringFromFile(iFile);
    parlay::sequence<char*> W = stringToWords(S);
    size_t N = std::stoul(W[0], nullptr, 10);
    int Dim = atoi(W[1]);
    assert(N >= 0 && Dim >= 1 && N >= K);

    auto pts = W.cut(2, W.size());
    assert(pts.size() % Dim == 0);
    size_t n = pts.size() / Dim;
    auto a = parlay::tabulate(Dim * n, [&](size_t i) -> coord {
        if constexpr (std::is_integral_v<coord>)
            return std::stol(pts[i]);
        else if (std::is_floating_point_v<coord>)
            return std::stod(pts[i]);
    });
    wp.resize(N);
    parlay::parallel_for(0, n, [&](size_t i) {
        for (int j = 0; j < Dim; j++) {
            wp[i].pnt[j] = a[i * Dim + j];
            if constexpr (std::is_same_v<point, PointType<coord, samplePoint.size()>>) {
            } else {
                wp[i].id = i;
            }
        }
    });
    return std::make_pair(N, Dim);
}
