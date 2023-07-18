#include "kdTree.h"
#include "kdTreeParallel.h"

using Typename = coord;

double aveDeep = 0.0;

void
traverseSerialTree( KDnode<Typename>* KDroot, int deep ) {
   if( KDroot->isLeaf ) {
      aveDeep += deep;
      return;
   }
   traverseSerialTree( KDroot->left, deep + 1 );
   traverseSerialTree( KDroot->right, deep + 1 );
   return;
}

template <typename tree>
void
traverseParallelTree( typename tree::node* root, int deep ) {
   if( root->is_leaf ) {
      aveDeep += deep;
      return;
   }
   typename tree::interior* TI = static_cast<typename tree::interior*>( root );
   traverseParallelTree<tree>( TI->left, deep + 1 );
   traverseParallelTree<tree>( TI->right, deep + 1 );
   return;
}

void
testSerialKDtree( int Dim, int LEAVE_WRAP, Point<Typename>* kdPoint, size_t N,
                  int K ) {
   parlay::internal::timer timer;

   KDtree<Typename> KD;

   timer.start();
   KDnode<Typename>* KDroot = KD.init( Dim, LEAVE_WRAP, kdPoint, N );
   timer.stop();

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( parlay::make_slice( kdPoint, kdPoint + N ) );
   Typename* kdknn = new Typename[N];

   timer.reset();
   timer.start();
   kBoundedQueue<Typename> bq;
   double aveVisNum = 0.0;
   for( size_t i = 0; i < N; i++ ) {
      size_t visNodeNum = 0;
      bq.resize( K );
      KD.k_nearest( KDroot, &kdPoint[i], 0, bq, visNodeNum );
      kdknn[i] = bq.top();
      aveVisNum += ( 1.0 * visNodeNum ) / N;
   }

   timer.stop();
   std::cout << timer.total_time() << " " << std::flush;

   aveDeep = 0.0;
   traverseSerialTree( KDroot, 1 );
   std::cout << aveDeep / ( N / LEAVE_WRAP ) << " " << aveVisNum << std::endl
             << std::flush;

   //* delete
   // KD.destory( KDroot );
   // delete[] kdPoint;
   // delete[] kdknn;

   return;
}

template <typename tree>
void
testParallelKDtree( int Dim, int LEAVE_WRAP, typename tree::points wp, int N,
                    int K ) {
   using points = typename tree::points;
   using node = typename tree::node;
   parlay::internal::timer timer;
   points wo( wp.size() );
   tree pkd;
   timer.start();
   node* KDParallelRoot =
       pkd.build( wp.cut( 0, wp.size() ), wo.cut( 0, wo.size() ), 0, Dim );
   timer.stop();

   std::cout << timer.total_time() << " " << std::flush;

   //* start test
   parlay::random_shuffle( wp.cut( 0, N ) );
   // Typename* kdknn = new Typename[N];

   // kBoundedQueue<Typename>* bq = new kBoundedQueue<Typename>[N];
   // for( int i = 0; i < N; i++ ) {
   //    bq[i].resize( K );
   // }
   parlay::sequence<double> visNum =
       parlay::sequence<double>::uninitialized( N );
   double aveVisNum = 0.0;

   timer.reset();
   timer.start();

   // parlay::parallel_for( 0, N, [&]( size_t i ) {
   //    size_t visNodeNum = 0;
   //    k_nearest( KDParallelRoot, wp[i], Dim, bq[i], visNodeNum );
   //    kdknn[i] = bq[i].top();
   //    visNum[i] = ( 1.0 * visNodeNum ) / N;
   // } );

   timer.stop();
   std::cout << timer.total_time() << " " << std::flush;

   aveDeep = 0.0;
   traverseParallelTree<tree>( KDParallelRoot, 1 );
   std::cout << aveDeep / ( N / LEAVE_WRAP ) << " "
             << parlay::reduce( visNum.cut( 0, N ) ) << std::endl
             << std::flush;

   // delete_tree( KDParallelRoot );
   // points().swap( wp );
   // points().swap( wo );
   // delete[] bq;
   // delete[] kdknn;

   return;
}

