#include <Catch2/catch.hpp>
#include <vector>
#include <random>
#include <uEmbedded-pp/math/matrix_static.hpp>
extern "C"
{
#include "uEmbedded/algorithm.h"
}

using namespace std;

TEST_CASE("Test lowerbound", "[algorithm]")
{
    vector<double> v;
    uniform_real_distribution<> db;
    random_device rd;
    mt19937_64 mt(rd());

    enum
    {
        numElem = 0x20
    };
    v.reserve(numElem);

    for (size_t i = 0; i < numElem; i++)
    {
        double val = db(mt);
        auto idx = lowerbound(v.data(), &val, sizeof(double), v.size(),
                              [](auto a, auto b) -> int { auto v = *( double*) a - *( double*) b; return v < 0 ? -1 : v > 0 ? 1 : 0; });
        REQUIRE(idx <= v.size());
        v.insert(v.begin() + idx, val);
    }

    // for ( int i = 0; i < numElem; ++i )
    // {
    //     v.push_back(db(mt));
    // }

    sort(v.begin(), v.end());

    for (int i = 0; i < numElem; ++i)
    {
        double val = db(mt); 
        auto org = std::lower_bound(v.begin(), v.end(), val) - v.begin();
        auto idx = lowerbound(v.data(), &val, sizeof(double), numElem,
                              [](auto a, auto b) -> int { auto v = *( double*) a - *( double*) b; return v < 0 ? -1 : v > 0 ? 1 : 0; });

        REQUIRE(org == idx);
    } 
}