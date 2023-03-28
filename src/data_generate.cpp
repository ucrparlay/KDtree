#pragma GCC optimize( 3 )
#include <bits/stdc++.h>
#include <parlay/delayed_sequence.h>
#include <parlay/internal/get_time.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/utilities.h>
const double EPS = 1e-9;
#define rep( i, a, b ) for( int i = ( a ); i < ( b ); i++ )
#define per( i, a, b ) for( int i = ( a ); i > ( b ); i-- )
#define LL long long
#define Lson ( index * 2 )
#define Rson ( index * 2 + 1 )
#define MOD ( (int)1000000007 )
#define MAXN 1000 + 5
///**********************************START*********************************///
const size_t pnum = 5e5;
const size_t dim = 5;
const size_t numFile = 5;
const double dataRange = 1e9;

std::string path = "../benchmark/craft_var_node";
std::default_random_engine generator;
struct kd_node_t
{
   double x[dim];
   struct kd_node_t *left, *right;
   int num;         // number of nodes in subtree plus itself
   double mxx[dim]; // mxx[i] the maximum of sub points on dimension i
   double mnx[dim]; // mnx[i] the minimum
};

kd_node_t* node;

inline double
getRealRandom( const double& a, const double& b )
{
   std::uniform_real_distribution<double> distribution( a, b );
   return distribution( generator );
}

inline std::string
toString( const size_t& a )
{
   return std::to_string( a );
}

void
generatePoints( std::ofstream& f )
{
   f.precision( 6 );
   f << std::fixed << pnum << " " << dim << std::endl;
   for( int i = 0; i < pnum; i++ )
   {
      int idx = 0;
      for( int j = 0; j < dim; j++ )
      {
         double a = getRealRandom( -dataRange, dataRange );
         node[i].x[j] = a;
         f << std::fixed << a << " ";
      }
      f << std::endl;
   }

   f << pnum << std::endl;
   rep( i, 0, pnum )
   {
      f << 100 << " ";
      rep( j, 0, dim )
      {
         f << std::fixed << node[i].x[j] << " ";
      }
      f << std::endl;
   }
}

int
main()
{
#ifndef ONLINE_JUDGE
   freopen( "input.txt", "r", stdin );
#endif
   path += "/" + toString( pnum ) + "_" + toString( dim ) + "/";
   std::filesystem::create_directory( path );
   std::ofstream f;
   node = (kd_node_t*)malloc( pnum * sizeof( kd_node_t ) );

   for( size_t i = 0; i < numFile; i++ )
   {
      std::string newpath = path + toString( i ) + ".in";
      std::cout << newpath << std::endl;
      f.open( newpath );
      generatePoints( f );
      f.close();
   }
   return 0;
}