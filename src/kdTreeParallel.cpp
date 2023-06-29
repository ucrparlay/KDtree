#include "kdTreeParallel.h"

int Dim, Q, K;
long N;

int
main( int argc, char* argv[] )
{
   points wp;
   std::string name( argv[1] );
   if( name.find( "/" ) != std::string::npos )
   {
      name = name.substr( name.rfind( "/" ) + 1 );
      std::cout << name << " ";

      freopen( argv[1], "r", stdin );

      scanf( "%ld %d", &N, &Dim );
      wp.resize( N );
      for( int i = 0; i < N; i++ )
      {
         wp[i].id = i;
         for( int j = 0; j < Dim; j++ )
         {
            scanf( "%ld", &wp[i].pnt[j] );
         }
      }
   }

   node* KDroot = build( wp.cut( 0, wp.size() ), 0, Dim );
   K = 2;

   kBoundedQueue<coord>* bq = new kBoundedQueue<coord>[N];
   for( int i = 0; i < N; i++ )
   {
      bq[i].resize( K );
   }
   for( int i = 0; i < N; i++ )
   {
      // LOG << "------query " << i << ENDL;
      k_nearest( KDroot, wp[i], 0, Dim, bq[i] );
      std::cout << bq[i].top() << std::endl;
   }
   return 0;
}