int
main( int argc, char* argv[] ) {
   assert( argc >= 2 );
   int K = 100, LEAVE_WRAP = 16, Dim;
   long N;
   Point<Typename>* wp;
   std::string name( argv[1] );

   //* initialize points
   if( name.find( "/" ) != std::string::npos ) { //* read from file
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";
      auto f = freopen( argv[1], "r", stdin );
      K = std::stoi( argv[2] );
      assert( f != nullptr );

      scanf( "%ld%d", &N, &Dim );
      assert( N >= K );
      wp = new Point<Typename>[N];
      for( int i = 0; i < N; i++ ) {
         for( int j = 0; j < Dim; j++ ) {
            scanf( "%ld", &wp[i].x[j] );
         }
      }
   } else { //* construct data byself
      K = 100;
      coord box_size = 1000000;

      std::random_device rd;       // a seed source for the random number engine
      std::mt19937 gen_mt( rd() ); // mersenne_twister_engine seeded with rd()
      std::uniform_int_distribution<int> distrib( 1, box_size );

      parlay::random_generator gen( distrib( gen_mt ) );
      std::uniform_int_distribution<int> dis( 0, box_size );

      long n = std::stoi( argv[1] );
      N = n;
      Dim = std::stoi( argv[2] );
      wp = new Point<Typename>[N];
      // generate n random points in a cube
      parlay::parallel_for(
          0, n,
          [&]( long i ) {
             auto r = gen[i];
             for( int j = 0; j < Dim; j++ ) {
                wp[i].x[j] = dis( r );
             }
          },
          1000 );
      name = std::to_string( n ) + "_" + std::to_string( Dim ) + ".in";
      std::cout << name << " ";
   }

   assert( N > 0 && Dim > 0 && K > 0 && LEAVE_WRAP >= 1 );

   if( argc >= 4 ) {
      if( std::stoi( argv[3] ) == 0 )
         testSerialKDtree( Dim, LEAVE_WRAP, wp, N, K );
      else if( Dim == 3 ) {
         auto pts = parlay::tabulate(
             N, [&]( size_t i ) -> point3D { return point3D( wp[i].x ); } );
         delete[] wp;
         testParallelKDtree<ParallelKDtree<point3D>>( Dim, LEAVE_WRAP, pts, N,
                                                      K );
      } else if( Dim == 5 ) {
         auto pts = parlay::tabulate(
             N, [&]( size_t i ) -> point5D { return point5D( wp[i].x ); } );
         delete[] wp;
         testParallelKDtree<ParallelKDtree<point5D>>( Dim, LEAVE_WRAP, pts, N,
                                                      K );
      } else if( Dim == 7 ) {
         auto pts = parlay::tabulate(
             N, [&]( size_t i ) -> point7D { return point7D( wp[i].x ); } );
         delete[] wp;
         testParallelKDtree<ParallelKDtree<point7D>>( Dim, LEAVE_WRAP, pts, N,
                                                      K );
      }
   }

   return 0;
}

template void
testParallelKDtree<ParallelKDtree<point3D>>( int Dim, int LEAVE_WRAP,
                                             ParallelKDtree<point3D>::points wp,
                                             int N, int K );
template void
testParallelKDtree<ParallelKDtree<point5D>>( int Dim, int LEAVE_WRAP,
                                             ParallelKDtree<point5D>::points wp,
                                             int N, int K );
template void
testParallelKDtree<ParallelKDtree<point7D>>( int Dim, int LEAVE_WRAP,
                                             ParallelKDtree<point7D>::points wp,
                                             int N, int K );

template void
traverseParallelTree<ParallelKDtree<point3D>>(
    ParallelKDtree<point3D>::node* root, int deep );

template void
traverseParallelTree<ParallelKDtree<point5D>>(
    ParallelKDtree<point5D>::node* root, int deep );

template void
traverseParallelTree<ParallelKDtree<point7D>>(
    ParallelKDtree<point7D>::node* root, int deep );