#include "testFramework.h"

template<typename point>
void
testParallelKDtree( const int& Dim, const int& LEAVE_WRAP, parlay::sequence<point>& wp,
                    const size_t& N, const int& K, const int& rounds,
                    const string& insertFile, const int& tag, const int& queryType ) {
    using tree = ParallelKDtree<point>;
    using points = typename tree::points;
    using node = typename tree::node;
    using interior = typename tree::interior;
    using leaf = typename tree::leaf;
    using node_tag = typename tree::node_tag;
    using node_tags = typename tree::node_tags;
    using box = typename tree::box;
    using boxs = parlay::sequence<box>;

    // auto boxs = gen_rectangles<point>( 1000, 2, wp, Dim );
    // for ( int i = 0; i < 10; i++ ) {
    //   LOG << boxs[i].first << " " << boxs[i].second << ENDL;
    // }
    // return;

    if ( N != wp.size() ) {
        puts( "input parameter N is different to input points size" );
        abort();
    }

    tree pkd;

    points wi;
    if ( tag >= 0 ) {
        auto [nn, nd] = read_points<point>( insertFile.c_str(), wi, K );
        if ( nd != Dim ) {
            puts( "read inserted points dimension wrong" );
            abort();
        }
    }

    Typename* kdknn = nullptr;

    //* begin test
    buildTree<point>( Dim, wp, rounds, pkd );

    //* batch insert
    if ( tag >= 1 ) {

        batchInsert<point>( pkd, wp, wi, Dim, rounds );

        if ( tag == 1 ) {
            wp.append( wi );
        }
    }

    //* batch delete
    if ( tag >= 2 ) {
        assert( wi.size() );
        batchDelete<point>( pkd, wp, wi, Dim, rounds );
    }

    if ( queryType & ( 1 << 0 ) ) {  //* KNN
        kdknn = new Typename[wp.size()];
        // int k[3] = { 1, 10, 100 };
        int k[3] = { 1, 10, 100 };
        // for ( int i = 0; i < 3; i++ ) {
        for ( int i = 0; i < 1; i++ ) {
            queryKNN<point>( Dim, wp, rounds, pkd, kdknn, k[i], false );
        }
        delete[] kdknn;
    }

    int recNum = 100;

    if ( queryType & ( 1 << 1 ) ) {  //* range count
        kdknn = new Typename[recNum];
        int type[3] = { 0, 1, 2 };

        for ( int i = 0; i < 3; i++ ) {
            rangeCountFix<point>( wp, pkd, kdknn, rounds, type[i], recNum, Dim );
        }

        delete[] kdknn;
    }

    if ( queryType & ( 1 << 2 ) ) {  //* range query

        int type[3] = { 0, 1, 2 };
        for ( int i = 0; i < 3; i++ ) {
            //* run range count to obtain size
            kdknn = new Typename[recNum];
            auto queryBox = gen_rectangles( recNum, type[i], wp, Dim );
            parlay::parallel_for( 0, recNum, [&]( size_t i ) {
                kdknn[i] = pkd.range_count( queryBox[i] );
            } );

            //* reduce max size
            auto maxReduceSize = parlay::reduce(
                parlay::delayed_tabulate(
                    recNum, [&]( size_t i ) { return size_t( kdknn[i] ); } ),
                parlay::maximum<Typename>() );
            points Out( recNum * maxReduceSize );

            //* range query
            rangeQueryFix<point>( wp, pkd, kdknn, rounds, Out, type[i], recNum, Dim );
        }

        delete[] kdknn;
    }

    if ( queryType & ( 1 << 3 ) ) {  //* generate knn
        // generate_knn<point>( Dim, wp, K, "/data9/zmen002/knn/GeoLifeNoScale.pbbs.out"
        // );
    }

    if ( queryType & ( 1 << 4 ) ) {  //* batch insertion with fraction
        double ratios[10] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
        for ( int i = 0; i < 10; i++ ) {
            batchInsert<point>( pkd, wp, wi, Dim, rounds, ratios[i] );
        }
    }

    if ( queryType & ( 1 << 5 ) ) {  //* batch deletion with fraction
        double ratios[10] = { 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0 };
        points tmp;
        for ( int i = 0; i < 10; i++ ) {
            batchDelete<point>( pkd, wp, tmp, Dim, rounds, 0, ratios[i] );
        }
    }

    if ( queryType & ( 1 << 6 ) ) {  //* incremental Build
        double step[4] = { 0.1, 0.2, 0.25, 0.5 };
        for ( int i = 0; i < 4; i++ ) {
            incrementalBuild<point>( Dim, wp, rounds, pkd, step[i] );
        }
    }

    if ( queryType & ( 1 << 7 ) ) {  //* incremental Delete
        double step[4] = { 0.1, 0.2, 0.25, 0.5 };
        for ( int i = 0; i < 4; i++ ) {
            incrementalDelete<point>( Dim, wp, wi, rounds, pkd, step[i] );
        }
    }

    if ( queryType & ( 1 << 8 ) ) {  //* batch insertion then knn
        kdknn = new Typename[wp.size()];

        //* first normal build
        buildTree<point, 0>( Dim, wp, rounds, pkd );
        queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

        //* then incremental build
        incrementalBuild<point, 0>( Dim, wp, rounds, pkd, 0.1 );
        queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

        delete[] kdknn;
    }

    if ( queryType & ( 1 << 9 ) ) {  //* batch deletion then knn
        kdknn = new Typename[wp.size()];

        //* first normal build
        buildTree<point, 0>( Dim, wp, rounds, pkd );
        queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

        //* then incremental delete
        incrementalDelete<point, 0>( Dim, wp, wi, rounds, pkd, 0.1 );
        queryKNN<point>( Dim, wp, rounds, pkd, kdknn, K, false );

        delete[] kdknn;
    }

    std::cout << std::endl << std::flush;
    return;
}

int
main( int argc, char* argv[] ) {
    commandLine P(
        argc, argv,
        "[-k {1,...,100}] [-d {2,3,5,7,9,10}] [-n <node num>] [-t "
        "<parallelTag>] [-p <inFile>] [-r {1,...,5}] [-q {0,1}] [-i <_insertFile>]" );
    char* iFile = P.getOptionValue( "-p" );
    char* _insertFile = P.getOptionValue( "-i" );
    int K = P.getOptionIntValue( "-k", 100 );
    int Dim = P.getOptionIntValue( "-d", 3 );
    size_t N = P.getOptionLongValue( "-n", -1 );
    int tag = P.getOptionIntValue( "-t", 1 );
    int rounds = P.getOptionIntValue( "-r", 3 );
    int queryType = P.getOptionIntValue( "-q", 0 );

    int LEAVE_WRAP = 32;
    // parlay::sequence<PointType<coord, 15>> wp;
    parlay::sequence<PointID<coord, 15>> wp;
    std::string name, insertFile;

    //* initialize points
    if ( iFile != NULL ) {
        name = std::string( iFile );
        name = name.substr( name.rfind( "/" ) + 1 );
        std::cout << name << " ";
        // auto [n, d] = read_points<PointType<coord, 15>>( iFile, wp, K );
        auto [n, d] = read_points<PointID<coord, 15>>( iFile, wp, K );
        N = n;
        assert( d == Dim );
    } else {  //* construct data byself
        K = 100;
        // generate_random_points<PointType<coord, 15>>( wp, 1000000, N, Dim );
        generate_random_points<PointID<coord, 15>>( wp, 1000000, N, Dim );
        std::string name = std::to_string( N ) + "_" + std::to_string( Dim ) + ".in";
        std::cout << name << " ";
    }

    if ( tag >= 0 ) {
        if ( _insertFile == NULL ) {
            int id = std::stoi( name.substr( 0, name.find_first_of( '.' ) ) );
            if ( Dim != 2 ) id = ( id + 1 ) % 3;  //! MOD graph number used to test
            if ( !id ) id++;
            int pos = std::string( iFile ).rfind( "/" ) + 1;
            insertFile =
                std::string( iFile ).substr( 0, pos ) + std::to_string( id ) + ".in";
        } else {
            insertFile = std::string( _insertFile );
        }
    }

    assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

    // if ( tag == -1 ) {
    //   //* serial run
    //   // todo rewrite test serial code
    //   // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    // } else if ( Dim == 2 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 2> {
    //     return PointID<coord, 2>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 2>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
    //   insertFile,
    //                                          tag, queryType );
    // } else if ( Dim == 3 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 3> {
    //     return PointID<coord, 3>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 3>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
    //   insertFile,
    //                                          tag, queryType );
    // } else if ( Dim == 5 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 5> {
    //     return PointID<coord, 5>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 5>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
    //   insertFile,
    //                                          tag, queryType );
    // } else if ( Dim == 7 ) {
    //   auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointID<coord, 7> {
    //     return PointID<coord, 7>( wp[i].pnt.begin(), i );
    //   } );
    //   decltype( wp )().swap( wp );
    //   testParallelKDtree<PointID<coord, 7>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
    //   insertFile,
    //                                          tag, queryType );
    // }

    if ( tag == -1 ) {
        //* serial run
        // todo rewrite test serial code
        // testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
    } else if ( Dim == 2 ) {
        auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 2> {
            return PointType<coord, 2>( wp[i].pnt.begin() );
        } );
        decltype( wp )().swap( wp );
        testParallelKDtree<PointType<coord, 2>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                                 insertFile, tag, queryType );
    } else if ( Dim == 3 ) {
        auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 3> {
            return PointType<coord, 3>( wp[i].pnt.begin() );
        } );
        decltype( wp )().swap( wp );
        testParallelKDtree<PointType<coord, 3>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                                 insertFile, tag, queryType );
    } else if ( Dim == 5 ) {
        auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 5> {
            return PointType<coord, 5>( wp[i].pnt.begin() );
        } );
        decltype( wp )().swap( wp );
        testParallelKDtree<PointType<coord, 5>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                                 insertFile, tag, queryType );
    } else if ( Dim == 7 ) {
        auto pts = parlay::tabulate( N, [&]( size_t i ) -> PointType<coord, 7> {
            return PointType<coord, 7>( wp[i].pnt.begin() );
        } );
        decltype( wp )().swap( wp );
        testParallelKDtree<PointType<coord, 7>>( Dim, LEAVE_WRAP, pts, N, K, rounds,
                                                 insertFile, tag, queryType );
    }

    return 0;
}
